#pragma once

#include <vte/vte.h>

VteTerminal *cathode_terminal_new(void);
void         cathode_terminal_spawn(VteTerminal *terminal);
