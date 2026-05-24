#include "shader.h"
#include <epoxy/gl.h>
#include <cairo.h>
#include <gsk/gsk.h>
#include <vte/vte.h>

typedef struct {
    CathodeConfig *cfg;
    GtkWidget     *terminal;
    GtkGLArea     *gl_area;
    GtkWidget     *overlay;

    GLuint  program_retro;
    GLuint  vao, vbo;

    GLuint  tex_terminal;

    int     width, height;
    bool    initialized;
    bool    needs_redraw;
    guint   redraw_idle_id;
} CathodeShaderState;

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
    char *path = g_strdup_printf("/org/cathode/Cathode/shaders/%s", name);
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

static void
ensure_tex(CathodeShaderState *st, int w, int h)
{
    if (w == st->width && h == st->height)
        return;
    if (st->tex_terminal)
        glDeleteTextures(1, &st->tex_terminal);
    st->width = w;
    st->height = h;
    glGenTextures(1, &st->tex_terminal);
    glBindTexture(GL_TEXTURE_2D, st->tex_terminal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
}

static void
capture_terminal(CathodeShaderState *st)
{
    GtkWidget *term = st->terminal;
    if (!GTK_IS_WIDGET(term)) return;
    int w = gtk_widget_get_width(term);
    int h = gtk_widget_get_height(term);
    if (w <= 0 || h <= 0) return;

    ensure_tex(st, w, h);

    GtkWidget *parent = gtk_widget_get_parent(term);
    if (!parent) return;

    GtkSnapshot *snap = gtk_snapshot_new();
    gtk_widget_snapshot_child(parent, term, snap);
    GskRenderNode *node = gtk_snapshot_free_to_node(snap);

    cairo_surface_t *cs = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    cairo_t *cr = cairo_create(cs);
    gsk_render_node_draw(node, cr);
    cairo_destroy(cr);
    gsk_render_node_unref(node);

    cairo_surface_flush(cs);

    unsigned char *data = cairo_image_surface_get_data(cs);
    int stride = cairo_image_surface_get_stride(cs);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, stride / 4);
    glBindTexture(GL_TEXTURE_2D, st->tex_terminal);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h,
                    GL_RGBA, GL_UNSIGNED_BYTE, data);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
        g_warning("GL error in capture_terminal: 0x%x", err);

    cairo_surface_destroy(cs);
}

static void
upload_retro(CathodeShaderState *st, int w, int h)
{
    CathodeConfig *c = st->cfg;
    GLuint p = st->program_retro;

    glUniform1i(glGetUniformLocation(p, "u_terminal"), 0);
    glUniform1f(glGetUniformLocation(p, "u_time"),
                (float)g_get_monotonic_time() / 1e6f);
    glUniform2f(glGetUniformLocation(p, "u_resolution"), (float)w, (float)h);

    float bg[4] = {0, 0, 0, 1};
    if (c->bg_color) {
        GdkRGBA rgba;
        if (gdk_rgba_parse(&rgba, c->bg_color)) {
            bg[0] = (float)rgba.red;
            bg[1] = (float)rgba.green;
            bg[2] = (float)rgba.blue;
        }
    }
    glUniform4fv(glGetUniformLocation(p, "u_background"), 1, bg);

    glUniform1f(glGetUniformLocation(p, "u_scanline_intensity"),
                c->scanline_intensity);
    glUniform1f(glGetUniformLocation(p, "u_scanline_period"),
                c->scanline_period);
    glUniform1f(glGetUniformLocation(p, "u_bloom_strength"),
                c->bloom_strength);
    glUniform1f(glGetUniformLocation(p, "u_bloom_sigma"),
                c->bloom_sigma);
    glUniform1f(glGetUniformLocation(p, "u_glow_strength"),
                c->glow_strength);
    glUniform1f(glGetUniformLocation(p, "u_glow_threshold_low"),
                c->glow_threshold_low);
    glUniform1f(glGetUniformLocation(p, "u_glow_threshold_high"),
                c->glow_threshold_high);
    glUniform1f(glGetUniformLocation(p, "u_mask_strength"),
                c->mask_strength);
    glUniform1f(glGetUniformLocation(p, "u_curvature"),
                c->curvature);
    glUniform1f(glGetUniformLocation(p, "u_chromatic_aberration"),
                c->chromatic_aberration);
    glUniform1f(glGetUniformLocation(p, "u_softening"),
                c->softening);
    glUniform1f(glGetUniformLocation(p, "u_color_bleed"),
                c->color_bleed);
    glUniform1f(glGetUniformLocation(p, "u_rounding"),
                c->rounding);
    glUniform1f(glGetUniformLocation(p, "u_shadow_strength"),
                c->shadow_strength);
    glUniform1f(glGetUniformLocation(p, "u_burn_in"),
                c->burn_in);
}

static void gl_check(const char *where);

static gboolean
render_cb(GtkGLArea *area, GdkGLContext *_ctx, gpointer data)
{
    (void)_ctx;
    (void)area;
    CathodeShaderState *st = data;

    if (!st->initialized) return FALSE;

    while (glGetError() != GL_NO_ERROR) {}

    capture_terminal(st);

    if (st->width <= 0 || st->height <= 0) return FALSE;

    int area_w = gtk_widget_get_width(GTK_WIDGET(area));
    int area_h = gtk_widget_get_height(GTK_WIDGET(area));
    if (area_w <= 0 || area_h <= 0) return FALSE;

    float scale = gtk_widget_get_scale_factor(GTK_WIDGET(area));

    glViewport(0, 0, (int)(area_w * scale),
                     (int)(area_h * scale));
    glBindVertexArray(st->vao);

    glUseProgram(st->program_retro);
    upload_retro(st, st->width, st->height);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, st->tex_terminal);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
        g_warning("CRT GL error: 0x%x", err);

    glBindVertexArray(0);
    glFlush();

    return TRUE;
}

