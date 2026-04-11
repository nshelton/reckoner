#include "MainScreen.h"
#include "app/App.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <cmath>
#include <future>
#include <chrono>

// Timing helper — prints to stderr if elapsed > threshold_ms
namespace {
    using Clock = std::chrono::steady_clock;
    using TP    = Clock::time_point;

    inline long ms_since(TP t0) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count();
    }

    void log_slow(const char* tag, TP t0, long threshold_ms = 16) {
        long ms = ms_since(t0);
        if (ms >= threshold_ms)
            std::cerr << "[SLOW " << ms << "ms] " << tag << "\n";
    }
}

MainScreen::MainScreen(AppModel* model)
    : m_model(model)
{}

void MainScreen::onAttach(App &app)
{
    m_app = &app;
    m_renderer.init();
    m_timelineRenderer.init();
    switchBackend(m_backendConfig.type);
}

void MainScreen::onResize(int /*width*/, int /*height*/)
{
    // Camera size is set per-window in onGui()
}

void MainScreen::onUpdate(double dt)
{
    // Frame heartbeat — printed every 60 frames so we know the main thread is alive
    static int s_frame = 0;
    if (++s_frame % 60 == 0) {
        std::cerr << "[FRAME " << s_frame
                  << "] gps=" << m_model->layers[0].entities.size()
                  << " photos=" << m_model->layers[1].entities.size()
                  << " calendar=" << m_model->layers[2].entities.size()
                  << " gtimeline=" << m_model->layers[3].entities.size()
                  << " gps_fetching=" << m_model->layers[0].is_fetching
                  << " photo_fetching=" << m_model->layers[1].is_fetching
                  << " calendar_fetching=" << m_model->layers[2].is_fetching
                  << " gtimeline_fetching=" << m_model->layers[3].is_fetching
                  << "\n";
    }

    // FPS tracking
    m_frameTimeAccum += dt;
    m_frameCount++;
    if (m_frameTimeAccum >= 0.5)
    {
        m_fps = m_frameCount / m_frameTimeAccum;
        m_frameMs = (m_frameTimeAccum / m_frameCount) * 1000.0;
        m_frameCount = 0;
        m_frameTimeAccum = 0.0;
    }

    auto t_update = Clock::now();

    { auto t = Clock::now(); updateSpatialExtent();              log_slow("updateSpatialExtent", t); }
    { auto t = Clock::now(); drainCompletedBatches();            log_slow("drainCompletedBatches", t); }
    { auto t = Clock::now(); m_interaction.update(*m_model);     log_slow("interaction.update", t, 5); }
    { auto t = Clock::now(); m_interaction.drainPhotoTexture();  log_slow("drainPhotoTexture", t, 5); }

    log_slow("onUpdate total", t_update);
}

void MainScreen::onRender()
{
    // Main rendering happens in the ImGui windows (onPostGuiRender)
}

void MainScreen::onDetach()
{
    cancelAndWaitAll();
    m_interaction.shutdown();  // waits for thumbnail fetch, deletes GL texture

    m_renderer.shutdown();
    m_timelineRenderer.shutdown();
}

void MainScreen::onFilesDropped(const std::vector<std::string>& /*paths*/)
{
    // Not implemented yet
}

void MainScreen::onMouseButton(int /*button*/, int /*action*/, int /*mods*/, Vec2 /*px*/)
{
    // Interaction is driven by ImGui canvas hover — see onGui()
}

void MainScreen::onCursorPos(Vec2 /*px*/)
{
    // Interaction is driven by ImGui canvas hover — see onGui()
}

void MainScreen::onScroll(double /*xoffset*/, double /*yoffset*/, Vec2 /*px*/)
{
    // Interaction is driven by ImGui canvas hover — see onGui()
}

