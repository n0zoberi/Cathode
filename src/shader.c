#include "shader.h"
#include <epoxy/gl.h>
#include <cairo.h>
#include <gsk/gsk.h>
#include <vte/vte.h>
#include <math.h>

typedef struct {
    CathodeConfig *cfg;
    GtkWidget     *terminal;
    GtkGLArea     *gl_area;
    GtkWidget     *overlay;

    GLuint  program_retro;
    GLuint  vao, vbo;

    GLuint  tex_terminal;
    GLuint  tex_blank;

    GLuint  program_blur;
    GLuint  fbo_blur;
    GLuint  tex_bloom[2];
    GLuint  tex_glow[2];
    int     blur_w, blur_h;

    int     width, height;
    bool    initialized;
    bool    disabled;
    bool    warned;
    bool    tex_dirty;
    guint   redraw_idle_id;
    guint   tick_id;
    guint   blink_id;

    unsigned char *accum_buffer;
    int     accum_w, accum_h;
    double  last_frame_time;

    cairo_surface_t *capture_surface;
    int     capture_w, capture_h;

    GLuint u_terminal;
    GLuint u_bloom_tex;
    GLuint u_glow_tex;
    GLuint u_time;
    GLuint u_resolution;
    GLuint u_background;
    GLuint u_scanline_mode;
    GLuint u_scanline_intensity;
    GLuint u_scanline_period;
    GLuint u_bloom_strength;
    GLuint u_bloom_sigma;
    GLuint u_glow_strength;
    GLuint u_glow_threshold_low;
    GLuint u_glow_threshold_high;
    GLuint u_mask_strength;
    GLuint u_curvature;
    GLuint u_chromatic_aberration;
    GLuint u_softening;
    GLuint u_color_bleed;
    GLuint u_rounding;
    GLuint u_shadow_strength;
    GLuint u_vignette_strength;
    GLuint u_burn_in;
    GLuint u_film_grain;
    GLuint u_jitter;
    GLuint u_flickering;
    GLuint u_glowing_line;

    GLuint u_blur_source;
    GLuint u_blur_texel;
    GLuint u_blur_axis;
    GLuint u_blur_sigma;
    GLuint u_blur_radius;
    GLuint u_blur_kind;
    GLuint u_blur_gate;
    GLuint u_blur_threshold_low;
    GLuint u_blur_threshold_high;
    GLuint u_blur_flip_y;
} CathodeShaderState;

#define BLUR_DOWNSCALE 2

static void queue_redraw_idle(CathodeShaderState *st);
static gboolean on_cursor_blink_tick(gpointer data);
static void shader_disable(CathodeShaderState *st, const char *reason);

static const float quad_vertices[] = {
    -1.0f, -1.0f, 0.0f, 0.0f,
     1.0f, -1.0f, 1.0f, 0.0f,
     1.0f,  1.0f, 1.0f, 1.0f,
    -1.0f, -1.0f, 0.0f, 0.0f,
     1.0f,  1.0f, 1.0f, 1.0f,
    -1.0f,  1.0f, 0.0f, 1.0f,
};

static GLuint
compile_shader(GLenum type, const char *source)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &source, NULL);
    glCompileShader(s);

    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetShaderInfoLog(s, sizeof(buf), NULL, buf);
        g_warning("Shader compile error (%s): %s",
                  type == GL_VERTEX_SHADER ? "vert" : "frag", buf);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint
link_program(const char *vert_src, const char *frag_src)
{
    GLuint v = compile_shader(GL_VERTEX_SHADER, vert_src);
    if (!v) return 0;
    GLuint f = compile_shader(GL_FRAGMENT_SHADER, frag_src);
    if (!f) { glDeleteShader(v); return 0; }

    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);

    GLint ok;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetProgramInfoLog(p, sizeof(buf), NULL, buf);
        g_warning("Shader link error: %s", buf);
        glDeleteProgram(p);
        p = 0;
    }
    glDeleteShader(v);
    glDeleteShader(f);
    return p;
}

