#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "config.h"

GtkWidget  *cathode_tab_view_new(CathodeConfig *cfg, GtkWindow *window);

void        cathode_tab_new_tab(void);
void        cathode_tab_close_current(void);
void        cathode_tab_toggle_search(void);
void        cathode_tab_rename_current(void);
void        cathode_tab_reapply_font(CathodeConfig *cfg);
void        cathode_tab_reapply_config(CathodeConfig *cfg);

int         cathode_tab_get_n_pages(void);
AdwTabPage *cathode_tab_get_selected_page(void);
