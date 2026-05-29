#include "tab.h"
#include "terminal.h"
#include "shader.h"
#include "search.h"
#include <vte/vte.h>
#include <glib/gi18n.h>

static VteTerminal *
get_terminal_from_page(AdwTabPage *page)
{
    GtkWidget *overlay = adw_tab_page_get_child(page);
    GtkWidget *first = gtk_widget_get_first_child(overlay);
    if (first && VTE_IS_TERMINAL(first))
        return VTE_TERMINAL(first);
    return NULL;
}

static void
update_window_title(CathodeTabState *state)
{
    const char *title = NULL;
    AdwTabPage *page = adw_tab_view_get_selected_page(state->view);
    if (page) {
        VteTerminal *term = get_terminal_from_page(page);
        if (term)
            title = vte_terminal_get_window_title(term);
    }
    const char *final_title = title && *title ? title : "Cathode";
    gtk_window_set_title(state->win, final_title);
    if (state->title_label)
        gtk_label_set_text(state->title_label, final_title);
}

static void
on_title_changed(VteTerminal *term, gpointer data)
{
    CathodeTabState *state = data;
    AdwTabPage *page = g_object_get_data(G_OBJECT(term), "page");
    const char *title = vte_terminal_get_window_title(term);
    adw_tab_page_set_title(page,
                           title && *title ? title : "Cathode");
    update_window_title(state);
}

static void
on_child_exited(VteTerminal *term, int status, gpointer data)
{
    (void)status;
    CathodeTabState *state = data;
    AdwTabPage *page = g_object_get_data(G_OBJECT(term), "page");
    if (adw_tab_page_get_child(page))
        adw_tab_view_close_page(state->view, page);
}

static void
on_close_page(AdwTabView *v, AdwTabPage *page, gpointer data)
{
    CathodeTabState *state = data;
    adw_tab_view_close_page_finish(v, page, TRUE);
    if (adw_tab_view_get_n_pages(v) == 0 && state->win)
        gtk_window_close(state->win);
}

static GtkWidget *
create_tab(CathodeConfig *cfg)
{
    GtkWidget *term = GTK_WIDGET(cathode_terminal_new(cfg));
    GtkWidget *overlay = cathode_shader_overlay_new(cfg, term);
    cathode_terminal_spawn(VTE_TERMINAL(term), cfg);
    return overlay;
}

static void
on_page_attached(AdwTabView *v, AdwTabPage *page, int pos, gpointer data)
{
    (void)v;
    (void)pos;
    CathodeTabState *state = data;

    VteTerminal *term = get_terminal_from_page(page);
    if (!term) return;

    g_object_set_data(G_OBJECT(term), "page", page);

    g_signal_connect(term, "window-title-changed",
                     G_CALLBACK(on_title_changed), state);
    g_signal_connect(term, "child-exited",
                     G_CALLBACK(on_child_exited), state);

    const char *title = vte_terminal_get_window_title(term);
    adw_tab_page_set_title(page, title && *title ? title : "Cathode");
}

static void
on_selected_page_changed(AdwTabView *v, GParamSpec *pspec, gpointer data)
{
    (void)v;
    (void)pspec;
    CathodeTabState *state = data;

    AdwTabPage *page = adw_tab_view_get_selected_page(state->view);
    if (page == state->prev_page) return;
    state->prev_page = page;

    VteTerminal *term = get_terminal_from_page(page);
    cathode_search_set_terminal(state->search_widget, term);

    if (term) {
        gtk_widget_grab_focus(GTK_WIDGET(term));
        vte_terminal_set_cursor_blink_mode(term,
            cathode_cursor_blink_to_vte(state->cfg->cursor_blink));
    }

    update_window_title(state);
}