static char *
load_shader(const char *name)
{
    char *path = g_strdup_printf("/com/n0zoberi/Cathode/shaders/%s", name);
    GBytes *b = g_resources_lookup_data(path, G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
    g_free(path);
    if (!b) {
        g_warning("Failed to load shader: %s", name);
        return NULL;
    }
    char *src = g_strndup(g_bytes_get_data(b, NULL), g_bytes_get_size(b));
    g_bytes_unref(b);
    return src;
}

static void
delete_tex(CathodeShaderState *st)
{
    if (st->tex_terminal) glDeleteTextures(1, &st->tex_terminal);
    st->tex_terminal = 0;
    st->width = st->height = 0;
}

static bool
ensure_tex(CathodeShaderState *st, int w, int h)
{
    if (w == st->width && h == st->height)
        return st->tex_terminal != 0;
    if (st->tex_terminal)
        glDeleteTextures(1, &st->tex_terminal);
    st->tex_terminal = 0;
    st->width = 0;
    st->height = 0;

    glGenTextures(1, &st->tex_terminal);
    glBindTexture(GL_TEXTURE_2D, st->tex_terminal);
    /* sRGB texture: the GPU performs the sRGB -> linear decode on sampling. */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, w, h, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    /* Cairo ARGB32 is a native-endian 32-bit ARGB value, stored as BGRX on
     * little-endian hosts; the R/B swizzle below is only valid there. */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        g_warning("Cannot allocate sRGB terminal texture: 0x%x", err);
        glDeleteTextures(1, &st->tex_terminal);
        st->tex_terminal = 0;
        return false;
    }

    st->width = w;
    st->height = h;
    return true;
}

static bool
capture_terminal(CathodeShaderState *st)
{
    GtkWidget *term = st->terminal;
    if (!GTK_IS_WIDGET(term)) return false;
    int w = gtk_widget_get_width(term);
    int h = gtk_widget_get_height(term);
    if (w <= 0 || h <= 0) return true;

    int scale = gtk_widget_get_scale_factor(term);
    if (scale < 1) scale = 1;
    int pw = w * scale;
    int ph = h * scale;

    if (!ensure_tex(st, pw, ph))
        return false;

    GtkWidget *parent = gtk_widget_get_parent(term);
    if (!parent) return false;

    GtkSnapshot *snap = gtk_snapshot_new();
    gtk_widget_snapshot_child(parent, term, snap);
    GskRenderNode *node = gtk_snapshot_free_to_node(snap);

    if (pw != st->capture_w || ph != st->capture_h) {
        if (st->capture_surface)
            cairo_surface_destroy(st->capture_surface);
        st->capture_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, pw, ph);
        st->capture_w = pw;
        st->capture_h = ph;
    }
    cairo_t *cr = cairo_create(st->capture_surface);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_scale(cr, scale, scale);
    gsk_render_node_draw(node, cr);
    cairo_destroy(cr);
    gsk_render_node_unref(node);

    cairo_surface_flush(st->capture_surface);

    unsigned char *data = cairo_image_surface_get_data(st->capture_surface);
    int stride = cairo_image_surface_get_stride(st->capture_surface);

    // ---- Burn-in / phosphor persistence (CPU-side frame accumulation) ----
    // Accumulates terminal frames with exponential decay so that bright content
    // lingers after it disappears from the terminal, mimicking CRT phosphor lag.
    if (st->cfg->burn_in > 0.001f) {
        int buf_size = ph * stride;
        if (st->accum_w != pw || st->accum_h != ph) {
            st->accum_buffer = g_realloc(st->accum_buffer, buf_size);
            st->accum_w = pw;
            st->accum_h = ph;
            st->last_frame_time = 0.0;
        }

        double now = g_get_monotonic_time() / 1e6;
        if (st->last_frame_time == 0.0) {
            memcpy(st->accum_buffer, data, buf_size);
        } else {
            double dt = now - st->last_frame_time;
            float half_life = st->cfg->burn_in * 2.0f;
            if (half_life < 0.001f) half_life = 0.001f;
            float decay = expf(-(float)dt / half_life);
            int decay_fixed = (int)(decay * 256.0f);
            if (decay_fixed < 0) decay_fixed = 0;
            if (decay_fixed > 256) decay_fixed = 256;

            for (int i = 0; i < buf_size; i++) {
                int decayed = (st->accum_buffer[i] * decay_fixed + 128) >> 8;
                st->accum_buffer[i] = decayed > data[i]
                    ? (unsigned char)decayed
                    : data[i];
            }
        }
        st->last_frame_time = now;
        data = st->accum_buffer;
    } else {
        st->last_frame_time = 0.0;
    }

    glPixelStorei(GL_UNPACK_ROW_LENGTH, stride / 4);
    glBindTexture(GL_TEXTURE_2D, st->tex_terminal);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, pw, ph,
                    GL_RGBA, GL_UNSIGNED_BYTE, data);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        g_warning("GL error in capture_terminal: 0x%x", err);
        return false;
    }
    return true;
}

