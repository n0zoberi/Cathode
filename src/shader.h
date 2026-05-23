#pragma once

#include <gtk/gtk.h>
#include "config.h"

GtkWidget *cathode_shader_overlay_new(CathodeConfig *cfg, GtkWidget *terminal);

bool cathode_shader_is_effect_active(CathodeConfig *cfg);

void cathode_shader_queue_redraw(GtkWidget *overlay);
void cathode_shader_refresh_visible(GtkWidget *overlay);