void MainScreen::onGui()
{
    m_mapViewportValid = false;
    m_timelineViewportValid = false;

    // --- Map Window ---
    ImGui::Begin("Map");
    {
        ImVec2 contentSize = ImGui::GetContentRegionAvail();
        if (contentSize.x != m_lastMapSize.x || contentSize.y != m_lastMapSize.y)
            m_lastMapSize = contentSize;

        ImVec2 cursorPos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("MapCanvas", contentSize);

        if (ImGui::IsItemHovered())
        {
            ImGuiIO& io = ImGui::GetIO();
            ImVec2 mousePos = ImGui::GetMousePos();
            Vec2 localPx(mousePos.x - cursorPos.x, mousePos.y - cursorPos.y);

            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                m_interaction.onMapDrag(m_camera, {io.MouseDelta.x, io.MouseDelta.y});

            if (io.MouseWheel != 0.0f)
                m_interaction.onMapScroll(m_camera, io.MouseWheel, localPx);

            m_interaction.onMapHover(m_camera, localPx, *m_model);

            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                m_interaction.onMapDoubleClick(m_timelineCamera, m_camera, localPx, *m_model);
            } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
                       !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                m_interaction.onMapClick(m_camera, localPx, *m_model);
            }
        }
        else
        {
            m_interaction.onMapUnhovered();
        }

        int fbWidth, fbHeight;
        glfwGetFramebufferSize(m_app->window(), &fbWidth, &fbHeight);
        ImGuiIO& io = ImGui::GetIO();
        float fbScale = fbWidth / io.DisplaySize.x;

        m_mapViewport.x = static_cast<int>(cursorPos.x * fbScale);
        m_mapViewport.y = static_cast<int>((io.DisplaySize.y - cursorPos.y - contentSize.y) * fbScale);
        m_mapViewport.w = static_cast<int>(contentSize.x * fbScale);
        m_mapViewport.h = static_cast<int>(contentSize.y * fbScale);
        m_mapViewportValid = true;

        m_camera.setSize(static_cast<int>(contentSize.x), static_cast<int>(contentSize.y));
    }
    ImGui::End();

    // --- Timeline Window ---
    ImGui::Begin("Timeline");
    {
        ImVec2 contentSize = ImGui::GetContentRegionAvail();
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();

        ImGui::InvisibleButton("TimelineCanvas", contentSize);

        if (ImGui::IsItemHovered())
        {
            ImGuiIO& tio = ImGui::GetIO();
            ImVec2 mousePos = ImGui::GetMousePos();
            float localX = mousePos.x - cursorPos.x;
            float localY = mousePos.y - cursorPos.y;

            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                m_interaction.onTimelineDrag(m_timelineCamera, tio.MouseDelta.x);

            if (tio.MouseWheel != 0.0f)
                m_interaction.onTimelineScroll(m_timelineCamera, tio.MouseWheel, localX);

            m_interaction.onTimelineHover(m_timelineCamera, localX, localY, contentSize.y, *m_model);

            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                m_interaction.onTimelineDoubleClick(m_camera, m_timelineCamera, localX, localY, contentSize.y, *m_model);
            } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
                       !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                m_interaction.onTimelineClick(m_timelineCamera, localX, localY, contentSize.y, *m_model);
            }
        }
        else
        {
            m_interaction.onTimelineUnhovered();
        }

        int fbWidth, fbHeight;
        glfwGetFramebufferSize(m_app->window(), &fbWidth, &fbHeight);
        ImGuiIO& tio = ImGui::GetIO();
        float fbScale = fbWidth / tio.DisplaySize.x;

        m_timelineViewport.x = static_cast<int>(cursorPos.x * fbScale);
        m_timelineViewport.y = static_cast<int>(
            (tio.DisplaySize.y - cursorPos.y - contentSize.y) * fbScale);
        m_timelineViewport.w = static_cast<int>(contentSize.x * fbScale);
        m_timelineViewport.h = static_cast<int>(contentSize.y * fbScale);
        m_timelineViewportValid = true;

        m_timelineCamera.setSize(static_cast<int>(contentSize.x), static_cast<int>(contentSize.y));
    }
    ImGui::End();

    // --- Controls Window ---
    ImGui::Begin("Controls");
    {
        ImGui::Text("%.1f FPS  (%.2f ms)", m_fps, m_frameMs);

        ImGui::Separator();
        if (ImGui::Button("Reset Map")) m_camera.reset();
        ImGui::SameLine();
        if (ImGui::Button("Reset Timeline")) m_timelineCamera.reset();

        ImGui::Separator();
        ImGui::Text("Backend Configuration:");

        const char *backend_types[] = {"Fake Data", "HTTP Backend"};
        int current_type = static_cast<int>(m_backendConfig.type);
        if (ImGui::Combo("Backend Type", &current_type, backend_types, 2))
            switchBackend(static_cast<BackendConfig::Type>(current_type));

        if (m_backendConfig.type == BackendConfig::Type::Http)
        {
            ImGui::InputText("Backend URL", m_backendUrl, sizeof(m_backendUrl));
            if (ImGui::Button("Apply HTTP Config"))
                switchBackend(BackendConfig::Type::Http);
        }

        ImGui::Separator();
        ImGui::Text("View Extent:");
        ImGui::Text("Lon: %.6f to %.6f",
            m_model->spatial_extent.min_lon, m_model->spatial_extent.max_lon);
        ImGui::Text("Lat: %.6f to %.6f",
            m_model->spatial_extent.min_lat, m_model->spatial_extent.max_lat);

        ImGui::Separator();
        ImGui::Text("Server Stats:");
        if (m_hasServerStats)
        {
            ImGui::Text("Total entities: %d", m_serverStats.total_entities);
            for (const auto& [type, count] : m_serverStats.entities_by_type)
                ImGui::Text("  %s: %d", type.c_str(), count);
            if (!m_serverStats.oldest_time.empty()) {
                ImGui::Text("Coverage: %s", m_serverStats.oldest_time.substr(0, 10).c_str());
                ImGui::Text("      to: %s", m_serverStats.newest_time.substr(0, 10).c_str());
            }
            ImGui::Text("DB size: %.1f MB", m_serverStats.db_size_mb);
        }
        else
        {
            ImGui::TextDisabled("No stats available");
        }

        // Per-layer status + visibility toggles
        ImGui::Separator();
        ImGui::Text("Layers:");
        for (auto& layer : m_model->layers) {
            ImGui::PushID(layer.name.c_str());
            bool vis = layer.visible;
            if (ImGui::Checkbox("##vis", &vis))
                layer.visible = vis;
            ImGui::SameLine();
            ImVec4 col(layer.color.r, layer.color.g, layer.color.b, 1.0f);
            ImGui::ColorButton("##col", col,
                ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder,
                ImVec2(14, 14));
            ImGui::SameLine();
            size_t count = layer.entities.size();
            if (layer.is_fetching)
                ImGui::Text("%s: %zu (loading...)", layer.name.c_str(), count);
            else
                ImGui::Text("%s: %zu", layer.name.c_str(), count);
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##alpha", &layer.color.a, 0.0f, 1.0f, "alpha %.3f");
            ImGui::PopID();
        }

        // GPS progress bar
        {
            const auto& gpsLayer = m_model->layers[0];
            size_t loaded = gpsLayer.entities.size();
            size_t total  = m_model->total_expected.load();
            if (gpsLayer.is_fetching) {
                if (total > 0) {
                    float progress = static_cast<float>(loaded) / static_cast<float>(total);
                    ImGui::ProgressBar(progress, ImVec2(-1, 0));
                } else {
                    ImGui::ProgressBar(-1.0f, ImVec2(-1, 0));
                }
            } else if (m_model->initial_load_complete.load()) {
                ImGui::Text("Loaded: %zu GPS  /  %zu photos  /  %zu calendar",
                    m_model->layers[0].entities.size(),
                    m_model->layers[1].entities.size(),
                    m_model->layers[2].entities.size());
            }
        }

        ImGui::Text("Points rendered: %d", m_renderer.totalPoints());

        if (m_model->fetch_latencies.count() > 0)
        {
            ImGui::Separator();
            ImGui::Text("Fetch Latency (ms):");
            ImGui::Text("  Avg: %.1f", m_model->fetch_latencies.average());
            ImGui::Text("  Min: %.1f", m_model->fetch_latencies.min());
            ImGui::Text("  Max: %.1f", m_model->fetch_latencies.max());
            ImGui::Text("  Samples: %zu", m_model->fetch_latencies.count());
        }

        ImGui::Separator();
        ImGui::Text("Timeline Overlays:");
        bool histogramEnabled = m_timelineRenderer.histogramEnabled();
        if (ImGui::Checkbox("Histogram", &histogramEnabled))
            m_timelineRenderer.setHistogramEnabled(histogramEnabled);
        bool solarAltEnabled = m_timelineRenderer.solarAltitudeEnabled();
        if (ImGui::Checkbox("Solar Altitude", &solarAltEnabled))
            m_timelineRenderer.setSolarAltitudeEnabled(solarAltEnabled);
        bool moonAltEnabled = m_timelineRenderer.moonAltitudeEnabled();
        if (ImGui::Checkbox("Moon Altitude", &moonAltEnabled))
            m_timelineRenderer.setMoonAltitudeEnabled(moonAltEnabled);

        ImGui::Separator();
        ImGui::Text("Rendering:");

        static const char* kTileModeLabels[] = {
            "None",
            "Vector (Versatiles)",
            "OSM Standard (labeled)",
            "CartoDB Positron (labeled)",
            "CartoDB Dark Matter (labeled)",
        };
        int tileMode = static_cast<int>(m_renderer.tileMode());
        if (ImGui::Combo("Map Tiles", &tileMode, kTileModeLabels, IM_ARRAYSIZE(kTileModeLabels)))
            m_renderer.setTileMode(static_cast<TileMode>(tileMode));

        float pointSize = m_renderer.pointSize();
        if (ImGui::SliderFloat("Point Size", &pointSize, 0.01f, 1.0f, "%.1f"))
            m_renderer.setPointSize(pointSize);

        ImGui::Separator();
        if (ImGui::Button("Reload All Data"))
            startFullLoad();

        ImGui::Separator();
        ImGui::Text("Map: center (%.4f, %.4f) zoom %.4f",
                     m_camera.center().x, m_camera.center().y, m_camera.zoom());
        ImGui::Text("Timeline: zoom %.0fs", m_timelineCamera.zoom());
    }
    ImGui::End();

    // --- Entity Details Window ---
    m_interaction.drawDetailsPanel(*m_model);
}

