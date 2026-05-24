# Cathode — CRT Terminal Emulator

A GTK4/libadwaita terminal emulator with a retro CRT scanline shader, tabs, search, and TOML configuration.

## Features

- **CRT retro effect pipeline** — 12 configurable effects:
  - Scanlines, phosphor glow, inline bloom, aperture grille, edge softening, color bleed
  - Pixel rounding, depth shadows, burn-in, jitter, flickering, glowing line
  - Curvature + chromatic aberration + vignetting + film grain + warm white point
- **Multiple tabs** — AdwTabView + AdwTabBar with keyboard shortcuts
- **Search** — Ctrl+Shift+F with VTE regex, Enter/Shift+Enter navigation
- **Header bar menu** — copy, paste, search, new/close/rename tab, clear screen, reset terminal, open config, quit
- **Config auto-reload** — GFileMonitor watches `cathode.toml`, applies changes on save
- **TOML config** — themes, fonts, shell, CRT parameters all configurable with sensible defaults
- **i18n / gettext** — Chinese (Simplified) translation built-in, English source

## Build

### Dependencies

- gtk4 >= 4.12
- libadwaita-1 >= 1.4
- vte-2.91-gtk4 >= 0.74
- epoxy >= 1.5
- cairo >= 1.16
- glib-2.0 >= 2.76
- meson >= 1.0.0

### Compile & Run

```bash
meson setup build
meson compile -C build
./build/src/cathode
```

### Install

```bash
meson install -C build
```

Installs:
- Binary → `$prefix/bin/cathode`
- Desktop entry → `$prefix/share/applications/com.n0zoberi.Cathode.desktop`
- Icons → `$prefix/share/icons/hicolor/` (16×16 to 512×512)
- Sample config + theme → `$prefix/share/cathode/`

### Arch Linux (PKGBUILD)

```bash
cd dist/arch
makepkg -si
```

## Config (`~/.config/cathode/cathode.toml`)

```toml
[general]
import = ["~/.config/cathode/themes/dracula.toml"]
auto_reload = true

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
bloom_strength = 0.05
glow_strength = 0.2
mask_strength = 0.012
```

### CRT Parameters

| Key | Default | Description |
|---|---|---|
| `scanline_intensity` | `0.06` | Scanline darkness (0=off, 1=black) |
| `scanline_period` | `6` | Pixel rows per scanline group |
| `bloom_strength` | `0.05` | Global screen brightness boost |
| `bloom_sigma` | `4.5` | Bloom blur radius |
| `glow_strength` | `0.2` | Phosphor glow on bright text |
| `glow_threshold_low` | `0.15` | Min luma for glow effect |
| `glow_threshold_high` | `0.6` | Luma threshold for full glow |
| `mask_strength` | `0.012` | Aperture grille stripe visibility |
| `curvature` | `0.0` | Barrel distortion |
| `chromatic_aberration` | `0.0` | RGB separation at edges |
| `softening` | `0.12` | Edge softening |
| `color_bleed` | `0.08` | Horizontal color smear |
| `rounding` | `0.15` | Pixel roundness |
| `shadow_strength` | `0.10` | Bezel + inner depth shadow |
| `burn_in` | `0.0` | Phosphor persistence |
| `jitter` | `0.0` | Electron beam jitter |
| `flickering` | `0.0` | Brightness ripple |
| `glowing_line` | `0.0` | Scrolling bright scanline |

Set any value to `0` to disable that effect. All CRT params at `0` → GLArea hidden, zero overhead.

Auto-reload: edit and save `cathode.toml` — changes apply immediately. Set `auto_reload = false` to require restart.

### Theme Format

Supports two formats via `[general].import`:

```toml
# Format 1: Flat section (main config)
[theme]
foreground = "#426644"
background = "#0f191c"

[theme.normal]
color0 = "#0f191c"
# ...

# Format 2: Nested section (separate theme.toml)
[colors.primary]
foreground = "#426644"
background = "#0f191c"

[colors.normal]
color0 = "#0f191c"
```

## Keyboard Shortcuts

| Shortcut | Action |
|---|---|
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

## i18n / Localization

Source strings are in English, wrapped with `_()` for gettext. Chinese (Simplified) translation is built-in.

```bash
# Test with Chinese locale (without install)
msgfmt po/zh_CN.po -o cathode.mo
mkdir -p ~/.local/share/locale/zh_CN/LC_MESSAGES
cp cathode.mo ~/.local/share/locale/zh_CN/LC_MESSAGES/cathode.mo
LANGUAGE=zh_CN ./build/src/cathode
```

See `AGENTS.md` for the full i18n workflow.

## Architecture

```
GtkOverlay
  ├── VteTerminal (base child)
  └── GtkGLArea (CRT shader overlay)
```

The terminal is snapshotted via GTK's scene graph to a Cairo surface, uploaded to an OpenGL texture, then processed by a single-pass CRT fragment shader. GLArea is transparent when all CRT effects are disabled.

## License

MIT. TOML parser: [tomlc99](https://github.com/cktan/tomlc99) (MIT).

## Acknowledgments

Several CRT effect concepts (burn-in, jitter, flickering, glowing line) were inspired by [cool-retro-term](https://github.com/Swordfish90/cool-retro-term) (GPL-3.0). All implementation is original to this project.
