#version 330 core

// Per-vertex attribute (quad corner, -1..1)
layout(location = 0) in vec2  in_quad_vertex;

// Per-instance attributes
layout(location = 1) in vec2  in_geo_pos;       // (lon, lat) — map visibility check
layout(location = 2) in float in_time_mid;      // x position on timeline + color
layout(location = 3) in float in_render_offset; // y position on timeline

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

// Current map viewport bounds — points outside are desaturated
uniform float u_mapMinLon;
uniform float u_mapMaxLon;
uniform float u_mapMinLat;
uniform float u_mapMaxLat;

// Draw pass:
//   0 = out-of-map only (gray background layer, drawn first)
//   1 = in-map only     (full color foreground layer, drawn on top)
uniform int u_filterMode;

void main() {
    bool inMapView = in_geo_pos.x >= u_mapMinLon && in_geo_pos.x <= u_mapMaxLon &&
                     in_geo_pos.y >= u_mapMinLat && in_geo_pos.y <= u_mapMaxLat;

    // Discard this instance if it belongs to the other pass
    if (u_filterMode == 0 &&  inMapView) { gl_Position = vec4(0.0); return; }
    if (u_filterMode == 1 && !inMapView) { gl_Position = vec4(0.0); return; }

    // Transform timeline position (time, render_offset) to NDC
    vec3 center_ndc = u_viewProjection * vec3(in_time_mid, in_render_offset, 1.0);

    vec2 offset_ndc = in_quad_vertex * u_size * 0.05;
    offset_ndc.x /= u_aspectRatio;

    gl_Position = vec4(center_ndc.xy + offset_ndc, 0.0, 1.0);
    v_coord = in_quad_vertex;

    // Time-based color
    float range = u_timeMax - u_timeMin;
    float t = (range > 0.0) ? (in_time_mid - u_timeMin) / range : -1.0;

    if (t < 0.0 || t > 1.0) {
        v_color = vec4(0.1, 0.1, 0.1, 0.5);
    } else if (!inMapView) {
        // Out-of-map: desaturated gray
        v_color = vec4(0.5, 0.5, 0.5, 0.2);
    } else {
        // In-map: full turbo color
        v_color = vec4(turbo(t), 0.5);
    }
}
