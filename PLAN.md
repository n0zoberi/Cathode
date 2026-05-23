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
- Single-pass rendering (bloom FBOs disabled — see issues)

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

## Open Issues

| Issue | Status | Notes |
|-------|--------|-------|
| Bloom FBO GL_INVALID_FRAMEBUFFER_OPERATION | Disabled | 2-pass blur FBOs trigger 0x506 on Mesa GLES 3.2. Kept single-pass CRT without bloom blur. |
| `vte_terminal_get_window_title` deprecated | Tolerated | No non-deprecated replacement in VTE GTK4. |
| FBO investigation for bloom | TODO | Try explicit sized format (GL_RGBA8), separate DRAW/READ framebuffer bindings, or non-FBO blur (merged into retro.frag) |
