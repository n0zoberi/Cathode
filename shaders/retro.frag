#version 300 es
precision highp float;

in vec2 v_tex;
out vec4 frag_color;

uniform sampler2D u_terminal;
uniform float     u_time;
uniform vec2      u_resolution;
uniform vec4      u_background;

uniform int   u_scanline_mode;          // 0 = gaussian beam, 1 = square wave
uniform float u_scanline_intensity;     // 0 = off
uniform float u_scanline_period;        // rows per scanline group
uniform float u_bloom_strength;         // isotropic bloom blend factor (0.3 = retro.hlsl match)
uniform float u_bloom_sigma;            // bloom blur radius
uniform float u_glow_strength;          // 0 = off
uniform float u_glow_threshold_low;
uniform float u_glow_threshold_high;
uniform float u_mask_strength;          // 0 = off, aperture grille
uniform float u_curvature;              // 0 = flat, barrel distortion
uniform float u_chromatic_aberration;   // 0 = off
uniform float u_softening;              // 0 = off, edge softening
uniform float u_color_bleed;            // 0 = off, horizontal color smear
uniform float u_rounding;               // 0 = off, pixel roundness
uniform float u_shadow_strength;        // 0 = off, depth/bezel shadows
uniform float u_vignette_strength;      // 0 = off, independent glass darkening
uniform float u_burn_in;
uniform float u_jitter;
uniform float u_flickering;
uniform float u_film_grain;            // 0 = off, phosphor coating noise
uniform float u_glowing_line;

float hash(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float luma(vec3 c)
{
    return dot(c, vec3(0.299, 0.587, 0.114));
}

vec3 srgb_to_linear(vec3 c)
{
    return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)),
               greaterThan(c, vec3(0.04045)));
}

vec3 linear_to_srgb(vec3 c)
{
    return mix(c * 12.92, 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055,
               greaterThan(c, vec3(0.0031308)));
}

vec2 barrel(vec2 uv, float k, out float dist_sq)
{
    vec2 c = uv * 2.0 - 1.0;
    dist_sq = dot(c, c);
    return uv + c * k * dist_sq;
}

