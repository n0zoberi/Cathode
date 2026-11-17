#include "terminal.h"
#include <adwaita.h>
#include <glib/gi18n.h>
#include <stdlib.h>
#include <string.h>

struct _CathodeTerminalRealizeData {
    CathodeConfig   *cfg;
};

static void
on_terminal_realize(GtkWidget *widget, gpointer data)
{
    struct _CathodeTerminalRealizeData *rdata = data;
    VteTerminal *term = VTE_TERMINAL(widget);

    vte_terminal_set_cursor_blink_mode(term,
        cathode_cursor_blink_to_vte(rdata->cfg->cursor_blink));

    g_signal_handlers_disconnect_by_func(widget,
        G_CALLBACK(on_terminal_realize), data);
    g_free(data);
}

VteTerminal *
cathode_terminal_new(CathodeConfig *cfg)
{
    VteTerminal *term = VTE_TERMINAL(vte_terminal_new());
    cathode_terminal_apply_config(term, cfg);

    struct _CathodeTerminalRealizeData *rdata = g_new(struct _CathodeTerminalRealizeData, 1);
    rdata->cfg  = cfg;
    g_signal_connect(GTK_WIDGET(term), "realize",
                     G_CALLBACK(on_terminal_realize), rdata);

    return term;
}

void
cathode_terminal_apply_config(VteTerminal *term, CathodeConfig *cfg)
{
    vte_terminal_set_scrollback_lines(term, cfg->scrollback);
    vte_terminal_set_enable_bidi(term, TRUE);
    vte_terminal_set_mouse_autohide(term, TRUE);
    vte_terminal_set_allow_hyperlink(term, TRUE);

    cathode_terminal_apply_font(term, cfg);
    vte_terminal_set_cursor_blink_mode(term,
        cathode_cursor_blink_to_vte(cfg->cursor_blink));

    /* set_colors(NULL, NULL, ...) resets fg/bg to VTE defaults; apply custom colors after. */
    if (cfg->palette_set) {
        GdkRGBA palette[16];
        for (int i = 0; i < 16; i++) {
            if (!cfg->palette[i] || !gdk_rgba_parse(&palette[i], cfg->palette[i]))
                palette[i] = (GdkRGBA){0, 0, 0, 1};
        }
        vte_terminal_set_colors(term, NULL, NULL, palette, 16);
    }

    GdkRGBA rgba;
    if (cfg->fg_color && gdk_rgba_parse(&rgba, cfg->fg_color))
        vte_terminal_set_color_foreground(term, &rgba);

    if (cfg->bg_color && gdk_rgba_parse(&rgba, cfg->bg_color))
        vte_terminal_set_color_background(term, &rgba);

    if (cfg->cursor_color && gdk_rgba_parse(&rgba, cfg->cursor_color))
        vte_terminal_set_color_cursor(term, &rgba);

    if (cfg->selection_bg && gdk_rgba_parse(&rgba, cfg->selection_bg))
        vte_terminal_set_color_highlight(term, &rgba);
}

void
cathode_terminal_apply_font(VteTerminal *term, CathodeConfig *cfg)
{
    char *str = g_strdup_printf("%s %d", cfg->font_family, cfg->font_size);
    PangoFontDescription *font = pango_font_description_from_string(str);
    vte_terminal_set_font(term, font);
    pango_font_description_free(font);
    g_free(str);
}

VteCursorBlinkMode
cathode_cursor_blink_to_vte(CursorBlinkMode mode)
{
    switch (mode) {
    case CURSOR_BLINK_OFF:    return VTE_CURSOR_BLINK_OFF;
    case CURSOR_BLINK_SYSTEM: return VTE_CURSOR_BLINK_SYSTEM;
    default:                  return VTE_CURSOR_BLINK_ON;
    }
}

typedef struct {
    VteTerminal   *term;
    CathodeConfig *cfg;
    bool           tried_fallback;
} SpawnData;

static void on_spawn_done(VteTerminal *term, GPid pid, GError *error,
                          gpointer data);

static void
spawn_shell(SpawnData *sd, const char *shell)
{
    char **envp = g_get_environ();
    if (sd->cfg->term)
        envp = g_environ_setenv(envp, "TERM", sd->cfg->term, TRUE);

    char *argv[] = { (char *)shell, NULL };

    vte_terminal_spawn_async(sd->term,
        VTE_PTY_DEFAULT,
        NULL,
        argv,
        envp,
        G_SPAWN_SEARCH_PATH,
        NULL, NULL, NULL,
        -1,
        NULL,
        on_spawn_done, sd);

    g_strfreev(envp);
}

static void
spawn_data_free(SpawnData *sd)
{
    if (sd->term)
        g_object_remove_weak_pointer(G_OBJECT(sd->term), (gpointer *)&sd->term);
    g_free(sd);
}

static void
show_spawn_message(VteTerminal *term, const char *title, const char *body)
{
    GtkRoot *root = gtk_widget_get_root(GTK_WIDGET(term));
    if (!GTK_IS_WINDOW(root))
        return;

    AdwAlertDialog *dialog = ADW_ALERT_DIALOG(
        adw_alert_dialog_new(title, body));
    adw_alert_dialog_add_responses(dialog, "ok", _("OK"), NULL);
    adw_alert_dialog_set_default_response(dialog, "ok");
    adw_alert_dialog_set_close_response(dialog, "ok");
    adw_dialog_present(ADW_DIALOG(dialog), GTK_WIDGET(root));
}

static void
on_spawn_done(VteTerminal *term, GPid pid, GError *error, gpointer data)
{
    (void)pid;
    SpawnData *sd = data;

    if (!sd->term) {
        g_free(sd);
        return;
    }

    if (!error) {
        spawn_data_free(sd);
        return;
    }

    g_warning("Shell spawn failed: %s", error->message);

    if (!sd->tried_fallback) {
        sd->tried_fallback = true;
        show_spawn_message(term, _("Shell Failed"),
            _("Could not start the configured shell. Falling back to /bin/sh."));
        spawn_shell(sd, "/bin/sh");
        return;
    }

    show_spawn_message(term, _("Shell Failed"),
        _("Could not start a shell. Check your configuration."));
    spawn_data_free(sd);
}

void
cathode_terminal_spawn(VteTerminal *term, CathodeConfig *cfg)
{
    const char *shell = cfg->shell_program;
    if (!shell || !*shell) {
        shell = g_getenv("SHELL");
        if (!shell || !*shell)
            shell = "/bin/sh";
    }

    SpawnData *sd = g_new0(SpawnData, 1);
    sd->term = term;
    sd->cfg = cfg;
    g_object_add_weak_pointer(G_OBJECT(term), (gpointer *)&sd->term);

    spawn_shell(sd, shell);
}
