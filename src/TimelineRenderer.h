#pragma once

#include "TimelineCamera.h"
#include "renderer/LineRenderer.h"
#include "renderer/PointRenderer.h"
#include "renderer/TextRenderer.h"
#include "AppModel.h"

class TimelineRenderer
{
public:
    TimelineRenderer();
    void shutdown();

    /// Render grid lines + labels + entities (call within glViewport/glScissor context).
    /// points is the shared PointRenderer owned by Renderer (map view).
    void render(const TimelineCamera& camera, const AppModel& model, PointRenderer& points);

private:
    LineRenderer m_lines;
    TextRenderer m_text;

    void renderGrid(const TimelineCamera& camera);
    void renderLabels(const TimelineCamera& camera);
    void renderEntities(const TimelineCamera& camera, const AppModel& model, PointRenderer& points);
};
