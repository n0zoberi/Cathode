#include "search.h"
#include <stdlib.h>
#include <string.h>
#include <adwaita.h>
#include <glib/gi18n.h>

#define PCRE2_MULTILINE 0x00000400u
#define PCRE2_CASELESS  0x00000008u

typedef struct {
    GtkSearchBar   *bar;
    GtkSearchEntry *entry;
    VteTerminal    *term;

    GtkWidget      *btn_case;
    bool            case_insensitive;

    char           *last_search;
} SearchState;

static void
search_state_free(gpointer data)
{
    SearchState *st = data;
    g_free(st->last_search);
    g_free(st);
}

static void
update_regex(SearchState *st)
{
    if (!st->term) return;

    const char *text = gtk_editable_get_text(GTK_EDITABLE(st->entry));

    if (!text || !*text) {
        vte_terminal_search_set_regex(st->term, NULL, 0);
        return;
    }

    guint32 flags = PCRE2_MULTILINE;
    if (st->case_insensitive)
        flags |= PCRE2_CASELESS;

    g_autofree char *escaped = g_regex_escape_string(text, -1);

    GError *error = NULL;
    VteRegex *regex = vte_regex_new_for_search(escaped, -1, flags, &error);
    if (!regex) {
        vte_terminal_search_set_regex(st->term, NULL, 0);
        g_clear_error(&error);
        return;
    }

    gboolean narrowing = st->last_search && g_strrstr(st->last_search, text);
    g_free(st->last_search);
    st->last_search = g_strdup(text);

    if (!narrowing)
        vte_terminal_search_find_previous(st->term);

    vte_terminal_search_set_regex(st->term, regex, 0);
    vte_terminal_search_set_wrap_around(st->term, TRUE);
    vte_regex_unref(regex);

    if (narrowing)
        vte_terminal_search_find_previous(st->term);

    vte_terminal_search_find_next(st->term);
}

static void
on_case_toggled(GtkToggleButton *btn, gpointer data)
{
    SearchState *st = data;
    st->case_insensitive = !gtk_toggle_button_get_active(btn);
    if (st->case_insensitive)
        gtk_widget_add_css_class(GTK_WIDGET(btn), "flat");
    else
        gtk_widget_remove_css_class(GTK_WIDGET(btn), "flat");
    update_regex(st);
}

static void
on_search_changed(GtkSearchEntry *entry, gpointer data)
{
    (void)entry;
    SearchState *st = data;
    update_regex(st);
}


static gboolean
on_search_entry_key(GtkEventControllerKey *controller,
                     guint keyval, guint _keycode,
                     GdkModifierType state, gpointer data)
{
    (void)controller;
    (void)_keycode;
    SearchState *st = data;

    if (keyval == GDK_KEY_Escape) {
        cathode_search_toggle(GTK_WIDGET(st->bar));
        return TRUE;
    }

    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
        if (state & GDK_SHIFT_MASK)
            cathode_search_activate(GTK_WIDGET(st->bar), true);
        else
            cathode_search_activate(GTK_WIDGET(st->bar), false);
        return TRUE;
    }

    return FALSE;
}

static void
on_next_clicked(GtkButton *_btn, gpointer data)
{
    (void)_btn;
    SearchState *st = data;
    cathode_search_activate(GTK_WIDGET(st->bar), false);
}

static void
on_prev_clicked(GtkButton *_btn, gpointer data)
{
    (void)_btn;
    SearchState *st = data;
    cathode_search_activate(GTK_WIDGET(st->bar), true);
}

static void
on_close_clicked(GtkButton *_btn, gpointer data)
{
    (void)_btn;
    SearchState *st = data;
    cathode_search_toggle(GTK_WIDGET(st->bar));
}

