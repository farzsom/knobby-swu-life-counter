#include "coin.h"
#include "esp_random.h"

#define COIN_SIZE 180

// ---------- state ----------
lv_obj_t *screen_coin = NULL;
int coin_result = COIN_NONE;
static lv_obj_t *coin_disc = NULL;
static lv_obj_t *label_coin_result = NULL;
static lv_obj_t *label_coin_hint = NULL;

// ---------- refresh ----------
void refresh_coin_ui(void)
{
    if (label_coin_result == NULL) return;

    switch (coin_result) {
    case COIN_HEADS: lv_label_set_text(label_coin_result, "HEADS"); break;
    case COIN_TAILS: lv_label_set_text(label_coin_result, "TAILS"); break;
    default:         lv_label_set_text(label_coin_result, "--");    break;
    }
}

/* Squash the disc horizontally so it reads as a coin spinning on its edge. */
static void coin_width_anim_cb(void *obj, int32_t width)
{
    lv_obj_set_width((lv_obj_t *)obj, (lv_coord_t)width);
    lv_obj_align((lv_obj_t *)obj, LV_ALIGN_CENTER, 0, -10);
}

static void flip_coin(void)
{
    lv_anim_t anim;

    coin_result = (esp_random() & 1U) ? COIN_HEADS : COIN_TAILS;
    refresh_coin_ui();

    if (coin_disc == NULL) return;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, coin_disc);
    lv_anim_set_values(&anim, COIN_SIZE, 8);
    lv_anim_set_time(&anim, 90);
    lv_anim_set_playback_time(&anim, 90);
    lv_anim_set_repeat_count(&anim, 3);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&anim, coin_width_anim_cb);
    lv_anim_start(&anim);
}

// ---------- open ----------
void open_coin_screen(void)
{
    flip_coin();
    load_screen_if_needed(screen_coin);
}

// ---------- events ----------
static void event_coin_hold(lv_event_t *e)
{
    (void)e;
    flip_coin();
}

void event_tool_coin(lv_event_t *e)
{
    (void)e;
    open_coin_screen();
}

// ---------- build ----------
void build_coin_screen(void)
{
    screen_coin = lv_obj_create(NULL);
    lv_obj_set_size(screen_coin, 360, 360);
    lv_obj_set_style_bg_color(screen_coin, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_coin, 0, 0);
    lv_obj_set_scrollbar_mode(screen_coin, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(screen_coin, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(screen_coin, event_coin_hold, LV_EVENT_LONG_PRESSED, NULL);

    coin_disc = lv_obj_create(screen_coin);
    lv_obj_remove_style_all(coin_disc);
    lv_obj_set_size(coin_disc, COIN_SIZE, COIN_SIZE);
    lv_obj_set_style_radius(coin_disc, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(coin_disc, lv_color_hex(0x3A2E00), 0);
    lv_obj_set_style_bg_opa(coin_disc, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(coin_disc, lv_color_hex(INITIATIVE_COLOR), 0);
    lv_obj_set_style_border_width(coin_disc, 6, 0);
    lv_obj_clear_flag(coin_disc, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(coin_disc, LV_ALIGN_CENTER, 0, -10);

    label_coin_result = lv_label_create(coin_disc);
    lv_label_set_text(label_coin_result, "--");
    lv_obj_set_style_text_color(label_coin_result, lv_color_hex(INITIATIVE_COLOR), 0);
    lv_obj_set_style_text_font(label_coin_result, &lv_font_montserrat_32, 0);
    lv_obj_center(label_coin_result);

    label_coin_hint = lv_label_create(screen_coin);
    lv_label_set_text(label_coin_hint, "Hold to flip again");
    lv_obj_set_style_text_color(label_coin_hint, lv_color_hex(0x8A8A8A), 0);
    lv_obj_set_style_text_font(label_coin_hint, &lv_font_montserrat_14, 0);
    lv_obj_align(label_coin_hint, LV_ALIGN_CENTER, 0, 104);
}
