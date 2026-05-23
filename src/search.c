#include "search.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    GtkSearchBar *bar;
    GtkSearchEntry *entry;
    VteTerminal *term;
    bool visible;
} SearchState;

static void
update_regex(SearchState *st)
{
    if (!st->term) return;

    const char *text = gtk_editable_get_text(GTK_EDITABLE(st->entry));

    if (!text || !*text) {
        vte_terminal_search_set_regex(st->term, NULL, 0);
        return;
    }

    GError *error = NULL;
    VteRegex *regex = vte_regex_new_for_search(text, -1,
                                                VTE_REGEX_FLAGS_DEFAULT,
                                                &error);
    if (!regex) {
        vte_terminal_search_set_regex(st->term, NULL, 0);
        g_clear_error(&error);
        return;
    }

    vte_terminal_search_set_regex(st->term, regex,
                                   VTE_REGEX_FLAGS_DEFAULT);
    vte_regex_unref(regex);

    vte_terminal_search_find_next(st->term);
}

static void
on_search_changed(GtkSearchEntry *entry, gpointer data)
{
    (void)entry;
    update_regex(data);
}

static gboolean
on_search_entry_key(GtkEventControllerKey *controller,
                     guint keyval, guint _keycode,
                     GdkModifierType state, gpointer data)
{
    (void)controller;
    (void)_keycode;
    (void)data;

    if (keyval == GDK_KEY_Escape) {
            cathode_search_toggle(NULL);
        return TRUE;
    }

    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
        if (state & GDK_SHIFT_MASK)
            cathode_search_activate(NULL, true);
        else
            cathode_search_activate(NULL, false);
        return TRUE;
    }

    return FALSE;
}

static void
on_entry_activate(GtkSearchEntry *_entry, gpointer data)
{
    (void)_entry;
    (void)data;
    cathode_search_activate(NULL, false);
}

static void
on_next_clicked(GtkButton *_btn, gpointer data)
{
    (void)_btn;
    (void)data;
    cathode_search_activate(NULL, false);
}

static void
on_prev_clicked(GtkButton *_btn, gpointer data)
{
    (void)_btn;
    (void)data;
    cathode_search_activate(NULL, true);
}

GtkWidget *
cathode_search_bar_new(GtkWindow *window)
{
    SearchState *st = g_new0(SearchState, 1);

    st->bar = GTK_SEARCH_BAR(gtk_search_bar_new());
    gtk_search_bar_set_key_capture_widget(st->bar, GTK_WIDGET(window));

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_start(hbox, 6);
    gtk_widget_set_margin_end(hbox, 6);
    gtk_widget_set_margin_top(hbox, 4);
    gtk_widget_set_margin_bottom(hbox, 4);

    st->entry = GTK_SEARCH_ENTRY(gtk_search_entry_new());
    gtk_widget_set_hexpand(GTK_WIDGET(st->entry), TRUE);
    gtk_box_append(GTK_BOX(hbox), GTK_WIDGET(st->entry));

    GtkWidget *btn_prev = gtk_button_new_with_label("▲");
    gtk_box_append(GTK_BOX(hbox), btn_prev);

    GtkWidget *btn_next = gtk_button_new_with_label("▼");
    gtk_box_append(GTK_BOX(hbox), btn_next);

    gtk_search_bar_set_child(st->bar, hbox);

    g_signal_connect(st->entry, "search-changed",
                     G_CALLBACK(on_search_changed), st);
    g_signal_connect(st->entry, "activate",
                     G_CALLBACK(on_entry_activate), st);
    g_signal_connect(btn_next, "clicked",
                     G_CALLBACK(on_next_clicked), st);
    g_signal_connect(btn_prev, "clicked",
                     G_CALLBACK(on_prev_clicked), st);

    GtkEventController *key_ctrl = gtk_event_controller_key_new();
    gtk_widget_add_controller(GTK_WIDGET(st->entry), key_ctrl);
    g_signal_connect(key_ctrl, "key-pressed",
                     G_CALLBACK(on_search_entry_key), st);

    g_object_set_data_full(G_OBJECT(st->bar), "search-state",
                           st, g_free);

    return GTK_WIDGET(st->bar);
}

void
cathode_search_set_terminal(GtkWidget *widget, VteTerminal *terminal)
{
    SearchState *st = g_object_get_data(G_OBJECT(widget), "search-state");
    if (st) {
        if (st->term)
            vte_terminal_search_set_regex(st->term, NULL, 0);
        st->term = terminal;
    }
}

void
cathode_search_toggle(GtkWidget *widget)
{
    static GtkWidget *global_bar = NULL;
    static SearchState *global_st = NULL;

    if (widget && g_object_get_data(G_OBJECT(widget), "search-state")) {
        global_bar = widget;
        global_st = g_object_get_data(G_OBJECT(widget), "search-state");
    }

    if (!global_bar) return;

    bool visible = gtk_search_bar_get_search_mode(GTK_SEARCH_BAR(global_bar));

    if (visible) {
        gtk_search_bar_set_search_mode(GTK_SEARCH_BAR(global_bar), FALSE);
        if (global_st && global_st->term)
            vte_terminal_search_set_regex(global_st->term, NULL, 0);
    } else {
        gtk_search_bar_set_search_mode(GTK_SEARCH_BAR(global_bar), TRUE);
        gtk_widget_grab_focus(GTK_WIDGET(global_st->entry));
    }
}

void
cathode_search_activate(GtkWidget *widget, bool reverse)
{
    static SearchState *global_st = NULL;

    if (widget && g_object_get_data(G_OBJECT(widget), "search-state"))
        global_st = g_object_get_data(G_OBJECT(widget), "search-state");

    if (!global_st || !global_st->term) return;

    if (reverse)
        vte_terminal_search_find_previous(global_st->term);
    else
        vte_terminal_search_find_next(global_st->term);
}
