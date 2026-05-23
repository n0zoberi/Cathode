#pragma once

#include <gtk/gtk.h>
#include <vte/vte.h>

GtkWidget *cathode_search_bar_new(GtkWindow *window);

void       cathode_search_set_terminal(GtkWidget *search_widget,
                                       VteTerminal *terminal);

void       cathode_search_toggle(GtkWidget *search_widget);

void       cathode_search_activate(GtkWidget *search_widget, bool reverse);
