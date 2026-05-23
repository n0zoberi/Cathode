#include "app.h"
#include "config.h"
#include "tab.h"
#include "terminal.h"

static CathodeConfig *app_config = NULL;

static VteTerminal *
get_current_terminal(void)
{
    AdwTabPage *page = cathode_tab_get_selected_page();
    if (!page) return NULL;

    GtkWidget *overlay = adw_tab_page_get_child(page);
    GtkWidget *first = gtk_widget_get_first_child(overlay);
    if (first && VTE_IS_TERMINAL(first))
        return VTE_TERMINAL(first);
    return NULL;
}

static void
on_copy(GSimpleAction *action, GVariant *param, gpointer data)
{
    (void)action;
    (void)param;
    (void)data;
    VteTerminal *term = get_current_terminal();
    if (term)
        vte_terminal_copy_clipboard_format(term, VTE_FORMAT_TEXT);
}

static void
on_paste(GSimpleAction *action, GVariant *param, gpointer data)
{
    (void)action;
    (void)param;
    (void)data;
    VteTerminal *term = get_current_terminal();
    if (term)
        vte_terminal_paste_clipboard(term);
}

static void
on_new_tab(GSimpleAction *action, GVariant *param, gpointer data)
{
    (void)action;
    (void)param;
    (void)data;
    cathode_tab_new_tab();
}

static void
on_close_tab(GSimpleAction *action, GVariant *param, gpointer data)
{
    (void)action;
    (void)param;
    (void)data;
    cathode_tab_close_current();
}

static void
on_toggle_search(GSimpleAction *action, GVariant *param, gpointer data)
{
    (void)action;
    (void)param;
    (void)data;
    cathode_tab_toggle_search();
}

static void
on_increase_font(GSimpleAction *action, GVariant *param, gpointer data)
{
    (void)action;
    (void)param;
    (void)data;
    if (app_config->font_size < 72)
        app_config->font_size++;
    cathode_tab_reapply_font(app_config);
}

static void
on_decrease_font(GSimpleAction *action, GVariant *param, gpointer data)
{
    (void)action;
    (void)param;
    (void)data;
    if (app_config->font_size > 4)
        app_config->font_size--;
    cathode_tab_reapply_font(app_config);
}

static void
on_reset_font(GSimpleAction *action, GVariant *param, gpointer data)
{
    (void)action;
    (void)param;
    (void)data;
    app_config->font_size = 11;
    cathode_tab_reapply_font(app_config);
}

static void
on_dialog_response(AdwAlertDialog *_dialog, const char *response, GtkWindow *window)
{
    (void)_dialog;
    if (g_strcmp0(response, "ok") == 0)
        gtk_window_destroy(window);
}

static gboolean
on_close_request(GtkWindow *window, gpointer data)
{
    (void)data;

    int n = cathode_tab_get_n_pages();
    if (n <= 1)
        return FALSE;

    char *body = g_strdup_printf("There are %d open tabs. Close anyway?", n);
    AdwAlertDialog *dialog = ADW_ALERT_DIALOG(
        adw_alert_dialog_new("Close Cathode?", body));
    g_free(body);

    adw_alert_dialog_add_responses(dialog,
        "cancel", "Cancel",
        "ok", "Close All",
        NULL);
    adw_alert_dialog_set_default_response(dialog, "cancel");
    adw_alert_dialog_set_close_response(dialog, "cancel");

    g_signal_connect(dialog, "response",
                     G_CALLBACK(on_dialog_response), window);

    adw_dialog_present(ADW_DIALOG(dialog), GTK_WIDGET(window));

    return TRUE;
}

static void
on_window_destroy(GtkWindow *window, GtkApplication *app)
{
    GList *windows = gtk_application_get_windows(app);
    windows = g_list_remove(windows, window);
    if (windows == NULL)
        g_application_quit(G_APPLICATION(app));
}

static void
on_activate(GtkApplication *app, gpointer user_data)
{
    (void)user_data;

    GtkWidget *window = adw_application_window_new(app);
    gtk_window_set_default_size(GTK_WINDOW(window), 900, 600);
    gtk_window_set_title(GTK_WINDOW(window), "Cathode");

    GActionEntry win_entries[] = {
        { "copy", on_copy, NULL, NULL, NULL, {0} },
        { "paste", on_paste, NULL, NULL, NULL, {0} },
        { "new-tab", on_new_tab, NULL, NULL, NULL, {0} },
        { "close-tab", on_close_tab, NULL, NULL, NULL, {0} },
        { "toggle-search", on_toggle_search, NULL, NULL, NULL, {0} },
        { "increase-font", on_increase_font, NULL, NULL, NULL, {0} },
        { "decrease-font", on_decrease_font, NULL, NULL, NULL, {0} },
        { "reset-font", on_reset_font, NULL, NULL, NULL, {0} },
    };
    g_action_map_add_action_entries(G_ACTION_MAP(window),
                                     win_entries,
                                     G_N_ELEMENTS(win_entries),
                                     window);

    const char *accels[][5] = {
        /* action,         accel1,           accel2,            NULL */
        { "win.copy",      "<Ctrl><Shift>C", "<Ctrl><Alt>C",    NULL },
        { "win.paste",     "<Ctrl><Shift>V", "<Ctrl><Alt>V",    NULL },
        { "win.new-tab",   "<Ctrl><Shift>T", NULL,              NULL },
        { "win.close-tab", "<Ctrl><Shift>W", NULL,              NULL },
        { "win.toggle-search", "<Ctrl><Shift>F", NULL,          NULL },
        { "win.increase-font",  "<Ctrl>equal",
                              "<Ctrl>plus",   "<Ctrl>KP_Add",   NULL },
        { "win.decrease-font",  "<Ctrl>minus",
                              "<Ctrl>KP_Subtract", NULL,        NULL },
        { "win.reset-font",     "<Ctrl>0",
                              "<Ctrl><Alt>0",  "<Ctrl>KP_0",   NULL },
    };

    for (size_t i = 0; i < G_N_ELEMENTS(accels); i++) {
        const char **list = accels[i];
        const char *action = list[0];
        GPtrArray *arr = g_ptr_array_new();
        for (int j = 1; list[j]; j++)
            g_ptr_array_add(arr, (char *)list[j]);
        g_ptr_array_add(arr, NULL);
        gtk_application_set_accels_for_action(GTK_APPLICATION(app),
            action, (const char **)arr->pdata);
        g_ptr_array_free(arr, TRUE);
    }

    GtkWidget *tab_content = cathode_tab_view_new(app_config,
                                                   GTK_WINDOW(window));
    gtk_window_set_child(GTK_WINDOW(window), tab_content);

    g_signal_connect(window, "close-request",
                     G_CALLBACK(on_close_request), NULL);
    g_signal_connect(window, "destroy",
                     G_CALLBACK(on_window_destroy), app);

    gtk_window_present(GTK_WINDOW(window));
}

int
cathode_app_run(int argc, char *argv[])
{
    app_config = cathode_config_load();

    GtkApplication *app = gtk_application_new("org.cathode.Cathode",
        G_APPLICATION_DEFAULT_FLAGS);

    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);

    cathode_config_free(app_config);
    g_object_unref(app);
    return status;
}
