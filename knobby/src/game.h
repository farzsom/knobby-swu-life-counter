#ifndef _GAME_H
#define _GAME_H

#include "types.h"

// ---------- state ----------
/* Damage counts up from 0. A base is destroyed once its damage reaches
   its base HP. */
extern int player_damage[MAX_DISPLAY_PLAYERS];
extern int player_base_hp[MAX_DISPLAY_PLAYERS];
extern bool player_force[MAX_DISPLAY_PLAYERS];
extern int initiative_player;   /* -1 when nobody holds the initiative */
extern bool player_selected[MAX_DISPLAY_PLAYERS];
extern char player_names[MAX_DISPLAY_PLAYERS][16];
extern int menu_player;
extern int pending_damage_delta;
extern bool damage_preview_active;
extern int hp_edit_value;
extern bool player_eliminated[MAX_DISPLAY_PLAYERS];

// ---------- functions ----------
void game_init(void);
void game_reset(void);
void change_player_damage(int delta);
void apply_damage_delta(int player, int delta);
void damage_preview_commit_cb(lv_timer_t *timer);
void undo_damage_change(int player, int delta);
bool base_is_destroyed(int player);

// ---------- player selection set ----------
int selection_count(void);
bool is_player_selected(int player);
void selection_clear(void);
void selection_toggle(int player);
void selection_set_single(int player);
void start_player_selection_animation(void);
void stop_player_selection_animation(void);
bool player_selection_animation_active(void);

// ---------- base HP ----------
void set_all_base_hp(int hp);
void begin_hp_edit(int player);
void change_hp_edit(int delta);
int hp_edit_pending_delta(void);
void apply_hp_edit(void);

// ---------- tokens ----------
void toggle_initiative(int player);
void toggle_force(int player);

// ---------- elimination ----------
bool elimination_action_available(int player);
void undo_elimination_action(int player);
void manual_eliminate_player(int player);
void manual_uneliminate_player(int player);
void check_player_elimination(int player);

// ---------- player colors ----------
extern int player_color_index[MAX_DISPLAY_PLAYERS];
extern bool player_hp_color[MAX_DISPLAY_PLAYERS];
extern bool player_has_override[MAX_DISPLAY_PLAYERS];

lv_color_t get_player_color_vib(int index, int vibrancy);
lv_color_t get_player_base_color(int index);
lv_color_t get_player_active_color(int index);
lv_color_t get_player_text_color(int index);
lv_color_t get_player_preview_color(int index, int damage_delta);
lv_color_t get_custom_color_vib(int index, int vibrancy);
const char *get_custom_color_name(int index);
lv_color_t get_effective_player_color(int player_i, int color_i, int vibrancy);

#endif // _GAME_H
