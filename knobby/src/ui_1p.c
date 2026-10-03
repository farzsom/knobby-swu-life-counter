#include "ui_1p.h"
#include "settings.h"
#include "ui_mp.h"
#include "ui_player_menu.h"
#include "game.h"
#include "timer.h"
#include "storage.h"
#include "hw.h"

#define DESTROYED_COLOR 0xFF1744
#define HEAL_COLOR      0x06D6A0

// ---------- screens ----------
lv_obj_t *screen_1p = NULL;

// ---------- main UI widgets ----------
static lv_obj_t *arc_damage = NULL;
static lv_obj_t *damage_hitbox = NULL;
static lv_obj_t *label_damage_total = NULL;
static lv_obj_t *label_damage_preview_total = NULL;
static lv_obj_t *label_base_hp = NULL;
static lv_obj_t *turn_container = NULL;
static lv_obj_t *label_turn = NULL;
static lv_obj_t *turn_live_dot = NULL;
static lv_obj_t *token_badges_1p[TOKEN_COUNT];

// ---------- refresh functions ----------
static void refresh_ring(void)
{
    int base_hp = player_base_hp[0];
    lv_color_t c = base_is_destroyed(0) ? lv_color_hex(DESTROYED_COLOR)
                                        : get_effective_player_color(0, 0, VIB_MID);

    /* The ring fills clockwise as the base takes damage. */
    lv_arc_set_range(arc_damage, 0, base_hp);
    lv_arc_set_value(arc_damage, get_arc_display_value(player_damage[0], base_hp));

    lv_obj_set_style_arc_color(arc_damage, lv_color_hex(0x202020), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_damage, 20, LV_PART_MAIN);

    lv_obj_set_style_arc_color(arc_damage, c, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_damage, 20, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc_damage, true, LV_PART_INDICATOR);
}

