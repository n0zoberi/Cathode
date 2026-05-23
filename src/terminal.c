#include "terminal.h"
#include <stdlib.h>
#include <string.h>

VteTerminal *
cathode_terminal_new(CathodeConfig *cfg)
{
    VteTerminal *term = VTE_TERMINAL(vte_terminal_new());
    cathode_terminal_apply_config(term, cfg);
    return term;
}

void
cathode_terminal_apply_config(VteTerminal *term, CathodeConfig *cfg)
{
    vte_terminal_set_scrollback_lines(term, cfg->scrollback);
    vte_terminal_set_enable_bidi(term, TRUE);
    vte_terminal_set_mouse_autohide(term, TRUE);
    vte_terminal_set_allow_hyperlink(term, TRUE);

    char *font_str = g_strdup_printf("%s %d", cfg->font_family, cfg->font_size);
    PangoFontDescription *font = pango_font_description_from_string(font_str);
    vte_terminal_set_font(term, font);
    pango_font_description_free(font);
    g_free(font_str);

    switch (cfg->cursor_blink) {
    case CURSOR_BLINK_OFF:
        vte_terminal_set_cursor_blink_mode(term, VTE_CURSOR_BLINK_OFF);
        break;
    case CURSOR_BLINK_SYSTEM:
        vte_terminal_set_cursor_blink_mode(term, VTE_CURSOR_BLINK_SYSTEM);
        break;
    default:
        vte_terminal_set_cursor_blink_mode(term, VTE_CURSOR_BLINK_ON);
        break;
    }

    GdkRGBA rgba;
    if (cfg->fg_color && gdk_rgba_parse(&rgba, cfg->fg_color))
        vte_terminal_set_color_foreground(term, &rgba);

    if (cfg->bg_color && gdk_rgba_parse(&rgba, cfg->bg_color))
        vte_terminal_set_color_background(term, &rgba);

    if (cfg->cursor_color && gdk_rgba_parse(&rgba, cfg->cursor_color))
        vte_terminal_set_color_cursor(term, &rgba);

    if (cfg->palette_set) {
        GdkRGBA palette_rgba[16];
        for (int i = 0; i < 16; i++) {
            if (cfg->palette[i] && gdk_rgba_parse(&palette_rgba[i], cfg->palette[i]))
                vte_terminal_set_color_bold(term, &palette_rgba[i]);
        }
    }
}

void
cathode_terminal_spawn(VteTerminal *term, CathodeConfig *cfg)
{
    const char *shell = cfg->shell_program;
    if (!shell || !*shell) {
        shell = g_getenv("SHELL");
        if (!shell)
            shell = "/bin/sh";
    }

    char **envp = g_get_environ();
    if (cfg->term)
        envp = g_environ_setenv(envp, "TERM", cfg->term, TRUE);

    char *argv[] = { (char *)shell, NULL };

    vte_terminal_spawn_async(term,
        VTE_PTY_DEFAULT,
        NULL,
        argv,
        envp,
        G_SPAWN_SEARCH_PATH,
        NULL, NULL, NULL,
        -1,
        NULL,
        NULL, NULL);

    g_strfreev(envp);
}
