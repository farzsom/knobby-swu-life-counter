#ifndef _TYPES_H
#define _TYPES_H

#include "knob.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// ---------- constants ----------
#define MAX_DISPLAY_PLAYERS 4
#define DAMAGE_MIN 0
#define DAMAGE_MAX 99
#define BASE_HP_MIN 1
#define BASE_HP_MAX 99
#define DEFAULT_BASE_HP 30
#define DEFAULT_PLAYERS_TO_TRACK 1
#define DEFAULT_BRIGHTNESS_PERCENT 30
#define INTRO_CHAR_COUNT 7
#define MULTIPLAYER_COUNT 4
#define KNOB_EVENT_QUEUE_SIZE 32

// ---------- color modes ----------
#define COLOR_MODE_PLAYER     0
#define COLOR_MODE_HP         1
#define COLOR_MODE_COUNT      2

#define CUSTOM_COLOR_COUNT 18

// ---------- token badges ----------
#define INITIATIVE_COLOR   0xF2C230
#define FORCE_COLOR        0x2F7FD0
#define INITIATIVE_BADGE_W 86
#define FORCE_BADGE_W      58
#define TOKEN_BADGE_GAP    6
#define TOKEN_COUNT        2   /* badge slots: initiative, force */

// ---------- orientation modes ----------
#define ORIENTATION_MODE_ABSOLUTE 0
#define ORIENTATION_MODE_CENTRIC  1
#define ORIENTATION_MODE_TABLETOP 2
#define ORIENTATION_MODE_COUNT    3

// ---------- display rotation (physical, degrees = value * 90) ----------
#define DISPLAY_ROTATION_COUNT 4

// ---------- auto-dim timeout options ----------
#define AUTO_DIM_OFF  0
#define AUTO_DIM_15S  1
#define AUTO_DIM_30S  2
#define AUTO_DIM_60S  3
#define AUTO_DIM_COUNT 4

static const uint32_t auto_dim_ms[] = {0, 15000, 30000, 60000};

// ---------- deselect timeout options ----------
#define DESELECT_NEVER 0
#define DESELECT_5S    1
#define DESELECT_15S   2
#define DESELECT_30S   3
#define DESELECT_COUNT 4

static const int deselect_ms[] = {0, 5000, 15000, 30000};

// ---------- types ----------
typedef struct {
    knob_event_t event;
} knob_input_event_t;

typedef struct {
    const char *label;
    lv_event_cb_t cb;
    bool enabled;
    lv_event_code_t event;
    const char *icon;
    const lv_font_t *icon_font;
    void *user_data;            /* passed to cb via lv_event_get_user_data */
} quad_item_t;

// ---------- utility functions ----------
static inline int clamp_damage(int value)
{
    if (value < DAMAGE_MIN) return DAMAGE_MIN;
    if (value > DAMAGE_MAX) return DAMAGE_MAX;
    return value;
}

static inline int clamp_base_hp(int value)
{
    if (value < BASE_HP_MIN) return BASE_HP_MIN;
    if (value > BASE_HP_MAX) return BASE_HP_MAX;
    return value;
}

static inline int clamp_brightness(int value)
{
    if (value < 1) return 1;
    if (value > 100) return 100;
    return value;
}

static inline int get_arc_display_value(int value, int max_value)
{
    if (value < 0) return 0;
    if (value > max_value) return max_value;
    return value;
}

// ---------- HP color tiers ----------
/* Tiers follow the HP a base has left, so a base turns yellow at half
   HP and red in its last quarter. */
#define HP_TIER_RED    0
#define HP_TIER_YELLOW 1
#define HP_TIER_GREEN  2
#define HP_TIER_COUNT  3

#define VIB_DIM  0
#define VIB_MID  1
#define VIB_VIV  2
#define VIB_COUNT 3

static const uint32_t hp_color_table[HP_TIER_COUNT][VIB_COUNT] = {
    /* dim        mid        vivid */
    {0x4D1C1C, 0xF44336, 0xFF0000},  /* red    */
    {0x4D4D00, 0xFFEB3B, 0xFFFF00},  /* yellow */
    {0x024D3A, 0x06D6A0, 0x66FFD9},  /* green  */
};

static inline int get_hp_tier(int damage, int base_hp)
{
    int remaining = base_hp - damage;

    if (remaining * 2 >= base_hp) return HP_TIER_GREEN;
    if (remaining * 4 >= base_hp) return HP_TIER_YELLOW;
    return HP_TIER_RED;
}

static inline lv_color_t get_hp_color_vib(int tier, int vibrancy)
{
    return lv_color_hex(hp_color_table[tier][vibrancy]);
}

static inline bool color_is_light(lv_color_t c)
{
    uint32_t c32 = lv_color_to32(c);
    int r = (c32 >> 16) & 0xFF;
    int g = (c32 >> 8) & 0xFF;
    int b = c32 & 0xFF;
    int lum = r * 299 + g * 587 + b * 114;
    return lum > 128000;
}

// ---------- LVGL widget helpers ----------
static inline lv_obj_t *make_button(lv_obj_t *parent, const char *txt,
                                     lv_coord_t w, lv_coord_t h,
                                     lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, txt);
    lv_obj_center(label);
    return btn;
}

static inline lv_obj_t *make_plain_box(lv_obj_t *parent,
                                        lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, w, h);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return obj;
}

/* Small rounded pill showing a token (Initiative, Force). Text color
   follows the pill so it stays readable. */
static inline lv_obj_t *make_token_badge(lv_obj_t *parent, const char *txt,
                                          uint32_t color, lv_coord_t w)
{
    lv_obj_t *badge = lv_label_create(parent);
    lv_color_t bg = lv_color_hex(color);

    lv_label_set_text(badge, txt);
    lv_obj_set_size(badge, w, 22);
    lv_obj_set_style_text_font(badge, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(badge, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(badge, color_is_light(bg) ? lv_color_black() : lv_color_white(), 0);
    lv_obj_set_style_bg_color(badge, bg, 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(badge, 11, 0);
    lv_obj_set_style_pad_top(badge, 3, 0);
    lv_obj_set_style_border_color(badge, lv_color_black(), 0);
    lv_obj_set_style_border_width(badge, 1, 0);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(badge, LV_OBJ_FLAG_HIDDEN);
    return badge;
}

/* Centers the visible token badges as one row. Writes each visible
   badge's x offset from the row center and returns how many show. */
static inline int token_row_offsets(const bool show[TOKEN_COUNT],
                                    lv_coord_t x_out[TOKEN_COUNT])
{
    static const lv_coord_t widths[TOKEN_COUNT] = {INITIATIVE_BADGE_W, FORCE_BADGE_W};
    lv_coord_t total = 0;
    lv_coord_t x;
    int count = 0;
    int i;

    for (i = 0; i < TOKEN_COUNT; i++) {
        if (!show[i]) continue;
        total += widths[i];
        count++;
    }
    if (count > 1) total += TOKEN_BADGE_GAP * (count - 1);

    x = -total / 2;
    for (i = 0; i < TOKEN_COUNT; i++) {
        x_out[i] = 0;
        if (!show[i]) continue;
        x_out[i] = x + widths[i] / 2;
        x += widths[i] + TOKEN_BADGE_GAP;
    }
    return count;
}

static inline void load_screen_if_needed(lv_obj_t *screen)
{
    if (screen != NULL && lv_scr_act() != screen) {
        lv_scr_load(screen);
    }
}

#endif // _TYPES_H