CathodeTabState *
cathode_tab_view_new(CathodeConfig *c, GtkWindow *window)
{
    CathodeTabState *state = g_new0(CathodeTabState, 1);
    state->cfg = c;
    state->win = window;

    state->view = ADW_TAB_VIEW(adw_tab_view_new());

    AdwTabViewShortcuts sc = ADW_TAB_VIEW_SHORTCUT_NONE;
    sc |= ADW_TAB_VIEW_SHORTCUT_CONTROL_HOME;
    sc |= ADW_TAB_VIEW_SHORTCUT_CONTROL_END;
    sc |= ADW_TAB_VIEW_SHORTCUT_CONTROL_TAB;
    sc |= ADW_TAB_VIEW_SHORTCUT_CONTROL_SHIFT_TAB;
    sc |= ADW_TAB_VIEW_SHORTCUT_CONTROL_PAGE_UP;
    sc |= ADW_TAB_VIEW_SHORTCUT_CONTROL_PAGE_DOWN;
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    adw_tab_view_set_shortcuts(state->view, sc);
    G_GNUC_END_IGNORE_DEPRECATIONS

    g_signal_connect(state->view, "page-attached",
                     G_CALLBACK(on_page_attached), state);
    g_signal_connect(state->view, "close-page",
                     G_CALLBACK(on_close_page), state);
    g_signal_connect(state->view, "notify::selected-page",
                     G_CALLBACK(on_selected_page_changed), state);

    AdwToolbarView *toolbar = ADW_TOOLBAR_VIEW(adw_toolbar_view_new());
    state->toolbar = GTK_WIDGET(toolbar);

    AdwHeaderBar *header = ADW_HEADER_BAR(adw_header_bar_new());
    adw_header_bar_set_show_start_title_buttons(header, TRUE);
    adw_header_bar_set_show_end_title_buttons(header, TRUE);

    state->title_label = GTK_LABEL(gtk_label_new("Cathode"));
    gtk_widget_add_css_class(GTK_WIDGET(state->title_label), "title");
    adw_header_bar_set_title_widget(header, GTK_WIDGET(state->title_label));

    GtkWidget *btn_new = gtk_button_new_from_icon_name("list-add-symbolic");
    gtk_button_set_has_frame(GTK_BUTTON(btn_new), FALSE);
    gtk_widget_set_tooltip_text(btn_new, _("New Tab"));
    g_signal_connect_swapped(btn_new, "clicked",
                             G_CALLBACK(cathode_tab_new_tab), state);
    adw_header_bar_pack_start(header, btn_new);

    GMenu *menu = g_menu_new();

    GMenu *sec1 = g_menu_new();
    g_menu_append(sec1, _("Copy"), "win.copy");
    g_menu_append(sec1, _("Paste"), "win.paste");
    g_menu_append(sec1, _("Search"), "win.toggle-search");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(sec1));

    GMenu *sec2 = g_menu_new();
    g_menu_append(sec2, _("New Tab"), "win.new-tab");
    g_menu_append(sec2, _("Close Tab"), "win.close-tab");
    g_menu_append(sec2, _("Rename Tab"), "win.rename-tab");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(sec2));

    GMenu *sec3 = g_menu_new();
    g_menu_append(sec3, _("Clear Screen"), "win.clear-screen");
    g_menu_append(sec3, _("Reset Terminal"), "win.reset-terminal");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(sec3));

    GMenu *sec4 = g_menu_new();
    g_menu_append(sec4, _("Open Config File"), "win.open-config");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(sec4));

    GMenu *sec5 = g_menu_new();
    g_menu_append(sec5, _("Quit"), "win.quit");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(sec5));

    GtkWidget *menu_btn = gtk_menu_button_new();
    gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(menu_btn), G_MENU_MODEL(menu));
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(menu_btn), "open-menu-symbolic");
    gtk_widget_set_tooltip_text(menu_btn, _("Menu"));
    adw_header_bar_pack_end(header, menu_btn);

    adw_toolbar_view_add_top_bar(toolbar, GTK_WIDGET(header));

    AdwTabBar *bar = ADW_TAB_BAR(adw_tab_bar_new());
    adw_tab_bar_set_view(bar, state->view);
    adw_toolbar_view_add_top_bar(toolbar, GTK_WIDGET(bar));

    state->search_widget = cathode_search_bar_new(window);
    adw_toolbar_view_add_top_bar(toolbar, state->search_widget);

    GtkWidget *content = GTK_WIDGET(state->view);
    gtk_widget_set_hexpand(content, TRUE);
    gtk_widget_set_vexpand(content, TRUE);
    adw_toolbar_view_set_content(toolbar, content);

    cathode_tab_new_tab(state);
    update_window_title(state);

    return state;
}

