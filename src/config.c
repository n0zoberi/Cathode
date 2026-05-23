#include "config.h"
#include "vendor/tomlc99/toml.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <glib.h>

static void
set_str(char **dest, const char *src)
{
    g_free(*dest);
    *dest = g_strdup(src);
}

static void
parse_colors_from_table(toml_table_t *table, CathodeConfig *cfg, const char *section)
{
    toml_table_t *t = toml_table_in(table, section);
    if (!t) return;

    toml_datum_t val;

    for (int i = 0; i < 16; i++) {
        char key[16];
        snprintf(key, sizeof(key), "color%d", i);
        val = toml_string_in(t, key);
        if (val.ok) {
            g_free(cfg->palette[i]);
            cfg->palette[i] = g_strdup(val.u.s);
            cfg->palette_set = true;
            free(val.u.s);
        }
    }
}

static void
parse_crt(toml_table_t *root, CathodeConfig *cfg)
{
    toml_table_t *t = toml_table_in(root, "crt");
    if (!t) return;

    toml_datum_t d;

    d = toml_double_in(t, "scanline_intensity");
    if (d.ok) cfg->scanline_intensity = (float)d.u.d;

    d = toml_double_in(t, "scanline_period");
    if (d.ok) cfg->scanline_period = (float)d.u.d;

    d = toml_double_in(t, "bloom_strength");
    if (d.ok) cfg->bloom_strength = (float)d.u.d;

    d = toml_double_in(t, "bloom_sigma");
    if (d.ok) cfg->bloom_sigma = (float)d.u.d;

    d = toml_double_in(t, "glow_strength");
    if (d.ok) cfg->glow_strength = (float)d.u.d;

    d = toml_double_in(t, "glow_threshold_low");
    if (d.ok) cfg->glow_threshold_low = (float)d.u.d;

    d = toml_double_in(t, "glow_threshold_high");
    if (d.ok) cfg->glow_threshold_high = (float)d.u.d;

    d = toml_double_in(t, "mask_strength");
    if (d.ok) cfg->mask_strength = (float)d.u.d;

    d = toml_double_in(t, "curvature");
    if (d.ok) cfg->curvature = (float)d.u.d;

    d = toml_double_in(t, "chromatic_aberration");
    if (d.ok) cfg->chromatic_aberration = (float)d.u.d;
}

static void
parse_font(toml_table_t *root, CathodeConfig *cfg)
{
    toml_table_t *t = toml_table_in(root, "font");
    if (!t) return;

    toml_datum_t d;
    d = toml_string_in(t, "family");
    if (d.ok) { set_str(&cfg->font_family, d.u.s); free(d.u.s); }

    d = toml_string_in(t, "style");
    if (d.ok) { set_str(&cfg->font_style, d.u.s); free(d.u.s); }

    d = toml_int_in(t, "size");
    if (d.ok) cfg->font_size = (int)d.u.i;
}

static void
parse_shell(toml_table_t *root, CathodeConfig *cfg)
{
    toml_table_t *t = toml_table_in(root, "shell");
    if (!t) return;

    toml_datum_t d;
    d = toml_string_in(t, "program");
    if (d.ok) { set_str(&cfg->shell_program, d.u.s); free(d.u.s); }
}

static void
parse_terminal(toml_table_t *root, CathodeConfig *cfg)
{
    toml_table_t *t = toml_table_in(root, "terminal");
    if (!t) return;

    toml_datum_t d;
    d = toml_int_in(t, "scrollback");
    if (d.ok) cfg->scrollback = (int)d.u.i;

    d = toml_string_in(t, "cursor_blink");
    if (d.ok) {
        if (strcmp(d.u.s, "off") == 0)
            cfg->cursor_blink = CURSOR_BLINK_OFF;
        else if (strcmp(d.u.s, "system") == 0)
            cfg->cursor_blink = CURSOR_BLINK_SYSTEM;
        else
            cfg->cursor_blink = CURSOR_BLINK_ON;
        free(d.u.s);
    }
}

static void
parse_env(toml_table_t *root, CathodeConfig *cfg)
{
    toml_table_t *t = toml_table_in(root, "env");
    if (!t) return;

    toml_datum_t d;
    d = toml_string_in(t, "TERM");
    if (d.ok) { set_str(&cfg->term, d.u.s); free(d.u.s); }
}

static void
parse_imports(toml_table_t *root, CathodeConfig *cfg)
{
    toml_table_t *general = toml_table_in(root, "general");
    if (!general) return;

    toml_array_t *arr = toml_array_in(general, "import");
    if (!arr) return;

    int len = toml_array_nelem(arr);
    for (int i = 0; i < len; i++) {
        toml_datum_t s = toml_string_at(arr, i);
        if (s.ok) {
            cfg->num_imports++;
            cfg->imports = g_realloc_n(cfg->imports, cfg->num_imports, sizeof(char *));
            cfg->imports[cfg->num_imports - 1] = g_strdup(s.u.s);
            free(s.u.s);
        }
    }
}