static void
realize_cb(GtkGLArea *area, gpointer data)
{
    CathodeShaderState *st = data;

    gtk_gl_area_make_current(area);
    if (gtk_gl_area_get_error(area)) {
        g_warning("GLArea realize error");
        return;
    }

    const char *version = (const char *)glGetString(GL_VERSION);
    const char *renderer = (const char *)glGetString(GL_RENDERER);
    g_message("GL version: %s, renderer: %s", version ? version : "?",
              renderer ? renderer : "?");

    char *vert = load_shader("retro.vert");
    char *frag = load_shader("retro.frag");

    if (!vert || !frag) {
        g_free(vert);
        g_free(frag);
        return;
    }

    st->program_retro = link_program(vert, frag);

    g_free(vert);
    g_free(frag);

    if (!st->program_retro)
        return;

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
    st->initialized = true;
}

static void
unrealize_cb(GtkGLArea *area, gpointer data)
{
    CathodeShaderState *st = data;

    if (st->redraw_idle_id) {
        g_source_remove(st->redraw_idle_id);
        st->redraw_idle_id = 0;
    }

    gtk_gl_area_make_current(area);
    if (gtk_gl_area_get_error(area)) {
        st->initialized = false;
        return;
    }

    if (st->program_retro) glDeleteProgram(st->program_retro);
    if (st->vao)           glDeleteVertexArrays(1, &st->vao);
    if (st->vbo)           glDeleteBuffers(1, &st->vbo);

    st->program_retro = 0;
    st->vao = st->vbo = 0;

    delete_tex(st);
    st->initialized = false;
}

static void
shader_state_free(gpointer data)
{
    CathodeShaderState *st = data;
    if (st->redraw_idle_id)
        g_source_remove(st->redraw_idle_id);
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
gl_check(const char *where)
{
    GLenum err = glGetError();
    while (err != GL_NO_ERROR) {
        g_warning("GL error at %s: 0x%x", where, err);
        err = glGetError();
    }
}

static void
queue_redraw_idle(CathodeShaderState *st)
{
    if (st->redraw_idle_id != 0) return;
    st->redraw_idle_id = g_idle_add_full(G_PRIORITY_DEFAULT,
        on_redraw_idle, st, NULL);
}

static void
on_term_changed(VteTerminal *term, gpointer data)
{
    (void)term;
    queue_redraw_idle(data);
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

    GtkWidget *gl_widget = gtk_gl_area_new();
    gtk_gl_area_set_allowed_apis(GTK_GL_AREA(gl_widget),
                                  GDK_GL_API_GL | GDK_GL_API_GLES);
    gtk_gl_area_set_required_version(GTK_GL_AREA(gl_widget), 3, 2);
    gtk_widget_set_can_target(gl_widget, FALSE);
    gtk_widget_set_halign(gl_widget, GTK_ALIGN_FILL);
    gtk_widget_set_valign(gl_widget, GTK_ALIGN_FILL);

    st->gl_area = GTK_GL_AREA(gl_widget);

    g_object_set_data_full(G_OBJECT(gl_widget), "cathode-shader",
                           st, shader_state_free);

    g_signal_connect(gl_widget, "realize", G_CALLBACK(realize_cb), st);
    g_signal_connect(gl_widget, "unrealize", G_CALLBACK(unrealize_cb), st);
    g_signal_connect(gl_widget, "render", G_CALLBACK(render_cb), st);

    g_signal_connect(terminal, "contents-changed",
                     G_CALLBACK(on_term_changed), st);
    g_signal_connect(terminal, "destroy",
                     G_CALLBACK(on_term_destroy), st);

    GtkWidget *overlay = gtk_overlay_new();
    gtk_overlay_set_child(GTK_OVERLAY(overlay), terminal);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), gl_widget);
    st->overlay = overlay;

    if (!cathode_shader_is_effect_active(cfg))
        gtk_widget_set_visible(gl_widget, FALSE);

    return overlay;
}

bool
cathode_shader_is_effect_active(CathodeConfig *cfg)
{
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
           cfg->burn_in              > 0.001f;
}

void
cathode_shader_queue_redraw(GtkWidget *overlay)
{
    CathodeShaderState *st = NULL;
    GtkWidget *child = gtk_widget_get_first_child(overlay);
    while (child) {
        if (GTK_IS_GL_AREA(child)) {
            st = g_object_get_data(G_OBJECT(child), "cathode-shader");
            break;
        }
        child = gtk_widget_get_next_sibling(child);
    }
    if (st)
        queue_redraw_idle(st);
}

void
cathode_shader_refresh_visible(GtkWidget *overlay)
{
    CathodeShaderState *st = NULL;
    GtkWidget *child = gtk_widget_get_first_child(overlay);
    while (child) {
        if (GTK_IS_GL_AREA(child)) {
            st = g_object_get_data(G_OBJECT(child), "cathode-shader");
            break;
        }
        child = gtk_widget_get_next_sibling(child);
    }
    if (!st) return;

    bool active = cathode_shader_is_effect_active(st->cfg);
    gtk_widget_set_visible(GTK_WIDGET(st->gl_area), active);

    if (active)
        queue_redraw_idle(st);
}