static void
on_rename_dialog_response(AdwAlertDialog *dialog, const char *response, gpointer data)
{
    (void)data;
    if (g_strcmp0(response, "ok") == 0) {
        GtkWidget *entry = g_object_get_data(G_OBJECT(dialog), "entry");
        AdwTabPage *page = g_object_get_data(G_OBJECT(dialog), "page");
        if (entry && page) {
            const char *new_title = gtk_editable_get_text(GTK_EDITABLE(entry));
            if (new_title && *new_title)
                adw_tab_page_set_title(page, new_title);
        }
    }
}

void
cathode_tab_rename_current(CathodeTabState *state)
{
    AdwTabPage *page = adw_tab_view_get_selected_page(state->view);
    if (!page) return;

    const char *current = adw_tab_page_get_title(page);

    GtkWidget *entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(entry), current);
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    gtk_widget_set_size_request(entry, 250, -1);

    AdwAlertDialog *dialog = ADW_ALERT_DIALOG(
        adw_alert_dialog_new(_("Rename Tab"), NULL));
    adw_alert_dialog_set_extra_child(dialog, entry);
    adw_alert_dialog_add_responses(dialog,
        "cancel", _("Cancel"),
        "ok", _("OK"),
        NULL);
    adw_alert_dialog_set_default_response(dialog, "ok");
    adw_alert_dialog_set_close_response(dialog, "cancel");

    g_object_set_data(G_OBJECT(dialog), "entry", entry);
    g_object_set_data(G_OBJECT(dialog), "page", page);
    g_signal_connect(dialog, "response",
                     G_CALLBACK(on_rename_dialog_response), NULL);

    adw_dialog_present(ADW_DIALOG(dialog), GTK_WIDGET(state->win));
}

void
cathode_tab_new_tab(CathodeTabState *state)
{
    GtkWidget *content = create_tab(state->cfg);
    adw_tab_view_append(state->view, content);
    int n = adw_tab_view_get_n_pages(state->view);
    AdwTabPage *page = adw_tab_view_get_nth_page(state->view, n - 1);
    adw_tab_view_set_selected_page(state->view, page);
}

void
cathode_tab_close_current(CathodeTabState *state)
{
    AdwTabPage *page = adw_tab_view_get_selected_page(state->view);
    if (page)
        adw_tab_view_close_page(state->view, page);
}

void
cathode_tab_toggle_search(CathodeTabState *state)
{
    cathode_search_toggle(state->search_widget);
}

void
cathode_tab_reapply_font(CathodeTabState *state, CathodeConfig *c)
{
    int n = adw_tab_view_get_n_pages(state->view);
    for (int i = 0; i < n; i++) {
        AdwTabPage *page = adw_tab_view_get_nth_page(state->view, i);
        VteTerminal *term = get_terminal_from_page(page);
        if (!term) continue;

        cathode_terminal_apply_font(term, c);
    }
}

void
cathode_tab_reapply_config(CathodeTabState *state, CathodeConfig *c)
{
    int n = adw_tab_view_get_n_pages(state->view);
    for (int i = 0; i < n; i++) {
        AdwTabPage *page = adw_tab_view_get_nth_page(state->view, i);
        GtkWidget *overlay = adw_tab_page_get_child(page);
        VteTerminal *term = get_terminal_from_page(page);
        if (!term) continue;

        cathode_terminal_apply_config(term, c);
        cathode_shader_refresh_visible(overlay);
    }
}

int
cathode_tab_get_n_pages(CathodeTabState *state)
{
    return adw_tab_view_get_n_pages(state->view);
}

AdwTabPage *
cathode_tab_get_selected_page(CathodeTabState *state)
{
    return adw_tab_view_get_selected_page(state->view);
}

VteTerminal *
cathode_tab_get_current_terminal(CathodeTabState *state)
{
    AdwTabPage *page = adw_tab_view_get_selected_page(state->view);
    if (!page) return NULL;
    return get_terminal_from_page(page);
}
