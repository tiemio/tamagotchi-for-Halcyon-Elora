// tamagotchi.c — Tamagotchi cat display for Halcyon Elora TFT (left side)
// Portrait 135x240, pixel art cat with health system, animations.
// Sprites extracted from "Cat Sprite Sheet.png".

#include "hlc_tft_display/hlc_tft_display.h"
#include <stdlib.h>

// ─── Screen ───
#define SCR_W 135
#define SCR_H 240

// ─── Scale factors ───
#define CAT_SCALE   3
#define ICON_SCALE  2
#define DIGIT_SCALE 3

// ─── Bitmap sizes ───
#define CAT_BMP_W  16
#define CAT_BMP_H  16
#define ICON_BMP_W  8
#define ICON_BMP_H  8

// ─── Display sizes (bitmap × scale) ───
#define CAT_W  (CAT_BMP_W * CAT_SCALE)     // 32
#define CAT_H  (CAT_BMP_H * CAT_SCALE)     // 32
#define ICON_W (ICON_BMP_W * ICON_SCALE)    // 16
#define ICON_H (ICON_BMP_H * ICON_SCALE)    // 16

// ─── Layout ───
#define WPM_Y       2
#define HEARTS_Y    20
#define GAME_Y      34
#define LVL_H       14
#define LVL_Y       (SCR_H - LVL_H)            // 226
#define GAME_H      (LVL_Y - GAME_Y)           // 192

// ─── Heart display (2x scale, 5 hearts) ───
#define HEART_BMP    7
#define HEART_SCALE  2
#define HEART_DISP   (HEART_BMP * HEART_SCALE)  // 14
#define HEART_COUNT  5
#define HEART_PITCH  (HEART_DISP + 3)           // 17
#define HEARTS_W     (HEART_COUNT * HEART_DISP + (HEART_COUNT - 1) * 3) // 82
#define HEARTS_X     ((SCR_W - HEARTS_W) / 2)

// ─── Level bar ───
#define LVL_SCALE   2
#define XP_BAR_H    3
#define XP_BAR_PAD  4
#define XP_BAR_W    (SCR_W - 2 * XP_BAR_PAD)   // 127

// ─── Timing & gameplay ───
#define FRAME_MS        100     // 10 FPS
#define DRAIN_MS        108000  // −1 HP every 108 s (~3 h full→dead)
#define IDLE_SLEEP_MS   600000  // 10 min idle → sleep
#define MAX_ICONS       3
#define MAX_HEALTH      100
#define REVIVE_HEALTH   20
#define ICON_HP_GAIN    5
#define MOVE_SPEED      2       // px/frame (compensates for 5 FPS)
#define POUNCE_DIST     40      // Manhattan px to start chasing icon
#define OVERLAY_PAD     16      // extra px above cat for zzz/? restore

// ─── Animation states ───
enum {
    ANIM_IDLE = 0,   // standing, tail wag
    ANIM_WALK,       // walking to target
    ANIM_SIT,        // sitting, waiting (idle < 10 min)
    ANIM_SLEEP,      // lying down (idle ≥ 10 min)
    ANIM_DEAD,       // lying down, dim
    ANIM_FLEE,       // running away from orange cat
};

// ─── Orange cat encounter phases ───
enum {
    ORANGE_NONE = 0,
    ORANGE_ENTER,    // walks in from edge
    ORANGE_IDLE,     // pauses, looks at main cat
    ORANGE_MAD,      // angry animation
    ORANGE_CHASE,    // main cat flees
};

// ─── Orange encounter timing ───
#define ORANGE_CHECK_MS    30000  // check spawn every 30s
#define ORANGE_SPAWN_PCT   10    // 10% chance per check (~5 min avg)
#define ORANGE_IDLE_MS     1500  // pause before getting mad
#define ORANGE_MAD_MS      2000  // angry phase

// ═══════════════════════════════════════════════════════════════════════
// SPRITE DATA — extracted from Cat Sprite Sheet.png
// 16×16, 2 bits per pixel packed in uint32_t (bits[31:30]=px0 .. bits[1:0]=px15)
// Color map: 0=transparent, 1=dark outline, 2=medium gray, 3=light fill
// ═══════════════════════════════════════════════════════════════════════

// HSV values for the 3 cat colors (index 1,2,3)
static const uint8_t cat_palette[4][3] = {
    {  0,   0,   0},   // 0: transparent (unused)
    {  0,   0,  46},   // 1: dark outline
    {  0,   0, 181},   // 2: medium body
    {  0,   0, 224},   // 3: light fill
};

// Orange cat palette (same structure, orange tones)
static const uint8_t orange_palette[4][3] = {
    {  0,   0,   0},   // 0: transparent (unused)
    { 12, 255,  55},   // 1: dark orange-brown outline
    { 16, 240, 190},   // 2: medium orange body
    { 20, 200, 235},   // 3: light orange/cream fill
};

// Walk: row 2, 4 frames — standing pose (used for SIT state)
static const uint32_t anim_walk[4][16] = {
    {0x00000000,0x00000000,0x00000000,0x00000000,0x00050140,0x00065640,0x0006FF40,0x00065940,
     0x0006FF40,0x0006AA40,0x001AF540,0x001ADBD0,0x001BD540,0x056B7400,0x1A6B7400,0x15555400},
    {0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00050140,0x00065640,0x0006FF40,
     0x00065940,0x0006FF40,0x001AF540,0x001A9BD0,0x001BD540,0x056B7400,0x1A6B7400,0x15555400},
    {0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00141400,0x00195500,0x00065900,
     0x0006FF40,0x0006FF40,0x001AA540,0x001ADBD0,0x001BD540,0x056B7400,0x1A6B7400,0x15555400},
    {0x00000000,0x00000000,0x00000000,0x00000000,0x00141400,0x00195500,0x00065900,0x0006FF40,
     0x0006FF40,0x0006AA40,0x001AF540,0x001ADBD0,0x001BD540,0x056B7400,0x1A6B7400,0x15555400},
};

