#include "tab.h"
#include "terminal.h"
#include "shader.h"
#include <vte/vte.h>

static CathodeConfig *st_cfg;
static AdwTabView    *st_view;
static GtkWindow     *st_window;

static void
on_title_changed(VteTerminal *term, gpointer data)
{
    AdwTabPage *page = ADW_TAB_PAGE(data);
    const char *title = vte_terminal_get_window_title(term);
    adw_tab_page_set_title(page, title && *title ? title : "Cathode");
}

static void
on_child_exited(VteTerminal *term, int status, gpointer data)
{
    (void)term;
    (void)status;
    AdwTabPage *page = ADW_TAB_PAGE(data);
    adw_tab_view_close_page(st_view, page);
}

static void
on_close_page(AdwTabView *view, AdwTabPage *page, gpointer data)
{
    (void)data;
    adw_tab_view_close_page_finish(view, page, TRUE);

    if (adw_tab_view_get_n_pages(view) == 0 && st_window)
        gtk_window_close(st_window);
}

static GtkWidget *
create_tab_content(void)
{
    GtkWidget *term = GTK_WIDGET(cathode_terminal_new(st_cfg));
    GtkWidget *overlay = cathode_shader_overlay_new(st_cfg, term);

    cathode_terminal_spawn(VTE_TERMINAL(term), st_cfg);

    return overlay;
}

static void
on_page_attached(AdwTabView *view, AdwTabPage *page, int pos, gpointer data)
{
    (void)view;
    (void)pos;
    (void)data;

    GtkWidget *overlay = adw_tab_page_get_child(page);
    GtkWidget *first = gtk_widget_get_first_child(overlay);

    if (first && VTE_IS_TERMINAL(first)) {
        VteTerminal *vte = VTE_TERMINAL(first);

        g_signal_connect(vte, "window-title-changed",
                         G_CALLBACK(on_title_changed), page);
        g_signal_connect(vte, "child-exited",
                         G_CALLBACK(on_child_exited), page);

        const char *title = vte_terminal_get_window_title(vte);
        adw_tab_page_set_title(page, title && *title ? title : "Cathode");
    }
}

GtkWidget *
cathode_tab_view_new(CathodeConfig *cfg, GtkWindow *window)
{
    st_cfg = cfg;
    st_window = window;

    AdwTabView *view = ADW_TAB_VIEW(adw_tab_view_new());
    st_view = view;

    AdwTabViewShortcuts shortcuts = ADW_TAB_VIEW_SHORTCUT_NONE;
    shortcuts |= ADW_TAB_VIEW_SHORTCUT_CONTROL_HOME;
    shortcuts |= ADW_TAB_VIEW_SHORTCUT_CONTROL_END;
    shortcuts |= ADW_TAB_VIEW_SHORTCUT_CONTROL_TAB;
    shortcuts |= ADW_TAB_VIEW_SHORTCUT_CONTROL_SHIFT_TAB;
    shortcuts |= ADW_TAB_VIEW_SHORTCUT_CONTROL_PAGE_UP;
    shortcuts |= ADW_TAB_VIEW_SHORTCUT_CONTROL_PAGE_DOWN;
    adw_tab_view_set_shortcuts(view, shortcuts);

    g_signal_connect(view, "page-attached",
                     G_CALLBACK(on_page_attached), NULL);
    g_signal_connect(view, "close-page",
                     G_CALLBACK(on_close_page), NULL);

    AdwToolbarView *toolbar = ADW_TOOLBAR_VIEW(adw_toolbar_view_new());

    AdwTabBar *bar = ADW_TAB_BAR(adw_tab_bar_new());
    adw_tab_bar_set_view(bar, view);
    adw_toolbar_view_add_top_bar(toolbar, GTK_WIDGET(bar));

    GtkWidget *child = GTK_WIDGET(view);
    gtk_widget_set_hexpand(child, TRUE);
    gtk_widget_set_vexpand(child, TRUE);
    adw_toolbar_view_set_content(toolbar, child);

    cathode_tab_new_tab();

    return GTK_WIDGET(toolbar);
}

void
cathode_tab_new_tab(void)
{
    GtkWidget *content = create_tab_content();
    adw_tab_view_append(st_view, content);

    int n = adw_tab_view_get_n_pages(st_view);
    AdwTabPage *page = adw_tab_view_get_nth_page(st_view, n - 1);
    adw_tab_view_set_selected_page(st_view, page);
}

void
cathode_tab_close_current(void)
{
    AdwTabPage *page = adw_tab_view_get_selected_page(st_view);
    if (page)
        adw_tab_view_close_page(st_view, page);
}
