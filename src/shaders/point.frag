#version 330 core

in vec2 v_coord;   // -1..1 quad coordinate
in vec4 v_color;

out vec4 out_color;

void main() {
    // Discard fragments outside the unit circle
    float dist = length(v_coord);
    if (dist > 1.0) discard;

    // Soft anti-aliased edge
    float alpha = 1.0 - smoothstep(0.8, 1.0, dist);

    out_color = vec4(v_color.rgb, v_color.a * alpha);
}
