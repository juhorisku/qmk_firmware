# KeyStateRGB keymap for the GMMK Numpad

The numpad as a Windows Dynamic Lighting device. `LAMPARRAY_ENABLE = yes` adds an HID
LampArray interface beside the keyboard, so Windows lists the numpad under *Dynamic Lighting*
and any LampArray consumer can drive its 31 lamps (17 keys, 14 underglow). Autonomous mode
keeps QMK's own RGB matrix effects; a host that takes control paints over them.

The one keymap change from `default`: NUM is `LT(1, KC_NUM)`, Num Lock on tap and the
function layer on hold, because a numpad that never sends Num Lock cannot show it.

Build: `qmk compile -kb gmmk/numpad -km keystatergb`. Flash: from the KeyStateRGB app, which
backs the unit's flash up first and can put Glorious's image back (its requirements R7, R8, A26).
