// Cathode CRT simulation — physically-inspired retro terminal shader
// Models: Trinitron aperture grille, gaussian beam scanlines,
//          P22 phosphor glow, bloom scatter, barrel curvature,
//          chromatic convergence error, vignetting, and film grain.
#version 300 es
precision highp float;

in vec2 v_tex;
out vec4 frag_color;

uniform sampler2D u_terminal;
uniform sampler2D u_bloom_tex;
uniform float     u_time;
uniform vec2      u_resolution;
uniform vec4      u_background;

uniform float u_scanline_intensity;   // 0 = off, 0.06 = default
uniform float u_scanline_period;      // rows per scanline group, default 2.0
uniform float u_bloom_strength;       // 0 = off, 0.12 = default
uniform float u_glow_strength;        // 0 = off, 0.06 = default
uniform float u_glow_threshold_low;   // luma below this = no glow, default 0.15
uniform float u_glow_threshold_high;  // luma above this = full glow, default 0.6
uniform float u_mask_strength;        // 0 = off, 0.005~0.03 typical
uniform float u_curvature;            // 0 = flat, 0.03 = typical CRT bulge
uniform float u_chromatic_aberration; // 0 = off, 0.0005~0.002 typical

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
    vec2 size = u_resolution;

    float dist_sq = 0.0;

    // ---- Screen curvature (barrel distortion) ----
    if (u_curvature > 0.0001) {
        uv = barrel(uv, u_curvature, dist_sq);
        /* clip pixels that fell outside the visible CRT area */
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

    // ---- Scanlines (gaussian beam-spot profile) ----
    float glow_amount = 0.0;
    if (u_scanline_intensity > 0.001) {
        float phase    = frag.y / u_scanline_period;
        float v        = fract(phase);
        float spread   = 0.24;          // beam width: narrower = tighter gaussian
        float d        = (v - 0.5) / spread;
        float beam     = exp(-0.5 * d * d);
        float scanline = mix(1.0 - u_scanline_intensity, 1.0, beam);

        /* bright content blooms through the scanline gap (beam blooming) */
        float lum    = luma(col);
        float reduce = smoothstep(0.0, 0.45, lum) * 0.8;
        col.rgb *= mix(scanline, 1.0, reduce);
    }

    // ---- Phosphor glow (P22 warm tone, blue ZnS:Ag emphasis) ----
    if (u_glow_strength > 0.001) {
        float lum  = luma(col);
        float glow = smoothstep(u_glow_threshold_low,
                                 u_glow_threshold_high, lum);
        glow = pow(glow, 0.6) * u_glow_strength;
        glow_amount = glow;

        /* warm glow hue + slight blue phosphor dominance */
        col.rgb += col.rgb * vec3(1.0, 0.93, 0.85) * glow * 1.5;
        col.b   += col.b * glow * 0.25;
    }

    // ---- Bloom (pre-filtered separable blur, amplified near glow) ----
    if (u_bloom_strength > 0.001) {
        vec3 bloom    = texture(u_bloom_tex, uv).rgb;
        float g_boost = 1.0 + glow_amount * 3.0;
        col.rgb += bloom * u_bloom_strength * g_boost;
    }

    // ---- Trinitron aperture grille (RGB vertical stripe mask) ----
    if (u_mask_strength > 0.0001) {
        float ph = fract(frag.x / 3.0) * 3.0;

        float s    = 0.15;                 // smoothness at stripe boundaries
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

    /* diagnostic: raw passthrough */
    frag_color = vec4(texture(u_terminal, uv).rgb, 1.0);
    /* frag_color = vec4(col, 1.0); */
}
