#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include <vte/vte.h>
#include "config.h"

typedef struct {
    CathodeConfig *cfg;
    AdwTabView    *view;
    GtkWindow     *win;
    GtkWidget     *toolbar;
    GtkLabel      *title_label;
    GtkWidget     *search_widget;
    AdwTabPage    *prev_page;
    bool           closing_confirmed;
} CathodeTabState;

CathodeTabState *cathode_tab_view_new(CathodeConfig *cfg, GtkWindow *window);

void             cathode_tab_new_tab(CathodeTabState *state);
void             cathode_tab_close_current(CathodeTabState *state);
void             cathode_tab_toggle_search(CathodeTabState *state);
void             cathode_tab_rename_current(CathodeTabState *state);
void             cathode_tab_reapply_font(CathodeTabState *state, CathodeConfig *cfg);
void             cathode_tab_reapply_config(CathodeTabState *state, CathodeConfig *cfg);

int              cathode_tab_get_n_pages(CathodeTabState *state);
AdwTabPage      *cathode_tab_get_selected_page(CathodeTabState *state);
VteTerminal     *cathode_tab_get_current_terminal(CathodeTabState *state);
