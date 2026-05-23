#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "config.h"

GtkWidget *cathode_tab_view_new(CathodeConfig *cfg, GtkWindow *window);

void       cathode_tab_new_tab(void);

void       cathode_tab_close_current(void);