static void
upload_retro(CathodeShaderState *st, int w, int h)
{
    CathodeConfig *c = st->cfg;

    glUniform1i(st->u_terminal, 0);
    glUniform1i(st->u_bloom_tex, 1);
    glUniform1i(st->u_glow_tex, 2);
    glUniform1f(st->u_time, (float)g_get_monotonic_time() / 1e6f);
    glUniform2f(st->u_resolution, (float)w, (float)h);

    float bg[4] = {0, 0, 0, 1};
    if (c->bg_color) {
        GdkRGBA rgba;
        if (gdk_rgba_parse(&rgba, c->bg_color)) {
            bg[0] = (float)rgba.red;
            bg[1] = (float)rgba.green;
            bg[2] = (float)rgba.blue;
        }
    }
    glUniform4fv(st->u_background, 1, bg);

    glUniform1i(st->u_scanline_mode, c->scanline_mode);
    glUniform1f(st->u_scanline_intensity, c->scanline_intensity);
    glUniform1f(st->u_scanline_period, (float)c->scanline_period);
    glUniform1f(st->u_bloom_strength, c->bloom_strength);
    glUniform1f(st->u_bloom_sigma, c->bloom_sigma);
    glUniform1f(st->u_glow_strength, c->glow_strength);
    glUniform1f(st->u_glow_threshold_low, c->glow_threshold_low);
    glUniform1f(st->u_glow_threshold_high, c->glow_threshold_high);
    glUniform1f(st->u_mask_strength, c->mask_strength);
    glUniform1f(st->u_curvature, c->curvature);
    glUniform1f(st->u_chromatic_aberration, c->chromatic_aberration);
    glUniform1f(st->u_softening, c->softening);
    glUniform1f(st->u_color_bleed, c->color_bleed);
    glUniform1f(st->u_rounding, c->rounding);
    glUniform1f(st->u_shadow_strength, c->shadow_strength);
    glUniform1f(st->u_vignette_strength, c->vignette_strength);
    glUniform1f(st->u_burn_in, c->burn_in);
    glUniform1f(st->u_film_grain, c->film_grain);
    glUniform1f(st->u_jitter, c->jitter);
    glUniform1f(st->u_flickering, c->flickering);
    glUniform1f(st->u_glowing_line, c->glowing_line);
}

static void
delete_blur_targets(CathodeShaderState *st)
{
    if (st->fbo_blur)
        glDeleteFramebuffers(1, &st->fbo_blur);
    if (st->tex_bloom[0])
        glDeleteTextures(2, st->tex_bloom);
    if (st->tex_glow[0])
        glDeleteTextures(2, st->tex_glow);

    st->fbo_blur = 0;
    st->tex_bloom[0] = st->tex_bloom[1] = 0;
    st->tex_glow[0] = st->tex_glow[1] = 0;
    st->blur_w = st->blur_h = 0;
}

