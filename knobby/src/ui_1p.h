#ifndef _UI_1P_H
#define _UI_1P_H

#include "types.h"

// ---------- screens ----------
extern lv_obj_t *screen_1p;

// ---------- functions ----------
void build_main_screen(void);

void refresh_main_ui(void);
void refresh_player_ui(void);

void back_to_main(void);

#endif // _UI_1P_H