// Trot: row 5, all 8 frames — full pounce/trot cycle
static const uint32_t anim_trot[8][16] = {
    {0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00005050,0x01406550,0x07406FD0,
     0x07406DA4,0x07406FF4,0x07556AA4,0x01FFFFD0,0x01FFFFD0,0x01DFDED0,0x01955DD0,0x01451450},
    {0x00000000,0x00000000,0x00000000,0x00000000,0x00014140,0x05019540,0x1D006FD0,0x1D006DA4,
     0x07406FF4,0x07556AA4,0x05FFF595,0x1FFFFF7D,0x15555554,0x00000000,0x00000000,0x00000000},
    {0x00000000,0x00000000,0x00000000,0x00014140,0x40019540,0xD0006FD0,0x74006DA4,0x1D006FF4,
     0x055565A5,0x1FFFFF55,0x15FFF554,0x00555000,0x00000000,0x00000000,0x00000000,0x00000000},
    {0x00000000,0x00000000,0x00000000,0x00000000,0x00014140,0x50019540,0x74006FD0,0x1D006DA4,
     0x07406FF4,0x01556AA4,0x05FFF595,0x1FFFFF7D,0x15555554,0x00000000,0x00000000,0x00000000},
    {0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00005050,0x05006550,
     0x1F406FD0,0x05D06DA4,0x01D06FF4,0x01556AA4,0x01FFFFD0,0x01DFDED0,0x00657740,0x00555140},
    {0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00014140,0x05419550,0x1FD06FD0,
     0x05D06DA4,0x01556FF4,0x01FFEAA4,0x01F7FF50,0x01FDDD40,0x005F57D0,0x00054140,0x00000000},
    {0x00000000,0x00000000,0x00000000,0x00000000,0x00514140,0x01D19550,0x01D06FD0,0x07406DA4,
     0x07556FF4,0x01FFEAA4,0x01F5FF50,0x01FF5D50,0x00555550,0x00000000,0x00000000,0x00000000},
    {0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x01414140,0x07419550,0x07406FD0,
     0x07406DA4,0x07556FF4,0x01FFEAA4,0x01F7FF50,0x01FDDD40,0x005F57D0,0x00054140,0x00000000},
};

// Sit: row 7, 4 frames — sitting with thought bubble, waiting
static const uint32_t anim_sit[4][16] = {
    {0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00014140,0x05019540,0x1D01BF40,
     0x1D01BF40,0x1D01B690,0x1D55AA90,0x07FFFF40,0x07FFFF40,0x077F7B40,0x06557740,0x05145140},
    {0x00000000,0x00000000,0x00000000,0x00000000,0x00000500,0x00001900,0x05051D00,0x1D067F40,
     0x1D06FD40,0x1D06DA40,0x1D56AA50,0x07FFFFE4,0x07FFFD54,0x077F7400,0x06557400,0x05145000},
    {0x00000000,0x00000000,0x00000000,0x00000000,0x00000500,0x00001900,0x00051D00,0x01467F40,
     0x0746FD40,0x1D06DA40,0x1D56AA40,0x1FFFFD00,0x07FFFF40,0x077F76D0,0x06557550,0x05145000},
    {0x00000000,0x00000000,0x00000000,0x00000000,0x00000500,0x00001900,0x05051D00,0x1D067F40,
     0x1D06FD40,0x0746DA40,0x0756AA40,0x07FFFD00,0x07FFFF40,0x077F76D0,0x06557550,0x05145000},
};

// Sleep: row 6, 4 frames — lying on side
static const uint32_t anim_sleep[4][16] = {
    {0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,
     0x00005014,0x00006564,0x00057FF4,0x005F5654,0x01FF7FF4,0x157F5594,0x7F9DBF7D,0x55555555},
    {0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,
     0x00005014,0x00006564,0x00157FF4,0x017F5654,0x07FF7FF4,0x157F5594,0x7F9DBF7D,0x55555555},
    {0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,
     0x00005014,0x00006564,0x00557FF4,0x01FF5654,0x07FF7FF4,0x157F5594,0x7F9DBF7D,0x55555555},
    {0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,
     0x00005014,0x00006564,0x00157FF4,0x017F5654,0x07FF7FF4,0x157F5594,0x7F9DBF7D,0x55555555},
};

// Angry: row 9, 8 frames — orange cat action/angry
static const uint32_t anim_angry[8][16] = {
    {0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00005050,0x01406550,0x07406FD0,
     0x07406DA4,0x07406FF4,0x07556AA4,0x01FFFFD0,0x01FFFFD0,0x01DFDED0,0x01955DD0,0x01451450},
    {0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x14001414,0x1D001954,
     0x1D0059F4,0x1D05D969,0x1B5FDBFD,0x05FFDAA9,0x01FFFFD4,0x01D55ED0,0x01951DD0,0x01411450},
    {0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x14001414,0x1D001954,
     0x1D0559F4,0x1D1FD969,0x1B7FDBFD,0x05FFDAA9,0x01FFFFD4,0x01D55ED0,0x01951DD0,0x01411450},
    {0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x14001414,0x1D155954,
     0x1D7FD9F4,0x1D7FD969,0x1BFFDBFD,0x05FFDAA9,0x01F57FD4,0x01D55ED0,0x01951DD0,0x01411450},
    {0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x40000000,0xD0001414,0x74155954,
     0x1D7FD9F4,0x07FFD969,0x01FFDBFD,0x01FFDAA9,0x01F57FD4,0x01D55ED0,0x01951DD0,0x01411450},
    {0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x40000000,0xD0001414,0x74155954,
     0x1D7FD9F4,0x07FFD969,0x01FFDBFD,0x01FFDAA9,0x01F57FD4,0x01D55ED0,0x01951DD0,0x01411450},
    {0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x14001414,0x1D001954,
     0x1D0559F4,0x1D5FD969,0x1BFFDBFD,0x05FFDAA9,0x01FFFFD4,0x01D55ED0,0x01951DD0,0x01411450},
    {0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00005050,0x01406550,0x07406FD0,
     0x07406DA4,0x07406FF4,0x07556AA4,0x01FFFFD0,0x01FFFFD0,0x01DFDED0,0x01955DD0,0x01451450},
};

// ─── Icon sprites: fish, droplet, lightning — 8×8 mono ───
static const uint8_t icon_fish[8]      = {0x00,0x18,0x3E,0x7F,0x7F,0x3E,0x18,0x00};
static const uint8_t icon_droplet[8]   = {0x10,0x10,0x38,0x7C,0x7C,0xFE,0x7C,0x38};
static const uint8_t icon_lightning[8] = {0x08,0x18,0x30,0x7E,0x1C,0x18,0x30,0x20};
static const uint8_t* const icon_sprites[3] = {icon_fish, icon_droplet, icon_lightning};
static const uint8_t icon_colors[3][3] = {
    {20, 200, 255}, {140, 200, 255}, {42, 255, 255},
};

