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

    /// Render grid lines + labels + entities (call within glViewport/glScissor context)
    void render(const TimelineCamera &camera, const AppModel &model);

private:
    LineRenderer m_lines;
    PointRenderer m_points;
    TextRenderer m_text;

    void renderGrid(const TimelineCamera &camera);
    void renderLabels(const TimelineCamera &camera);
    void renderEntities(const TimelineCamera &camera, const AppModel &model);
};
