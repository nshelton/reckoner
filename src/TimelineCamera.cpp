#include "TimelineCamera.h"

TimelineCamera::TimelineCamera()
{
    m_center = static_cast<double>(std::time(nullptr));
    m_zoom = 3600.0 * 12.0; // 12 hours half-width = 24h visible
}

void TimelineCamera::setSize(int width, int height)
{
    if (width > 0) m_width = width;
    if (height > 0) m_height = height;
}

void TimelineCamera::panByPixels(float dx)
{
    double secondsPerPixel = (2.0 * m_zoom) / m_width;
    m_center -= dx * secondsPerPixel;
}

void TimelineCamera::zoomAtPixel(float px, float wheelSteps)
{
    double anchorTime = screenToTime(px);
    double ndc = (static_cast<double>(px) / m_width) * 2.0 - 1.0;

    double factor = std::pow(1.1, static_cast<double>(wheelSteps));
    m_zoom = clamp(m_zoom / factor, m_minZoom, m_maxZoom);

    m_center = anchorTime - ndc * m_zoom;
}