void MainScreen::onPostGuiRender()
{
    // Helper to draw a map highlight ring for any valid PickResult
    auto drawMapHighlight = [&](PickResult pick) {
        if (!pick.valid()) return;
        if (pick.layerIndex >= static_cast<int>(m_model->layers.size())) return;
        const auto& entities = m_model->layers[pick.layerIndex].entities;
        if (pick.entityIndex >= static_cast<int>(entities.size())) return;
        const auto& e = entities[pick.entityIndex];
        if (e.has_location())
            m_renderer.drawMapHighlight(m_camera, *e.lon, *e.lat);
    };

    // Helper to draw a timeline highlight ring for any valid PickResult
    auto drawTimelineHighlight = [&](PickResult pick) {
        if (!pick.valid()) return;
        if (pick.layerIndex >= static_cast<int>(m_model->layers.size())) return;
        const auto& entities = m_model->layers[pick.layerIndex].entities;
        if (pick.entityIndex >= static_cast<int>(entities.size())) return;
        const auto& e = entities[pick.entityIndex];
        m_timelineRenderer.drawHighlight(m_timelineCamera, e.time_mid(), e.render_offset);
    };

    if (m_mapViewportValid)
    {
        int fbWidth, fbHeight;
        glfwGetFramebufferSize(m_app->window(), &fbWidth, &fbHeight);

        glViewport(m_mapViewport.x, m_mapViewport.y, m_mapViewport.w, m_mapViewport.h);
        glScissor(m_mapViewport.x, m_mapViewport.y, m_mapViewport.w, m_mapViewport.h);
        glEnable(GL_SCISSOR_TEST);

        glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        m_renderer.render(m_camera, *m_model, m_interaction.state());

        drawMapHighlight(m_interaction.hoveredMap());
        drawMapHighlight(m_interaction.selected());

        glDisable(GL_SCISSOR_TEST);
        glViewport(0, 0, fbWidth, fbHeight);
    }

    if (m_timelineViewportValid)
    {
        int fbWidth, fbHeight;
        glfwGetFramebufferSize(m_app->window(), &fbWidth, &fbHeight);

        glViewport(m_timelineViewport.x, m_timelineViewport.y,
                   m_timelineViewport.w, m_timelineViewport.h);
        glScissor(m_timelineViewport.x, m_timelineViewport.y,
                  m_timelineViewport.w, m_timelineViewport.h);
        glEnable(GL_SCISSOR_TEST);

        glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        std::vector<PointRenderer*> layerRenderers;
        for (size_t i = 0; i < m_model->layers.size(); ++i)
            layerRenderers.push_back(m_renderer.layerRenderer(i));

        m_timelineRenderer.render(m_timelineCamera, *m_model, layerRenderers);

        drawTimelineHighlight(m_interaction.hoveredTimeline());
        drawTimelineHighlight(m_interaction.selected());

        glDisable(GL_SCISSOR_TEST);
        glViewport(0, 0, fbWidth, fbHeight);
    }
}