// ─── Heart sprites (7×7, bits 6..0) ───
static const uint8_t heart_full[7]  = {0x36,0x7F,0x7F,0x7F,0x3E,0x1C,0x08};
static const uint8_t heart_empty[7] = {0x36,0x41,0x41,0x41,0x22,0x14,0x08};

// ─── 3×5 digit font ───
static const uint8_t digits_3x5[10][5] = {
    {0x07,0x05,0x05,0x05,0x07}, {0x02,0x06,0x02,0x02,0x07},
    {0x07,0x01,0x07,0x04,0x07}, {0x07,0x01,0x07,0x01,0x07},
    {0x05,0x05,0x07,0x01,0x01}, {0x07,0x04,0x07,0x01,0x07},
    {0x07,0x04,0x07,0x05,0x07}, {0x07,0x01,0x02,0x02,0x02},
    {0x07,0x05,0x07,0x05,0x07}, {0x07,0x05,0x07,0x01,0x07},
};

// z and ? glyphs (3×5, same format as digits)
static const uint8_t glyph_z[5] = {0x07,0x01,0x02,0x04,0x07};
static const uint8_t glyph_q[5] = {0x06,0x01,0x02,0x00,0x02};
static const uint8_t glyph_l[5] = {0x04,0x04,0x04,0x04,0x07};
static const uint8_t glyph_v[5] = {0x05,0x05,0x05,0x05,0x02};
static const uint8_t glyph_d[5] = {0x06,0x05,0x05,0x05,0x06};
static const uint8_t glyph_e[5] = {0x07,0x04,0x07,0x04,0x07};
static const uint8_t glyph_a[5] = {0x02,0x05,0x07,0x05,0x05};


// ═══════════════════════════════════════════════════════════════════════
// GAME STATE
// ═══════════════════════════════════════════════════════════════════════

typedef struct {
    int16_t  cat_x, cat_y;
    int16_t  prev_cat_x, prev_cat_y;
    int16_t  target_x, target_y;
    uint16_t frame;
    bool     facing_left;
    bool     is_dead;
    uint16_t health;
    uint32_t last_drain;
    uint32_t last_active;       // last time wpm > 0
    uint32_t last_icon_spawn;
    uint8_t  next_icon_type;
    uint8_t  anim_state;
    uint8_t  bounce_timer;      // frames remaining for eat-bounce
    uint8_t  prev_wpm;
    uint8_t  prev_half_hearts;
    bool     prev_was_dead;
    uint8_t  cur_wpm;               // cached WPM for current frame
    const uint32_t *prev_sprite;    // previous cat sprite pointer (skip-redraw)
    uint8_t  prev_cat_bright;       // previous cat brightness
    bool     prev_cat_facing;       // previous facing direction

    struct {
        int16_t x, y;
        int16_t prev_x, prev_y;
        int8_t  dx, dy;
        uint8_t type;
        bool    active;
        bool    prev_drawn;
    } icons[MAX_ICONS];

    // Level / XP
    uint16_t level;
    uint32_t xp;
    uint32_t xp_next;       // cached xp_for_level(level)
    uint32_t xp_bar_div;   // precomputed xp_next / XP_BAR_W (avoids 64-bit math)
    uint16_t prev_level;
    uint8_t  prev_bar_fill;

    // Orange cat encounter
    uint8_t  orange_phase;
    int16_t  orange_x, orange_y;
    int16_t  prev_orange_x, prev_orange_y;
    bool     prev_orange_drawn;
    const uint32_t *prev_orange_sprite;
    bool     prev_orange_facing;
    int16_t  orange_target_x;
    uint16_t orange_frame;
    bool     orange_facing_left;
    uint32_t orange_phase_timer;
    uint32_t last_orange_check;
} tama_state_t;

static tama_state_t st;
static bool tama_inited = false;
static uint32_t last_frame_time = 0;

// ─── Frame timing instrumentation ───
// Displays ms/frame in bottom-right corner.
// Remove this block once performance is verified.
#define SHOW_FRAME_TIMING 0
#if SHOW_FRAME_TIMING
static uint8_t last_frame_ms = 0;
#endif

// ═══════════════════════════════════════════════════════════════════════
// DRAWING HELPERS
// ═══════════════════════════════════════════════════════════════════════

// Clear a rectangle to black (game area only)
static void clear_rect(int16_t x, int16_t y, int16_t w, int16_t h) {
    int16_t x1 = (x < 0) ? 0 : x;
    int16_t y1 = (y < GAME_Y) ? GAME_Y : y;
    int16_t x2 = x + w - 1; if (x2 >= SCR_W) x2 = SCR_W - 1;
    int16_t y2 = y + h - 1; if (y2 >= LVL_Y) y2 = LVL_Y - 1;
    if (x1 <= x2 && y1 <= y2)
        qp_rect(lcd_surface, x1, y1, x2, y2, 0, 0, 0, true);
}

static void draw_cat(int16_t ox, int16_t oy, const uint32_t *sprite,
                     bool mirror, uint8_t brightness,
                     const uint8_t (*palette)[3]) {
    for (int row = 0; row < CAT_BMP_H; row++) {
        uint32_t bits = sprite[row];
        if (!bits) continue;
        int16_t py = oy + row * CAT_SCALE;
        int16_t py2 = py + CAT_SCALE - 1;
        if (py2 < GAME_Y || py >= LVL_Y) continue;  // row fully off-screen
        if (py < GAME_Y) py = GAME_Y;
        if (py2 >= LVL_Y) py2 = LVL_Y - 1;
        int col = 0;
        while (col < CAT_BMP_W) {
            int src_col = mirror ? (CAT_BMP_W - 1 - col) : col;
            uint8_t c = (bits >> (2 * (CAT_BMP_W - 1 - src_col))) & 3;
            if (c == 0) { col++; continue; }
            // Merge adjacent same-color pixels into one wider rect
            int run = 1;
            while (col + run < CAT_BMP_W) {
                int ns = mirror ? (CAT_BMP_W - 1 - (col + run)) : (col + run);
                if (((bits >> (2 * (CAT_BMP_W - 1 - ns))) & 3) != c) break;
                run++;
            }
            int16_t px = ox + col * CAT_SCALE;
            int16_t px2 = px + run * CAT_SCALE - 1;
            if (px2 < 0 || px >= SCR_W) { col += run; continue; }  // fully off-screen horizontally
            if (px < 0) px = 0;
            if (px2 >= SCR_W) px2 = SCR_W - 1;
            const uint8_t *pal = palette[c];
            uint8_t v = (uint16_t)pal[2] * brightness / 255;
            qp_rect(lcd_surface, px, py, px2, py2, pal[0], pal[1], v, true);
            col += run;
        }
    }
}

