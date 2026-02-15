# Tamagotchi Cat for splitkb Halcyon Elora

A tamagotchi-style virtual pet that lives on your Halcyon Elora's TFT display. A pixel art cat reacts to your typing, needs feeding, sleeps when you're idle, and occasionally gets visited by a mischievous orange cat.

## Features

- **Your cat reacts to typing** — walks around, chases food icons, sits when idle, sleeps after 10 minutes of inactivity
- **Health system** — health drains slowly over time; type to spawn food icons that restore HP. Stop typing too long and your cat dies (but revives when you start typing again!)
- **WPM display** — your current words-per-minute shown at the top
- **Heart meter** — 5 hearts showing your cat's health
- **Level & XP system** — gain XP by typing, level up over time. The level bar at the bottom changes color as you progress (green → cyan → blue → purple → gold)
- **Orange cat encounters** — every ~5 minutes on average, an orange cat wanders onto screen, gets angry, and your cat chases it away
- **Vial compatible** — full Vial support for remapping keys without reflashing

## What it looks like

The 135x240 TFT shows:
```
  ┌─────────────┐
  │   120 WPM   │  ← typing speed
  │  ♥♥♥♥♡      │  ← health hearts
  │             │
  │    🐱       │  ← your cat
  │        🐟   │  ← food icon
  │             │
  │   Lv 12     │  ← level
  │ ████░░░░░░░ │  ← XP bar
  └─────────────┘
```

The other half runs the default Game of Life animation.

## Quick start — pre-built firmware

Check the [Releases](../../releases) page for pre-built `.uf2` files:

- **`tamagotchi-elora-left.uf2`** — tamagotchi on the left display (default)
- **`tamagotchi-elora-right.uf2`** — tamagotchi on the right display

Flash by holding the BOOT button on your Elora's RP2040, then drag the `.uf2` file onto the USB drive that appears.

> New builds are created automatically on every push to `main`.

## Building from source

### Option A: Use this repo as your QMK userspace

This repository is structured as a complete QMK userspace. Clone it and compile directly:

```bash
git clone https://github.com/YOUR_USERNAME/tamagotchi-for-Halcyon-Elora.git
cd tamagotchi-for-Halcyon-Elora

# Set up QMK with vial-qmk (first time only)
pip install qmk
qmk setup -H ~/qmk_firmware vial-kb/vial-qmk -b vial -y

# Compile
qmk compile -kb splitkb/halcyon/elora/rev2 -km tamagotchi -e HLC_TFT_DISPLAY=1
```

> **macOS note:** If `arm-none-eabi-gcc` is not on your PATH (common with Homebrew):
> ```bash
> export PATH="/opt/homebrew/Cellar/arm-none-eabi-gcc@8/8.5.0_2/bin:/opt/homebrew/Cellar/arm-none-eabi-binutils/2.41/bin:$PATH"
> ```

### Option B: Copy into your existing userspace

1. Copy the keymap folder:
   ```bash
   cp -r keyboards/splitkb/halcyon/elora/keymaps/tamagotchi/ \
     /path/to/your/userspace/keyboards/splitkb/halcyon/elora/keymaps/tamagotchi/
   ```

2. Replace the display module to add the `HLC_DISABLE_DEFAULT_DISPLAY` guards:
   ```bash
   cp users/halcyon_modules/splitkb/hlc_tft_display/hlc_tft_display.c \
     /path/to/your/userspace/users/halcyon_modules/splitkb/hlc_tft_display/hlc_tft_display.c
   ```

3. Compile:
   ```bash
   qmk compile -kb splitkb/halcyon/elora/rev2 -km tamagotchi -e HLC_TFT_DISPLAY=1
   ```

## Customizing the keymap

The included `keymap.json` provides a sensible default (QWERTY + Dvorak + Colemak with nav/symbol/function layers), but you can customize it:

- Edit `keymap.json` directly, or
- Use [Vial](https://get.vial.today/) to remap keys live after flashing

## Configuration

### Display side

By default, the tamagotchi runs on the **left** display. If your Elora only has a display on the right half (or you prefer the right side), uncomment this line in `config.h`:

```c
#define TAMAGOTCHI_ON_RIGHT
```

Or grab the pre-built `tamagotchi-elora-right.uf2` from the [Releases](../../releases) page.

### Gameplay values

You can tweak gameplay values at the top of `tamagotchi.c`:

| Define | Default | Description |
|--------|---------|-------------|
| `FRAME_MS` | `100` | Frame interval in ms (100 = 10 FPS) |
| `DRAIN_MS` | `108000` | Health drain interval (~3h from full to dead) |
| `IDLE_SLEEP_MS` | `600000` | Idle time before cat sleeps (10 min) |
| `ORANGE_CHECK_MS` | `30000` | Orange cat spawn check interval (30s) |
| `ORANGE_SPAWN_PCT` | `10` | Spawn chance per check (10% = ~5 min avg) |
| `MOVE_SPEED` | `2` | Cat movement speed in px/frame |

### Debug mode

To test the orange cat encounter, temporarily set:
```c
#define ORANGE_CHECK_MS    10000   // check every 10s
#define ORANGE_SPAWN_PCT   100     // guaranteed spawn
#define SHOW_FRAME_TIMING  1       // show ms/frame in bottom-right
```

## Technical details

- All sprites are hand-crafted 16x16 pixel art at 2 bits per pixel, rendered at 3x scale (48x48 on screen)
- Rendering uses QMK's Quantum Painter surface buffer for tear-free compositing
- RLE row-merging reduces SPI calls (~40 `qp_rect` calls per cat instead of ~256)
- Screen-bounds clipping prevents expensive off-screen drawing
- Proximity-based dirty tracking: sprites only force-redraw when overlapping
- Frame timing stays under 20ms even during orange cat encounters

## License

This project is licensed under the **GNU General Public License v2.0 or later** (GPL-2.0-or-later), consistent with QMK firmware and splitkb's Halcyon modules.

See [LICENSE](LICENSE) for the full text.

### Attribution

- Halcyon display framework: Copyright 2024 [splitkb.com](https://splitkb.com)
- Tamagotchi game code: original work, built on splitkb's Quantum Painter framework
