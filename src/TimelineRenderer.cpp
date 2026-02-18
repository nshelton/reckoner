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

static const double kGridFadeMin  =  4.0;
static const double kGridFadeMax  = 200.0;
static const double kLabelFadeMin =  60.0;
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

void TimelineRenderer::render(const TimelineCamera& camera, const AppModel& model,
                               PointRenderer& points)
{
    m_lines.clear();
    renderGrid(camera);
    m_lines.draw(camera.getTransform());

    renderLabels(camera);
    renderEntities(camera, model, points);
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

    float textSize  = 0.05f;
    float rowHeight = textSize * 1.3f;

    m_text.begin(camera.getTransform(), camera.aspectRatio());

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
        float y = -0.95f + static_cast<float>(row) * rowHeight;

        double firstTick = std::floor(left / interval) * interval;
        for (double t = firstTick; t <= right; t += interval)
        {
            std::time_t tt = static_cast<std::time_t>(t);
            std::tm *tm_info = std::gmtime(&tt);
            if (!tm_info) continue;

            char buf[64];
            std::strftime(buf, sizeof(buf), kTickLevels[i].labelFormat, tm_info);
            m_text.addText(buf, Vec2(static_cast<float>(t), y), textColor, textSize, 0.5f);
        }

        row++;
    }

    m_text.end();
}

void TimelineRenderer::renderEntities(const TimelineCamera& camera, const AppModel& model,
                                       PointRenderer& points)
{
    if (model.entities.empty()) return;

    size_t numChunks = (model.entities.size() + PointRenderer::CHUNK_SIZE - 1)
                       / PointRenderer::CHUNK_SIZE;

    float aspect = static_cast<float>(camera.width()) / 100.0f;
    TimeExtent visible = camera.getTimeExtent();
    float tMin = static_cast<float>(visible.start);
    float tMax = static_cast<float>(visible.end);

    PointRenderer::MapExtent mapExtent{
        static_cast<float>(model.spatial_extent.min_lon),
        static_cast<float>(model.spatial_extent.max_lon),
        static_cast<float>(model.spatial_extent.min_lat),
        static_cast<float>(model.spatial_extent.max_lat)
    };
    points.drawForTimeline(camera.getTransform(), aspect, numChunks, tMin, tMax, mapExtent);
}