static bool
ensure_blur_targets(CathodeShaderState *st)
{
    int w = st->width / BLUR_DOWNSCALE;
    int h = st->height / BLUR_DOWNSCALE;
    if (w < 1) w = 1;
    if (h < 1) h = 1;

    if (st->fbo_blur && w == st->blur_w && h == st->blur_h)
        return true;

    delete_blur_targets(st);
    st->blur_w = w;
    st->blur_h = h;

    GLuint *targets[4] = {
        &st->tex_bloom[0], &st->tex_bloom[1],
        &st->tex_glow[0],  &st->tex_glow[1],
    };
    for (int i = 0; i < 4; i++) {
        glGenTextures(1, targets[i]);
        glBindTexture(GL_TEXTURE_2D, *targets[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    glGenFramebuffers(1, &st->fbo_blur);
    glBindFramebuffer(GL_FRAMEBUFFER, st->fbo_blur);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, st->tex_bloom[0], 0);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        g_warning("Blur framebuffer incomplete: 0x%x", status);
        delete_blur_targets(st);
        return false;
    }
    return true;
}

static int
blur_radius(int full_radius)
{
    int r = (full_radius + BLUR_DOWNSCALE - 1) / BLUR_DOWNSCALE;
    return r < 1 ? 1 : r;
}

static void
blur_pass(CathodeShaderState *st, GLuint target, GLuint source,
          bool horizontal, float sigma, int radius, int kind,
          bool gate, bool flip_y)
{
    glBindFramebuffer(GL_FRAMEBUFFER, st->fbo_blur);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, target, 0);
    glViewport(0, 0, st->blur_w, st->blur_h);

    glUseProgram(st->program_blur);
    glUniform1i(st->u_blur_source, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, source);
    glUniform2f(st->u_blur_texel, 1.0f / (float)st->blur_w,
                1.0f / (float)st->blur_h);
    glUniform2f(st->u_blur_axis, horizontal ? 1.0f : 0.0f,
                horizontal ? 0.0f : 1.0f);
    glUniform1f(st->u_blur_sigma, sigma);
    glUniform1i(st->u_blur_radius, radius);
    glUniform1i(st->u_blur_kind, kind);
    glUniform1i(st->u_blur_gate, gate ? 1 : 0);
    glUniform1f(st->u_blur_threshold_low, st->cfg->glow_threshold_low);
    glUniform1f(st->u_blur_threshold_high, st->cfg->glow_threshold_high);
    glUniform1i(st->u_blur_flip_y, flip_y ? 1 : 0);

    glBindVertexArray(st->vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

static gboolean
render_cb(GtkGLArea *area, GdkGLContext *_ctx, gpointer data)
{
    (void)_ctx;
    CathodeShaderState *st = data;

    if (!st->initialized || st->disabled) return FALSE;
    if (!GTK_IS_WIDGET(st->terminal)) return FALSE;

    while (glGetError() != GL_NO_ERROR) {}

    int term_scale = gtk_widget_get_scale_factor(st->terminal);
    if (term_scale < 1) term_scale = 1;
    if (gtk_widget_get_width(st->terminal) * term_scale != st->width ||
        gtk_widget_get_height(st->terminal) * term_scale != st->height)
        st->tex_dirty = true;

    if (st->tex_dirty) {
        if (!capture_terminal(st)) {
            shader_disable(st, "failed to capture the terminal");
            return FALSE;
        }
        st->tex_dirty = false;
    }

    if (st->width <= 0 || st->height <= 0) return FALSE;

    int area_w = gtk_widget_get_width(GTK_WIDGET(area));
    int area_h = gtk_widget_get_height(GTK_WIDGET(area));
    if (area_w <= 0 || area_h <= 0) return FALSE;

    float scale = gtk_widget_get_scale_factor(GTK_WIDGET(area));
    GLint prev_fbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);

    CathodeConfig *c = st->cfg;
    bool want_bloom = c->bloom_strength > 0.001f;
    bool want_glow  = c->glow_strength > 0.001f;

    if ((want_bloom || want_glow) && !ensure_blur_targets(st)) {
        shader_disable(st, "blur framebuffer unavailable");
        return FALSE;
    }

    if (want_bloom) {
        float sigma = c->bloom_sigma > 0.5f ? c->bloom_sigma : 0.5f;
        int full_radius = (int)ceilf(sigma * 2.5f);
        if (full_radius < 1) full_radius = 1;
        if (full_radius > 13) full_radius = 13;
        int radius = blur_radius(full_radius);
        float sigma_low = sigma / BLUR_DOWNSCALE;

        blur_pass(st, st->tex_bloom[0], st->tex_terminal, true,
                  sigma_low, radius, 0, false, true);
        blur_pass(st, st->tex_bloom[1], st->tex_bloom[0], false,
                  sigma_low, radius, 0, false, false);
    }

    if (want_glow) {
        float sigma = 1.2f + c->glow_strength * 10.0f;
        float sigma_h = sigma * 1.5f;
        float sigma_v = sigma * 0.6f;
        int full_radius = (int)ceilf(sigma * 3.0f);
        if (full_radius < 1) full_radius = 1;
        if (full_radius > 6) full_radius = 6;
        int radius = blur_radius(full_radius);

        blur_pass(st, st->tex_glow[0], st->tex_terminal, true,
                  sigma_h / BLUR_DOWNSCALE, radius, 1, true, true);
        blur_pass(st, st->tex_glow[1], st->tex_glow[0], false,
                  sigma_v / BLUR_DOWNSCALE, radius, 1, false, false);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo);
    glViewport(0, 0, (int)(area_w * scale),
                     (int)(area_h * scale));
    glBindVertexArray(st->vao);

    glUseProgram(st->program_retro);
    upload_retro(st, st->width, st->height);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, st->tex_terminal);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D,
                  (want_bloom && st->tex_bloom[1]) ? st->tex_bloom[1] : st->tex_blank);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D,
                  (want_glow && st->tex_glow[1]) ? st->tex_glow[1] : st->tex_blank);
    glActiveTexture(GL_TEXTURE0);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
        g_warning("CRT GL error: 0x%x", err);

    glBindVertexArray(0);
    glFlush();

    return TRUE;
}

