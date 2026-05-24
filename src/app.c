#include "app.h"
#include "config.h"
#include "tab.h"
#include "terminal.h"
#include <glib/gi18n.h>

static CathodeConfig *app_config = NULL;
static GFileMonitor  *config_monitor = NULL;
static guint          reload_debounce_id = 0;

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
on_rename_tab(GSimpleAction *action, GVariant *param, gpointer data)
{
    (void)action;
    (void)param;
    (void)data;
    cathode_tab_rename_current();
}

static void
on_clear_screen(GSimpleAction *action, GVariant *param, gpointer data)
{
    (void)action;
    (void)param;
    (void)data;
    VteTerminal *term = get_current_terminal();
    if (term)
        vte_terminal_reset(term, TRUE, FALSE);
}

static void
on_reset_terminal(GSimpleAction *action, GVariant *param, gpointer data)
{
    (void)action;
    (void)param;
    (void)data;
    VteTerminal *term = get_current_terminal();
    if (term)
        vte_terminal_reset(term, TRUE, TRUE);
}

static void
on_open_config(GSimpleAction *action, GVariant *param, gpointer data)
{
    (void)action;
    (void)param;
    (void)data;
    GtkWindow *window = GTK_WINDOW(data);
    char *path = g_build_filename(g_get_user_config_dir(), "cathode", "cathode.toml", NULL);
    GFile *file = g_file_new_for_path(path);
    GtkFileLauncher *launcher = gtk_file_launcher_new(file);
    gtk_file_launcher_launch(launcher, window, NULL, NULL, NULL);
    g_object_unref(launcher);
    g_object_unref(file);
    g_free(path);
}

static void
on_quit(GSimpleAction *action, GVariant *param, gpointer data)
{
    (void)action;
    (void)param;
    gtk_window_close(GTK_WINDOW(data));
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

static bool closing_confirmed = false;

static void
on_dialog_response(AdwAlertDialog *_dialog, const char *response, GtkWindow *window)
{
    (void)_dialog;
    if (g_strcmp0(response, "ok") == 0) {
        closing_confirmed = true;
        gtk_window_destroy(window);
    }
}

static gboolean
on_close_request(GtkWindow *window, gpointer data)
{
    (void)data;

    if (closing_confirmed)
        return FALSE;

    int n = cathode_tab_get_n_pages();
    if (n <= 1)
        return FALSE;

    char *body = g_strdup_printf("There are %d open tabs. Close anyway?", n);
    AdwAlertDialog *dialog = ADW_ALERT_DIALOG(
        adw_alert_dialog_new(_("Close Cathode?"), body));
    g_free(body);

    adw_alert_dialog_add_responses(dialog,
        "cancel", _("Cancel"),
        "ok", _("Close All"),
        NULL);
    adw_alert_dialog_set_default_response(dialog, "cancel");
    adw_alert_dialog_set_close_response(dialog, "cancel");

    g_signal_connect(dialog, "response",
                     G_CALLBACK(on_dialog_response), window);

    adw_dialog_present(ADW_DIALOG(dialog), GTK_WIDGET(window));

    return TRUE;
}

static void
on_window_destroy(GtkWindow *_window, GtkApplication *app)
{
    (void)_window;
    GList *windows = gtk_application_get_windows(app);
    if (g_list_length(windows) <= 1)
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
        { "rename-tab", on_rename_tab, NULL, NULL, NULL, {0} },
        { "clear-screen", on_clear_screen, NULL, NULL, NULL, {0} },
        { "reset-terminal", on_reset_terminal, NULL, NULL, NULL, {0} },
        { "open-config", on_open_config, NULL, NULL, NULL, {0} },
        { "quit", on_quit, NULL, NULL, NULL, {0} },
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
    adw_application_window_set_content(ADW_APPLICATION_WINDOW(window),
                                        tab_content);

    g_signal_connect(window, "close-request",
                     G_CALLBACK(on_close_request), NULL);
    g_signal_connect(window, "destroy",
                     G_CALLBACK(on_window_destroy), app);

    gtk_window_present(GTK_WINDOW(window));
}

static gboolean
reload_config_cb(gpointer data)
{
    (void)data;
    reload_debounce_id = 0;
    g_message("Config file changed, reloading...");
    cathode_config_reload(app_config);
    cathode_tab_reapply_config(app_config);
    return G_SOURCE_REMOVE;
}

static void
on_config_changed(GFileMonitor *monitor, GFile *file,
                  GFile *other, GFileMonitorEvent event,
                  gpointer data)
{
    (void)monitor;
    (void)file;
    (void)other;
    (void)data;

    if (event != G_FILE_MONITOR_EVENT_CHANGED &&
        event != G_FILE_MONITOR_EVENT_CREATED)
        return;

    if (!app_config || !app_config->auto_reload)
        return;

    if (reload_debounce_id != 0)
        g_source_remove(reload_debounce_id);
    reload_debounce_id = g_timeout_add(500, reload_config_cb, NULL);
}

static void
setup_config_monitor(void)
{
    const char *path = g_build_filename(
        g_get_user_config_dir(), "cathode", "cathode.toml", NULL);
    GFile *file = g_file_new_for_path(path);
    g_free((char *)path);

    GError *err = NULL;
    config_monitor = g_file_monitor_file(file, G_FILE_MONITOR_NONE, NULL, &err);
    if (err) {
        g_warning("Cannot monitor config file: %s", err->message);
        g_clear_error(&err);
    } else {
        g_signal_connect(config_monitor, "changed",
                         G_CALLBACK(on_config_changed), NULL);
    }
    g_object_unref(file);
}

int
cathode_app_run(int argc, char *argv[])
{
    app_config = cathode_config_load();
    setup_config_monitor();

    GtkApplication *app = gtk_application_new("com.n0zoberi.Cathode",
        G_APPLICATION_DEFAULT_FLAGS);

    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);

    if (reload_debounce_id)
        g_source_remove(reload_debounce_id);
    if (config_monitor) {
        g_file_monitor_cancel(config_monitor);
        g_object_unref(config_monitor);
    }
    cathode_config_free(app_config);
    g_object_unref(app);
    return status;
}
