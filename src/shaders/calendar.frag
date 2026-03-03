#version 330 core

in vec4 v_color;
in vec2 v_coord;  // -1..1 quad coordinate

out vec4 out_color;

void main() {
    // Slightly rounded top/bottom corners: clip the Y extent with a small rounding curve.
    // The bar is rectangular along X; we soften the Y edges for a cleaner look.
    float edgeSoftness = smoothstep(0.85, 1.0, abs(v_coord.y));
    float alpha = v_color.a * (1.0 - edgeSoftness);
    out_color = vec4(v_color.rgb, alpha);
}