static bool
has_time_effects(CathodeConfig *cfg)
{
    return cfg->film_grain  > 0.001f ||
           cfg->jitter      > 0.001f ||
           cfg->flickering  > 0.001f ||
           cfg->glowing_line > 0.001f ||
           cfg->burn_in     > 0.001f;
}

static gboolean
on_frame_tick(GtkWidget *widget, GdkFrameClock *clock, gpointer data)
{
    (void)clock;
    CathodeShaderState *st = data;
    if (!st->initialized || st->disabled || !gtk_widget_get_visible(widget))
        return G_SOURCE_CONTINUE;

    if (!has_time_effects(st->cfg))
        return G_SOURCE_CONTINUE;

    /* Burn-in decays over time, so it needs fresh frames. */
    if (st->cfg->burn_in > 0.001f)
        st->tex_dirty = true;

    gtk_widget_queue_draw(widget);
    return G_SOURCE_CONTINUE;
}

static void
update_tick(CathodeShaderState *st)
{
    bool want = st->initialized && !st->disabled &&
                cathode_shader_is_effect_active(st->cfg) &&
                has_time_effects(st->cfg);

    if (want && st->tick_id == 0) {
        st->tick_id = gtk_widget_add_tick_callback(GTK_WIDGET(st->gl_area),
            on_frame_tick, st, NULL);
    } else if (!want && st->tick_id != 0) {
        gtk_widget_remove_tick_callback(GTK_WIDGET(st->gl_area), st->tick_id);
        st->tick_id = 0;
    }
}

