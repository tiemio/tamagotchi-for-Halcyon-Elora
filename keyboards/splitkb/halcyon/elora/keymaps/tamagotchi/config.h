// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define VIAL_KEYBOARD_UID {0x91, 0xC0, 0x72, 0xB1, 0xEA, 0x3F, 0xF0, 0xBA}

#define VIAL_UNLOCK_COMBO_ROWS { 0, 6 }
#define VIAL_UNLOCK_COMBO_COLS { 1, 1 }

// Increase the EEPROM size for layout options
#define VIA_EEPROM_LAYOUT_OPTIONS_SIZE 2

#define RGB_MATRIX_FRAMEBUFFER_EFFECTS
#define RGB_MATRIX_KEYPRESSES

#define DYNAMIC_KEYMAP_LAYER_COUNT 8

#define SPLIT_WPM_ENABLE

// Disable default layer/lock display — tamagotchi replaces it
#define HLC_DISABLE_DEFAULT_DISPLAY

// Uncomment to run the tamagotchi on the right display instead of left
// #define TAMAGOTCHI_ON_RIGHT
