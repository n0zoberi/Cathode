# CRT Rendering Performance Baseline

Record the fixed test machine and capture all numbers before/after the M2
changes with the same window size and shell output.

## Test environment

| Item | Value |
|---|---|
| CPU | TBD |
| GPU | TBD |
| Driver | TBD |
| Session | TBD (Wayland/X11, GDK_SCALE) |
| Window size | 900x600 |
| Terminal shell | TBD |
| Config | `cathode.sample.toml` defaults |

## Measurements

| Metric | CRT off | CRT on (before) | CRT on (after) | Target |
|---|---|---|---|---|
| Idle CPU, 60 s average (%) | TBD | TBD | TBD | ~0 (same as off) |
| GPU frame time, default config (ms) | n/a | TBD | TBD | discrete < 8, integrated < 16 |
| Key echo latency (ms) | TBD | TBD | TBD | <= CRT-off + 2 ms |
| Frame rate while `cat` of a large file (fps) | TBD | TBD | TBD | no stutter |

## Method

- CPU: `top` / `pidstat` over 60 s with no terminal output.
- GPU: `GL_TIME_ELAPSED` timer query or RenderDoc frame capture.
- Echo latency: high-speed camera or input timestamps with `wev`.
- Throughput: `cat` a large file and record the reported frame rate.
- Visual equivalence: before/after screenshots, pixel diff <= 2/255; bloom
  downsampling edges confirmed by QA.

> These values must be collected on the target hardware; this repository copy
> was prepared in a non-Linux environment where the GL pipeline cannot run.