static void
shader_disable(CathodeShaderState *st, const char *reason)
{
    st->initialized = false;
    st->disabled = true;
    gtk_widget_set_visible(GTK_WIDGET(st->gl_area), FALSE);

    if (st->tick_id) {
        gtk_widget_remove_tick_callback(GTK_WIDGET(st->gl_area), st->tick_id);
        st->tick_id = 0;
    }
    if (!st->warned) {
        st->warned = true;
        g_warning("CRT disabled: %s", reason);
    }
}

static void
realize_cb(GtkGLArea *area, gpointer data)
{
    CathodeShaderState *st = data;

    gtk_gl_area_make_current(area);
    GError *gl_error = gtk_gl_area_get_error(area);
    if (gl_error) {
        shader_disable(st, gl_error->message);
        return;
    }

    const char *version = (const char *)glGetString(GL_VERSION);
    const char *renderer = (const char *)glGetString(GL_RENDERER);
    g_message("GL version: %s, renderer: %s", version ? version : "?",
              renderer ? renderer : "?");

    char *vert = load_shader("retro.vert");
    char *frag = load_shader("retro.frag");
    char *blur = load_shader("blur.frag");

    if (!vert || !frag || !blur) {
        g_free(vert);
        g_free(frag);
        g_free(blur);
        shader_disable(st, "failed to load shaders");
        return;
    }

    st->program_retro = link_program(vert, frag);
    st->program_blur  = link_program(vert, blur);

    g_free(vert);
    g_free(frag);
    g_free(blur);

    if (!st->program_retro || !st->program_blur) {
        shader_disable(st, "shader compile/link failed");
        return;
    }

    glGenVertexArrays(1, &st->vao);
    glBindVertexArray(st->vao);

    glGenBuffers(1, &st->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, st->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices,
                 GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void *)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

#   define CACHE_UNIFORM(name) st->u_##name = glGetUniformLocation(st->program_retro, "u_" #name)
    CACHE_UNIFORM(terminal);
    CACHE_UNIFORM(bloom_tex);
    CACHE_UNIFORM(glow_tex);
    CACHE_UNIFORM(time);
    CACHE_UNIFORM(resolution);
    CACHE_UNIFORM(background);
    CACHE_UNIFORM(scanline_mode);
    CACHE_UNIFORM(scanline_intensity);
    CACHE_UNIFORM(scanline_period);
    CACHE_UNIFORM(bloom_strength);
    CACHE_UNIFORM(bloom_sigma);
    CACHE_UNIFORM(glow_strength);
    CACHE_UNIFORM(glow_threshold_low);
    CACHE_UNIFORM(glow_threshold_high);
    CACHE_UNIFORM(mask_strength);
    CACHE_UNIFORM(curvature);
    CACHE_UNIFORM(chromatic_aberration);
    CACHE_UNIFORM(softening);
    CACHE_UNIFORM(color_bleed);
    CACHE_UNIFORM(rounding);
    CACHE_UNIFORM(shadow_strength);
    CACHE_UNIFORM(vignette_strength);
    CACHE_UNIFORM(burn_in);
    CACHE_UNIFORM(film_grain);
    CACHE_UNIFORM(jitter);
    CACHE_UNIFORM(flickering);
    CACHE_UNIFORM(glowing_line);
#   undef CACHE_UNIFORM

#   define CACHE_BLUR(name) st->u_blur_##name = glGetUniformLocation(st->program_blur, "u_" #name)
    CACHE_BLUR(source);
    CACHE_BLUR(texel);
    CACHE_BLUR(axis);
    CACHE_BLUR(sigma);
    CACHE_BLUR(radius);
    CACHE_BLUR(kind);
    CACHE_BLUR(gate);
    CACHE_BLUR(threshold_low);
    CACHE_BLUR(threshold_high);
    CACHE_BLUR(flip_y);
#   undef CACHE_BLUR

    static const unsigned char blank_pixel[4] = { 0, 0, 0, 0 };
    glGenTextures(1, &st->tex_blank);
    glBindTexture(GL_TEXTURE_2D, st->tex_blank);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, blank_pixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    st->initialized = true;
    st->tex_dirty = true;

    st->blink_id = g_timeout_add(500, on_cursor_blink_tick, st);
    update_tick(st);
    queue_redraw_idle(st);
}