static void draw_icon_sprite(int16_t ox, int16_t oy, const uint8_t *sprite,
                              uint8_t h, uint8_t s, uint8_t v) {
    for (int row = 0; row < ICON_BMP_H; row++) {
        uint8_t bits = sprite[row];
        if (!bits) continue;
        int col = 0;
        while (col < ICON_BMP_W) {
            if (!(bits & (1 << (7 - col)))) { col++; continue; }
            int run = 1;
            while (col + run < ICON_BMP_W && (bits & (1 << (7 - col - run)))) run++;
            int16_t px = ox + col * ICON_SCALE;
            int16_t py = oy + row * ICON_SCALE;
            qp_rect(lcd_surface, px, py,
                     px + run * ICON_SCALE - 1, py + ICON_SCALE - 1,
                     h, s, v, true);
            col += run;
        }
    }
}

static void draw_glyph(int16_t ox, int16_t oy, const uint8_t *glyph,
                        uint8_t scale, uint8_t h, uint8_t s, uint8_t v) {
    for (int row = 0; row < 5; row++) {
        uint8_t bits = glyph[row];
        if (!bits) continue;
        int col = 0;
        while (col < 3) {
            if (!(bits & (1 << (2 - col)))) { col++; continue; }
            int run = 1;
            while (col + run < 3 && (bits & (1 << (2 - col - run)))) run++;
            int16_t px = ox + col * scale;
            int16_t py = oy + row * scale;
            qp_rect(lcd_surface, px, py,
                     px + run * scale - 1, py + scale - 1,
                     h, s, v, true);
            col += run;
        }
    }
}

static void draw_digit(int16_t ox, int16_t oy, uint8_t digit,
                        uint8_t h, uint8_t s, uint8_t v) {
    if (digit <= 9) draw_glyph(ox, oy, digits_3x5[digit], DIGIT_SCALE, h, s, v);
}

// fill: 0=empty, 1=half (left filled), 2=full — drawn at HEART_SCALE
static void draw_heart(int16_t ox, int16_t oy, uint8_t fill) {
    for (int row = 0; row < HEART_BMP; row++) {
        for (int col = 0; col < HEART_BMP; col++) {
            int bit = 1 << (6 - col);
            bool in_full = heart_full[row] & bit;
            bool in_outline = heart_empty[row] & bit;
            uint8_t h = 0, s = 0, v = 0;
            bool draw = false;
            if (fill == 2 && in_full) {
                s = 255; v = 220; draw = true;
            } else if (fill == 1) {
                if (col < 4 && in_full) { s = 255; v = 220; draw = true; }
                else if (in_outline) { v = 60; draw = true; }
            } else if (fill == 0 && in_outline) {
                v = 60; draw = true;
            }
            if (draw) {
                int16_t px = ox + col * HEART_SCALE;
                int16_t py = oy + row * HEART_SCALE;
                qp_rect(lcd_surface, px, py,
                         px + HEART_SCALE - 1, py + HEART_SCALE - 1,
                         h, s, v, true);
            }
        }
    }
}

static void draw_wpm(uint8_t wpm) {
    uint8_t d[3] = { wpm / 100, (wpm / 10) % 10, wpm % 10 };
    int start = (d[0] == 0) ? ((d[1] == 0) ? 2 : 1) : 0;
    int n = 3 - start;
    int total_w = n * 3 * DIGIT_SCALE + (n - 1) * DIGIT_SCALE;
    int16_t x = (SCR_W - total_w) / 2;
    uint8_t v = (wpm > 0) ? 200 : 80;
    for (int i = start; i < 3; i++) {
        draw_digit(x, WPM_Y, d[i], 0, 0, v);
        x += 4 * DIGIT_SCALE;
    }
}

// half_hearts: 0–10 (each heart = 2 halves, 5 hearts total)
static void draw_hearts(uint8_t half_hearts) {
    for (int i = 0; i < HEART_COUNT; i++) {
        uint8_t fill;
        if (half_hearts >= (i + 1) * 2)     fill = 2;  // full
        else if (half_hearts >= i * 2 + 1)  fill = 1;  // half
        else                                 fill = 0;  // empty
        draw_heart(HEARTS_X + i * HEART_PITCH, HEARTS_Y, fill);
    }
}

// ─── Level / XP ───
static uint32_t xp_for_level(uint16_t level) {
    uint32_t n = (uint32_t)level * (level + 1);
    if (n > 170000) return 4250000000UL;  // cap at ~lv414 to avoid overflow
    return n * 25000;
}

static uint8_t level_hue(uint16_t level) {
    if (level < 10)  return 85;   // green
    if (level < 50)  return 128;  // cyan
    if (level < 100) return 170;  // blue
    if (level < 500) return 200;  // purple
    return 32;                     // gold
}

static void draw_level_bar(uint16_t level, uint8_t bar_fill) {
    qp_rect(lcd_surface, 0, LVL_Y, SCR_W - 1, SCR_H - 1, 0, 0, 0, true);

    uint8_t hue = level_hue(level);

    // Decompose level into digits
    uint8_t digs[5];
    int nd = 0;
    uint16_t tmp = level;
    do { digs[nd++] = tmp % 10; tmp /= 10; } while (tmp > 0);
    for (int i = 0; i < nd / 2; i++) {
        uint8_t t = digs[i]; digs[i] = digs[nd-1-i]; digs[nd-1-i] = t;
    }

    // Total text width: "Lv" + digits, each 4*LVL_SCALE pitch, minus trailing gap
    int total_w = (2 + nd) * 4 * LVL_SCALE - LVL_SCALE;
    int16_t x = (SCR_W - total_w) / 2;
    int16_t y = LVL_Y;

    draw_glyph(x, y, glyph_l, LVL_SCALE, hue, 180, 200);
    x += 4 * LVL_SCALE;
    draw_glyph(x, y, glyph_v, LVL_SCALE, hue, 180, 200);
    x += 4 * LVL_SCALE;
    for (int i = 0; i < nd; i++) {
        draw_glyph(x, y, digits_3x5[digs[i]], LVL_SCALE, hue, 200, 220);
        x += 4 * LVL_SCALE;
    }

    // XP bar background
    int16_t bar_y = SCR_H - XP_BAR_H;
    qp_rect(lcd_surface, XP_BAR_PAD, bar_y,
             XP_BAR_PAD + XP_BAR_W - 1, bar_y + XP_BAR_H - 1,
             0, 0, 30, true);
    // XP bar fill
    if (bar_fill > 0) {
        qp_rect(lcd_surface, XP_BAR_PAD, bar_y,
                 XP_BAR_PAD + bar_fill - 1, bar_y + XP_BAR_H - 1,
                 hue, 220, 180, true);
        // Bright tip at fill edge
        if (bar_fill < XP_BAR_W)
            qp_rect(lcd_surface, XP_BAR_PAD + bar_fill - 1, bar_y,
                     XP_BAR_PAD + bar_fill - 1, bar_y + XP_BAR_H - 1,
                     hue, 100, 255, true);
    }
}

