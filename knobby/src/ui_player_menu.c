#include "ui_player_menu.h"
#include "game.h"
#include "rename.h"
#include "settings.h"
#include "storage.h"
#include "ui_1p.h"

#define TILE_DEFAULT_COLOR    0x1A1A2E
#define TILE_INITIATIVE_COLOR 0x7A5C00
#define TILE_FORCE_COLOR      0x0D47A1

// ---------- screens ----------
lv_obj_t *screen_player_menu = NULL;
lv_obj_t *screen_hp_edit = NULL;
lv_obj_t *screen_eliminated_player_menu = NULL;
lv_obj_t *screen_player_color_menu = NULL;
lv_obj_t *screen_player_color_picker = NULL;

// ---------- widgets ----------
static lv_obj_t *tile_base_hp = NULL;
static lv_obj_t *tile_initiative = NULL;
static lv_obj_t *tile_force = NULL;
static lv_obj_t *label_hp_edit_title = NULL;
static lv_obj_t *label_hp_edit_value = NULL;
static lv_obj_t *label_hp_edit_delta = NULL;

// ---------- refresh ----------
static void set_tile(lv_obj_t *tile, const char *text, uint32_t color) {
  if (tile == NULL)
    return;
  lv_label_set_text(lv_obj_get_child(tile, 0), text);
  lv_obj_set_style_bg_color(tile, lv_color_hex(color), 0);
}

/* The player menu is one screen shared by every player, so its tiles
   are relabeled for whoever opened it. */
void refresh_player_menu_ui(void) {
  char buf[24];
  bool has_initiative = (initiative_player == menu_player);
  bool has_force = player_force[menu_player];

  snprintf(buf, sizeof(buf), "Base HP\n%d", player_base_hp[menu_player]);
  set_tile(tile_base_hp, buf, TILE_DEFAULT_COLOR);
  set_tile(tile_initiative,
           has_initiative ? "Has\nInitiative" : "Take\nInitiative",
           has_initiative ? TILE_INITIATIVE_COLOR : TILE_DEFAULT_COLOR);
  set_tile(tile_force, has_force ? "Force\nON" : "Force\nOFF",
           has_force ? TILE_FORCE_COLOR : TILE_DEFAULT_COLOR);
}