static void
unrealize_cb(GtkGLArea *area, gpointer data)
{
    CathodeShaderState *st = data;

    if (st->redraw_idle_id) {
        g_source_remove(st->redraw_idle_id);
        st->redraw_idle_id = 0;
    }

    if (st->tick_id) {
        gtk_widget_remove_tick_callback(GTK_WIDGET(area), st->tick_id);
        st->tick_id = 0;
    }

    if (st->blink_id) {
        g_source_remove(st->blink_id);
        st->blink_id = 0;
    }

    gtk_gl_area_make_current(area);
    if (gtk_gl_area_get_error(area)) {
        st->initialized = false;
        return;
    }

    if (st->program_retro) glDeleteProgram(st->program_retro);
    if (st->program_blur)  glDeleteProgram(st->program_blur);
    if (st->vao)           glDeleteVertexArrays(1, &st->vao);
    if (st->vbo)           glDeleteBuffers(1, &st->vbo);
    if (st->tex_blank)     glDeleteTextures(1, &st->tex_blank);

    st->program_retro = 0;
    st->program_blur = 0;
    st->tex_blank = 0;
    st->vao = st->vbo = 0;

    delete_blur_targets(st);
    delete_tex(st);
    st->initialized = false;

    /* Keep st->terminal: terminal destruction is handled by on_term_destroy. */
}

static void
shader_state_free(gpointer data)
{
    CathodeShaderState *st = data;
    if (st->redraw_idle_id)
        g_source_remove(st->redraw_idle_id);
    if (st->blink_id)
        g_source_remove(st->blink_id);
    if (st->capture_surface)
        cairo_surface_destroy(st->capture_surface);
    g_free(st->accum_buffer);
    g_free(st);
}

static gboolean
on_redraw_idle(gpointer data)
{
    CathodeShaderState *st = data;
    st->redraw_idle_id = 0;
    if (GTK_IS_WIDGET(st->gl_area))
        gtk_widget_queue_draw(GTK_WIDGET(st->gl_area));
    return G_SOURCE_REMOVE;
}

static void
queue_redraw_idle(CathodeShaderState *st)
{
    if (st->redraw_idle_id != 0) return;
    st->redraw_idle_id = g_idle_add_full(G_PRIORITY_DEFAULT,
        on_redraw_idle, st, NULL);
}

static gboolean
on_cursor_blink_tick(gpointer data)
{
    CathodeShaderState *st = data;
    if (st->initialized && !st->disabled &&
        gtk_widget_get_visible(GTK_WIDGET(st->gl_area)) &&
        st->cfg->cursor_blink != CURSOR_BLINK_OFF) {
        st->tex_dirty = true;
        queue_redraw_idle(st);
    }
    return G_SOURCE_CONTINUE;
}

static void
on_term_changed(VteTerminal *term, gpointer data)
{
    (void)term;
    CathodeShaderState *st = data;
    st->tex_dirty = true;
    queue_redraw_idle(st);
}

static void
on_term_destroy(GtkWidget *_term, gpointer data)
{
    (void)_term;
    CathodeShaderState *st = data;
    st->terminal = NULL;
    if (st->redraw_idle_id) {
        g_source_remove(st->redraw_idle_id);
        st->redraw_idle_id = 0;
    }
}