void MainScreen::updateSpatialExtent()
{
    Vec2 center = m_camera.center();
    double zoom = m_camera.zoom();
    double aspect = static_cast<double>(m_camera.width()) / static_cast<double>(m_camera.height());

    m_model->spatial_extent.min_lon = center.x - aspect * zoom;
    m_model->spatial_extent.max_lon = center.x + aspect * zoom;
    m_model->spatial_extent.min_lat = center.y - zoom;
    m_model->spatial_extent.max_lat = center.y + zoom;

    TimeExtent visibleTime = m_timelineCamera.getTimeExtent();
    m_model->time_extent = visibleTime;
}

void MainScreen::drainCompletedBatches()
{
    std::deque<PendingBatch> batches;
    {
        std::lock_guard<std::mutex> lock(m_batchMutex);
        batches.swap(m_completedBatches);
    }

    for (auto& pb : batches) {
        if (pb.layerIndex < 0 || pb.layerIndex >= static_cast<int>(m_model->layers.size())) continue;
        auto& layerEntities = m_model->layers[pb.layerIndex].entities;
        layerEntities.reserve(layerEntities.size() + pb.entities.size());
        for (auto& entity : pb.entities)
            layerEntities.push_back(std::move(entity));
    }

    if (!batches.empty())
        m_interaction.markPickersDirty();
}