GtkWidget *
cathode_search_bar_new(GtkWindow *window)
{
    SearchState *st = g_new0(SearchState, 1);
    st->case_insensitive = true;

    st->bar = GTK_SEARCH_BAR(gtk_search_bar_new());
    gtk_widget_add_css_class(GTK_WIDGET(st->bar), "view");
    gtk_search_bar_set_key_capture_widget(st->bar, GTK_WIDGET(window));

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

    st->entry = GTK_SEARCH_ENTRY(gtk_search_entry_new());
    gtk_box_append(GTK_BOX(hbox), GTK_WIDGET(st->entry));

    st->btn_case = gtk_toggle_button_new_with_label("Aa");
    gtk_widget_add_css_class(st->btn_case, "flat");
    gtk_widget_set_tooltip_text(st->btn_case, _("Toggle case sensitivity"));
    gtk_box_append(GTK_BOX(hbox), st->btn_case);

    GtkWidget *btn_prev = gtk_button_new_from_icon_name("go-up-symbolic");
    gtk_widget_set_tooltip_text(btn_prev, _("Previous match"));
    gtk_box_append(GTK_BOX(hbox), btn_prev);

    GtkWidget *btn_next = gtk_button_new_from_icon_name("go-down-symbolic");
    gtk_widget_set_tooltip_text(btn_next, _("Next match"));
    gtk_box_append(GTK_BOX(hbox), btn_next);

    GtkWidget *btn_close = gtk_button_new_from_icon_name("window-close-symbolic");
    gtk_widget_set_tooltip_text(btn_close, _("Close search"));
    gtk_box_append(GTK_BOX(hbox), btn_close);

    GtkWidget *clamp = adw_clamp_new();
    adw_clamp_set_maximum_size(ADW_CLAMP(clamp), 500);
    adw_clamp_set_child(ADW_CLAMP(clamp), hbox);
    gtk_widget_set_margin_start(clamp, 6);
    gtk_widget_set_margin_end(clamp, 6);
    gtk_widget_set_margin_top(clamp, 4);
    gtk_widget_set_margin_bottom(clamp, 4);

    gtk_search_bar_set_child(st->bar, clamp);

    g_signal_connect(st->entry, "search-changed",
                     G_CALLBACK(on_search_changed), st);
    g_signal_connect(btn_next, "clicked",
                     G_CALLBACK(on_next_clicked), st);
    g_signal_connect(btn_prev, "clicked",
                     G_CALLBACK(on_prev_clicked), st);
    g_signal_connect(st->btn_case, "toggled",
                     G_CALLBACK(on_case_toggled), st);
    g_signal_connect(btn_close, "clicked",
                     G_CALLBACK(on_close_clicked), st);

    GtkEventController *key_ctrl = gtk_event_controller_key_new();
    gtk_widget_add_controller(GTK_WIDGET(st->entry), key_ctrl);
    g_signal_connect(key_ctrl, "key-pressed",
                     G_CALLBACK(on_search_entry_key), st);

    g_object_set_data_full(G_OBJECT(st->bar), "search-state",
                           st, search_state_free);

    return GTK_WIDGET(st->bar);
}

void
cathode_search_set_terminal(GtkWidget *widget, VteTerminal *terminal)
{
    SearchState *st = g_object_get_data(G_OBJECT(widget), "search-state");
    if (!st) return;

    if (st->term)
        vte_terminal_search_set_regex(st->term, NULL, 0);
    st->term = terminal;

    if (!terminal) return;

    vte_terminal_search_set_wrap_around(terminal, TRUE);

    g_clear_pointer(&st->last_search, g_free);

    if (gtk_search_bar_get_search_mode(st->bar)) {
        const char *text = gtk_editable_get_text(GTK_EDITABLE(st->entry));
        if (text && *text)
            update_regex(st);
    }
}

void
cathode_search_toggle(GtkWidget *widget)
{
    if (!widget) return;
    SearchState *st = g_object_get_data(G_OBJECT(widget), "search-state");
    if (!st) return;

    bool visible = gtk_search_bar_get_search_mode(GTK_SEARCH_BAR(widget));

    if (visible) {
        gtk_search_bar_set_search_mode(GTK_SEARCH_BAR(widget), FALSE);
        g_clear_pointer(&st->last_search, g_free);
        if (st->term)
            vte_terminal_search_set_regex(st->term, NULL, 0);
    } else {
        gtk_search_bar_set_search_mode(GTK_SEARCH_BAR(widget), TRUE);

        if (st->term && vte_terminal_get_has_selection(st->term)) {
            char *selected = vte_terminal_get_text_selected(st->term, VTE_FORMAT_TEXT);
            if (selected && *selected) {
                gtk_editable_set_text(GTK_EDITABLE(st->entry), selected);
                gtk_editable_select_region(GTK_EDITABLE(st->entry), 0, -1);
            }
            g_free(selected);
        }

        gtk_widget_grab_focus(GTK_WIDGET(st->entry));
    }
}

void
cathode_search_activate(GtkWidget *widget, bool reverse)
{
    if (!widget) return;
    SearchState *st = g_object_get_data(G_OBJECT(widget), "search-state");
    if (!st || !st->term) return;

    if (reverse)
        vte_terminal_search_find_previous(st->term);
    else
        vte_terminal_search_find_next(st->term);
}
