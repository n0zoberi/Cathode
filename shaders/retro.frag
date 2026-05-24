// Cathode CRT simulation — physically-inspired retro terminal shader
// Models: Trinitron aperture grille, gaussian beam scanlines,
//          P22 phosphor glow, inline bloom, barrel curvature,
//          chromatic convergence error, edge softening, color bleed,
//          pixel rounding, depth shadows, vignetting, and film grain.
#version 300 es
precision highp float;

in vec2 v_tex;
out vec4 frag_color;

uniform sampler2D u_terminal;
uniform float     u_time;
uniform vec2      u_resolution;
uniform vec4      u_background;

uniform float u_scanline_intensity;   // 0 = off, 0.06 = default
uniform float u_scanline_period;      // rows per scanline group, default 2.0
uniform float u_bloom_strength;       // 0 = off, 0.12 = default
uniform float u_bloom_sigma;          // bloom blur radius, default 2.5
uniform float u_glow_strength;        // 0 = off, 0.06 = default
uniform float u_glow_threshold_low;   // luma below this = no glow, default 0.15
uniform float u_glow_threshold_high;  // luma above this = full glow, default 0.6
uniform float u_mask_strength;        // 0 = off, 0.005~0.03 typical
uniform float u_curvature;            // 0 = flat, 0.03 = typical CRT bulge
uniform float u_chromatic_aberration; // 0 = off, 0.0005~0.002 typical
uniform float u_softening;            // 0 = off, edge softening, default 0.12
uniform float u_color_bleed;          // 0 = off, horizontal color smear, default 0.08
uniform float u_rounding;             // 0 = off, pixel roundness, default 0.15
uniform float u_shadow_strength;      // 0 = off, depth/bezel shadows, default 0.10
uniform float u_burn_in;              // 0 = off, phosphor persistence trail, default 0.0

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

vec2 barrel(vec2 uv, float k, out float dist_sq)
{
    vec2 c = uv * 2.0 - 1.0;
    dist_sq = dot(c, c);
    return uv + c * k * dist_sq;
}