void refresh_turn_ui(void)
{
    char buf[48];
    uint32_t total_seconds = get_turn_elapsed_ms() / 1000;
    uint32_t hours = total_seconds / 3600;
    uint32_t minutes = (total_seconds % 3600) / 60;

    if (turn_number <= 0) {
        snprintf(buf, sizeof(buf), "round  %lu:%02lu",
                 (unsigned long)hours, (unsigned long)minutes);
    } else {
        snprintf(buf, sizeof(buf), "round %d  %lu:%02lu",
                 turn_number, (unsigned long)hours, (unsigned long)minutes);
    }
    lv_label_set_text(label_turn, buf);

    if (turn_live_dot != NULL) {
        lv_obj_align_to(turn_live_dot, label_turn, LV_ALIGN_OUT_RIGHT_MID, 6, 0);
    }

    if (turn_container != NULL) {
        if (turn_ui_visible) {
            lv_obj_clear_flag(turn_container, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(turn_container, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (turn_live_dot != NULL) {
        if (turn_timer_enabled) {
            lv_obj_clear_flag(turn_live_dot, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(turn_live_dot, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (turn_container != NULL) {
        lv_obj_set_style_opa(turn_container, turn_indicator_visible ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    }
}

static void refresh_damage_digits(void)
{
    char buf[16];
    lv_color_t c;

    if (damage_preview_active) {
        c = (pending_damage_delta > 0) ? lv_color_hex(DESTROYED_COLOR)
                                       : lv_color_hex(HEAL_COLOR);
        snprintf(buf, sizeof(buf), "%+d", pending_damage_delta);
    } else {
        c = base_is_destroyed(0) ? lv_color_hex(DESTROYED_COLOR)
                                 : get_effective_player_color(0, 0, VIB_MID);
        snprintf(buf, sizeof(buf), "%d", player_damage[0]);
    }

    lv_label_set_text(label_damage_total, buf);
    lv_obj_set_style_text_color(label_damage_total, c, 0);
    lv_obj_align(label_damage_total, LV_ALIGN_CENTER, 0, -6);

    /* While dialing, the line under the number shows the damage the base
       will have; otherwise it shows the base HP being counted toward. */
    if (damage_preview_active) {
        snprintf(buf, sizeof(buf), "= %d", player_damage[0] + pending_damage_delta);
        lv_label_set_text(label_damage_preview_total, buf);
        lv_obj_clear_flag(label_damage_preview_total, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(label_base_hp, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(label_damage_preview_total, LV_OBJ_FLAG_HIDDEN);
        if (base_is_destroyed(0)) {
            lv_label_set_text(label_base_hp, "DESTROYED");
            lv_obj_set_style_text_color(label_base_hp, lv_color_hex(DESTROYED_COLOR), 0);
        } else {
            snprintf(buf, sizeof(buf), "/ %d", player_base_hp[0]);
            lv_label_set_text(label_base_hp, buf);
            lv_obj_set_style_text_color(label_base_hp, lv_color_hex(0x9A9A9A), 0);
        }
        lv_obj_clear_flag(label_base_hp, LV_OBJ_FLAG_HIDDEN);
    }
}

static void refresh_1p_tokens(void)
{
    bool show[TOKEN_COUNT] = {initiative_player == 0, player_force[0]};
    lv_coord_t x[TOKEN_COUNT];
    int i;

    token_row_offsets(show, x);
    for (i = 0; i < TOKEN_COUNT; i++) {
        if (token_badges_1p[i] == NULL) continue;
        if (!show[i]) {
            lv_obj_add_flag(token_badges_1p[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_align(token_badges_1p[i], LV_ALIGN_TOP_MID, x[i], 54);
        lv_obj_clear_flag(token_badges_1p[i], LV_OBJ_FLAG_HIDDEN);
    }
}

void refresh_main_ui(void)
{
    refresh_ring();
    refresh_damage_digits();
    refresh_turn_ui();
    refresh_1p_tokens();
}

void refresh_player_ui(void)
{
    if (nvs_get_players_to_track() == 1)
        refresh_main_ui();
    else
        refresh_multiplayer_ui();
}

// ---------- navigation ----------
void back_to_main(void)
{
    int track = nvs_get_players_to_track();
    if (track > 1) {
        refresh_multiplayer_ui();
        load_screen_if_needed(screen_multiplayer);
    } else {
        refresh_main_ui();
        load_screen_if_needed(screen_1p);
    }
}

// ---------- events ----------
static void event_open_1p_menu(lv_event_t *e)
{
    (void)e;
    open_player_menu(0);
    lv_indev_wait_release(lv_indev_get_act());
}

// ---------- screen builders ----------
void build_main_screen(void)
{
    screen_1p = lv_obj_create(NULL);
    lv_obj_set_size(screen_1p, 360, 360);
    lv_obj_set_style_bg_color(screen_1p, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_1p, 0, 0);
    lv_obj_set_scrollbar_mode(screen_1p, LV_SCROLLBAR_MODE_OFF);

    arc_damage = lv_arc_create(screen_1p);
    lv_obj_set_size(arc_damage, 360, 360);
    lv_obj_center(arc_damage);
    lv_arc_set_rotation(arc_damage, 270);
    lv_arc_set_bg_angles(arc_damage, 0, 360);
    lv_arc_set_range(arc_damage, 0, player_base_hp[0]);
    lv_arc_set_value(arc_damage, 0);
    lv_obj_remove_style(arc_damage, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc_damage, LV_OBJ_FLAG_CLICKABLE);

    damage_hitbox = make_plain_box(screen_1p, 360, 360);
    lv_obj_align(damage_hitbox, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(damage_hitbox, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(damage_hitbox, event_open_1p_menu, LV_EVENT_LONG_PRESSED, NULL);

    label_damage_total = lv_label_create(screen_1p);
    lv_label_set_text(label_damage_total, "0");
    lv_obj_set_style_text_font(label_damage_total, &lv_font_montserrat_bold_116, 0);
    lv_obj_set_style_text_color(label_damage_total, lv_color_white(), 0);
    lv_obj_align(label_damage_total, LV_ALIGN_CENTER, 0, -6);

    label_damage_preview_total = lv_label_create(screen_1p);
    lv_label_set_text(label_damage_preview_total, "");
    lv_obj_set_style_text_color(label_damage_preview_total, lv_color_hex(0xB8B8B8), 0);
    lv_obj_set_style_text_font(label_damage_preview_total, &lv_font_montserrat_regular_48, 0);
    lv_obj_align(label_damage_preview_total, LV_ALIGN_CENTER, 0, 80);
    lv_obj_add_flag(label_damage_preview_total, LV_OBJ_FLAG_HIDDEN);

    label_base_hp = lv_label_create(screen_1p);
    lv_label_set_text(label_base_hp, "");
    lv_obj_set_style_text_color(label_base_hp, lv_color_hex(0x9A9A9A), 0);
    lv_obj_set_style_text_font(label_base_hp, &lv_font_montserrat_32, 0);
    lv_obj_align(label_base_hp, LV_ALIGN_CENTER, 0, 72);

    turn_container = make_plain_box(screen_1p, 240, 32);
    lv_obj_align(turn_container, LV_ALIGN_CENTER, 0, 120);
    lv_obj_add_flag(turn_container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(turn_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(turn_container, event_turn_tap, LV_EVENT_CLICKED, NULL);

    label_turn = lv_label_create(turn_container);
    lv_label_set_text(label_turn, "round  0:00");
    lv_obj_set_style_text_color(label_turn, lv_color_hex(0xB8B8B8), 0);
    lv_obj_set_style_text_font(label_turn, &lv_font_montserrat_22, 0);
    lv_obj_align(label_turn, LV_ALIGN_CENTER, 0, 0);

    turn_live_dot = lv_obj_create(turn_container);
    lv_obj_remove_style_all(turn_live_dot);
    lv_obj_set_size(turn_live_dot, 10, 10);
    lv_obj_set_style_radius(turn_live_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(turn_live_dot, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_bg_opa(turn_live_dot, LV_OPA_COVER, 0);
    lv_obj_align_to(turn_live_dot, label_turn, LV_ALIGN_OUT_RIGHT_MID, 6, 0);
    lv_obj_add_flag(turn_live_dot, LV_OBJ_FLAG_HIDDEN);

    token_badges_1p[0] = make_token_badge(screen_1p, "INITIATIVE", INITIATIVE_COLOR, INITIATIVE_BADGE_W);
    token_badges_1p[1] = make_token_badge(screen_1p, "FORCE", FORCE_COLOR, FORCE_BADGE_W);

    {
        lv_obj_t *batt = lv_label_create(screen_1p);
        lv_label_set_text(batt, LV_SYMBOL_BATTERY_EMPTY);
        lv_obj_set_style_text_color(batt, lv_palette_main(LV_PALETTE_RED), 0);
        lv_obj_set_style_text_font(batt, &lv_font_montserrat_22, 0);
        lv_obj_align(batt, LV_ALIGN_TOP_MID, 0, 24);
        battery_icon_register(batt);
    }
}
