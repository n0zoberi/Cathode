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
- **Import syntax:** `[general].import` can load theme files
- **Default theme:** `theme.toml` (installed to `share/cathode/`)
- All CRT parameters set to `0` → GLArea hidden, zero shader overhead

## Project Structure

```
src/
  main.c              — entry point
  app.c/h             — GtkApplication, actions, accelerators
  tab.c/h             — AdwTabView + AdwTabBar, tab lifecycle
  terminal.c/h        — VteTerminal setup, spawn, config apply
  shader.c/h          — GtkGLArea overlay, CRT shader pipeline, multi-pass FBO
  search.c/h          — Ctrl+Shift+F search bar, VteRegex integration
  config.c/h          — TOML parsing (tomlc99), defaults, theme merge
  vendor/tomlc99/     — vendored tomlc99 (MIT)
shaders/              — GLSL shaders (embedded via gresource)
data/                 — .desktop file
```

## Architecture Notes

- **Rendering:** GtkOverlay with VteTerminal underneath and GtkGLArea on top.
  Terminal is snapshotted via `gtk_widget_snapshot_child()` + `gsk_render_node_draw()`
  to a Cairo surface, uploaded to GL, then processed through the CRT shader pipeline.
- **Shader pipeline:** 2-pass separable Gaussian blur (`blur.frag`) for bloom,
  then CRT composite (`retro.vert` + `retro.frag`). All uniforms from config.
- **Tabs:** `AdwTabView` with `AdwTabBar`. Built-in Ctrl+PageUp/Down/Tab switching.
- **Search:** `GtkSearchBar` in `AdwToolbarView`. On tab switch, search is re-bound
  to the new terminal.
- **Config:** Loaded once at startup. `cathode_tab_reapply_font()` for runtime font size changes.

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