void MainScreen::startFullLoad()
{
    std::cerr << "[LOAD] startFullLoad begin\n";

    // Clear all layer data
    for (auto& layer : m_model->layers) {
        layer.entities.clear();
        layer.is_fetching = false;
    }
    m_model->initial_load_complete.store(false);
    m_model->total_expected.store(0);

    m_interaction.resetPickers();

    if (m_backendConfig.type == BackendConfig::Type::Http) {
        // --- GPS fetch (streaming NDJSON export) ---
        if (m_backends.gps) {
            auto* gps = m_backends.gps.get();
            m_model->layers[0].startFetch();
            m_pendingGpsFetch = std::async(std::launch::async, [this, gps]() {
                std::cerr << "[GPS] stream started\n";
                size_t batchNum = 0;
                gps->streamAllEntities(
                    [this](size_t total) {
                        std::cerr << "[GPS] total expected: " << total << "\n";
                        m_model->total_expected.store(total);
                    },
                    [this, &batchNum](std::vector<Entity>&& batch) {
                        ++batchNum;
                        std::cerr << "[GPS] batch " << batchNum
                                  << " size=" << batch.size() << "\n";
                        auto t = Clock::now();
                        std::lock_guard<std::mutex> lock(m_batchMutex);
                        long lockMs = ms_since(t);
                        if (lockMs > 5)
                            std::cerr << "[GPS] waited " << lockMs << "ms for batchMutex\n";
                        m_completedBatches.push_back({0, std::move(batch)});
                    });
                std::cerr << "[GPS] stream complete, " << batchNum << " batches\n";
                auto end = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    end - m_model->layers[0].last_fetch_start);
                m_model->fetch_latencies.push(static_cast<float>(duration.count()));
                m_model->layers[0].endFetch();
                m_model->initial_load_complete.store(true);
            });
        }

        // --- Type-filtered fetches (photo, calendar, google timeline) ---
        struct TypeFetch {
            int layerIndex;
            Backend* backend;
            std::future<void>* future;
            const char* tag;
        };
        TypeFetch typeFetches[] = {
            {1, m_backends.photo.get(),          &m_pendingPhotoFetch,           "PHOTO"},
            {2, m_backends.calendar.get(),       &m_pendingCalendarFetch,        "CALENDAR"},
            {3, m_backends.googleTimeline.get(),  &m_pendingGoogleTimelineFetch, "GTIMELINE"},
        };

        for (auto& tf : typeFetches) {
            if (!tf.backend) continue;
            int li = tf.layerIndex;
            Backend* be = tf.backend;
            const char* tag = tf.tag;
            m_model->layers[li].startFetch();
            *tf.future = std::async(std::launch::async, [this, be, li, tag]() {
                std::cerr << "[" << tag << "] fetch started\n";
                be->streamAllByType(
                    0.0, 2000000000.0,
                    [this, li, tag](std::vector<Entity>&& batch) {
                        std::cerr << "[" << tag << "] batch size=" << batch.size() << "\n";
                        auto t = Clock::now();
                        std::lock_guard<std::mutex> lock(m_batchMutex);
                        long lockMs = ms_since(t);
                        if (lockMs > 5)
                            std::cerr << "[" << tag << "] waited " << lockMs << "ms for batchMutex\n";
                        m_completedBatches.push_back({li, std::move(batch)});
                    });
                std::cerr << "[" << tag << "] fetch complete\n";
                m_model->layers[li].endFetch();
            });
        }
    } else {
        // Fake backend — GPS only, photos not available in fake mode
        m_model->layers[0].startFetch();
        TimeExtent fullTime = m_model->time_extent;
        SpatialExtent fullSpace;
        fullSpace.min_lat = -90.0;
        fullSpace.max_lat =  90.0;
        fullSpace.min_lon = -180.0;
        fullSpace.max_lon =  180.0;

        m_pendingGpsFetch = std::async(std::launch::async, [this, fullTime, fullSpace]() {
            m_backends.gps->fetchEntities(fullTime, fullSpace,
                [this](std::vector<Entity>&& batch) {
                    std::lock_guard<std::mutex> lock(m_batchMutex);
                    m_completedBatches.push_back({0, std::move(batch)});
                });
            m_model->layers[0].endFetch();
            m_model->initial_load_complete.store(true);
        });
    }
}

