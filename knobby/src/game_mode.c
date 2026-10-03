#include "game_mode.h"
#include "game.h"
#include "storage.h"
#include "settings.h"
#include "ui_mp.h"
#include "net_sync.h"

// Forward declarations
extern void reset_all_values(void);
extern void back_to_main(void);

// ---------- screens ----------
lv_obj_t *screen_game_mode_menu = NULL;
lv_obj_t *screen_custom_hp = NULL;

// ---------- dynamic labels ----------
static lv_obj_t *label_gm_players = NULL;
static lv_obj_t *label_gm_base_hp = NULL;
static lv_obj_t *label_gm_random_first = NULL;

// ---------- custom HP widgets ----------
static lv_obj_t *label_custom_hp_value = NULL;

// ---------- temp settings (applied on Apply) ----------
static int temp_players;
static int temp_base_hp;
static int temp_random_first;

// ---------- refresh ----------
void refresh_game_mode_menu_ui(void)
{
    char buf[32];

    snprintf(buf, sizeof(buf), "Players\n%d", temp_players);
    lv_label_set_text(label_gm_players, buf);

    snprintf(buf, sizeof(buf), "Base HP\n%d", temp_base_hp);
    lv_label_set_text(label_gm_base_hp, buf);

    lv_label_set_text(label_gm_random_first,
                      temp_random_first ? "Random\nInitiative\nON" : "Random\nInitiative\nOFF");
}

void refresh_custom_hp_ui(void)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", temp_base_hp);
    lv_label_set_text(label_custom_hp_value, buf);
}

// ---------- navigation ----------
void open_game_mode_menu(void)
{
    temp_players = nvs_get_players_to_track();
    temp_base_hp = nvs_get_base_hp();
    temp_random_first = nvs_get_random_first();
    refresh_game_mode_menu_ui();
    lv_scr_load(screen_game_mode_menu);
}

// ---------- knob input ----------
void change_custom_hp(int delta)
{
    temp_base_hp = clamp_base_hp(temp_base_hp + delta);
    refresh_custom_hp_ui();
}

// ---------- events ----------
static void event_gm_players(lv_event_t *e)
{
    (void)e;

    temp_players++;
    if (temp_players > MAX_DISPLAY_PLAYERS) temp_players = 1;

    refresh_game_mode_menu_ui();
}

static void event_gm_base_hp(lv_event_t *e)
{
    (void)e;
    refresh_custom_hp_ui();
    lv_scr_load(screen_custom_hp);
}

static void event_gm_random_first(lv_event_t *e)
{
    (void)e;
    temp_random_first = !temp_random_first;
    refresh_game_mode_menu_ui();
}

static void event_gm_apply(lv_event_t *e)
{
    (void)e;
    /* Applying game mode redefines the game (players, view, base HP),
       and settings don't sync — so rather than broadcasting a reset at
       THIS device's config over the whole table, leave the session
       before the reset below can reach it. Same-config new games use
       the shared reset; config changes re-pair. (No-op when not
       synced.) */
    net_sync_leave_game();
    nvs_set_players_to_track(temp_players);
    nvs_set_base_hp(temp_base_hp);
    nvs_set_random_first(temp_random_first);
    set_all_base_hp(temp_base_hp);
    settings_save();
    reset_all_values();
    rebuild_multiplayer_layout(temp_players);
    back_to_main();
    lv_indev_wait_release(lv_indev_get_act());
}

// ---------- screen builders ----------
void build_game_mode_menu_screen(void)
{
    lv_obj_t *btn;

    quad_item_t items[4] = {
        {"Players\n2",               event_gm_players,      true, LV_EVENT_CLICKED},
        {"Base HP\n30",              event_gm_base_hp,      true, LV_EVENT_CLICKED},
        {"Random\nInitiative\nON",   event_gm_random_first, true, LV_EVENT_CLICKED},
        {"Apply\n(Hold)",            event_gm_apply,        true, LV_EVENT_LONG_PRESSED},
    };
    build_quad_screen(&screen_game_mode_menu, items);

    // Store label references for dynamic updates
    btn = lv_obj_get_child(screen_game_mode_menu, 0);
    label_gm_players = lv_obj_get_child(btn, 0);
    btn = lv_obj_get_child(screen_game_mode_menu, 1);
    label_gm_base_hp = lv_obj_get_child(btn, 0);
    btn = lv_obj_get_child(screen_game_mode_menu, 2);
    label_gm_random_first = lv_obj_get_child(btn, 0);
}

void build_custom_hp_screen(void)
{
    lv_obj_t *title;
    lv_obj_t *hint;

    screen_custom_hp = lv_obj_create(NULL);
    lv_obj_set_size(screen_custom_hp, 360, 360);
    lv_obj_set_style_bg_color(screen_custom_hp, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_custom_hp, 0, 0);
    lv_obj_set_scrollbar_mode(screen_custom_hp, LV_SCROLLBAR_MODE_OFF);

    title = lv_label_create(screen_custom_hp);
    lv_label_set_text(title, "Base HP\n(all players)");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 46);

    label_custom_hp_value = lv_label_create(screen_custom_hp);
    lv_label_set_text(label_custom_hp_value, "30");
    lv_obj_set_style_text_color(label_custom_hp_value, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_custom_hp_value, &lv_font_montserrat_bold_56, 0);
    lv_obj_align(label_custom_hp_value, LV_ALIGN_CENTER, 0, 4);

    hint = lv_label_create(screen_custom_hp);
    lv_label_set_text(hint, "Turn knob, swipe back when done");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x6A6A6A), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 52);
}
