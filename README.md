# Cathode — CRT Terminal Emulator

A GTK4/libadwaita terminal emulator with a retro CRT scanline shader, tabs, search, and TOML configuration.

## Features

- **CRT retro effect pipeline** — 12 configurable effects with uniform control:
  - Scanlines (gaussian beam-spot profile)
  - Phosphor glow (P22 warm tone, blue ZnS:Ag emphasis)
  - Inline bloom (gaussian glow, no FBO overhead)
  - Aperture grille (RGB vertical stripe mask)
  - Edge softening (sub-pixel gaussian)
  - Color bleed (horizontal phosphor trail)
  - Pixel rounding (2D circular beam spot)
  - Depth shadows (bezel + inner shadow)
  - Burn-in (phosphor persistence with temporal accumulation)
  - Jitter (electron beam analog instability)
  - Flickering (power supply ripple)
  - Glowing line (horizontal scanline bleed)
  - Curvature + chromatic aberration + vignetting + film grain + warm white point
- **Multiple tabs** — AdwTabView + AdwTabBar with keyboard shortcuts
- **Search** — Ctrl+Shift+F with VTE regex, Enter/Shift+Enter navigation
- **Config auto-reload** — GFileMonitor watches `cathode.toml`, applies changes on save
- **TOML config** — themes, fonts, shell, CRT parameters all configurable with sensible defaults
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
auto_reload = true       # watch config file for changes

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
scanline_period = 6
bloom_strength = 0.12
bloom_sigma = 2.5
glow_strength = 0.06
glow_threshold_low = 0.15
glow_threshold_high = 0.6
mask_strength = 0.012
curvature = 0.0
chromatic_aberration = 0.0
softening = 0.12
color_bleed = 0.08
rounding = 0.15
shadow_strength = 0.10
```

### CRT Parameters

| Key | Default | Description |
|-----|---------|-------------|
| `scanline_intensity` | `0.06` | Scanline darkness (0=off, 1=black) |
| `scanline_period` | `6` | Pixel rows per scanline group |
| `bloom_strength` | `0.12` | Bloom glow intensity around bright content |
| `bloom_sigma` | `2.5` | Bloom blur radius (kernel spread) |
| `glow_strength` | `0.06` | Phosphor glow on bright text |
| `glow_threshold_low` | `0.15` | Min luma for glow effect |
| `glow_threshold_high` | `0.6` | Luma threshold for full glow |
| `mask_strength` | `0.012` | Aperture grille stripe visibility |
| `curvature` | `0.0` | Barrel distortion (0.01–0.08 typical) |
| `chromatic_aberration` | `0.0` | RGB separation at edges |
| `softening` | `0.12` | Edge softening (sub-pixel gaussian) |
| `color_bleed` | `0.08` | Horizontal color smear (phosphor trail) |
| `rounding` | `0.15` | Pixel roundness (2D beam spot) |
| `shadow_strength` | `0.10` | Bezel + inner depth shadow |
| `burn_in` | `0.0` | Phosphor persistence (afterimage trail, 0.0–0.2) |
| `jitter` | `0.0` | Electron beam jitter (analog instability, 0–0.01) |
| `flickering` | `0.0` | Power supply ripple brightness modulation (0–0.3) |
| `glowing_line` | `0.0` | Scrolling bright horizontal scanline (0–0.5) |

Set any value to `0` to disable that effect. All CRT params at `0` → GLArea hidden, zero overhead.

**Auto-reload:** Edit and save `cathode.toml` — changes apply immediately to all open tabs.
Set `auto_reload = false` in `[general]` to require a restart.

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
texture, then processed by a single-pass CRT fragment shader with 14 configurable effects.
GLArea is transparent when all CRT effects are disabled.

Bloom uses an inline gaussian kernel (no FBO) sampling the terminal texture directly,
avoiding GLES framebuffer compatibility issues.

Config is monitored via GFileMonitor — save `cathode.toml` and settings re-apply immediately.

## License

MIT. CRT shader based on CRT-Lottes approach (inline bloom, gaussian beam scanlines).
TOML parser: [tomlc99](https://github.com/cktan/tomlc99) (MIT).

## Acknowledgments

Several CRT effect concepts (burn-in persistence, jitter, flickering, glowing line,
horizontal sync distortion, and rasterization variants) were inspired by
[cool-retro-term](https://github.com/Swordfish90/cool-retro-term) by Filippo Scognamiglio,
licensed under GPL-3.0. All implementation is original to this project.
