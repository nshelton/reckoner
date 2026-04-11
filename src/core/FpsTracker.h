#pragma once

/// Tracks frame rate using a rolling accumulation window.
class FpsTracker {
public:
    void tick(double dt) {
        m_accum += dt;
        m_count++;
        if (m_accum >= 0.5) {
            m_fps = m_count / m_accum;
            m_frameMs = (m_accum / m_count) * 1000.0;
            m_count = 0;
            m_accum = 0.0;
        }
    }

    double fps() const { return m_fps; }
    double frameMs() const { return m_frameMs; }

private:
    double m_accum{0.0};
    int m_count{0};
    double m_fps{0.0};
    double m_frameMs{0.0};
};
