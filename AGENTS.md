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
- **Auto-reload:** `[general].auto_reload = true` watches via GFileMonitor, re-applies on save
- **Default theme:** `theme.toml` (installed to `share/cathode/`)
- All CRT parameters set to `0` → GLArea hidden, zero shader overhead

## Project Structure

```
src/
  main.c              — entry point
  app.c/h             — GtkApplication, actions, accelerators, config file monitor
  tab.c/h             — AdwTabView + AdwTabBar, tab lifecycle, config re-apply
  terminal.c/h        — VteTerminal setup, spawn, config apply
  shader.c/h          — GtkGLArea overlay, CRT shader pipeline, visibility control
  search.c/h          — Ctrl+Shift+F search bar, VteRegex integration
  config.c/h          — TOML parsing (tomlc99), defaults, theme merge, auto-reload
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
3. **CRT Shader:** Single-pass `retro.frag` with all effects inline:
   - Curvature (barrel distortion)
   - Chromatic aberration (RGB convergence error)
   - Terminal sampling
   - Edge softening (3×3 gaussian)
   - Color bleed (asymmetric horizontal smear)
   - Scanlines (gaussian beam-spot profile)
   - Phosphor glow (P22 warm tone)
   - Inline bloom (gaussian kernel, luminance-gated, no FBO)
   - Aperture grille (RGB vertical stripe mask)
   - Vignetting (glass-depth darkening)
   - Pixel rounding (2D beam spot)
   - Depth shadows (bezel + inner shadow)
   - Film grain (3-octave hash noise)
   - Warm white point (~6500K)

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
- **Inline bloom (no FBO)** — avoids GLES framebuffer issues; bloom computed directly in `retro.frag` by sampling the terminal texture with a gaussian kernel
- **Tab lifecycle** — `child-exited` auto-closes tab; `destroy` signal nulls terminal pointer
- **Window close confirm** — AdwAlertDialog when multiple tabs open, `closing_confirmed` flag prevents infinite loop
- **Config file monitor** — `GFileMonitor` + 500ms debounce watches `cathode.toml`; changes re-parse and re-apply to all tabs without restart

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

## Shader Uniforms

| GLSL uniform | Config field | Type | Default |
|---|---|---|---|
| `u_scanline_intensity` | `scanline_intensity` | float | 0.06 |
| `u_scanline_period` | `scanline_period` | float | 2.0 |
| `u_bloom_strength` | `bloom_strength` | float | 0.20 |
| `u_bloom_sigma` | `bloom_sigma` | float | 4.5 |
| `u_glow_strength` | `glow_strength` | float | 0.13 |
| `u_glow_threshold_low` | `glow_threshold_low` | float | 0.15 |
| `u_glow_threshold_high` | `glow_threshold_high` | float | 0.6 |
| `u_mask_strength` | `mask_strength` | float | 0.012 |
| `u_curvature` | `curvature` | float | 0.0 |
| `u_chromatic_aberration` | `chromatic_aberration` | float | 0.0 |
| `u_softening` | `softening` | float | 0.12 |
| `u_color_bleed` | `color_bleed` | float | 0.08 |
| `u_rounding` | `rounding` | float | 0.15 |
| `u_shadow_strength` | `shadow_strength` | float | 0.10 |
| `u_burn_in` | `burn_in` | float | 0.0 |

## CRT Effect Defaults

See `cathode.sample.toml` for full documentation. Effects enabled by default:

| Effect | Uniform | Default | Description |
|---|---|---|---|
| Scanlines | `u_scanline_intensity` | 0.06 | Gaussian beam-spot profile |
| Inline bloom | `u_bloom_strength` | 0.20 | Luminance-gated gaussian glow |
| Phosphor glow | `u_glow_strength` | 0.13 | P22 warm tone emphasis |
| Aperture grille | `u_mask_strength` | 0.012 | RGB vertical stripe mask |
| Edge softening | `u_softening` | 0.12 | 3x3 gaussian edge blur |
| Color bleed | `u_color_bleed` | 0.08 | Horizontal phosphor trail |
| Pixel rounding | `u_rounding` | 0.15 | 2D circular beam spot |
| Depth shadows | `u_shadow_strength` | 0.10 | Bezel + inner shadow |
| Burn-in | `u_burn_in` | 0.0 | Phosphor persistence ghosting |

## Git Commits

When making changes to the codebase, use conventional commit prefixes:

| Prefix | Use for |
|---|---|
| `feat:` | New features (effects, config options, monitor) |
| `fix:` | Bug fixes |
| `refactor:` | Code restructuring without behavior change |
| `shader:` | GLSL shader changes |
| `docs:` | README, PLAN, AGENTS, sample.toml comments |
| `build:` | Meson, dependencies, resource files |

Example: `shader: replace FBO bloom with inline single-pass gaussian`

Keep commits **atomic** — one logical change per commit. Write messages in English, present tense, imperative mood.

## Known Issues

- **`vte_terminal_get_window_title` deprecated:** No non-deprecated replacement in VTE GTK4 API — using the existing function is safe.
