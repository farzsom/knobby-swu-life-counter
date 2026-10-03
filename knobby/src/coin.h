#ifndef _COIN_H
#define _COIN_H

#include "types.h"

#define COIN_NONE  0
#define COIN_HEADS 1
#define COIN_TAILS 2

// ---------- state ----------
extern lv_obj_t *screen_coin;
extern int coin_result;

// ---------- functions ----------
void build_coin_screen(void);
void refresh_coin_ui(void);
void open_coin_screen(void);

// event callback used in menu builder
void event_tool_coin(lv_event_t *e);

#endif // _COIN_H
