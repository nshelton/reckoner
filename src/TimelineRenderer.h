#pragma once

#include "TimelineCamera.h"
#include "renderer/LineRenderer.h"
#include "renderer/PointRenderer.h"
#include "renderer/TextRenderer.h"
#include "renderer/HistogramRenderer.h"
#include "AppModel.h"

class TimelineRenderer
{
public:
    TimelineRenderer();
    void shutdown();

    /// Render grid lines + labels + histogram + entities (call within glViewport/glScissor context).
    /// points is the shared PointRenderer owned by Renderer (map view).
    void render(const TimelineCamera& camera, const AppModel& model, PointRenderer& points);

    int  histogramBins() const { return m_histogramBins; }
    void setHistogramBins(int n) { m_histogramBins = n; }

    bool histogramEnabled() const { return m_histogramEnabled; }
    void setHistogramEnabled(bool on) { m_histogramEnabled = on; }

    /// Draw a highlight ring at the given timeline position.
    /// Call after render() while the same GL viewport/scissor is still active.
    void drawHighlight(const TimelineCamera &camera, double time, float renderOffset);

private:
    LineRenderer      m_lines;
    TextRenderer      m_text;
    HistogramRenderer m_histogram;
    int               m_histogramBins    = 100;
    bool              m_histogramEnabled = true;

    void renderWeekends(const TimelineCamera& camera);
    void renderCursor(const TimelineCamera& camera);
    void renderGrid(const TimelineCamera& camera);
    void renderLabels(const TimelineCamera& camera);
    void renderHistogram(const TimelineCamera& camera, const AppModel& model);
    void renderEntities(const TimelineCamera& camera, const AppModel& model, PointRenderer& points);
};