void refresh_hp_edit_ui(void) {
  char buf[48];
  int delta = hp_edit_pending_delta();

  if (label_hp_edit_title != NULL) {
    snprintf(buf, sizeof(buf), "%s\nBase HP", player_names[menu_player]);
    lv_label_set_text(label_hp_edit_title, buf);
  }

  if (label_hp_edit_value != NULL) {
    snprintf(buf, sizeof(buf), "%d", hp_edit_value);
    lv_label_set_text(label_hp_edit_value, buf);
  }

  if (label_hp_edit_delta != NULL) {
    if (delta != 0) {
      /* Undialed knob delta, annotated above the (already-updated) value */
      snprintf(buf, sizeof(buf), "%+d", delta);
      lv_label_set_text(label_hp_edit_delta, buf);
      lv_obj_set_style_text_color(label_hp_edit_delta,
                                  (delta > 0) ? lv_color_hex(0x06D6A0)
                                              : lv_color_hex(0xFF1744),
                                  0);
      lv_obj_clear_flag(label_hp_edit_delta, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(label_hp_edit_delta, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

// ---------- navigation ----------
void open_player_menu(int player_index) {
  menu_player = player_index;
  refresh_player_menu_ui();
  load_screen_if_needed(screen_player_menu);
}

void open_hp_edit_screen(void) {
  begin_hp_edit(menu_player);
  refresh_hp_edit_ui();
  load_screen_if_needed(screen_hp_edit);
}

// ---------- events ----------
static void event_menu_rename(lv_event_t *e) {
  (void)e;
  open_rename_screen();
}

static void event_menu_rename_all(lv_event_t *e) {
  (void)e;
  open_rename_all_screen();
  lv_indev_wait_release(lv_indev_get_act());
}

static void event_menu_base_hp(lv_event_t *e) {
  (void)e;
  open_hp_edit_screen();
}

static void event_menu_initiative(lv_event_t *e) {
  (void)e;
  toggle_initiative(menu_player);
  back_to_main();
}

static void event_menu_force(lv_event_t *e) {
  (void)e;
  toggle_force(menu_player);
  back_to_main();
}

static void event_eliminated_undo(lv_event_t *e) {
  (void)e;
  if (elimination_action_available(menu_player)) {
    undo_elimination_action(menu_player);
  } else {
    manual_uneliminate_player(menu_player);
  }
  back_to_main();
}

static void event_menu_concede(lv_event_t *e) {
  (void)e;
  manual_eliminate_player(menu_player);
  back_to_main();
  lv_indev_wait_release(lv_indev_get_act());
}

static void event_hp_apply(lv_event_t *e) {
  (void)e;
  apply_hp_edit();
  back_to_main();
}

// ---------- per-player color ----------
static lv_obj_t *color_picker_swatch = NULL;
static lv_obj_t *color_picker_name_label = NULL;
static lv_obj_t *color_picker_title_label = NULL;
static int color_picker_index = 0;

static void event_menu_color(lv_event_t *e) {
  (void)e;
  load_screen_if_needed(screen_player_color_menu);
}

static void event_color_default(lv_event_t *e) {
  (void)e;
  if (menu_player < 0 || menu_player >= MAX_DISPLAY_PLAYERS)
    return;
  player_has_override[menu_player] = false;
  player_hp_color[menu_player] = false;
  refresh_player_ui();
  back_to_main();
}

static void event_color_hp(lv_event_t *e) {
  (void)e;
  if (menu_player < 0 || menu_player >= MAX_DISPLAY_PLAYERS)
    return;
  player_has_override[menu_player] = true;
  player_hp_color[menu_player] = true;
  refresh_player_ui();
  back_to_main();
}

static void event_color_custom(lv_event_t *e) {
  char title_buf[32];
  (void)e;
  if (menu_player < 0 || menu_player >= MAX_DISPLAY_PLAYERS)
    return;
  color_picker_index = player_color_index[menu_player];
  if (color_picker_title_label != NULL) {
    snprintf(title_buf, sizeof(title_buf), "%s\nColor",
             player_names[menu_player]);
    lv_label_set_text(color_picker_title_label, title_buf);
  }
  if (color_picker_swatch != NULL)
    lv_obj_set_style_bg_color(
        color_picker_swatch,
        get_custom_color_vib(color_picker_index, VIB_MID), 0);
  if (color_picker_name_label != NULL)
    lv_label_set_text(color_picker_name_label,
                      get_custom_color_name(color_picker_index));
  load_screen_if_needed(screen_player_color_picker);
}

void change_player_color(int delta) {
  color_picker_index += delta;
  if (color_picker_index < 0)
    color_picker_index = CUSTOM_COLOR_COUNT - 1;
  if (color_picker_index >= CUSTOM_COLOR_COUNT)
    color_picker_index = 0;

  if (color_picker_swatch != NULL)
    lv_obj_set_style_bg_color(
        color_picker_swatch,
        get_custom_color_vib(color_picker_index, VIB_MID), 0);
  if (color_picker_name_label != NULL)
    lv_label_set_text(color_picker_name_label,
                      get_custom_color_name(color_picker_index));
}

void commit_player_color(void) {
  if (menu_player < 0 || menu_player >= MAX_DISPLAY_PLAYERS)
    return;
  player_has_override[menu_player] = true;
  player_color_index[menu_player] = color_picker_index;
  player_hp_color[menu_player] = false;
  refresh_player_ui();
}

static void event_color_apply(lv_event_t *e) {
  (void)e;
  commit_player_color();
  back_to_main();
}

// ---------- screen builders ----------
void build_player_menu_screen(void) {
  quad_item_t items[4] = {
      {"Name/\nColor", event_menu_color, true, LV_EVENT_CLICKED},
      {"Base HP\n30", event_menu_base_hp, true, LV_EVENT_SHORT_CLICKED},
      {"Take\nInitiative", event_menu_initiative, true, LV_EVENT_CLICKED},
      {"Force\nOFF", event_menu_force, true, LV_EVENT_CLICKED},
  };
  build_quad_screen(&screen_player_menu, items);

  tile_base_hp = lv_obj_get_child(screen_player_menu, 1);
  tile_initiative = lv_obj_get_child(screen_player_menu, 2);
  tile_force = lv_obj_get_child(screen_player_menu, 3);

  /* Long-press Base HP to concede (multiplayer only) */
  lv_obj_add_event_cb(tile_base_hp, event_menu_concede, LV_EVENT_LONG_PRESSED,
                      NULL);
}

void build_eliminated_player_menu_screen(void) {
  screen_eliminated_player_menu = lv_obj_create(NULL);
  lv_obj_set_size(screen_eliminated_player_menu, 360, 360);
  lv_obj_set_style_bg_color(screen_eliminated_player_menu, lv_color_black(), 0);
  lv_obj_set_style_border_width(screen_eliminated_player_menu, 0, 0);
  lv_obj_set_scrollbar_mode(screen_eliminated_player_menu,
                            LV_SCROLLBAR_MODE_OFF);

  lv_obj_t *title = lv_label_create(screen_eliminated_player_menu);
  lv_label_set_text(title, "Base destroyed");
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);

  lv_obj_t *hint = lv_label_create(screen_eliminated_player_menu);
  lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(hint, 240);
  lv_label_set_text(hint, "Undo the damage that destroyed this base");
  lv_obj_set_style_text_color(hint, lv_color_hex(0x7A7A7A), 0);
  lv_obj_set_style_text_font(hint, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(hint, LV_ALIGN_CENTER, 0, -6);

  lv_obj_t *btn = make_button(screen_eliminated_player_menu, "Undo", 120, 46,
                              event_eliminated_undo);
  lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -46);
}

void build_hp_edit_screen(void) {
  lv_obj_t *btn;
  lv_obj_t *hint;

  screen_hp_edit = lv_obj_create(NULL);
  lv_obj_set_size(screen_hp_edit, 360, 360);
  lv_obj_set_style_bg_color(screen_hp_edit, lv_color_black(), 0);
  lv_obj_set_style_border_width(screen_hp_edit, 0, 0);
  lv_obj_set_scrollbar_mode(screen_hp_edit, LV_SCROLLBAR_MODE_OFF);

  label_hp_edit_title = lv_label_create(screen_hp_edit);
  lv_label_set_text(label_hp_edit_title, "P1\nBase HP");
  lv_obj_set_style_text_color(label_hp_edit_title, lv_color_white(), 0);
  lv_obj_set_style_text_font(label_hp_edit_title, &lv_font_montserrat_22, 0);
  lv_obj_set_style_text_align(label_hp_edit_title, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(label_hp_edit_title, LV_ALIGN_TOP_MID, 0, 36);

  label_hp_edit_value = lv_label_create(screen_hp_edit);
  lv_label_set_text(label_hp_edit_value, "30");
  lv_obj_set_style_text_color(label_hp_edit_value, lv_color_white(), 0);
  lv_obj_set_style_text_font(label_hp_edit_value, &lv_font_montserrat_bold_56, 0);
  lv_obj_align(label_hp_edit_value, LV_ALIGN_CENTER, 0, 6);

  /* Pending knob delta, annotated above the running value */
  label_hp_edit_delta = lv_label_create(screen_hp_edit);
  lv_label_set_text(label_hp_edit_delta, "");
  lv_obj_set_style_text_color(label_hp_edit_delta, lv_color_white(), 0);
  lv_obj_set_style_text_font(label_hp_edit_delta, &lv_font_montserrat_22, 0);
  lv_obj_align(label_hp_edit_delta, LV_ALIGN_CENTER, 0, -38);
  lv_obj_add_flag(label_hp_edit_delta, LV_OBJ_FLAG_HIDDEN);

  hint = lv_label_create(screen_hp_edit);
  lv_label_set_text(hint, "Turn knob, then apply");
  lv_obj_set_style_text_color(hint, lv_color_hex(0x7A7A7A), 0);
  lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
  lv_obj_align(hint, LV_ALIGN_CENTER, 0, 54);

  btn = make_button(screen_hp_edit, "Apply", 120, 46, event_hp_apply);
  lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -46);
}

// ---------- per-player color screens ----------
void build_player_color_menu_screen(void) {
  quad_item_t items[4] = {
      {"Rename", event_menu_rename, true, LV_EVENT_SHORT_CLICKED},
      {"Default\nSetting", event_color_default, true, LV_EVENT_CLICKED},
      {"HP\nColor", event_color_hp, true, LV_EVENT_CLICKED},
      {"Custom\nColor", event_color_custom, true, LV_EVENT_CLICKED},
  };
  build_quad_screen(&screen_player_color_menu, items);

  /* Long-press Rename to rename all players sequentially */
  lv_obj_t *name_btn = lv_obj_get_child(screen_player_color_menu, 0);
  lv_obj_add_event_cb(name_btn, event_menu_rename_all, LV_EVENT_LONG_PRESSED,
                      NULL);
}

void build_player_color_picker_screen(void) {
  lv_obj_t *hint;

  screen_player_color_picker = lv_obj_create(NULL);
  lv_obj_set_size(screen_player_color_picker, 360, 360);
  lv_obj_set_style_bg_color(screen_player_color_picker, lv_color_black(), 0);
  lv_obj_set_style_border_width(screen_player_color_picker, 0, 0);
  lv_obj_set_scrollbar_mode(screen_player_color_picker, LV_SCROLLBAR_MODE_OFF);

  color_picker_title_label = lv_label_create(screen_player_color_picker);
  lv_label_set_text(color_picker_title_label, "Color");
  lv_obj_set_style_text_color(color_picker_title_label, lv_color_white(), 0);
  lv_obj_set_style_text_font(color_picker_title_label, &lv_font_montserrat_22,
                             0);
  lv_obj_set_style_text_align(color_picker_title_label, LV_TEXT_ALIGN_CENTER,
                              0);
  lv_obj_align(color_picker_title_label, LV_ALIGN_TOP_MID, 0, 20);

  color_picker_swatch = lv_obj_create(screen_player_color_picker);
  lv_obj_remove_style_all(color_picker_swatch);
  lv_obj_set_size(color_picker_swatch, 120, 120);
  lv_obj_align(color_picker_swatch, LV_ALIGN_CENTER, 0, -15);
  lv_obj_set_style_radius(color_picker_swatch, 12, 0);
  lv_obj_set_style_bg_opa(color_picker_swatch, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(color_picker_swatch,
                            get_custom_color_vib(0, VIB_MID), 0);
  lv_obj_clear_flag(color_picker_swatch,
                    LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

  color_picker_name_label = lv_label_create(screen_player_color_picker);
  lv_label_set_text(color_picker_name_label, get_custom_color_name(0));
  lv_obj_set_style_text_color(color_picker_name_label, lv_color_white(), 0);
  lv_obj_set_style_text_font(color_picker_name_label, &lv_font_montserrat_16,
                             0);
  lv_obj_align(color_picker_name_label, LV_ALIGN_CENTER, 0, 54);

  hint = lv_label_create(screen_player_color_picker);
  lv_label_set_text(hint, "Turn knob, then apply");
  lv_obj_set_style_text_color(hint, lv_color_hex(0x7A7A7A), 0);
  lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
  lv_obj_align(hint, LV_ALIGN_CENTER, 0, 73);

  lv_obj_t *btn = make_button(screen_player_color_picker, "Apply", 120, 46,
                              event_color_apply);
  lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -46);
}
