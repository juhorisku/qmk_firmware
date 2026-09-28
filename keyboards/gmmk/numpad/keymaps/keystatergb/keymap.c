// Copyright 2021 Glorious, LLC <salman@pcgamingrace.com>
// Modified 2022 by rustedaperture for qmk_firmware
// Modified 2026 for KeyStateRGB: NUM toggles Num Lock on tap and opens the layer on hold
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#ifdef MIDI_ENABLE
#    include "analog.h"
#    include "qmk_midi.h"
#endif

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
    KC_P4,         KC_P5,     KC_P6,     KC_CALC,
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

// Potentiometer slider, MIDI control, as in the default keymap. A diagnostic build with the
// QMK console on has no endpoint left for MIDI (the WB32's budget is four besides control:
// shared, MIDI in, MIDI out, LampArray), so it is built with MIDI_ENABLE=no and no slider.
#ifdef MIDI_ENABLE
static uint8_t divisor = 0;

static void slider(void) {
    if (divisor++) { /* only run the slider function 1/256 times it's called */
        return;
    }
    midi_send_cc(&midi_device, 2, 0x3E, 0x7F + (analogReadPin(SLIDER_PIN) >> 3));
}

void housekeeping_task_user(void) {
    slider();
}
#endif