static void
merge_theme_table(toml_table_t *root, CathodeConfig *cfg)
{
    toml_table_t *colors = toml_table_in(root, "colors");
    if (!colors) {
        colors = toml_table_in(root, "theme");
        if (!colors) return;
    }

    toml_datum_t val;

    val = toml_string_in(colors, "foreground");
    if (val.ok) { set_str(&cfg->fg_color, val.u.s); free(val.u.s); }

    val = toml_string_in(colors, "background");
    if (val.ok) { set_str(&cfg->bg_color, val.u.s); free(val.u.s); }

    val = toml_string_in(colors, "cursor");
    if (val.ok) { set_str(&cfg->cursor_color, val.u.s); free(val.u.s); }

    val = toml_string_in(colors, "selection_background");
    if (val.ok) { set_str(&cfg->selection_bg, val.u.s); free(val.u.s); }

    parse_colors_from_table(colors, cfg, "normal");
    parse_colors_from_table(colors, cfg, "bright");
}

CathodeConfig *
cathode_config_default(void)
{
    CathodeConfig *cfg = g_new0(CathodeConfig, 1);
    cfg->scrollback          = 2000;
    cfg->cursor_blink        = CURSOR_BLINK_ON;
    cfg->shell_program       = g_strdup("/bin/zsh");
    cfg->term                = g_strdup("xterm-256color");
    cfg->font_family         = g_strdup("monospace");
    cfg->font_size           = 11;
    cfg->scanline_intensity  = 0.06f;
    cfg->scanline_period     = 2.0f;
    cfg->bloom_strength      = 0.12f;
    cfg->bloom_sigma         = 3.0f;
    cfg->glow_strength       = 0.06f;
    cfg->glow_threshold_low  = 0.15f;
    cfg->glow_threshold_high = 0.6f;
    cfg->mask_strength       = 0.012f;
    cfg->curvature           = 0.0f;
    cfg->chromatic_aberration = 0.0f;
    cfg->fg_color            = g_strdup("#ffffff");
    cfg->bg_color            = g_strdup("#000000");
    return cfg;
}

void
cathode_config_merge_theme(CathodeConfig *cfg, const char *theme_path)
{
    FILE *fp = fopen(theme_path, "r");
    if (!fp) {
        g_warning("Cannot open theme file: %s", theme_path);
        return;
    }

    char errbuf[256];
    toml_table_t *root = toml_parse_file(fp, errbuf, sizeof(errbuf));
    fclose(fp);

    if (!root) {
        g_warning("Theme parse error: %s", errbuf);
        return;
    }

    merge_theme_table(root, cfg);
    toml_free(root);
}

CathodeConfig *
cathode_config_load(void)
{
    CathodeConfig *cfg = cathode_config_default();

    const char *config_path = g_build_filename(
        g_get_user_config_dir(), "cathode", "cathode.toml", NULL);

    FILE *fp = fopen(config_path, "r");
    if (!fp) {
        g_message("No config at %s, using defaults", config_path);
        return cfg;
    }

    char errbuf[256];
    toml_table_t *root = toml_parse_file(fp, errbuf, sizeof(errbuf));
    fclose(fp);

    if (!root) {
        g_warning("Config parse error: %s", errbuf);
        return cfg;
    }

    parse_imports(root, cfg);
    parse_terminal(root, cfg);
    parse_shell(root, cfg);
    parse_env(root, cfg);
    parse_font(root, cfg);
    parse_crt(root, cfg);
    merge_theme_table(root, cfg);

    toml_free(root);

    for (int i = 0; i < cfg->num_imports; i++) {
        const char *imp = cfg->imports[i];

        if (g_str_has_prefix(imp, "~/")) {
            char *expanded = g_build_filename(g_get_home_dir(), imp + 2, NULL);
            cathode_config_merge_theme(cfg, expanded);
            g_free(expanded);
        } else {
            cathode_config_merge_theme(cfg, imp);
        }
    }

    return cfg;
}

void
cathode_config_free(CathodeConfig *cfg)
{
    if (!cfg) return;

    for (int i = 0; i < cfg->num_imports; i++)
        g_free(cfg->imports[i]);
    g_free(cfg->imports);

    g_free(cfg->shell_program);

    if (cfg->shell_args) {
        for (int i = 0; i < cfg->num_shell_args; i++)
            g_free(cfg->shell_args[i]);
        g_free(cfg->shell_args);
    }

    g_free(cfg->term);
    g_free(cfg->font_family);
    g_free(cfg->font_style);
    g_free(cfg->fg_color);
    g_free(cfg->bg_color);
    g_free(cfg->cursor_color);
    g_free(cfg->selection_bg);

    for (int i = 0; i < 16; i++)
        g_free(cfg->palette[i]);

    g_free(cfg);
}