static void draw_zzz(int16_t cx, int16_t cy, uint16_t frame) {
    // Three z's floating above sleeping cat, staggered
    int bob = (frame / 3) % 6;  // 0-5 cycle
    int offsets[3] = { bob, (bob + 2) % 6, (bob + 4) % 6 };
    for (int i = 0; i < 3; i++) {
        int16_t zx = cx + CAT_W / 2 - 4 + i * 8;
        int16_t zy = cy - 8 - offsets[i];
        if (zy >= GAME_Y)
            draw_glyph(zx, zy, glyph_z, 2, 0, 0, 180);
    }
}

static void draw_question(int16_t cx, int16_t cy, uint16_t frame) {
    int bob = ((frame / 4) % 3);  // 0,1,2 gentle bob
    int16_t qx = cx + CAT_W / 2 - 3;
    int16_t qy = cy - 12 - bob;
    if (qy >= GAME_Y)
        draw_glyph(qx, qy, glyph_q, 2, 0, 0, 200);
}

static void draw_dead_text(void) {
    const int16_t sx = (SCR_W - 45) / 2;
    const int16_t sy = SCR_H / 2 + 20;
    draw_glyph(sx,      sy, glyph_d, 3, 0, 255, 220);
    draw_glyph(sx + 12, sy, glyph_e, 3, 0, 255, 220);
    draw_glyph(sx + 24, sy, glyph_a, 3, 0, 255, 220);
    draw_glyph(sx + 36, sy, glyph_d, 3, 0, 255, 220);
}

// ═══════════════════════════════════════════════════════════════════════
// GAME LOGIC
// ═══════════════════════════════════════════════════════════════════════

static void pick_new_target(void) {
    st.target_x = rand() % (SCR_W - CAT_W);
    st.target_y = GAME_Y + rand() % (GAME_H - CAT_H);
}

static void spawn_icon(void) {
    for (int i = 0; i < MAX_ICONS; i++) {
        if (st.icons[i].active) continue;
        st.icons[i].active = true;
        st.icons[i].type = st.next_icon_type;
        st.next_icon_type = (st.next_icon_type + 1) % 3;
        int edge = rand() % 4;
        switch (edge) {
            case 0:
                st.icons[i].x = rand() % (SCR_W - ICON_W);
                st.icons[i].y = GAME_Y;
                break;
            case 1:
                st.icons[i].x = rand() % (SCR_W - ICON_W);
                st.icons[i].y = GAME_Y + GAME_H - ICON_H;
                break;
            case 2:
                st.icons[i].x = 0;
                st.icons[i].y = GAME_Y + rand() % (GAME_H - ICON_H);
                break;
            default:
                st.icons[i].x = SCR_W - ICON_W;
                st.icons[i].y = GAME_Y + rand() % (GAME_H - ICON_H);
                break;
        }
        int16_t ddx = (st.cat_x + CAT_W/2) - (st.icons[i].x + ICON_W/2);
        int16_t ddy = (st.cat_y + CAT_H/2) - (st.icons[i].y + ICON_H/2);
        st.icons[i].dx = (ddx > 0) ? 1 : ((ddx < 0) ? -1 : 0);
        st.icons[i].dy = (ddy > 0) ? 1 : ((ddy < 0) ? -1 : 0);
        if (!st.icons[i].dx && !st.icons[i].dy) st.icons[i].dy = -1;
        return;
    }
}

static void update_icons(void) {
    int16_t cat_cx = st.cat_x + CAT_W / 2;
    int16_t cat_cy = st.cat_y + CAT_H / 2;

    for (int i = 0; i < MAX_ICONS; i++) {
        if (!st.icons[i].active) continue;
        int16_t icx = st.icons[i].x + ICON_W / 2;
        int16_t icy = st.icons[i].y + ICON_H / 2;
        int16_t ddx = cat_cx - icx;
        int16_t ddy = cat_cy - icy;
        st.icons[i].dx = (ddx > 0) ? MOVE_SPEED : ((ddx < 0) ? -MOVE_SPEED : 0);
        st.icons[i].dy = (ddy > 0) ? MOVE_SPEED : ((ddy < 0) ? -MOVE_SPEED : 0);
        st.icons[i].x += st.icons[i].dx;
        st.icons[i].y += st.icons[i].dy;

        if (abs(cat_cx - (st.icons[i].x + ICON_W/2)) < 16 &&
            abs(cat_cy - (st.icons[i].y + ICON_H/2)) < 16) {
            st.icons[i].active = false;
            if (st.health <= MAX_HEALTH - ICON_HP_GAIN)
                st.health += ICON_HP_GAIN;
            else
                st.health = MAX_HEALTH;
            st.bounce_timer = 3;
        }
    }
}

static void get_spawn_interval(uint8_t wpm, uint16_t *min_ms, uint16_t *range_ms) {
    if (wpm <= 20)      { *min_ms = 8000; *range_ms = 4000; }
    else if (wpm <= 50) { *min_ms = 4000; *range_ms = 4000; }
    else if (wpm <= 80) { *min_ms = 2000; *range_ms = 2000; }
    else                { *min_ms = 1000; *range_ms = 1000; }
}

