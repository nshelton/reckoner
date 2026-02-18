#version 330 core

// Per-vertex attribute (quad corner, -1..1)
layout(location = 0) in vec2 in_quad_vertex;

// Per-instance attributes
layout(location = 1) in vec2  in_position;
layout(location = 4) in float in_timestamp;

// Turbo colormap approximation (Google AI, Apache 2.0)
// https://ai.googleblog.com/2019/08/turbo-improved-rainbow-colormap-for.html
vec3 turbo(float t) {
    const vec4 kR  = vec4( 0.13572138,   4.61539260, -42.66032258,  132.13108234);
    const vec4 kG  = vec4( 0.09140261,   2.19418839,   4.84296658,  -14.18503333);
    const vec4 kB  = vec4( 0.10667330,  12.64194608, -60.58204836,  110.36276771);
    const vec2 kRa = vec2(-152.94239396, 59.28637943);
    const vec2 kGa = vec2(   4.27729857,  2.82956604);
    const vec2 kBa = vec2( -89.90310912, 27.34824973);

    t = clamp(t, 0.0, 1.0);
    vec4 v4 = vec4(1.0, t, t*t, t*t*t);
    vec2 v2 = v4.zw * v4.z;
    return vec3(
        dot(v4, kR) + dot(v2, kRa),
        dot(v4, kG) + dot(v2, kGa),
        dot(v4, kB) + dot(v2, kBa)
    );
}

out vec4 v_color;
out vec2 v_coord;

uniform mat3  u_viewProjection;
uniform float u_aspectRatio;
uniform float u_timeMin;
uniform float u_timeMax;
uniform float u_size;

void main() {
    // Transform center position to NDC
    vec3 center_ndc = u_viewProjection * vec3(in_position, 1.0);

    // Apply size offset in NDC space (after projection).
    // Scale by 0.01 so that u_size == 1 is roughly 1% of screen height.
    vec2 offset_ndc = in_quad_vertex * u_size * 0.01;

    // Compensate for aspect ratio to keep points circular
    offset_ndc.x /= u_aspectRatio;

    gl_Position = vec4(center_ndc.xy + offset_ndc, 0.0, 1.0);

    v_coord = in_quad_vertex;  // passed to fragment for circle clipping

    // Map timestamp to [0,1]; values outside range get a muted grey colour
    float range = u_timeMax - u_timeMin;
    float t = (range > 0.0) ? (in_timestamp - u_timeMin) / range : -1.0;

    if (t < 0.0 || t > 1.0)
        v_color = vec4(0.1, 0.1, 0.1, 0.5);
    else
        v_color = vec4(turbo(t), 0.5);
}
