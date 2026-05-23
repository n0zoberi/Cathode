# AGENTS.md — Cathode

## About

Cathode is a GTK4/libadwaita terminal emulator with a retro CRT scanline shader,
tabs, search, and TOML configuration. Written in C, built with Meson.

## Build & Run

```bash
meson setup build
meson compile -C build
./build/src/cathode
```

## Install

```bash
meson install -C build
```

- Binary → `$prefix/bin/cathode`
- Desktop entry → `$prefix/share/applications/org.cathode.Cathode.desktop`
- Icon → `$prefix/share/icons/hicolor/256x256/apps/org.cathode.Cathode.png`
- Sample config + theme → `$prefix/share/cathode/`

## Dependencies

- gtk4 >= 4.12
- libadwaita-1 >= 1.4
- vte-2.91-gtk4 >= 0.74
- epoxy >= 1.5
- cairo >= 1.16
- meson >= 1.0.0

## Config

- **User config:** `~/.config/cathode/cathode.toml` (auto-loaded on startup)
- **Reference:** `cathode.sample.toml` (in repo root, installed to `share/cathode/`)
- **Import syntax:** `[general].import` can load theme files; `~/` paths expand
- **Default theme:** `theme.toml` (installed to `share/cathode/`)
- All CRT parameters set to `0` → GLArea hidden, zero shader overhead

## Project Structure

```
src/
  main.c              — entry point
  app.c/h             — GtkApplication, actions, accelerators
  tab.c/h             — AdwTabView + AdwTabBar, tab lifecycle
  terminal.c/h        — VteTerminal setup, spawn, config apply
  shader.c/h          — GtkGLArea overlay, CRT shader pipeline
  search.c/h          — Ctrl+Shift+F search bar, VteRegex integration
  config.c/h          — TOML parsing (tomlc99), defaults, theme merge
  vendor/tomlc99/     — vendored tomlc99 (MIT)
shaders/              — GLSL shaders (embedded via gresource)
data/                 — .desktop file
```

## Architecture Notes

### Rendering Pipeline

```
GtkOverlay
  ├── VteTerminal (base child)       ← terminal output, input events
  └── GtkGLArea (overlay)            ← CRT post-processing, transparent if inactive
```

1. **Capture:** `gtk_widget_snapshot_child()` → `gsk_render_node_draw()` → Cairo ARGB32 surface
2. **Upload:** Cairo data → GL texture via `glTexSubImage2D(GL_RGBA, ...)` with `GL_TEXTURE_SWIZZLE_R=GL_BLUE` (hardware B↔R swap, no CPU conversion)
3. **CRT Shader:** Single-pass `retro.frag` — scanlines, phosphor glow, aperture grille mask, barrel curvature, chromatic aberration, vignetting, film grain, warm white point
4. **Bloom:** Disabled pending GLES FBO fix (see Known Issues)

### HiDPI / Scale Factor

- Terminal capture → **logical pixels** (e.g., 898×552 at 1×)
- Cairo surface + GL texture → **logical pixels**
- GL viewport → **physical pixels** (e.g., 1796×1104 at 2×) via `gtk_widget_get_scale_factor()` queried in `render_cb`
- Terminal texture is GL_LINEAR-upscaled to fill the physical viewport

### Key Design Decisions

- **GtkGLArea** not `GskGLShaderNode` — `GskGLShader` was deprecated in GTK 4.16
- **GLES + Desktop GL** — `gtk_gl_area_set_allowed_apis(GL|GLES)`, version 3.2
- **`GL_RGBA` + texture swizzle** — avoids `GL_BGRA` incompatibility with strict GLES
- **Cairo stride handling** — `glPixelStorei(GL_UNPACK_ROW_LENGTH, stride/4)` for padded rows
- **Tab lifecycle** — `child-exited` auto-closes tab; `destroy` signal nulls terminal pointer
- **Window close confirm** — AdwAlertDialog when multiple tabs open, `closing_confirmed` flag prevents infinite loop

## Keyboard Shortcuts

| Shortcut            | Action           |
|---------------------|------------------|
| Ctrl+Shift+T        | New tab          |
| Ctrl+Shift+W        | Close tab        |
| Ctrl+Shift+F        | Toggle search    |
| Ctrl+Shift+C / Ctrl+Alt+C | Copy       |
| Ctrl+Shift+V / Ctrl+Alt+V | Paste      |
| Ctrl+= / Ctrl++     | Increase font    |
| Ctrl+-              | Decrease font    |
| Ctrl+0              | Reset font size  |
| Ctrl+Tab / Ctrl+PgDown | Next tab      |
| Ctrl+Shift+Tab / Ctrl+PgUp | Prev tab  |

## Known Issues

- **Bloom effect disabled:** The 2-pass gaussian blur FBO pipeline triggers `GL_INVALID_FRAMEBUFFER_OPERATION (0x506)` on Mesa GLES 3.2. Root cause under investigation. Workaround: bloom rendered as self-glow (no actual blur).
- **`vte_terminal_get_window_title` deprecated:** No non-deprecated replacement in VTE GTK4 API — using the existing function is safe.
