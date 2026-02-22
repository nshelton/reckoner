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
    {1.0,        "%H:%M:%S"},    // 1 second
    {10.0,       "%H:%M:%S"},    // 10 seconds
    {60.0,       "%H:%M"},       // 1 minute
    {600.0,      "%H:%M"},       // 10 minutes
    {3600.0,     "%H:%M"},       // 1 hour
    {21600.0,    "%b %d %Hh"},   // 6 hours — e.g. "Dec 19 06h"
    {86400.0,    "%a %b %d"},    // 1 day   — e.g. "Thu Dec 19"  (weekday added)
    {604800.0,   "%b %d"},       // 1 week
    {2592000.0,  "%B %Y"},       // 1 month — e.g. "December 2024" (full name)
    {31536000.0, "%Y"},          // 1 year
};
static const int kNumTickLevels = sizeof(kTickLevels) / sizeof(kTickLevels[0]);

// Line width per tick level: finer = 1px, coarser = heavier
static const float kTickLineWidth[] = {
    1.0f,  // 1s
    1.0f,  // 10s
    1.0f,  // 60s
    1.0f,  // 10m
    1.0f,  // 1h
    1.0f,  // 6h
    1.5f,  // day
    2.0f,  // week
    2.0f,  // month
    2.0f,  // year
};

// Grid line brightness per tick level: finer = darker, coarser = brighter
static const float kTickBrightness[] = {
    0.30f,  // 1s
    0.33f,  // 10s
    0.38f,  // 60s
    0.42f,  // 10m
    0.48f,  // 1h
    0.55f,  // 6h
    0.65f,  // day
    0.72f,  // week
    0.78f,  // month
    0.85f,  // year
};

static const double kGridFadeMin  =  4.0;
static const double kGridFadeMax  = 200.0;
static const double kLabelFadeMin =  60.0;
static const double kLabelFadeMax = 200.0;

TimelineRenderer::TimelineRenderer()
{
    m_lines.init();
    m_text.init();
    m_histogram.init();
    m_solarAltitude.init();
    m_moonAltitude.init();
}

void TimelineRenderer::shutdown()
{
    m_lines.shutdown();
    m_text.shutdown();
    m_histogram.shutdown();
    m_solarAltitude.shutdown();
    m_moonAltitude.shutdown();
}

void TimelineRenderer::render(const TimelineCamera& camera, const AppModel& model,
                               PointRenderer& points)
{
    // Derive local-time display offset from the center longitude of the spatial extent.
    // This makes all tick labels, weekend shading, and day boundaries show local solar time
    // (rounded to the nearest whole hour) rather than UTC.
    {
        double centerLon = (model.spatial_extent.min_lon + model.spatial_extent.max_lon) / 2.0;
        m_displayOffsetSecs = static_cast<int>(std::round(centerLon / 15.0)) * 3600;
    }

    renderWeekends(camera);         // background shading first, drawn under everything else
    renderSolarAltitude(camera, model);
    renderMoonAltitude(camera, model);
    renderGrid(camera);             // manages its own clear/draw per level
    renderLabels(camera);
    renderHistogram(camera, model);
    renderEntities(camera, model, points);
    renderEdgeLabels(camera);       // pinned corner date labels — over histogram, under cursor
    renderCursor(camera);           // drawn last so it sits on top of everything
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

    // Start from the local midnight before the left edge.
    // Local midnights in UTC = k*86400 - m_displayOffsetSecs (e.g. UTC 08:00 for PST).
    double off = static_cast<double>(m_displayOffsetSecs);
    double firstMidnight = std::floor((left + off) / kDaySecs) * kDaySecs - off;

    for (double dayStart = firstMidnight; dayStart < right; dayStart += kDaySecs) {
        // Sample local noon (UTC midpoint + offset) to determine day-of-week in local time.
        std::time_t mid = static_cast<std::time_t>(dayStart + kDaySecs * 0.5) + m_displayOffsetSecs;
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

        // Tick height grows with interval level (finer = shorter, coarser = full height)
        float tickHeight = 0.25f + 0.75f * static_cast<float>(i) / static_cast<float>(kNumTickLevels - 1);

        float b = kTickBrightness[i];
        Color tickColor(b, b, b, alpha * 0.9f);

        // Each level is its own draw call so we can vary line width
        m_lines.clear();
        m_lines.setLineWidth(kTickLineWidth[i]);

        double off = static_cast<double>(m_displayOffsetSecs);
        double firstTick = std::floor((left + off) / interval) * interval - off;
        for (double t = firstTick; t <= right; t += interval)
        {
            float x = static_cast<float>(t);
            m_lines.addLine(Vec2(x, -tickHeight), Vec2(x, tickHeight), tickColor);
        }

        m_lines.draw(camera.getTransform());
    }

    m_lines.setLineWidth(1.0f);  // restore default
}