// ─── Orange cat encounter logic ───
static void update_orange(uint32_t now) {
    if (st.is_dead) return;  // no encounter while dead

    if (st.orange_phase == ORANGE_NONE) {
        // Check for spawn
        if (timer_elapsed32(st.last_orange_check) >= ORANGE_CHECK_MS) {
            st.last_orange_check = now;
            if ((rand() % 100) < ORANGE_SPAWN_PCT) {
                // Start encounter — pick entry side
                bool from_left = (rand() % 2) == 0;
                st.orange_y = GAME_Y + 20 + rand() % (GAME_H - CAT_H - 40);
                if (from_left) {
                    st.orange_x = -CAT_W;
                    st.orange_target_x = 10 + rand() % 30;
                    st.orange_facing_left = false;
                } else {
                    st.orange_x = SCR_W;
                    st.orange_target_x = SCR_W - CAT_W - 10 - rand() % 30;
                    st.orange_facing_left = true;
                }
                st.orange_frame = 0;
                st.prev_orange_drawn = false;
                st.orange_phase = ORANGE_ENTER;
                st.orange_phase_timer = now;
            }
        }
        return;
    }

    st.orange_frame++;

    switch (st.orange_phase) {
        case ORANGE_ENTER: {
            // Walk toward target
            int16_t dx = st.orange_target_x - st.orange_x;
            int speed = MOVE_SPEED;
            if (dx > 0)      { st.orange_x += (dx < speed ? dx : speed); st.orange_facing_left = false; }
            else if (dx < 0) { st.orange_x += (dx > -speed ? dx : -speed); st.orange_facing_left = true; }
            if (abs(dx) <= speed) {
                st.orange_phase = ORANGE_IDLE;
                st.orange_phase_timer = now;
            }
            break;
        }
        case ORANGE_IDLE:
            // Pause, face the main cat
            st.orange_facing_left = (st.cat_x < st.orange_x);
            if (timer_elapsed32(st.orange_phase_timer) >= ORANGE_IDLE_MS) {
                st.orange_phase = ORANGE_MAD;
                st.orange_phase_timer = now;
                st.orange_frame = 0;  // restart angry anim
            }
            break;
        case ORANGE_MAD:
            // Main cat gets angry, faces orange cat
            if (timer_elapsed32(st.orange_phase_timer) >= ORANGE_MAD_MS) {
                st.orange_phase = ORANGE_CHASE;
                st.orange_phase_timer = now;
            }
            break;
        case ORANGE_CHASE: {
            // Orange cat flees to nearest edge
            int16_t exit_x = (st.orange_x < SCR_W / 2) ? -CAT_W : SCR_W;
            int speed = MOVE_SPEED + 2;
            if (exit_x > st.orange_x)      { st.orange_x += speed; st.orange_facing_left = false; }
            else if (exit_x < st.orange_x) { st.orange_x -= speed; st.orange_facing_left = true; }
            if (st.orange_x <= -CAT_W || st.orange_x >= SCR_W) {
                st.orange_phase = ORANGE_NONE;
                st.prev_orange_drawn = false;
                pick_new_target();
            }
            break;
        }
    }
}

static void update_game(void) {
    st.cur_wpm = get_current_wpm();
    uint8_t wpm = st.cur_wpm;
    uint32_t now = timer_read32();

    // Health drain
    if (timer_elapsed32(st.last_drain) >= DRAIN_MS) {
        st.last_drain = now;
        if (st.health > 0) {
            st.health--;
            if (st.health == 0) st.is_dead = true;
        }
    }

    // Track typing activity
    if (wpm > 0) st.last_active = now;

    // Revive on typing
    if (wpm > 0 && st.is_dead) {
        st.is_dead = false;
        st.health = REVIVE_HEALTH;
        st.last_drain = now;
        pick_new_target();
    }

    uint32_t idle_ms = timer_elapsed32(st.last_active);

    // Determine animation state
    if (st.is_dead) {
        st.anim_state = ANIM_DEAD;
    } else if (wpm == 0 && idle_ms >= IDLE_SLEEP_MS) {
        st.anim_state = ANIM_SLEEP;
    } else if (wpm == 0 && idle_ms > 0) {
        st.anim_state = ANIM_SIT;
    } else {
        // Typing — walk toward target, idle when arrived
        int16_t dx = st.target_x - st.cat_x;
        int16_t dy = st.target_y - st.cat_y;
        if (abs(dx) <= MOVE_SPEED && abs(dy) <= MOVE_SPEED) {
            st.anim_state = ANIM_IDLE;
            pick_new_target();
        } else {
            st.anim_state = ANIM_WALK;
        }
    }

    if (st.is_dead) { st.frame++; update_orange(now); return; }

    // Orange cat encounter
    update_orange(now);

    // Main cat gets mad during orange encounter — face the intruder
    if (st.orange_phase == ORANGE_MAD || st.orange_phase == ORANGE_CHASE) {
        st.facing_left = (st.orange_x < st.cat_x);
    }

    // Movement — only in WALK/IDLE states
    if (st.anim_state == ANIM_WALK || st.anim_state == ANIM_IDLE) {
        // Check for nearby icon → pounce toward it
        int32_t best_dist = 9999;
        int best_ix = -1;
        for (int i = 0; i < MAX_ICONS; i++) {
            if (!st.icons[i].active) continue;
            int32_t d = abs(st.cat_x - st.icons[i].x) + abs(st.cat_y - st.icons[i].y);
            if (d < POUNCE_DIST && d < best_dist) {
                best_dist = d;
                best_ix = i;
            }
        }
        int16_t tx, ty;
        int speed = MOVE_SPEED;
        if (best_ix >= 0) {
            tx = st.icons[best_ix].x;
            ty = st.icons[best_ix].y;
            speed = MOVE_SPEED + 1; // slightly faster pounce
            st.anim_state = ANIM_WALK;
        } else {
            tx = st.target_x;
            ty = st.target_y;
        }

        // Mood: low health = slower
        if (st.health < 30) speed = 1;
        else if (st.health < 60 && (st.frame % 2)) speed = 0;

        if (speed > 0) {
            int16_t dx = tx - st.cat_x;
            int16_t dy = ty - st.cat_y;
            if (dx > 0)      { st.cat_x += (dx < speed ? dx : speed); st.facing_left = false; }
            else if (dx < 0) { st.cat_x += (dx > -speed ? dx : -speed); st.facing_left = true; }
            if (dy > 0)      st.cat_y += (dy < speed ? dy : speed);
            else if (dy < 0) st.cat_y += (dy > -speed ? dy : -speed);
        }

        // Low health: occasionally sit
        if (st.health < 60 && (rand() % 8) == 0)
            st.anim_state = ANIM_SIT;
    }

    // Icon spawning
    if (wpm > 0) {
        uint16_t min_ms, range_ms;
        get_spawn_interval(wpm, &min_ms, &range_ms);
        if (timer_elapsed32(st.last_icon_spawn) >= (uint32_t)(min_ms + (rand() % range_ms))) {
            spawn_icon();
            st.last_icon_spawn = now;
        }
    }

    update_icons();
    if (st.bounce_timer > 0) st.bounce_timer--;

    // XP gain from typing
    if (wpm > 0) {
        st.xp += wpm;
        while (st.xp >= st.xp_next) {
            st.xp -= st.xp_next;
            st.level++;
            st.xp_next = xp_for_level(st.level);
            st.xp_bar_div = st.xp_next / XP_BAR_W;
            if (st.xp_bar_div == 0) st.xp_bar_div = 1;
        }
    }

    st.frame++;
}

