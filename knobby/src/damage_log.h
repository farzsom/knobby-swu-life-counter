#ifndef _DAMAGE_LOG_H
#define _DAMAGE_LOG_H

#include "types.h"

#define DAMAGE_LOG_MAX 256

extern lv_obj_t *screen_damage_log;

void damage_log_add(int player, int delta);
void damage_log_reset(void);
void damage_log_remove_last_for(int player);
void damage_log_select_next(void);
void damage_log_select_prev(void);
void damage_log_undo_selected(void);

void build_damage_log_screen(void);
void open_damage_log_screen(void);

#endif // _DAMAGE_LOG_H