void main()
{
    vec2 uv   = v_tex;
    uv.y      = 1.0 - uv.y;
    vec2 frag = gl_FragCoord.xy;
    vec2 texel = 1.0 / u_resolution;

    vec2 centered = uv * 2.0 - 1.0;
    float dist_sq = dot(centered, centered);

    // ---- Gamma: sRGB input → linear space ----
    vec3 col = srgb_to_linear(texture(u_terminal, uv).rgb);

    // ---- Screen curvature (barrel distortion) ----
    if (u_curvature > 0.0001) {
        uv = barrel(uv, u_curvature, dist_sq);
        centered = uv * 2.0 - 1.0;
        dist_sq = dot(centered, centered);
        if (uv.x < -0.04 || uv.x > 1.04 ||
            uv.y < -0.04 || uv.y > 1.04) {
            frag_color = vec4(0.0, 0.0, 0.0, 1.0);
            return;
        }
    }

    // ---- Electron beam jitter ----
    if (u_jitter > 0.0001) {
        vec2 j = vec2(hash(uv + fract(u_time * 0.1)),
                      hash(uv + fract(u_time * 0.13)));
        uv += (j - 0.5) * texel * u_jitter;
    }

    // ---- Chromatic aberration (radial RGB separation) ----
    if (u_chromatic_aberration > 0.00001) {
        float r2 = dot(centered, centered);
        if (r2 > 0.0001) {
            vec2 dir = centered * inversesqrt(r2);
            float d = u_chromatic_aberration * r2 * 2.5;
            col.r = srgb_to_linear(texture(u_terminal, uv + dir * d).rgb).r;
            col.g = srgb_to_linear(texture(u_terminal, uv).rgb).g;
            col.b = srgb_to_linear(texture(u_terminal, uv - dir * d).rgb).b;
        }
    }

    // ---- Edge softening (3x3 gaussian) ----
    if (u_softening > 0.001) {
        vec3 soft = vec3(0.0);
        float total = 0.0;
        for (int y = -1; y <= 1; y++) {
            for (int x = -1; x <= 1; x++) {
                float w = exp(-1.0 * float(x*x + y*y));
                soft += srgb_to_linear(texture(u_terminal, uv + vec2(float(x), float(y)) * texel).rgb) * w;
                total += w;
            }
        }
        col = mix(col, soft / total, u_softening);
    }

    // ---- Color bleed (horizontal luminance-gated smear) ----
    if (u_color_bleed > 0.001) {
        vec3 bleed = vec3(0.0);
        float total = 0.0;
        for (int i = -3; i <= 3; i++) {
            float t = float(i);
            float bias = 1.0 + (t > 0.0 ? 0.35 : 0.0);
            float w = exp(-0.22 * t * t) * bias;
            bleed += srgb_to_linear(texture(u_terminal, uv + vec2(t * texel.x, 0.0)).rgb) * w;
            total += w;
        }
        bleed /= total;
        float lum = luma(col);
        float weight = u_color_bleed * smoothstep(0.2, 0.7, lum);
        col = mix(col, bleed, weight);
    }

    // ---- Bloom (isotropic 2D gaussian, retro.hlsl style) ----
    if (u_bloom_strength > 0.001) {
        float sigma = max(u_bloom_sigma, 0.5);
        int radius = int(ceil(sigma * 2.5));
        radius = clamp(radius, 1, 13);

        vec3 bloom_sum = vec3(0.0);
        float total = 0.0;
        for (int y = -radius; y <= radius; y++) {
            for (int x = -radius; x <= radius; x++) {
                float w = exp(-0.5 * float(x*x + y*y) / (sigma * sigma));
                bloom_sum += srgb_to_linear(texture(u_terminal, uv + vec2(float(x), float(y)) * texel).rgb) * w;
                total += w;
            }
        }
        vec3 bloom = bloom_sum / max(total, 0.001);
        col.rgb += bloom * u_bloom_strength;
    }

    // ---- Phosphor glow (P22 warm spatial halo) ----
    if (u_glow_strength > 0.001) {
        float sigma = 1.2 + u_glow_strength * 10.0;
        float sigma_h = sigma * 1.5;
        float sigma_v = sigma * 0.6;
        int spread = int(ceil(sigma * 3.0));
        spread = clamp(spread, 1, 6);

        vec3 glow = vec3(0.0);
        float total = 0.0;

        for (int y = -spread; y <= spread; y++) {
            float wy = exp(-abs(float(y)) / sigma_v);
            for (int x = -spread; x <= spread; x++) {
                float wx = exp(-abs(float(x)) / sigma_h);
                float w = wx * wy;
                vec3 s = srgb_to_linear(texture(u_terminal, uv + vec2(float(x), float(y)) * texel).rgb);
                float slum = luma(s);
                float sgate = smoothstep(u_glow_threshold_low,
                                         u_glow_threshold_high, slum);
                vec3 phosphorWeight = vec3(1.0, 1.15, 0.85);
                glow += s * phosphorWeight * w * sgate;
                total += w;
            }
        }

        glow /= max(total, 0.001);

        float brightness = max(col.r, max(col.g, col.b));
        col.rgb += glow * u_glow_strength * 2.5 * brightness;
    }

    // ---- Pixel rounding (2D gaussian beam spot) ----
    if (u_rounding > 0.001) {
        vec2 pixel_center = floor(frag) + 0.5;
        vec2 offset = frag - pixel_center;
        float dist = length(offset * 2.0);
        float spot = exp(-2.5 * dist * dist);
        float factor = mix(1.0 - u_rounding * 0.7, 1.0, spot);
        col.rgb *= factor;
    }

    // ---- Scanlines ----
    if (u_scanline_intensity > 0.001) {
        if (u_scanline_mode == 0) {
            // Gaussian beam-spot profile
            float phase    = frag.y / u_scanline_period;
            float v        = fract(phase);
            float spread   = 0.24;
            float d        = (v - 0.5) / spread;
            float beam     = exp(-0.5 * d * d);
            float scanline = mix(1.0 - u_scanline_intensity, 1.0, beam);

            float lum    = luma(col);
            float reduce = smoothstep(0.0, 0.45, lum) * 0.8;
            col.rgb *= mix(scanline, 1.0, reduce);
        } else {
            // Square wave (retro.hlsl / Windows Terminal style)
            float wave = 1.0 - mod(floor(frag.y / u_scanline_period), 2.0) * u_scanline_intensity;
            col.rgb *= wave;
        }
    }

    // ---- Aperture grille (Trinitron RGB vertical stripe mask) ----
    if (u_mask_strength > 0.0001) {
        float ph = fract(frag.x / 3.0) * 3.0;

        float s    = 0.15;
        float r_st = smoothstep(0.0, s, ph) *
                     (1.0 - smoothstep(1.0 - s, 1.0, ph));
        float g_st = smoothstep(1.0, 1.0 + s, ph) *
                     (1.0 - smoothstep(2.0 - s, 2.0, ph));
        float b_st = smoothstep(2.0, 2.0 + s, ph) *
                     (1.0 - smoothstep(3.0 - s, 3.0, ph));

        float mask  = mix(1.0, 0.55, u_mask_strength * 2.5);
        col.r *= mix(1.0, mask, 1.0 - r_st);
        col.g *= mix(1.0, mask, 1.0 - g_st);
        col.b *= mix(1.0, mask, 1.0 - b_st);
    }

    // ---- Vignette (independent glass-depth darkening) ----
    if (u_vignette_strength > 0.001) {
        float v = clamp(1.0 - dist_sq * 0.35, 0.72, 1.0);
        v = pow(v, 1.35);
        col.rgb *= mix(1.0, v, u_vignette_strength);
    }

    // ---- Depth shadows ----
    if (u_shadow_strength > 0.001) {
        float corner = dist_sq;

        float bezel = smoothstep(0.55, 0.92, corner);
        col.rgb *= (1.0 - bezel * u_shadow_strength * 0.55);

        float edge_x = 1.0 - centered.x * centered.x;
        float edge_y = 1.0 - centered.y * centered.y;
        float inner = smoothstep(0.08, 0.4, min(edge_x, edge_y));
        col.rgb *= (1.0 - (1.0 - inner) * u_shadow_strength * 0.25);

        float edge_grad = smoothstep(0.85, 1.0, corner);
        col.rgb *= (1.0 - edge_grad * u_shadow_strength * 0.15);
    }

    // ---- Glowing line ----
    if (u_glowing_line > 0.001) {
        float line_pos = fract(u_time * 0.08);
        float line = smoothstep(0.008, 0.0, abs(uv.y - line_pos)) * u_glowing_line;
        col.rgb += line;
    }

    // ---- Film grain (phosphor coating irregularity noise) ----
    if (u_film_grain > 0.001) {
        float n1 = hash(frag + vec2(0.0,      u_time * 23.0)) * 0.035;
        float n2 = hash(frag * 0.6 + vec2(u_time * 17.0, 0.0)) * 0.025;
        float n3 = hash(frag * 0.3 + vec2(u_time * 11.0, u_time * 13.0)) * 0.015;
        float grain = n1 + n2 + n3 - 0.0375;

        float lum      = luma(col);
        float strength = (1.0 - lum) * 0.35 + dist_sq * 0.18;
        col.rgb += grain * strength * u_film_grain;
    }

    // ---- Flickering ----
    if (u_flickering > 0.001) {
        float flicker = 1.0 + (hash(vec2(u_time * 0.05, u_time * 0.07)) - 0.5) * u_flickering;
        col.rgb *= clamp(flicker, 0.0, 2.0);
    }

    // ---- Gamma: linear → sRGB output ----
    col = linear_to_srgb(col);

    // ---- CRT warm white point (~6500 K) ----
    col.rgb *= vec3(1.025, 1.0, 0.95);

    frag_color = vec4(col, 1.0);
}
