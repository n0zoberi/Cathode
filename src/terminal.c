#include "terminal.h"
#include <stdlib.h>

VteTerminal *
cathode_terminal_new(void)
{
    VteTerminal *term = VTE_TERMINAL(vte_terminal_new());

    vte_terminal_set_scrollback_lines(term, 2000);
    vte_terminal_set_enable_bidi(term, TRUE);
    vte_terminal_set_mouse_autohide(term, TRUE);
    vte_terminal_set_allow_hyperlink(term, TRUE);

    PangoFontDescription *font = pango_font_description_from_string("monospace 11");
    vte_terminal_set_font(term, font);
    pango_font_description_free(font);

    vte_terminal_set_cursor_blink_mode(term, VTE_CURSOR_BLINK_ON);

    return term;
}

void
cathode_terminal_spawn(VteTerminal *term)
{
    const char *shell = g_getenv("SHELL");
    if (!shell)
        shell = "/bin/sh";

    char **envp = g_get_environ();
    envp = g_environ_setenv(envp, "TERM", "xterm-256color", TRUE);

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
