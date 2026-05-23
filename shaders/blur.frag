#version 300 es
precision highp float;

in vec2 v_tex;
out vec4 frag_color;

uniform sampler2D u_tex;
uniform vec2      u_direction;
uniform float     u_sigma;

void main()
{
    vec2 texel = 1.0 / vec2(textureSize(u_tex, 0));
    vec4 color = vec4(0.0);
    float total = 0.0;

    int r = int(u_sigma * 2.5);
    if (r < 4) r = 4;
    if (r > 14) r = 14;

    for (int i = -r; i <= r; i++) {
        float t = float(i);
        float w = exp(-0.5 * t * t / (u_sigma * u_sigma));
        color += texture(u_tex, v_tex + texel * u_direction * t) * w;
        total += w;
    }

    frag_color = color / total;
}
