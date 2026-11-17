#version 300 es
precision highp float;

in vec2 v_tex;
out vec4 frag_color;

uniform sampler2D u_source;
uniform vec2      u_texel;           // 1 / destination size
uniform vec2      u_axis;            // (1,0) horizontal, (0,1) vertical
uniform float     u_sigma;           // sigma in destination texels
uniform int       u_radius;          // radius in destination texels
uniform int       u_kind;            // 0 = gaussian bloom, 1 = exponential glow
uniform int       u_gate;            // apply luma gate (glow horizontal pass)
uniform float     u_threshold_low;
uniform float     u_threshold_high;
uniform int       u_flip_y;          // flip source V when sampling the terminal

float luma(vec3 c)
{
    return dot(c, vec3(0.299, 0.587, 0.114));
}

void main()
{
    vec2 src_uv = v_tex;
    if (u_flip_y != 0)
        src_uv.y = 1.0 - src_uv.y;

    float sigma = max(u_sigma, 0.001);
    vec3 sum = vec3(0.0);
    float total = 0.0;

    for (int i = -13; i <= 13; i++) {
        if (i < -u_radius || i > u_radius)
            continue;

        float t = float(i);
        float w = (u_kind == 0)
            ? exp(-0.5 * t * t / (sigma * sigma))
            : exp(-abs(t) / sigma);

        vec3 s = texture(u_source, src_uv + u_axis * t * u_texel).rgb;
        if (u_gate != 0) {
            float l = luma(s);
            float g = (u_threshold_high > u_threshold_low)
                ? smoothstep(u_threshold_low, u_threshold_high, l)
                : step(u_threshold_low, l);
            s *= g;
        }

        sum += s * w;
        total += w;
    }

    frag_color = vec4(sum / max(total, 0.001), 1.0);
}
