#include "config.h"
#include "shader.h"
#include <glib.h>

static void
zero_effects(CathodeConfig *cfg)
{
    cfg->crt_enabled            = true;
    cfg->scanline_intensity     = 0.0f;
    cfg->bloom_strength         = 0.0f;
    cfg->glow_strength          = 0.0f;
    cfg->mask_strength          = 0.0f;
    cfg->curvature              = 0.0f;
    cfg->chromatic_aberration   = 0.0f;
    cfg->softening              = 0.0f;
    cfg->color_bleed            = 0.0f;
    cfg->rounding               = 0.0f;
    cfg->shadow_strength        = 0.0f;
    cfg->vignette_strength      = 0.0f;
    cfg->burn_in                = 0.0f;
    cfg->film_grain             = 0.0f;
    cfg->jitter                 = 0.0f;
    cfg->flickering             = 0.0f;
    cfg->glowing_line           = 0.0f;
}

static void
test_no_effects(void)
{
    CathodeConfig *cfg = cathode_config_default();
    zero_effects(cfg);
    g_assert_false(cathode_shader_is_effect_active(cfg));
    cathode_config_free(cfg);
}

static void
test_crt_disabled(void)
{
    CathodeConfig *cfg = cathode_config_default();
    cfg->crt_enabled = false;
    g_assert_false(cathode_shader_is_effect_active(cfg));
    cathode_config_free(cfg);
}

static void
test_each_effect(void)
{
    CathodeConfig *cfg = cathode_config_default();

#define CHECK_EFFECT(field, value)                       \
    do {                                                 \
        zero_effects(cfg);                               \
        cfg->field = (value);                            \
        g_assert_true(cathode_shader_is_effect_active(cfg)); \
    } while (0)

    CHECK_EFFECT(scanline_intensity, 0.01f);
    CHECK_EFFECT(bloom_strength, 0.01f);
    CHECK_EFFECT(glow_strength, 0.01f);
    CHECK_EFFECT(mask_strength, 0.001f);
    CHECK_EFFECT(curvature, 0.001f);
    CHECK_EFFECT(chromatic_aberration, 0.0001f);
    CHECK_EFFECT(softening, 0.01f);
    CHECK_EFFECT(color_bleed, 0.01f);
    CHECK_EFFECT(rounding, 0.01f);
    CHECK_EFFECT(shadow_strength, 0.01f);
    CHECK_EFFECT(vignette_strength, 0.01f);
    CHECK_EFFECT(burn_in, 0.01f);
    CHECK_EFFECT(film_grain, 0.01f);
    CHECK_EFFECT(jitter, 0.01f);
    CHECK_EFFECT(flickering, 0.01f);
    CHECK_EFFECT(glowing_line, 0.01f);

#undef CHECK_EFFECT

    zero_effects(cfg);
    cfg->mask_strength = 0.0001f;
    g_assert_false(cathode_shader_is_effect_active(cfg));

    cathode_config_free(cfg);
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/shader/active/none", test_no_effects);
    g_test_add_func("/shader/active/crt-disabled", test_crt_disabled);
    g_test_add_func("/shader/active/each-effect", test_each_effect);

    return g_test_run();
}
