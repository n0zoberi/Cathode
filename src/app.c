#include "app.h"
#include "config.h"
#include "tab.h"

static CathodeConfig *app_config = NULL;

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
        { "new-tab", on_new_tab, NULL, NULL, NULL, {0} },
        { "close-tab", on_close_tab, NULL, NULL, NULL, {0} },
        { "toggle-search", on_toggle_search, NULL, NULL, NULL, {0} },
    };
    g_action_map_add_action_entries(G_ACTION_MAP(window),
                                     win_entries,
                                     G_N_ELEMENTS(win_entries),
                                     window);

    gtk_application_set_accels_for_action(GTK_APPLICATION(app),
        "win.new-tab", (const char *[]) { "<Ctrl><Shift>T", NULL });
    gtk_application_set_accels_for_action(GTK_APPLICATION(app),
        "win.close-tab", (const char *[]) { "<Ctrl><Shift>W", NULL });
    gtk_application_set_accels_for_action(GTK_APPLICATION(app),
        "win.toggle-search", (const char *[]) { "<Ctrl><Shift>F", NULL });

    GtkWidget *tab_content = cathode_tab_view_new(app_config,
                                                   GTK_WINDOW(window));
    gtk_window_set_child(GTK_WINDOW(window), tab_content);

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
