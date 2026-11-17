#include "config.h"
#include <glib.h>
#include <glib/gstdio.h>

static char *tmp_dir;
static char *cfg_dir;
static char *cfg_path;

static void
write_config(const char *text)
{
    g_mkdir_with_parents(cfg_dir, 0700);
    GError *error = NULL;
    g_assert_true(g_file_set_contents(cfg_path, text, -1, &error));
    g_assert_no_error(error);
}

static void
test_defaults(void)
{
    CathodeConfig *cfg = cathode_config_default();

    g_assert_null(cfg->shell_program);
    g_assert_cmpstr(cfg->term, ==, "xterm-256color");
    g_assert_cmpstr(cfg->font_family, ==, "monospace");
    g_assert_cmpint(cfg->font_size, ==, 11);
    g_assert_cmpint(cfg->scrollback, ==, 2000);
    g_assert_cmpint(cfg->scanline_period, ==, 6);
    g_assert_false(cfg->palette_set);
    g_assert_true(cfg->crt_enabled);

    cathode_config_free(cfg);
}

static void
test_parse(void)
{
    write_config(
        "[terminal]\n"
        "scrollback = 500\n"
        "cursor_blink = \"off\"\n"
        "[shell]\n"
        "program = \"/bin/bash\"\n"
        "[font]\n"
        "family = \"Fira Code\"\n"
        "size = 14\n"
        "[crt]\n"
        "scanline_period = 4\n"
        "bloom_strength = 0.5\n"
        "[colors]\n"
        "foreground = \"#aabbcc\"\n"
        "[colors.normal]\n"
        "color1 = \"#112233\"\n"
        "[colors.bright]\n"
        "color8 = \"#445566\"\n");

    CathodeConfig *cfg = cathode_config_load();

    g_assert_cmpint(cfg->scrollback, ==, 500);
    g_assert_cmpint(cfg->cursor_blink, ==, CURSOR_BLINK_OFF);
    g_assert_cmpstr(cfg->shell_program, ==, "/bin/bash");
    g_assert_cmpstr(cfg->font_family, ==, "Fira Code");
    g_assert_cmpint(cfg->font_size, ==, 14);
    g_assert_cmpint(cfg->scanline_period, ==, 4);
    g_assert_cmpfloat_with_epsilon(cfg->bloom_strength, 0.5, 1e-6);
    g_assert_cmpstr(cfg->fg_color, ==, "#aabbcc");
    g_assert_true(cfg->palette_set);
    g_assert_cmpstr(cfg->palette[1], ==, "#112233");
    g_assert_cmpstr(cfg->palette[8], ==, "#445566");

    cathode_config_free(cfg);
}

/* Regression for palette_set: removing the [colors] section on reload must
 * clear the flag and the old palette instead of leaving all 16 slots black. */
static void
test_reload_clears_removed_state(void)
{
    write_config(
        "[shell]\n"
        "program = \"/bin/bash\"\n"
        "[crt]\n"
        "scanline_period = 2\n"
        "bloom_strength = 0.9\n"
        "[colors]\n"
        "foreground = \"#123456\"\n"
        "[colors.normal]\n"
        "color1 = \"#112233\"\n");

    CathodeConfig *cfg = cathode_config_load();
    g_assert_true(cfg->palette_set);
    g_assert_cmpstr(cfg->palette[1], ==, "#112233");

    write_config("[font]\nsize = 13\n");
    cathode_config_reload(cfg);

    g_assert_false(cfg->palette_set);
    for (int i = 0; i < 16; i++)
        g_assert_null(cfg->palette[i]);
    g_assert_cmpstr(cfg->fg_color, ==, "#ffffff");
    g_assert_null(cfg->shell_program);
    g_assert_cmpint(cfg->scanline_period, ==, 6);
    g_assert_cmpfloat_with_epsilon(cfg->bloom_strength, 0.30, 1e-6);
    g_assert_cmpint(cfg->font_size, ==, 13);

    cathode_config_free(cfg);
}

static void
test_invalid_values_are_clamped(void)
{
    write_config(
        "[terminal]\n"
        "scrollback = -5\n"
        "[font]\n"
        "size = 2\n"
        "[crt]\n"
        "scanline_period = 0\n"
        "glow_threshold_low = 0.9\n"
        "glow_threshold_high = 0.2\n");

    g_test_expect_message(NULL, G_LOG_LEVEL_WARNING,
                          "*crt.scanline_period=0 is out of range*");
    g_test_expect_message(NULL, G_LOG_LEVEL_WARNING,
                          "*font.size=2 is out of range*");
    g_test_expect_message(NULL, G_LOG_LEVEL_WARNING,
                          "*terminal.scrollback=-5 is out of range*");
    g_test_expect_message(NULL, G_LOG_LEVEL_WARNING,
                          "*glow_threshold_low*swapping*");
    CathodeConfig *cfg = cathode_config_load();
    g_test_assert_expected_messages();
    g_assert_cmpint(cfg->scrollback, ==, 0);
    g_assert_cmpint(cfg->font_size, ==, 4);
    g_assert_cmpint(cfg->scanline_period, ==, 1);
    g_assert_cmpfloat_with_epsilon(cfg->glow_threshold_low, 0.2, 1e-6);
    g_assert_cmpfloat_with_epsilon(cfg->glow_threshold_high, 0.9, 1e-6);
    cathode_config_free(cfg);

    write_config("[font]\nsize = 999\n");
    g_test_expect_message(NULL, G_LOG_LEVEL_WARNING,
                          "*font.size=999 is out of range*");
    cfg = cathode_config_load();
    g_test_assert_expected_messages();
    g_assert_cmpint(cfg->font_size, ==, 72);
    cathode_config_free(cfg);
}

static void
test_float_scanline_period(void)
{
    write_config("[crt]\nscanline_period = 6.0\n");

    CathodeConfig *cfg = cathode_config_load();
    g_assert_cmpint(cfg->scanline_period, ==, 6);
    cathode_config_free(cfg);
}

int
main(int argc, char **argv)
{
    tmp_dir = g_dir_make_tmp("cathode-tests-XXXXXX", NULL);
    g_assert_nonnull(tmp_dir);
    cfg_dir = g_build_filename(tmp_dir, "cathode", NULL);
    cfg_path = g_build_filename(cfg_dir, "cathode.toml", NULL);
    g_setenv("XDG_CONFIG_HOME", tmp_dir, TRUE);

    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/config/defaults", test_defaults);
    g_test_add_func("/config/parse", test_parse);
    g_test_add_func("/config/reload-clears-removed-state",
                    test_reload_clears_removed_state);
    g_test_add_func("/config/invalid-values-are-clamped",
                    test_invalid_values_are_clamped);
    g_test_add_func("/config/float-scanline-period",
                    test_float_scanline_period);

    int status = g_test_run();

    g_unlink(cfg_path);
    g_rmdir(cfg_dir);
    g_rmdir(tmp_dir);
    g_free(cfg_path);
    g_free(cfg_dir);
    g_free(tmp_dir);

    return status;
}
