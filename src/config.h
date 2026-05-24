#pragma once

#include <stdbool.h>

typedef enum {
    CURSOR_BLINK_ON,
    CURSOR_BLINK_OFF,
    CURSOR_BLINK_SYSTEM,
} CursorBlinkMode;

typedef struct {
    char **imports;
    int    num_imports;

    int    scrollback;
    CursorBlinkMode cursor_blink;

    char  *shell_program;
    char **shell_args;
    int    num_shell_args;

    char  *term;

    char  *font_family;
    char  *font_style;
    int    font_size;

    float scanline_intensity;
    float scanline_period;
    float bloom_strength;
    float bloom_sigma;
    float glow_strength;
    float glow_threshold_low;
    float glow_threshold_high;
    float mask_strength;
    float curvature;
    float chromatic_aberration;
    float softening;
    float color_bleed;
    float rounding;
    float shadow_strength;
    float burn_in;

    char  *fg_color;
    char  *bg_color;
    char  *cursor_color;
    char  *selection_bg;
    bool   auto_reload;

    char  *palette[16];
    bool   palette_set;
} CathodeConfig;

CathodeConfig *cathode_config_load(void);
CathodeConfig *cathode_config_default(void);
void           cathode_config_free(CathodeConfig *cfg);
void           cathode_config_reload(CathodeConfig *cfg);
void           cathode_config_merge_theme(CathodeConfig *cfg, const char *theme_path);
