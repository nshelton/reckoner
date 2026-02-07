#pragma once

#include "core/Mat3.h"
#include "core/TimeExtent.h"
#include <cmath>
#include <ctime>

class TimelineCamera
{
public:
    TimelineCamera();

    void setSize(int width, int height);

    void panByPixels(float dx);

    void zoomAtPixel(float px, float wheelSteps);

    double screenToTime(float px) const
    {
        double ndc = (static_cast<double>(px) / m_width) * 2.0 - 1.0;
        return m_center + ndc * m_zoom;
    }

    float timeToScreen(double t) const
    {
        double ndc = (t - m_center) / m_zoom;
        return static_cast<float>((ndc + 1.0) * 0.5 * m_width);
    }

    Mat3 getTransform() const
    {
        Mat3 t;
        t.setOrtho(
            static_cast<float>(m_center - m_zoom),
            static_cast<float>(m_center + m_zoom),
            -1.0f,
            1.0f);
        return t;
    }

    TimeExtent getTimeExtent() const
    {
        return {m_center - m_zoom, m_center + m_zoom};
    }

    void reset()
    {
        m_center = static_cast<double>(std::time(nullptr));
        m_zoom = 3600.0 * 12.0;
    }

    double center() const { return m_center; }
    double zoom() const { return m_zoom; }
    int width() const { return m_width; }
    int height() const { return m_height; }
    float aspectRatio() const { return static_cast<float>(m_width) / static_cast<float>(m_height); }

private:
    static double clamp(double v, double lo, double hi)
    {
        return v < lo ? lo : (v > hi ? hi : v);
    }

    double m_center;
    double m_zoom;
    int m_width{800};
    int m_height{200};

    double m_minZoom{1.0};
    double m_maxZoom{50.0 * 365.25 * 86400.0};
};