void TimelineRenderer::renderLabels(const TimelineCamera &camera)
{
    double left = camera.center() - camera.zoom();
    double right = camera.center() + camera.zoom();
    double secondsPerPixel = (2.0 * camera.zoom()) / camera.width();

    // Base text size; day+ levels are slightly larger for readability
    static const float kBaseTextSize  = 0.045f;
    static const float kLargeTextSize = 0.055f;  // for day / week / month / year
    static const float kDayIntervalThreshold = 86400.0;

    float rowHeight = kLargeTextSize * 1.4f;

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

        // Coarser levels get brighter labels to reinforce visual hierarchy
        float b = 0.55f + 0.45f * static_cast<float>(i) / static_cast<float>(kNumTickLevels - 1);
        Color textColor(b, b, b, alpha);

        // Day and above use a slightly larger size so they stand out
        float textSize = (interval >= kDayIntervalThreshold) ? kLargeTextSize : kBaseTextSize;

        float y = -0.97f + static_cast<float>(row) * rowHeight;

        double off = static_cast<double>(m_displayOffsetSecs);
        double firstTick = std::floor((left + off) / interval) * interval - off;
        for (double t = firstTick; t <= right; t += interval)
        {
            // Add display offset so gmtime returns local-time fields.
            std::time_t tt = static_cast<std::time_t>(t) + m_displayOffsetSecs;
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

void TimelineRenderer::renderEdgeLabels(const TimelineCamera& camera)
{
    double left  = camera.center() - camera.zoom();
    double right = camera.center() + camera.zoom();

    // Small inward nudge so the glyphs don't clip at the scissor edge (2% of span)
    double margin = 0.02 * camera.zoom();

    static const float kTextSize = 0.065f;
    static const float kLineGap  = kTextSize * 1.45f;
    static const float kTopY     = 0.86f;  // near top of NDC [-1, 1]

    Color col(0.95f, 0.95f, 0.95f, 0.90f);

    m_text.begin(camera.getTransform(), camera.aspectRatio());

    auto drawCorner = [&](double t, float xAlign, double xOffset) {
        std::time_t tt = static_cast<std::time_t>(t) + m_displayOffsetSecs;
        std::tm* tm = std::gmtime(&tt);
        if (!tm) return;

        char wday[16], mday[16], year[8];
        std::strftime(wday, sizeof(wday), "%a",    tm);  // "Wed"
        std::strftime(mday, sizeof(mday), "%b %d", tm);  // "Jan 16"
        std::strftime(year, sizeof(year), "%Y",    tm);  // "2020"

        float x = static_cast<float>(t + xOffset);
        m_text.addText(wday, Vec2(x, kTopY),              col, kTextSize, xAlign);
        m_text.addText(mday, Vec2(x, kTopY - kLineGap),   col, kTextSize, xAlign);
        m_text.addText(year, Vec2(x, kTopY - 2*kLineGap), col, kTextSize, xAlign);
    };

    drawCorner(left,  0.0f, +margin);  // top-left,  left-aligned
    drawCorner(right, 1.0f, -margin);  // top-right, right-aligned

    m_text.end();
}

void TimelineRenderer::renderHistogram(const TimelineCamera& camera, const AppModel& model)
{
    if (!m_histogramEnabled || model.entities.empty()) return;

    TimeExtent visible = camera.getTimeExtent();
    m_histogram.draw(camera.getTransform(), model.entities,
                     visible.start, visible.end, m_histogramBins);
}

void TimelineRenderer::renderSolarAltitude(const TimelineCamera& camera, const AppModel& model)
{
    if (!m_solarAltitudeEnabled) return;

    // Use the center of the current spatial extent as the observer location.
    double lat = (model.spatial_extent.min_lat + model.spatial_extent.max_lat) / 2.0;
    double lon = (model.spatial_extent.min_lon + model.spatial_extent.max_lon) / 2.0;

    TimeExtent visible = camera.getTimeExtent();
    m_solarAltitude.draw(camera.getTransform(),
                         visible.start, visible.end,
                         lat, lon);
}

void TimelineRenderer::renderMoonAltitude(const TimelineCamera& camera, const AppModel& model)
{
    if (!m_moonAltitudeEnabled) return;

    TimeExtent visible = camera.getTimeExtent();
    m_moonAltitude.draw(camera.getTransform(), visible.start, visible.end);
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
