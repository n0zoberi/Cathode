#pragma once

#include <gtk/gtk.h>
#include <vte/vte.h>
#include "config.h"

GtkWidget   *cathode_shader_overlay_new(CathodeConfig *cfg, GtkWidget *terminal);
VteTerminal *cathode_shader_overlay_get_terminal(GtkWidget *overlay);

bool cathode_shader_is_effect_active(CathodeConfig *cfg);

void cathode_shader_refresh_visible(GtkWidget *overlay);
