#include "TimelineRenderer.h"
#include "core/Color.h"
#include <cmath>
#include <ctime>
#include <cstring>

struct TickLevel
{
    double intervalSeconds;
    const char *labelFormat;
};

static const TickLevel kTickLevels[] = {
    {1.0, "%H:%M:%S"},
    {10.0, "%H:%M:%S"},
    {60.0, "%H:%M"},
    {600.0, "%H:%M"},
    {3600.0, "%H:%M"},
    {21600.0, "%b %d %H:%M"},
    {86400.0, "%b %d"},
    {604800.0, "%b %d"},
    {2592000.0, "%b %Y"},
    {31536000.0, "%Y"},
};
static const int kNumTickLevels = sizeof(kTickLevels) / sizeof(kTickLevels[0]);

// Fade thresholds (in pixels per interval)
static const double kGridFadeMin = 4.0;
static const double kGridFadeMax = 200.0;
static const double kLabelFadeMin = 60.0;
static const double kLabelFadeMax = 200.0;

TimelineRenderer::TimelineRenderer()
{
    m_lines.init();
    m_text.init();
}

void TimelineRenderer::shutdown()
{
    m_lines.shutdown();
    m_text.shutdown();
}

void TimelineRenderer::render(const TimelineCamera &camera, const AppModel &model)
{
    m_lines.clear();
    renderGrid(camera);
    m_lines.draw(camera.getTransform());

    renderLabels(camera);
    renderEntities(camera, model);
}

void TimelineRenderer::renderGrid(const TimelineCamera &camera)
{
    double left = camera.center() - camera.zoom();
    double right = camera.center() + camera.zoom();
    double secondsPerPixel = (2.0 * camera.zoom()) / camera.width();

    for (int i = 0; i < kNumTickLevels; i++)
    {
        double interval = kTickLevels[i].intervalSeconds;
        double pixelsPerInterval = interval / secondsPerPixel;

        if (pixelsPerInterval < kGridFadeMin)
            continue;

        float alpha = 1.0f;
        if (pixelsPerInterval < kGridFadeMax)
            alpha = static_cast<float>((pixelsPerInterval - kGridFadeMin) / (kGridFadeMax - kGridFadeMin));

        // Coarser levels get taller ticks
        float tickHeight = 0.3f + 0.7f * static_cast<float>(i) / static_cast<float>(kNumTickLevels - 1);

        Color tickColor(0.5f, 0.5f, 0.5f, alpha * 0.8f);

        double firstTick = std::floor(left / interval) * interval;
        for (double t = firstTick; t <= right; t += interval)
        {
            float x = static_cast<float>(t);
            m_lines.addLine(Vec2(x, -tickHeight), Vec2(x, tickHeight), tickColor);
        }
    }
}

void TimelineRenderer::renderLabels(const TimelineCamera &camera)
{
    double left = camera.center() - camera.zoom();
    double right = camera.center() + camera.zoom();
    double secondsPerPixel = (2.0 * camera.zoom()) / camera.width();

    float textSize = 0.05f;
    float rowHeight = textSize * 1.3f; // spacing between label rows

    m_text.begin(camera.getTransform(), camera.aspectRatio());

    // Assign each visible level a row, coarsest at bottom (row 0)
    int row = 0;
    for (int i = kNumTickLevels - 1; i >= 0; i--)
    {
        double interval = kTickLevels[i].intervalSeconds;
        double pixelsPerInterval = interval / secondsPerPixel;

        if (pixelsPerInterval < kLabelFadeMin)
            continue;

        float alpha = 1.0f;
        if (pixelsPerInterval < kLabelFadeMax)
            alpha = static_cast<float>((pixelsPerInterval - kLabelFadeMin) / (kLabelFadeMax - kLabelFadeMin));

        Color textColor(0.8f, 0.8f, 0.8f, alpha);

        // Y position: bottom of viewport, stacking upward per row
        float y = -0.95f + static_cast<float>(row) * rowHeight;

        double firstTick = std::floor(left / interval) * interval;
        for (double t = firstTick; t <= right; t += interval)
        {
            std::time_t tt = static_cast<std::time_t>(t);
            std::tm *tm_info = std::gmtime(&tt);
            if (!tm_info)
                continue;

            char buf[64];
            std::strftime(buf, sizeof(buf), kTickLevels[i].labelFormat, tm_info);

            Vec2 labelPos(static_cast<float>(t), y);
            m_text.addText(buf, labelPos, textColor, textSize, 0.5f);
        }

        row++;
    }

    m_text.end();
}

void TimelineRenderer::renderEntities(const TimelineCamera &camera, const AppModel &model)
{
    float aspectRatio = static_cast<float>(camera.width()) / 100.0f;
    m_points.begin(camera.getTransform(), aspectRatio);

    int idx = 0;
    TimeExtent visible = camera.getTimeExtent();

    for (const auto &entity : model.entities)
    {
        if (entity.time_start > visible.end || entity.time_end < visible.start)
            continue;

        float y = 0.0f;
        float x = static_cast<float>(entity.time_mid());
        Color c(1.0f, 1.0f, 1.0f, 0.9f);  // Alpha only; RGB from turbo colormap
        m_points.addPoint(Vec2(x, y), c, 2.0f, static_cast<float>(entity.time_start));
        idx++;
    }

    float timeMin = static_cast<float>(visible.start);
    float timeMax = static_cast<float>(visible.end);
    m_points.end(timeMin, timeMax);
}
