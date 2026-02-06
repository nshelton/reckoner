#pragma once

struct Color {
    float r, g, b, a;

    Color(float r = 0, float g = 0, float b = 0, float a = 1.0f)
        : r(r), g(g), b(b), a(a) {}
};
