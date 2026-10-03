#ifndef _GAME_MODE_H
#define _GAME_MODE_H

#include "types.h"

// ---------- screens ----------
extern lv_obj_t *screen_game_mode_menu;
extern lv_obj_t *screen_custom_hp;

// ---------- functions ----------
void build_game_mode_menu_screen(void);
void build_custom_hp_screen(void);
void refresh_game_mode_menu_ui(void);
void refresh_custom_hp_ui(void);
void open_game_mode_menu(void);
void change_custom_hp(int delta);

#endif // _GAME_MODE_H
