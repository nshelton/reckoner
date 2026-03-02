#version 330 core

// Unit quad [0,1] x [0,1] — remapped to tile lon/lat bounds in the shader
layout(location = 0) in vec2 in_uv;

uniform mat3  u_viewProjection;
uniform float u_lonLeft;
uniform float u_lonRight;
uniform float u_latBottom;
uniform float u_latTop;

out vec2 v_uv;

void main() {
    float lon = mix(u_lonLeft,   u_lonRight,  in_uv.x);
    float lat = mix(u_latBottom, u_latTop,    in_uv.y);
    vec3 ndc = u_viewProjection * vec3(lon, lat, 1.0);
    gl_Position = vec4(ndc.xy, 0.0, 1.0);
    v_uv = in_uv;
}