// ═══════════════════════════════════════════════════════════════════════
// FRAME RENDERING — dirty-rect approach to minimize SPI transfer
// ═══════════════════════════════════════════════════════════════════════

static void draw_frame(void) {
    uint8_t wpm = st.cur_wpm;
    uint8_t half_hearts = st.health / 10;

    // ── Select animation frame ──
    const uint32_t (*frames)[16];
    uint8_t num_frames, anim_speed;
    switch (st.anim_state) {
        case ANIM_WALK:  frames = anim_trot;  num_frames = 8; anim_speed = 3; break;
        case ANIM_FLEE:  frames = anim_trot;  num_frames = 8; anim_speed = 2; break;
        case ANIM_SIT:   frames = anim_walk;  num_frames = 4; anim_speed = 12; break;
        case ANIM_SLEEP: frames = anim_sleep; num_frames = 4; anim_speed = 8; break;
        case ANIM_DEAD:  frames = anim_sleep; num_frames = 4; anim_speed = 8; break;
        default:         frames = anim_sit;   num_frames = 4; anim_speed = 6; break;
    }
    if (st.orange_phase == ORANGE_MAD || st.orange_phase == ORANGE_CHASE) {
        frames = anim_angry; num_frames = 8; anim_speed = 3;
    }
    int fi = (st.frame / anim_speed) % num_frames;
    const uint32_t *sprite = frames[fi];

    // Cat brightness from health
    uint8_t cat_bright;
    if (st.is_dead)          cat_bright = 80;
    else if (st.health < 30) cat_bright = 140;
    else                     cat_bright = 255;

    // Bounce offset when eating
    int16_t bounce_y = 0;
    if (st.bounce_timer > 0) {
        static const int8_t bounce_off[] = {-4, -6, -2};
        bounce_y = bounce_off[3 - st.bounce_timer];
    }

    // ── Determine if cat sprite needs redrawing ──
    int16_t render_y = st.cat_y + bounce_y;
    bool cat_dirty = (sprite != st.prev_sprite ||
                      st.cat_x != st.prev_cat_x ||
                      render_y != st.prev_cat_y ||
                      st.facing_left != st.prev_cat_facing ||
                      cat_bright != st.prev_cat_bright);

    // ── Determine if orange cat needs redrawing ──
    const uint32_t *osprite = NULL;
    bool orange_dirty = false;
    if (st.orange_phase != ORANGE_NONE) {
        const uint32_t (*oframes)[16];
        uint8_t onum, ospeed;
        if (st.orange_phase == ORANGE_CHASE) {
            oframes = anim_trot; onum = 8; ospeed = 2;
        } else if (st.orange_phase == ORANGE_MAD) {
            oframes = anim_walk; onum = 4; ospeed = 8;
        } else if (st.orange_phase == ORANGE_IDLE) {
            oframes = anim_walk; onum = 4; ospeed = 10;
        } else {
            oframes = anim_trot; onum = 8; ospeed = 3;
        }
        int ofi = (st.orange_frame / ospeed) % onum;
        osprite = oframes[ofi];
        orange_dirty = (osprite != st.prev_orange_sprite ||
                        st.orange_x != st.prev_orange_x ||
                        st.orange_y != st.prev_orange_y ||
                        st.orange_facing_left != st.prev_orange_facing);
    }

    // Only force both cats to redraw when they're close enough that
    // clear_rect of one could leave black holes in the other
    if (st.orange_phase != ORANGE_NONE &&
        abs(st.cat_x - st.orange_x) < CAT_W * 2 &&
        abs(render_y - st.orange_y) < CAT_H * 2) {
        cat_dirty = true;
        orange_dirty = true;
    }

    // ── Clear old positions ──
    clear_rect(st.prev_cat_x, st.prev_cat_y - OVERLAY_PAD, CAT_W, OVERLAY_PAD);
    if (cat_dirty)
        clear_rect(st.prev_cat_x, st.prev_cat_y, CAT_W, CAT_H);
    bool was_orange_drawn = st.prev_orange_drawn;
    st.prev_orange_drawn = false;
    if (was_orange_drawn && orange_dirty)
        clear_rect(st.prev_orange_x, st.prev_orange_y, CAT_W, CAT_H);
    else if (st.orange_phase == ORANGE_NONE && was_orange_drawn)
        clear_rect(st.prev_orange_x, st.prev_orange_y, CAT_W, CAT_H);
    for (int i = 0; i < MAX_ICONS; i++) {
        if (st.icons[i].prev_drawn)
            clear_rect(st.icons[i].prev_x, st.icons[i].prev_y, ICON_W, ICON_H);
    }
    if (st.prev_was_dead && !st.is_dead)
        clear_rect((SCR_W - 45) / 2, SCR_H / 2 + 20, 48, 18);

    // ── Top bar (WPM + hearts) ──
    if (wpm != st.prev_wpm || half_hearts != st.prev_half_hearts) {
        qp_rect(lcd_surface, 0, 0, SCR_W - 1, GAME_Y - 1, 0, 0, 0, true);
        draw_wpm(wpm);
        draw_hearts(half_hearts);
        st.prev_wpm = wpm;
        st.prev_half_hearts = half_hearts;
    }

    // ── Level bar ──
    uint8_t bar_fill = (uint8_t)(st.xp / st.xp_bar_div);
    if (bar_fill > XP_BAR_W) bar_fill = XP_BAR_W;
    if (st.level != st.prev_level || bar_fill != st.prev_bar_fill) {
        draw_level_bar(st.level, bar_fill);
        st.prev_level = st.level;
        st.prev_bar_fill = bar_fill;
    }

    // ── Layer 1 (bottom): Orange cat ──
    if (st.orange_phase != ORANGE_NONE && st.orange_x > -CAT_W && st.orange_x < SCR_W) {
        if (orange_dirty) {
            draw_cat(st.orange_x, st.orange_y, osprite,
                     st.orange_facing_left, 255, orange_palette);
            st.prev_orange_x = st.orange_x;
            st.prev_orange_y = st.orange_y;
            st.prev_orange_sprite = osprite;
            st.prev_orange_facing = st.orange_facing_left;
        }
        st.prev_orange_drawn = true;
    }

    // ── Layer 2: Icons ──
    for (int i = 0; i < MAX_ICONS; i++) {
        st.icons[i].prev_drawn = false;
        if (!st.icons[i].active) continue;
        uint8_t t = st.icons[i].type;
        draw_icon_sprite(st.icons[i].x, st.icons[i].y, icon_sprites[t],
                         icon_colors[t][0], icon_colors[t][1], icon_colors[t][2]);
        st.icons[i].prev_drawn = true;
        st.icons[i].prev_x = st.icons[i].x;
        st.icons[i].prev_y = st.icons[i].y;
    }

    // ── Layer 3: Overlays (zzz, ?, DEAD) ──
    if (st.anim_state == ANIM_SLEEP)
        draw_zzz(st.cat_x, st.cat_y, st.frame);
    else if (st.anim_state == ANIM_SIT)
        draw_question(st.cat_x, st.cat_y, st.frame);
    else if (st.is_dead)
        draw_dead_text();

    // ── Layer 4 (top): Main cat — always on top of everything ──
    if (cat_dirty)
        draw_cat(st.cat_x, render_y, sprite,
                 st.facing_left, cat_bright, cat_palette);

    // ── Single flush ──
    qp_surface_draw(lcd_surface, lcd, 0, 0, 0);
    qp_flush(lcd);

    // ── Store previous state ──
    st.prev_cat_x = st.cat_x;
    st.prev_cat_y = render_y;
    st.prev_sprite = sprite;
    st.prev_cat_bright = cat_bright;
    st.prev_cat_facing = st.facing_left;
    st.prev_was_dead = st.is_dead;
}

