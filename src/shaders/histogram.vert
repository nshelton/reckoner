#version 330 core

// Bar corner in timeline coordinate space: (time_x, y)
// y is already in [-1, 1] since the timeline camera's Y range is [-1, 1].
layout(location = 0) in vec2 in_pos;

uniform mat3 u_viewProjection;

void main() {
    vec3 p = u_viewProjection * vec3(in_pos, 1.0);
    gl_Position = vec4(p.xy, 0.0, 1.0);
}
