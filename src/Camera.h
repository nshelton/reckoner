#pragma once

#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>
#include "core/Vec2.h"
#include "core/Mat3.h"
#include <cmath>

/// Camera manages the view transformation in geographic coordinates (lat/lon)
/// Supports pan (move) and zoom operations
/// Uses an orthographic projection for 2D visualization
/// All coordinates are in degrees (latitude/longitude)
class Camera
{
public:
    Camera();

    void setSize(int width, int height);
    void reset();
    void move(Vec2 delta);
    Vec2 getSize() const
    {
        return Vec2(m_width, m_height);
    }

    Vec2 screenToWorld(Vec2 screen_px) const
    {
        Vec2 ndc = Vec2(
            (screen_px.x / static_cast<float>(m_width)) * 2.0f - 1.0f,
            1.0f - (screen_px.y / static_cast<float>(m_height)) * 2.0f);
        return m_viewTransform.applyInverse(ndc);
    }

    void setCenter(Vec2 center)
    {
        float lonH = lonHalf(center.y);
        m_left = center.x - lonH;
        m_right = center.x + lonH;
        m_bottom = center.y - m_zoom;
        m_top = center.y + m_zoom;
        m_viewTransform.setOrtho(m_left, m_right, m_bottom, m_top);
    }

    Vec2 center() const
    {
        return Vec2(
            (m_left + m_right) * 0.5f,
            (m_bottom + m_top) * 0.5f);
    }

    // Current transform from mm to NDC
    Mat3 Transform() const { return m_viewTransform; }
    int width() const { return m_width; }
    int height() const { return m_height; }
    void zoomAtPixel(const Vec2 &px, float wheelSteps);

    float zoom() const { return m_zoom; }
    void setCenterAndZoom(Vec2 center, float zoom)
    {
        m_zoom = zoom;
        float lonH = lonHalf(center.y);
        m_left = center.x - lonH;
        m_right = center.x + lonH;
        m_bottom = center.y - m_zoom;
        m_top = center.y + m_zoom;
        m_viewTransform.setOrtho(m_left, m_right, m_bottom, m_top);
    }

    float lonLeft()   const { return m_left; }
    float lonRight()  const { return m_right; }
    float latBottom() const { return m_bottom; }
    float latTop()    const { return m_top; }

private:
    static float clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

    // Longitude half-extent with cosine latitude correction so that
    // east-west and north-south scales match at the given center latitude.
    float lonHalf(float centerLat) const
    {
        float latRad = centerLat * static_cast<float>(M_PI / 180.0);
        float cosLat = std::max(0.001f, std::cos(latRad));
        return m_aspect * m_zoom / cosLat;
    }

    int m_width{0};
    int m_height{0};

    Vec2 m_last;

    // Geographic (lat/lon in degrees) to NDC transform
    Mat3 m_viewTransform;

    // zoom is the vertical half-size in degrees
    float m_zoom;

    float m_left;   // min longitude (degrees)
    float m_right;  // max longitude (degrees)
    float m_top;    // max latitude (degrees)
    float m_bottom; // min latitude (degrees)
    float m_aspect;

    // Min/max zoom in degrees (larger = more zoomed out)
    float m_minZoom{0.001f};  // Very zoomed in (~100 meters)
    float m_maxZoom{10.0f};   // Very zoomed out (~1000 km)
};
