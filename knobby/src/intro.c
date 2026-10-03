#include "intro.h"

// Forward declaration
extern void back_to_main(void);

lv_obj_t *screen_intro = NULL;

static lv_obj_t *intro_letters[INTRO_CHAR_COUNT];
static lv_obj_t *intro_subtitle = NULL;
static uint8_t intro_step = 0;
static lv_timer_t *intro_timer = NULL;

static const char *intro_text[INTRO_CHAR_COUNT] = {"k", "n", "o", "b", "b", "y", "."};
/* One letter per aspect: Vigilance, Command, Aggression, Cunning, Heroism,
   Villainy (grey, since black would vanish on the black screen). */
static const uint32_t intro_colors[INTRO_CHAR_COUNT] = {0x2F7FD0, 0x3FA34D, 0xD3322F, 0xF2C230, 0xEDE6D6, 0x8A8A8A, 0xFFFFFF};
static const lv_coord_t intro_x[INTRO_CHAR_COUNT] = {56, 98, 140, 182, 214, 246, 284};

void refresh_intro_ui(void)
{
    int i;

    for (i = 0; i < INTRO_CHAR_COUNT; i++) {
        if (intro_letters[i] == NULL) continue;

        if (i < intro_step) {
            lv_obj_clear_flag(intro_letters[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(intro_letters[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (intro_subtitle != NULL) {
        if (intro_step >= INTRO_CHAR_COUNT) {
            lv_obj_clear_flag(intro_subtitle, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(intro_subtitle, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void intro_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (intro_step < INTRO_CHAR_COUNT) {
        intro_step++;
        refresh_intro_ui();
        return;
    }

    if (intro_timer != NULL) {
        lv_timer_pause(intro_timer);
    }
    back_to_main();
}

void build_intro_screen(void)
{
    int i;

    screen_intro = lv_obj_create(NULL);
    lv_obj_set_size(screen_intro, 360, 360);
    lv_obj_set_style_bg_color(screen_intro, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_intro, 0, 0);
    lv_obj_set_scrollbar_mode(screen_intro, LV_SCROLLBAR_MODE_OFF);

    for (i = 0; i < INTRO_CHAR_COUNT; i++) {
        intro_letters[i] = lv_label_create(screen_intro);
        lv_label_set_text(intro_letters[i], intro_text[i]);
        lv_obj_set_style_text_color(intro_letters[i], lv_color_hex(intro_colors[i]), 0);
        lv_obj_set_style_text_font(intro_letters[i], &lv_font_montserrat_32, 0);
        lv_obj_set_pos(intro_letters[i], intro_x[i], 146);
        lv_obj_add_flag(intro_letters[i], LV_OBJ_FLAG_HIDDEN);
    }

    intro_subtitle = lv_label_create(screen_intro);
    lv_label_set_text(intro_subtitle, "SWU base counter");
    lv_obj_set_style_text_color(intro_subtitle, lv_color_hex(0x8A8A8A), 0);
    lv_obj_set_style_text_font(intro_subtitle, &lv_font_montserrat_16, 0);
    lv_obj_align(intro_subtitle, LV_ALIGN_CENTER, 0, 30);
    lv_obj_add_flag(intro_subtitle, LV_OBJ_FLAG_HIDDEN);

    refresh_intro_ui();
}

void knob_intro_init(void)
{
    intro_step = 0;
    refresh_intro_ui();
    intro_timer = lv_timer_create(intro_timer_cb, 500, NULL);
    if (intro_timer != NULL) {
        lv_timer_ready(intro_timer);
    }
}
