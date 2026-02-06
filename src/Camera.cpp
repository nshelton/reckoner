#include "Camera.h"
#include <iostream>

Camera::Camera()
{
    m_zoom = 0.15f; // initial zoom in degrees (~16 km vertical)
    m_aspect = 4.0f / 3.0f; // default aspect
    reset();
}

void Camera::setSize(int width, int height) 
{
    m_width = width;
    m_height = height;
    m_aspect = static_cast<float>(width) / static_cast<float>(height);

    Vec2 center = Vec2(m_left + (m_right - m_left) * 0.5f, (m_bottom + m_top) * 0.5f);

    m_left = center.x - m_aspect * m_zoom;
    m_right = center.x + m_aspect * m_zoom;
    m_bottom = center.y - m_zoom;
    m_top = center.y + m_zoom;

    m_viewTransform.setOrtho(m_left, m_right, m_bottom, m_top);
    // Debug output disabled - enable in MainScreen instead
    // std::cout << "Camera setSize: " << width << "x" << height << ", aspect: " << m_aspect << std::endl;
}


void Camera::move(Vec2 delta) {
    m_left += delta.x;
    m_right += delta.x;
    m_bottom += delta.y;
    m_top += delta.y;
    m_viewTransform.setOrtho(m_left, m_right, m_bottom, m_top);
}

void Camera::reset() {
    // Center on LA area where the data is
    // Longitude: -118.45 (center of data -118.78 to -118.13)
    // Latitude: 34.08 (center of data 33.98 to 34.17)
    Vec2 center = Vec2(-118.45f, 34.08f);

    // Reset zoom to initial value
    m_zoom = 0.15f;

    m_left = center.x - m_aspect * m_zoom;
    m_right = center.x + m_aspect * m_zoom;
    m_bottom = center.y - m_zoom;
    m_top = center.y + m_zoom;

    m_viewTransform = Mat3();
    m_viewTransform.setOrtho(m_left, m_right, m_bottom, m_top);

    std::cout << "Camera reset to LA area: lon [" << m_left << ", " << m_right
              << "], lat [" << m_bottom << ", " << m_top << "]" << std::endl;
}

void Camera::zoomAtPixel(const Vec2 &px, float wheelSteps)
{
    // convert pixel to NDC
    Vec2 screen = getSize();
    Vec2 ndc((px.x / screen.x) * 2.0f - 1.0f,
             1.0f - (px.y / screen.y) * 2.0f);

    // world point under cursor before zoom
    Vec2 anchor = screenToWorld(px);

    // zoom factor (>1 zooms in)
    float factor = std::pow(1.1f, wheelSteps);

    // update zoom (vertical half-size in degrees) with clamping
    m_zoom = m_zoom / factor;
    m_zoom = clamp(m_zoom, m_minZoom, m_maxZoom);

    // choose new center so anchor stays fixed under cursor
    Vec2 newCenter;
    newCenter.x = anchor.x - ndc.x * m_aspect * m_zoom;
    newCenter.y = anchor.y - ndc.y * m_zoom;

    m_left   = newCenter.x - m_aspect * m_zoom;
    m_right  = newCenter.x + m_aspect * m_zoom;
    m_bottom = newCenter.y - m_zoom;
    m_top    = newCenter.y + m_zoom;

    m_viewTransform.setOrtho(m_left, m_right, m_bottom, m_top);
}