void MainScreen::fetchServerStats()
{
    m_hasServerStats = false;
    if (m_backends.gps) {
        m_serverStats = m_backends.gps->fetchStats();
        m_hasServerStats = (m_serverStats.total_entities > 0);
    }
}

void MainScreen::cancelAndWaitAll()
{
    m_backends.cancelAll();
    if (m_pendingGpsFetch.valid())             m_pendingGpsFetch.wait();
    if (m_pendingPhotoFetch.valid())           m_pendingPhotoFetch.wait();
    if (m_pendingCalendarFetch.valid())        m_pendingCalendarFetch.wait();
    if (m_pendingGoogleTimelineFetch.valid())  m_pendingGoogleTimelineFetch.wait();
}

void MainScreen::switchBackend(BackendConfig::Type type)
{
    cancelAndWaitAll();
    // Must wait for thumbnail fetch before destroying the backend it holds a pointer to
    m_interaction.waitForPhotoFetch();

    m_backendConfig.type = type;
    m_backends = createBackends(m_backendConfig, m_backendUrl);

    if (m_backends.photo) {
        Backend* pb = m_backends.photo.get();
        m_interaction.setPhotoFetcher([pb](const std::string& id) -> std::vector<uint8_t> {
            return pb->fetchPhotoThumb(id);
        });
    } else {
        m_interaction.setPhotoFetcher({});
    }

    if (type == BackendConfig::Type::Http)
        fetchServerStats();
    else
        m_hasServerStats = false;

    startFullLoad();
}
