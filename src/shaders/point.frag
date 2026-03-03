#version 330 core

in vec2 v_coord;   // -1..1 quad coordinate
in vec4 v_color;

out vec4 out_color;

uniform int u_shape;  // 0=circle, 1=square

void main() {
    float alpha;
    if (u_shape == 1) {
        // Square: soft edge on the outer border of the quad
        float edge = max(abs(v_coord.x), abs(v_coord.y));
        alpha = 1.0 - smoothstep(0.8, 1.0, edge);
    } else {
        // Circle (default): discard fragments outside the unit circle
        float dist = length(v_coord);
        if (dist > 1.0) discard;
        alpha = 1.0 - smoothstep(0.8, 1.0, dist);
    }

    out_color = vec4(v_color.rgb, v_color.a * alpha);
}
