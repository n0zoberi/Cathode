# Cathode — CRT Terminal Emulator

A GTK4/libadwaita terminal emulator with a retro CRT scanline shader, tabs, search, and TOML configuration.

## Features

- **CRT retro effect** — scanlines, phosphor glow, aperture grille, barrel curvature, chromatic aberration, vignetting, film grain
- **Multiple tabs** — AdwTabView + AdwTabBar with keyboard shortcuts
- **Search** — Ctrl+Shift+F with VTE regex, Enter/Shift+Enter navigation
- **TOML config** — themes, fonts, shell, CRT parameters all configurable
- **Header bar** — window title follows terminal, new-tab button

## Screenshot

*CRT effects active: scanlines + phosphor glow + aperture grille on a green-on-dark theme*

## Build

```bash
# Dependencies
sudo pacman -S gtk4 libadwaita vte3 epoxy cairo meson

# Build
meson setup build
meson compile -C build
./build/src/cathode

# Install (optional)
meson install -C build
```

## Config (~/.config/cathode/cathode.toml)

```toml
[general]
import = ["~/.config/cathode/themes/dracula.toml"]

[terminal]
scrollback = 2000
cursor_blink = "on"

[shell]
program = "/bin/zsh"

[font]
family = "monospace"
size = 11

[crt]
scanline_intensity = 0.06
scanline_period = 2.0
bloom_strength = 0.12
bloom_sigma = 3.0
glow_strength = 0.06
glow_threshold_low = 0.15
glow_threshold_high = 0.6
mask_strength = 0.012
curvature = 0.0
chromatic_aberration = 0.0
```

### CRT Parameters

| Key | Default | Description |
|-----|---------|-------------|
| `scanline_intensity` | `0.06` | Scanline darkness (0=off, 1=black) |
| `scanline_period` | `2.0` | Pixel rows per scanline group |
| `bloom_strength` | `0.12` | Light scattering intensity |
| `bloom_sigma` | `3.0` | Bloom blur radius (pixels) |
| `glow_strength` | `0.06` | Phosphor glow on bright text |
| `glow_threshold_low` | `0.15` | Min luma for glow effect |
| `glow_threshold_high` | `0.6` | Max luma for full glow |
| `mask_strength` | `0.012` | Aperture grille stripe visibility |
| `curvature` | `0.0` | Barrel distortion (0.01–0.08 typical) |
| `chromatic_aberration` | `0.0` | RGB separation at edges |

Set any value to `0` to disable that effect. All CRT params at `0` → GLArea hidden, zero overhead.

### Theme Format

Supports two formats via `[general].import`:

```toml
# Format 1: Flat section (main config)
[theme]
foreground = "#426644"
background = "#0f191c"
cursor = "#384545"

[theme.normal]
color0 = "#0f191c"
# ... color1–color7

[theme.bright]
color8 = "#688060"
# ... color9–color15

# Format 2: Nested section (theme.toml)
[colors.primary]
foreground = "#426644"
background = "#0f191c"
cursor = "#384545"

[colors.normal]
color0 = "#0f191c"
# ...
[colors.bright]
color8 = "#688060"
# ...
```

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+Shift+T | New tab |
| Ctrl+Shift+W | Close tab |
| Ctrl+Shift+F | Toggle search |
| Ctrl+Shift+C / Ctrl+Alt+C | Copy |
| Ctrl+Shift+V / Ctrl+Alt+V | Paste |
| Ctrl+= / Ctrl++ | Increase font |
| Ctrl+- | Decrease font |
| Ctrl+0 | Reset font size |
| Ctrl+Tab / Ctrl+PgDown | Next tab |
| Ctrl+Shift+Tab / Ctrl+PgUp | Prev tab |

## Mouse

- Left click → focus terminal
- Middle click → paste primary selection
- Scroll → scrollback history

## Architecture

```
GtkOverlay
  ├── VteTerminal (base child)
  └── GtkGLArea (CRT shader overlay)
```

Terminal is snapshotted via GTK's scene graph to a Cairo surface, uploaded to an OpenGL
texture, then processed by the CRT fragment shader. GLArea is transparent when all CRT
effects are disabled.

## License

MIT. CRT shader ported from [Windows Terminal](https://github.com/microsoft/terminal) (MIT).
TOML parser: [tomlc99](https://github.com/cktan/tomlc99) (MIT).
