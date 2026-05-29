#pragma once

#include <vte/vte.h>
#include "config.h"

VteTerminal        *cathode_terminal_new(CathodeConfig *cfg);
void                cathode_terminal_spawn(VteTerminal *term, CathodeConfig *cfg);
void                cathode_terminal_apply_config(VteTerminal *term, CathodeConfig *cfg);
void                cathode_terminal_apply_font(VteTerminal *term, CathodeConfig *cfg);
VteCursorBlinkMode  cathode_cursor_blink_to_vte(CursorBlinkMode mode);
