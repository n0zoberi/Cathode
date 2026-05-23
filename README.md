# Cathode — CRT Terminal Emulator

A GTK4/libadwaita terminal emulator with a retro CRT scanline shader, tabs, search, and TOML configuration.
Use gtk4, libadwaita, vte, and meson
File: `~/.config/cathode/cathode.toml` — see `cathode.sample.toml` for reference.

### `[crt]`

Retro CRT effect parameters. Set to `0` to disable an effect.
Based on [Windows Terminal Retro.hlsl](https://github.com/microsoft/terminal) with gaussian scanlines, beam blooming, aperture grille, curvature, chromatic aberration, and phosphor glow.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `scanline_intensity` | float | `0.06` | Scanline darkness (0–1) |
| `scanline_period` | float | `2.0` | Pixel rows per scanline group |
| `bloom_strength` | float | `0.12` | Light scattering intensity |
| `bloom_sigma` | float | `3.0` | Bloom blur radius |
| `glow_strength` | float | `0.06` | Phosphor glow on bright text |
| `glow_threshold_low` | float | `0.15` | Min luma for glow |
| `glow_threshold_high` | float | `0.6` | Max luma for glow |
| `mask_strength` | float | `0.012` | Aperture grille visibility |
| `curvature` | float | `0.0` | Barrel distortion (0.01–0.08 typical) |
| `chromatic_aberration` | float | `0.0` | RGB separation at edges (0.0005–0.002) |

### `[theme]`

| Key | Type | Description |
|-----|------|-------------|
| `foreground` | string | Text color |
| `background` | string | Terminal + header color |
| `cursor` | string | Cursor color |

`[theme.normal]` / `[theme.bright]` — 8-color palette each. Keys: `color0`–`color15` or `black`/`red`/`green`/`yellow`/`blue`/`magenta`/`cyan`/`white`. `colorN` takes priority.

## Mouse

- **Left click:** Focus | **Middle click:** Paste primary | **Scroll:** Scrollback

## Search

`Ctrl+Shift+F` to open. `Enter`/`Shift+Enter` next/previous. `Escape` to close.

## License

MIT. CRT shader ported from [Windows Terminal](https://github.com/microsoft/terminal) (MIT). TOML parser from [tomlc99](https://github.com/cktan/tomlc99) (MIT).
