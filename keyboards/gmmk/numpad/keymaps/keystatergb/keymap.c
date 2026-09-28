// Copyright 2021 Glorious, LLC <salman@pcgamingrace.com>
// Modified 2022 by rustedaperture for qmk_firmware
// Modified 2026 for KeyStateRGB: NUM toggles Num Lock on tap and opens the layer on hold; the
// dial, its press and the slider go to the host over MIDI while the KeyStateRGB app claims them
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#ifdef MIDI_ENABLE
#    include "analog.h"
#    include "qmk_midi.h"
#endif

enum custom_keycodes {
    DIAL_PUSH = SAFE_RANGE,
};

// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

//      NUM      /       *       -
//      7        8       9       +
//      4        5       6       ENCODER
//      1        2       3       RET
//      0                        .

  // The default keymap makes NUM a bare layer key, so the numpad never sends Num Lock and the
  // host's Num Lock state never changes from it. KeyStateRGB lights Num Lock from the host's
  // state (R1, R4), so NUM has to send it: tap for Num Lock, hold for the layer.
  [0] = LAYOUT(
    LT(1, KC_NUM), KC_PSLS,   KC_PAST,   KC_PMNS,
    KC_P7,         KC_P8,     KC_P9,     KC_PPLS,
    KC_P4,         KC_P5,     KC_P6,     DIAL_PUSH,
    KC_P1,         KC_P2,     KC_P3,     KC_PENT,
    KC_P0,                               KC_PDOT
  ),
  [1] = LAYOUT(
    _______,   KC_PSLS,   KC_PAST,   KC_PMNS,
    KC_P7,     RM_VALU,   KC_P9,     KC_PPLS,
    RM_PREV,   KC_P5,     RM_NEXT,   KC_CALC,
    KC_P1,     RM_VALD,   KC_P3,     KC_PENT,
    RM_TOGG,                         QK_BOOT
  )
};
// clang-format on

// The way into the bootloader stays the one Glorious's firmware taught: Plus and Enter held
// while plugging in (the user, 2026-09-28: keeping the entry the same is more obvious). NUM
// alone, QMK's usual bootmagic key for this board, works too. Bootmagic has scanned the matrix
// twice before asking. Positions from keyboard.json: NUM [0,0], "+" [1,3], "RET" [3,3].
bool bootmagic_should_reset(void) {
    bool num   = matrix_get_row(0) & (1 << 0);
    bool plus  = matrix_get_row(1) & (1 << 3);
    bool enter = matrix_get_row(3) & (1 << 3);
    return num || (plus && enter);
}

/*
 * The numpad's controls and the KeyStateRGB app: MIDI control-change messages on channel 2
 * (zero-based), the contract in the app's KeyStateRGB.Core/Numpad/NumpadMessage.cs. The
 * numbers here and there have to agree.
 *
 *   CC 62  numpad -> app  slider position 0..127 (Glorious's number)
 *   CC 63  numpad -> app  dial: 65 one detent clockwise, 63 one counter-clockwise
 *   CC 64  numpad -> app  dial press: 127 down, 0 up
 *   CC 100 app -> numpad  127: the app is in control of the dial; repeated every 2 s
 *
 * While the app's claim is fresh (under 5 s old) the dial's turns and presses only go to the
 * app. Without a claim the numpad does what it did before the app existed: the dial is the
 * system volume and its press opens the calculator (R6: it keeps working without the app).
 * The slider always reports; it never had a meaning of its own.
 *
 * A diagnostic build with the QMK console on has no endpoint left for MIDI (the WB32 has four
 * besides control: shared, MIDI in, MIDI out, LampArray), so it is built with MIDI_ENABLE=no,
 * and then everything here falls back to the firmware's own defaults.
 */
#ifdef MIDI_ENABLE

#    define KSR_CHANNEL 2
#    define KSR_CC_SLIDER 62
#    define KSR_CC_DIAL 63
#    define KSR_CC_DIAL_PRESS 64
#    define KSR_CC_CLAIM 100
#    define KSR_CLAIM_TIMEOUT_MS 5000

static bool     claimed_ever = false;
static uint32_t claimed_at   = 0;

static bool app_claims(void) {
    return claimed_ever && timer_elapsed32(claimed_at) < KSR_CLAIM_TIMEOUT_MS;
}

static void on_midi_cc(MidiDevice *device, uint8_t channel, uint8_t number, uint8_t value) {
    (void)device;
    if (channel != KSR_CHANNEL || number != KSR_CC_CLAIM) {
        return;
    }
    if (value == 127) {
        claimed_ever = true;
        claimed_at   = timer_read32();
    } else {
        claimed_ever = false; // the app released the dial on its way out
    }
}

void keyboard_post_init_user(void) {
    // Replaces qmk_midi.c's placeholder callback; MIDI is set up before post-init runs.
    midi_register_cc_callback(&midi_device, on_midi_cc);
}

bool encoder_update_user(uint8_t index, bool clockwise) {
    (void)index;
    if (app_claims()) {
        midi_send_cc(&midi_device, KSR_CHANNEL, KSR_CC_DIAL, clockwise ? 65 : 63);
        return false; // handled here; QMK's default (volume keys) stays off
    }
    return true; // QMK's default: volume up and down
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (keycode != DIAL_PUSH) {
        return true;
    }
    if (app_claims()) {
        midi_send_cc(&midi_device, KSR_CHANNEL, KSR_CC_DIAL_PRESS, record->event.pressed ? 127 : 0);
    } else if (record->event.pressed) {
        register_code(KC_CALC);
    } else {
        unregister_code(KC_CALC);
    }
    return false;
}

// The potentiometer, read every 256th call as Glorious's keymap did, but sent as a clean
// 0..127 and only when it has moved: Glorious's 0x7F + value wrapped past 127, and sending it
// unchanged ten times a second was noise the host had to ignore. A change of one unit is ADC
// noise unless it reaches an end of travel.
static uint8_t scan_divisor  = 0;
static int16_t last_position = -1;

static void slider(void) {
    if (scan_divisor++) {
        return;
    }
    uint8_t position = analogReadPin(SLIDER_PIN) >> 3; // 10-bit reading to 0..127
    int16_t delta    = (int16_t)position - last_position;
    if (delta < 0) delta = -delta;
    bool at_end = position == 0 || position == 127;
    if (last_position >= 0 && delta < 2 && !(at_end && delta != 0)) {
        return;
    }
    last_position = position;
    midi_send_cc(&midi_device, KSR_CHANNEL, KSR_CC_SLIDER, position);
}

void housekeeping_task_user(void) {
    slider();
}

#else

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    // Without MIDI there is no app to talk to: the press is the calculator, as in the default keymap.
    if (keycode != DIAL_PUSH) {
        return true;
    }
    if (record->event.pressed) {
        register_code(KC_CALC);
    } else {
        unregister_code(KC_CALC);
    }
    return false;
}

#endif
