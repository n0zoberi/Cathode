#include "app.h"
#include "terminal.h"

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

    VteTerminal *term = cathode_terminal_new();

    gtk_window_set_child(GTK_WINDOW(window), GTK_WIDGET(term));

    g_signal_connect(window, "destroy",
                     G_CALLBACK(on_window_destroy), app);

    gtk_window_present(GTK_WINDOW(window));

    cathode_terminal_spawn(term);
}

int
cathode_app_run(int argc, char *argv[])
{
    GtkApplication *app = gtk_application_new("org.cathode.Cathode",
        G_APPLICATION_DEFAULT_FLAGS);

    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
