# PLAN.md — Cathode Development Plan

## Phase 1: Skeleton & Terminal ✅

- GTK4 application with GtkApplication
- VteTerminal spawning user shell
- Monospace font, scrollback, basic env
- Clean meson build system

## Phase 2: Config & Theme ✅

- TOML parsing with vendored tomlc99
- Load `~/.config/cathode/cathode.toml`
- Auto-create defaults if missing
- `[general].import` for theme files
- Apply `[theme]`, `[font]`, `[shell]`, `[crt]`, `[terminal]` settings
- Support both `[colors.primary]` and `[theme]` formats

## Phase 3: Shader Integration ✅

- GtkGLArea overlay on top of VteTerminal
- CRT fragment shader (`retro.frag`) from Windows Terminal reference
- Cairo-based terminal capture pipeline
- GL texture upload with hardware swizzle (GL_RGBA + GL_TEXTURE_SWIZZLE)
- HiDPI scale factor handling
- Single-pass rendering

## Phase 4: Tabs ✅

- AdwTabView + AdwTabBar
- Ctrl+Shift+T new tab, Ctrl+Shift+W close
- Ctrl+PageUp/Down, Ctrl+Tab switching
- Terminal title → tab label auto-update
- Window title follows active terminal

## Phase 5: Search ✅

- Ctrl+Shift+F search bar
- VteRegex integration with live highlighting
- Enter/Shift+Enter next/previous navigation
- Escape to close
- Search re-bound on tab switch

## Phase 6: Window Management ✅

- Ctrl+Shift+C/V copy/paste with Ctrl+Alt fallback
- Ctrl+=/-, Ctrl+0 font size control
- Multi-tab close confirmation dialog (AdwAlertDialog)
- Font re-apply across all tabs

## Phase 7: Packaging ✅

- Desktop entry file
- Icon and sample config install rules
- meson install support

## Phase 8: Title Bar ✅

- AdwHeaderBar with title and new-tab button
- Window title follows terminal `window-title-changed`
- Flat-style new-tab button (left-aligned, no frame)

## Phase 9: CRT Shader Overhaul ✅

- **Inline bloom** — 2D gaussian kernel sampled from terminal texture
  - `bloom_strength` controls global screen brightness uniformly across the entire frame
  - Adjustable spread via `bloom_sigma`
- **Edge softening** — 3×3 gaussian softening of pixel edges
- **Color bleed** — asymmetric horizontal luminance-dependent color smearing
- **Pixel rounding** — 2D gaussian beam spot simulating circular CRT beams
- **Depth shadows** — bezel shadow + inner depth darkening at screen edges
- Removed `program_blur`, `fbo_blur_h/v`, `tex_blur_h/v`, `blur.frag` from resources
- Simplified `render_cb` to single-pass: capture → retro shader
- All 5 new effects configurable via `[crt]` TOML section

## Phase 10: Config Auto-Reload ✅

- `GFileMonitor` watching `~/.config/cathode/cathode.toml`
- 500ms debounce to avoid rapid re-parse
- `auto_reload` toggle in `[general]` (default `true`)
- On change: re-parse file, re-apply terminal settings and CRT params to all tabs
- GLArea visibility refreshed based on active effects
- Monitor cleanup on application exit

## Debugging & Fixes ✅

- Fixed GL_BGRA invalid in GLES 3.0 → GL_RGBA + texture swizzle
- Fixed Cairo stride alignment → `glPixelStorei(GL_UNPACK_ROW_LENGTH)`
- Fixed GLES-only API restriction → allow GL|GLES
- Fixed HiDPI 2× scaling → `gtk_widget_get_scale_factor()` in render_cb
- Fixed NULL terminal crash on tab switch
- Fixed close-request infinite loop with `closing_confirmed` flag
- Fixed `g_list_remove` corrupting GTK internal list
- Fixed palette API misuse → `vte_terminal_set_colors()`
- Fixed `AdwApplicationWindow` → `adw_application_window_set_content()`
- Fixed config_path memory leak
- Added GL error diagnostics (`glCheckFramebufferStatus`, `glGetError`)
- Fixed bloom FBO → replaced with inline single-pass bloom → global brightness boost

## Phase 11: CRT Effects Refinement ✅

- **Burn-in fix** — Replaced broken spatial-ghost + global-time-decay with CPU-side frame accumulation:
  - Persistent `accum_buffer` holds blended frames with exponential decay
  - `max(accum * decay, current)` phosphor model: bright content lingers after disappearing
  - Half-life mapped from `burn_in` config: `half_life = burn_in * 2.0`
  - Removed incorrect `u_time`-based global decay from shader
  - Added `-lm` link dependency for `expf`
- **Phosphor glow** — Rewrote from per-pixel brightness boost to spatial gaussian halo:
  - Samples N×N kernel around each pixel, luminance-gated by thresholds
  - Kernel width scales with `glow_strength` (higher = wider + brighter)
  - Warm P22 tint applied to the spread glow
- **Jitter** — Sub-pixel UV displacement via hash noise, simulates electron beam instability
- **Flickering** — Global brightness modulation via noise, simulates PSU ripple
- **Glowing line** — Slowly scrolling horizontal bright scanline overlay
- **All effects driven by cool-retro-term inspiration** (see README acknowledgments)

## Open Issues

| Issue | Status | Notes |
|-------|--------|-------|
| `vte_terminal_get_window_title` deprecated | Tolerated | No non-deprecated replacement in VTE GTK4. |
| Theme import `{theme}` placeholder | TODO | Support variable substitution in import paths for quick theme switching. |
| Möbius/3D warp mode | TODO | Stretch goal — configurable CRT geometry transforms. |

## Future

- Command palette (Ctrl+Shift+P)
- Drag-and-drop tabs between windows
- Split panes
- Session restore
- Drop-down / quake mode
- Color scheme import (Xresources, iTerm2)
