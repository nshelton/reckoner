#version 330 core

// Per-vertex attribute (quad corner, -1..1)
layout(location = 0) in vec2  in_quad_vertex;

// Per-instance attributes
layout(location = 1) in float in_time_start;    // world-space left edge
layout(location = 2) in float in_time_end;      // world-space right edge
layout(location = 3) in float in_render_offset; // world-space y center
layout(location = 4) in vec4  in_color;         // per-entity RGBA

out vec4 v_color;
out vec2 v_coord;

uniform mat3  u_viewProjection;
uniform float u_yOffset;          // screen-space NDC Y shift (layer offset)
uniform float u_barHalfHeight;    // NDC half-height of the bar
uniform float u_minBarHalfWidth;  // minimum half-width in NDC (prevents 0-width bars)

void main() {
    // Project both time edges to NDC x.
    // Since the timeline projection is affine in x, we can project the endpoints
    // directly rather than interpolating through the quad parameter.
    vec3 p_left  = u_viewProjection * vec3(in_time_start, in_render_offset, 1.0);
    float right_x = (u_viewProjection * vec3(in_time_end, in_render_offset, 1.0)).x;

    float center_x = (p_left.x + right_x) * 0.5;
    float half_w   = max((right_x - p_left.x) * 0.5, u_minBarHalfWidth);

    float x = center_x + in_quad_vertex.x * half_w;
    float y = p_left.y + in_quad_vertex.y * u_barHalfHeight + u_yOffset;

    gl_Position = vec4(x, y, 0.0, 1.0);
    v_color = in_color;
    v_coord = in_quad_vertex;
}