GtkWidget *
cathode_shader_overlay_new(CathodeConfig *cfg, GtkWidget *terminal)
{
    CathodeShaderState *st = g_new0(CathodeShaderState, 1);
    st->cfg = cfg;
    st->terminal = terminal;
    st->tex_dirty = true;

    GtkWidget *gl_widget = gtk_gl_area_new();
    gtk_gl_area_set_allowed_apis(GTK_GL_AREA(gl_widget),
                                  GDK_GL_API_GL | GDK_GL_API_GLES);
    gtk_gl_area_set_required_version(GTK_GL_AREA(gl_widget), 3, 0);
    gtk_widget_set_can_target(gl_widget, FALSE);
    gtk_widget_set_halign(gl_widget, GTK_ALIGN_FILL);
    gtk_widget_set_valign(gl_widget, GTK_ALIGN_FILL);

    st->gl_area = GTK_GL_AREA(gl_widget);

    g_signal_connect(gl_widget, "realize", G_CALLBACK(realize_cb), st);
    g_signal_connect(gl_widget, "unrealize", G_CALLBACK(unrealize_cb), st);
    g_signal_connect(gl_widget, "render", G_CALLBACK(render_cb), st);

    g_signal_connect(terminal, "contents-changed",
                     G_CALLBACK(on_term_changed), st);
    g_signal_connect(terminal, "cursor-moved",
                     G_CALLBACK(on_term_changed), st);
    g_signal_connect(terminal, "selection-changed",
                     G_CALLBACK(on_term_changed), st);
    /* Scrolling repaints the terminal without changing its contents. */
    GtkAdjustment *vadj =
        gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(terminal));
    if (vadj)
        g_signal_connect(vadj, "value-changed",
                         G_CALLBACK(on_term_changed), st);
    g_signal_connect(terminal, "destroy",
                     G_CALLBACK(on_term_destroy), st);

    GtkWidget *overlay = gtk_overlay_new();
    gtk_overlay_set_child(GTK_OVERLAY(overlay), terminal);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), gl_widget);
    st->overlay = overlay;

    g_object_set_data_full(G_OBJECT(overlay), "cathode-shader",
                           st, shader_state_free);

    if (!cathode_shader_is_effect_active(cfg))
        gtk_widget_set_visible(gl_widget, FALSE);

    return overlay;
}

bool
cathode_shader_is_effect_active(CathodeConfig *cfg)
{
    if (!cfg->crt_enabled) return false;
    return cfg->scanline_intensity   > 0.001f ||
           cfg->bloom_strength       > 0.001f ||
           cfg->glow_strength        > 0.001f ||
           cfg->mask_strength        > 0.0001f ||
           cfg->curvature            > 0.0001f ||
           cfg->chromatic_aberration > 0.00001f ||
           cfg->softening            > 0.001f ||
           cfg->color_bleed          > 0.001f ||
           cfg->rounding             > 0.001f ||
           cfg->shadow_strength      > 0.001f ||
           cfg->vignette_strength    > 0.001f ||
           cfg->burn_in              > 0.001f ||
           cfg->film_grain           > 0.001f ||
           cfg->jitter               > 0.001f ||
           cfg->flickering           > 0.001f ||
           cfg->glowing_line         > 0.001f;
}

static CathodeShaderState *
get_shader_state_from_overlay(GtkWidget *overlay)
{
    return g_object_get_data(G_OBJECT(overlay), "cathode-shader");
}

VteTerminal *
cathode_shader_overlay_get_terminal(GtkWidget *overlay)
{
    CathodeShaderState *st = get_shader_state_from_overlay(overlay);
    return st && st->terminal ? VTE_TERMINAL(st->terminal) : NULL;
}

void
cathode_shader_refresh_visible(GtkWidget *overlay)
{
    CathodeShaderState *st = get_shader_state_from_overlay(overlay);
    if (!st) return;

    bool active = !st->disabled && cathode_shader_is_effect_active(st->cfg);
    gtk_widget_set_visible(GTK_WIDGET(st->gl_area), active);

    update_tick(st);

    if (active) {
        st->tex_dirty = true;
        queue_redraw_idle(st);
    }
}