// ═══════════════════════════════════════════════════════════════════════
// QMK HOOKS
// ═══════════════════════════════════════════════════════════════════════

bool module_post_init_user(void) {
#ifdef TAMAGOTCHI_ON_RIGHT
    if (is_keyboard_left()) return true;
#else
    if (!is_keyboard_left()) return true;
#endif

    srand(timer_read32());
    uint32_t now = timer_read32();

    st.cat_x = (SCR_W - CAT_W) / 2;
    st.cat_y = GAME_Y + (GAME_H - CAT_H) / 2;
    st.prev_cat_x = st.cat_x;
    st.prev_cat_y = st.cat_y;
    st.facing_left = false;
    st.is_dead = false;
    st.prev_was_dead = false;
    st.health = MAX_HEALTH;
    st.last_drain = now;
    st.last_active = now;
    st.last_icon_spawn = now;
    st.next_icon_type = 0;
    st.frame = 0;
    st.anim_state = ANIM_IDLE;
    st.bounce_timer = 0;
    st.prev_wpm = 255;          // force first redraw
    st.prev_half_hearts = 255;
    st.cur_wpm = 0;
    st.prev_sprite = NULL;      // force first cat draw
    st.prev_cat_bright = 0;
    st.prev_cat_facing = false;

    // Level / XP init
    st.level = 1;
    st.xp = 0;
    st.xp_next = xp_for_level(1);
    st.xp_bar_div = st.xp_next / XP_BAR_W;
    st.prev_level = 0;         // force first redraw
    st.prev_bar_fill = 255;

    // Orange cat encounter init
    st.orange_phase = ORANGE_NONE;
    st.prev_orange_drawn = false;
    st.prev_orange_sprite = NULL;
    st.prev_orange_facing = false;
    st.last_orange_check = now;

    for (int i = 0; i < MAX_ICONS; i++) {
        st.icons[i].active = false;
        st.icons[i].prev_drawn = false;
    }

    pick_new_target();
    tama_inited = true;

    // Initial full screen draw: black top bar + grass game area
    qp_rect(lcd_surface, 0, 0, SCR_W - 1, GAME_Y - 1, 0, 0, 0, true);
    qp_rect(lcd_surface, 0, GAME_Y, SCR_W - 1, SCR_H - 1, 0, 0, 0, true);
    draw_wpm(get_current_wpm());
    draw_hearts(st.health / 10);
    draw_level_bar(st.level, 0);
    draw_cat(st.cat_x, st.cat_y, anim_sit[0], false, 255, cat_palette);

    qp_surface_draw(lcd_surface, lcd, 0, 0, true);  // full initial blit
    qp_flush(lcd);

    return true;  // signal success to Halcyon module framework
}

bool display_module_housekeeping_task_user(bool second_display) {
    if (!tama_inited) return true;     // before init, let framework handle
    if (second_display) return false;  // prevent framework surface flush from overwriting our LCD draws

    if (timer_elapsed32(last_frame_time) < FRAME_MS) return false;  // nothing to draw, skip framework flush
    last_frame_time = timer_read32();

    update_game();

#if SHOW_FRAME_TIMING
    uint32_t t0 = timer_read32();
#endif

    draw_frame();    // draws to surface + flushes + draws orange cat on LCD

#if SHOW_FRAME_TIMING
    {
        uint8_t ms = (uint8_t)timer_elapsed32(t0);
        if (ms != last_frame_ms) {
            // Clear old digits (3-digit max: "999")
            qp_rect(lcd_surface, SCR_W - 30, LVL_Y, SCR_W - 1, LVL_Y + 9, 0, 0, 0, true);
            // Draw ms value in small text at bottom-right
            uint8_t d[3] = { ms / 100, (ms / 10) % 10, ms % 10 };
            int start = (d[0] == 0) ? ((d[1] == 0) ? 2 : 1) : 0;
            int16_t x = SCR_W - (3 - start) * 8;
            uint8_t v = (ms > 50) ? 255 : 120;  // bright red if slow
            uint8_t hue = (ms > 50) ? 0 : 85;   // red if >50ms, green otherwise
            for (int i = start; i < 3; i++) {
                draw_glyph(x, LVL_Y, digits_3x5[d[i]], 2, hue, 200, v);
                x += 8;
            }
            qp_surface_draw(lcd_surface, lcd, 0, 0, 0);
            qp_flush(lcd);
            last_frame_ms = ms;
        }
    }
#endif

    return false;    // skip framework's update_display() and redundant flush
}
