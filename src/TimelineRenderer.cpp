#include "TimelineRenderer.h"
#include "core/Color.h"
#include <cmath>
#include <ctime>
#include <cstring>

static constexpr float kTLPi = 3.14159265358979323846f;

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
    m_histogram.init();
}

void TimelineRenderer::shutdown()
{
    m_lines.shutdown();
    m_text.shutdown();
    m_histogram.shutdown();
}

void TimelineRenderer::render(const TimelineCamera& camera, const AppModel& model,
                               PointRenderer& points)
{
    renderWeekends(camera);  // background shading first, drawn under everything else

    m_lines.clear();
    renderGrid(camera);
    m_lines.draw(camera.getTransform());

    renderLabels(camera);
    renderHistogram(camera, model);
    renderEntities(camera, model, points);
    renderCursor(camera);  // drawn last so it sits on top of everything
}

void TimelineRenderer::renderWeekends(const TimelineCamera& camera)
{
    static constexpr double kDaySecs = 86400.0;

    double left  = camera.center() - camera.zoom();
    double right = camera.center() + camera.zoom();

    // Only draw when individual days are wide enough to be visible (>= 3 px)
    double pixelsPerDay = kDaySecs / ((2.0 * camera.zoom()) / camera.width());
    if (pixelsPerDay < 3.0) return;

    // Collect Saturday/Sunday spans in the visible range
    std::vector<HistogramRenderer::TimeRange> rects;

    // Start from the midnight before the left edge
    double firstMidnight = std::floor(left / kDaySecs) * kDaySecs;

    for (double dayStart = firstMidnight; dayStart < right; dayStart += kDaySecs) {
        // Sample the middle of the day to avoid DST/leap-second edge cases
        std::time_t mid = static_cast<std::time_t>(dayStart + kDaySecs * 0.5);
        std::tm* tm = std::gmtime(&mid);
        if (!tm) continue;

        // tm_wday: 0 = Sunday, 6 = Saturday
        if (tm->tm_wday == 0 || tm->tm_wday == 6) {
            float x0 = static_cast<float>(dayStart);
            float x1 = static_cast<float>(dayStart + kDaySecs);
            rects.push_back({x0, x1});
        }
    }

    if (rects.empty()) return;

    // Slightly lighter than the 0.12 background — subtle, not distracting
    m_histogram.drawRects(camera.getTransform(), rects, -1.0f, 1.0f,
                          0.5f, 0.5f, 0.5f, 0.12f);
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

void TimelineRenderer::renderHistogram(const TimelineCamera& camera, const AppModel& model)
{
    if (!m_histogramEnabled || model.entities.empty()) return;

    TimeExtent visible = camera.getTimeExtent();
    m_histogram.draw(camera.getTransform(), model.entities,
                     visible.start, visible.end, m_histogramBins);
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

void TimelineRenderer::renderCursor(const TimelineCamera& camera)
{
    m_lines.clear();
    m_lines.setLineWidth(2.0f);

    float cx = static_cast<float>(camera.center());
    m_lines.addLine(Vec2(cx, -1.0f), Vec2(cx, 1.0f), Color(1.0f, 0.85f, 0.1f, 0.85f));

    m_lines.draw(camera.getTransform());
    m_lines.setLineWidth(1.0f);
}

void TimelineRenderer::drawHighlight(const TimelineCamera& camera,
                                     double time, float renderOffset)
{
    m_lines.clear();
    m_lines.setLineWidth(2.0f);

    Color c(1.0f, 0.95f, 0.2f, 0.9f);

    // Compute world-space radii for a fixed screen pixel size.
    // Time axis: (2 * zoom) seconds spans the full width in pixels.
    // Y axis:    [-1, 1] range spans the full height in pixels.
    const float pixelRadius = 10.0f;
    double time_r = static_cast<double>(pixelRadius)
                    * 2.0 * camera.zoom() / static_cast<double>(camera.width());
    float y_r = pixelRadius * 2.0f / static_cast<float>(camera.height());

    constexpr int N = 24;
    for (int i = 0; i < N; ++i) {
        float a0 = 2.0f * kTLPi * static_cast<float>(i)     / static_cast<float>(N);
        float a1 = 2.0f * kTLPi * static_cast<float>(i + 1) / static_cast<float>(N);
        Vec2 p0(static_cast<float>(time + time_r * std::cos(a0)),
                renderOffset + y_r * std::sin(a0));
        Vec2 p1(static_cast<float>(time + time_r * std::cos(a1)),
                renderOffset + y_r * std::sin(a1));
        m_lines.addLine(p0, p1, c);
    }

    m_lines.draw(camera.getTransform());
    m_lines.setLineWidth(1.0f);
}