void main()
{
    vec2 uv   = v_tex;
    uv.y      = 1.0 - uv.y; // Cairo→GL Y-axis flip
    vec2 frag = gl_FragCoord.xy;
    vec2 texel = 1.0 / u_resolution;

    float dist_sq = 0.0;

    // ---- Screen curvature (barrel distortion) ----
    if (u_curvature > 0.0001) {
        uv = barrel(uv, u_curvature, dist_sq);
        if (uv.x < -0.04 || uv.x > 1.04 ||
            uv.y < -0.04 || uv.y > 1.04) {
            frag_color = vec4(0.0, 0.0, 0.0, 1.0);
            return;
        }
    }

    // ---- Terminal sampling + chromatic aberration ----
    vec3 col;
    float aberration_dist = dot(uv * 2.0 - 1.0, uv * 2.0 - 1.0);

    if (u_chromatic_aberration > 0.00001 && aberration_dist > 0.0001) {
        vec2 c   = uv * 2.0 - 1.0;
        vec2 dir = normalize(c + vec2(0.0001));
        float d  = u_chromatic_aberration * aberration_dist * 2.5;
        col.r = texture(u_terminal, uv + dir * d).r;
        col.g = texture(u_terminal, uv).g;
        col.b = texture(u_terminal, uv - dir * d).b;
    } else {
        col = texture(u_terminal, uv).rgb;
    }

    // ---- Edge softening (subtle 3×3 gaussian, softens pixel edges) ----
    if (u_softening > 0.001) {
        vec3 soft = vec3(0.0);
        float total = 0.0;
        for (int y = -1; y <= 1; y++) {
            for (int x = -1; x <= 1; x++) {
                float w = exp(-1.0 * float(x*x + y*y));
                soft += texture(u_terminal, uv + vec2(float(x), float(y)) * texel).rgb * w;
                total += w;
            }
        }
        col = mix(col, soft / total, u_softening);
    }

    // ---- Color bleed (horizontal luminance-dependent smearing, rightward bias) ----
    if (u_color_bleed > 0.001) {
        vec3 bleed = vec3(0.0);
        float total = 0.0;
        for (int i = -3; i <= 3; i++) {
            float t = float(i);
            float bias = 1.0 + (t > 0.0 ? 0.35 : 0.0);
            float w = exp(-0.22 * t * t) * bias;
            bleed += texture(u_terminal, uv + vec2(t * texel.x, 0.0)).rgb * w;
            total += w;
        }
        bleed /= total;
        float lum = luma(col);
        float weight = u_color_bleed * smoothstep(0.2, 0.7, lum);
        col = mix(col, bleed, weight);
    }

    // ---- Scanlines (gaussian beam-spot profile) ----
    float glow_amount = 0.0;
    if (u_scanline_intensity > 0.001) {
        float phase    = frag.y / u_scanline_period;
        float v        = fract(phase);
        float spread   = 0.24;
        float d        = (v - 0.5) / spread;
        float beam     = exp(-0.5 * d * d);
        float scanline = mix(1.0 - u_scanline_intensity, 1.0, beam);

        float lum    = luma(col);
        float reduce = smoothstep(0.0, 0.45, lum) * 0.8;
        col.rgb *= mix(scanline, 1.0, reduce);
    }

    // ---- Phosphor glow (P22 warm tone, blue ZnS:Ag emphasis) ----
    if (u_glow_strength > 0.001) {
        float lum  = luma(col);
        float glow = smoothstep(u_glow_threshold_low,
                                 u_glow_threshold_high, lum);
        glow = pow(glow, 0.4) * u_glow_strength;
        glow_amount = glow;

        col.rgb += col.rgb * vec3(1.0, 0.93, 0.85) * glow * 2.5;
        col.b   += col.b * glow * 0.35;
    }

    // ---- Inline bloom (gaussian kernel, luminance-gated, no FBO needed) ----
    if (u_bloom_strength > 0.001) {
        float sigma = max(u_bloom_sigma, 1.0);
        int spread = int(ceil(sigma * 2.0));
        if (spread < 2) spread = 2;
        if (spread > 8) spread = 8;

        vec3 bloom_sum = vec3(0.0);
        float total = 0.0;
        for (int y = -1; y <= 1; y++) {
            float wy = exp(-0.5 * float(y*y) / (sigma * sigma));
            for (int x = -spread; x <= spread; x++) {
                float wx = exp(-0.5 * float(x*x) / (sigma * sigma));
                float w = wx * wy;
                bloom_sum += texture(u_terminal, uv + vec2(float(x) * texel.x, float(y) * texel.y)).rgb * w;
                total += w;
            }
        }
        vec3 bloom = bloom_sum / total;
        float lum = luma(bloom);
        float gating = smoothstep(0.15, 0.5, lum);
        col.rgb += bloom * u_bloom_strength * gating;
    }

    // ---- Burn-in / phosphor persistence (afterimage trail) ----
    if (u_burn_in > 0.001) {
        float lum = luma(col);
        float gate = smoothstep(0.15, 0.55, lum) * u_burn_in;

        vec3 trail = vec3(0.0);
        float total = 0.0;
        int trail_taps = 5;
        for (int i = 0; i < trail_taps; i++) {
            float t = float(i + 1);
            float angle = float(i) * 1.3 + u_time * 0.3;
            vec2 offset = vec2(cos(angle), sin(angle)) * t * 0.7;
            float w = exp(-0.6 * t);
            trail += texture(u_terminal, uv + offset * texel * 2.5).rgb * w;
            total += w;
        }
        trail /= total;

        float decay = exp(-0.8 * u_time * 0.15 + 1.0);
        vec3 afterglow = mix(col, trail, 0.55);
        col = mix(col, afterglow, gate * clamp(decay, 0.0, 1.0));
    }

    // ---- Trinitron aperture grille (RGB vertical stripe mask) ----
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

    // ---- Vignetting (glass-depth darkening toward edges) ----
    if (u_curvature > 0.0001) {
        float v = clamp(1.0 - dist_sq * 0.35, 0.72, 1.0);
        v = pow(v, 1.35);
        col.rgb *= v;
    }

    // ---- Pixel rounding (2D gaussian beam spot for rounded pixels) ----
    if (u_rounding > 0.001) {
        vec2 pixel_center = floor(frag) + 0.5;
        vec2 offset = frag - pixel_center;
        float dist = length(offset * 2.0);
        float spot = exp(-2.5 * dist * dist);
        float factor = mix(1.0 - u_rounding * 0.7, 1.0, spot);
        col.rgb *= factor;
    }

    // ---- Depth shadows (bezel shadow + inner depth) ----
    if (u_shadow_strength > 0.001) {
        vec2 centered = uv * 2.0 - 1.0;
        float corner = centered.x * centered.x + centered.y * centered.y;

        float bezel = smoothstep(0.55, 0.92, corner);
        col.rgb *= (1.0 - bezel * u_shadow_strength * 0.55);

        float edge_x = 1.0 - centered.x * centered.x;
        float edge_y = 1.0 - centered.y * centered.y;
        float inner = smoothstep(0.08, 0.4, min(edge_x, edge_y));
        col.rgb *= (1.0 - (1.0 - inner) * u_shadow_strength * 0.25);

        float edge_grad = smoothstep(0.85, 1.0, corner);
        col.rgb *= (1.0 - edge_grad * u_shadow_strength * 0.15);
    }

    // ---- Film grain / phosphor coating irregularity ----
    {
        float n1 = hash(frag + vec2(0.0,      u_time * 23.0)) * 0.035;
        float n2 = hash(frag * 0.6 + vec2(u_time * 17.0, 0.0)) * 0.025;
        float n3 = hash(frag * 0.3 + vec2(u_time * 11.0, u_time * 13.0)) * 0.015;
        float grain = n1 + n2 + n3 - 0.0375;

        float lum      = luma(col);
        float strength = (1.0 - lum) * 0.35 + dist_sq * 0.18;
        col.rgb += grain * strength;
    }

    // ---- CRT warm white point (~6500 K) ----
    col.rgb *= vec3(1.025, 1.0, 0.95);

    frag_color = vec4(col, 1.0);
}
