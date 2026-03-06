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
    switchBackend(m_backendType);
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
    // Cancel any in-progress fetches
    if (m_backendType == BackendType::Http) {
        if (auto* h = dynamic_cast<HttpBackend*>(m_backend.get()))                 h->cancelFetch();
        if (auto* h = dynamic_cast<HttpBackend*>(m_photoBackend.get()))            h->cancelFetch();
        if (auto* h = dynamic_cast<HttpBackend*>(m_calendarBackend.get()))         h->cancelFetch();
        if (auto* h = dynamic_cast<HttpBackend*>(m_googleTimelineBackend.get()))   h->cancelFetch();
    }
    if (m_pendingGpsFetch.valid())             m_pendingGpsFetch.wait();
    if (m_pendingPhotoFetch.valid())           m_pendingPhotoFetch.wait();
    if (m_pendingCalendarFetch.valid())        m_pendingCalendarFetch.wait();
    if (m_pendingGoogleTimelineFetch.valid())  m_pendingGoogleTimelineFetch.wait();
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
        int current_type = static_cast<int>(m_backendType);
        if (ImGui::Combo("Backend Type", &current_type, backend_types, 2))
            switchBackend(static_cast<BackendType>(current_type));

        if (m_backendType == BackendType::Http)
        {
            ImGui::InputText("Backend URL", m_backendUrl, sizeof(m_backendUrl));
            if (ImGui::Button("Apply HTTP Config"))
                switchBackend(BackendType::Http);
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

    // Cancel any in-progress fetches
    if (m_backendType == BackendType::Http) {
        if (auto* h = dynamic_cast<HttpBackend*>(m_backend.get()))                h->cancelFetch();
        if (auto* h = dynamic_cast<HttpBackend*>(m_photoBackend.get()))           h->cancelFetch();
        if (auto* h = dynamic_cast<HttpBackend*>(m_calendarBackend.get()))        h->cancelFetch();
        if (auto* h = dynamic_cast<HttpBackend*>(m_googleTimelineBackend.get()))  h->cancelFetch();
    }
    if (m_pendingGpsFetch.valid()) {
        std::cerr << "[LOAD] waiting for GPS fetch to cancel...\n";
        auto t = Clock::now();
        m_pendingGpsFetch.wait();
        std::cerr << "[LOAD] GPS fetch done after " << ms_since(t) << "ms\n";
    }
    if (m_pendingPhotoFetch.valid()) {
        std::cerr << "[LOAD] waiting for photo fetch to cancel...\n";
        auto t = Clock::now();
        m_pendingPhotoFetch.wait();
        std::cerr << "[LOAD] photo fetch done after " << ms_since(t) << "ms\n";
    }
    if (m_pendingCalendarFetch.valid()) {
        std::cerr << "[LOAD] waiting for calendar fetch to cancel...\n";
        auto t = Clock::now();
        m_pendingCalendarFetch.wait();
        std::cerr << "[LOAD] calendar fetch done after " << ms_since(t) << "ms\n";
    }
    if (m_pendingGoogleTimelineFetch.valid()) {
        std::cerr << "[LOAD] waiting for google timeline fetch to cancel...\n";
        auto t = Clock::now();
        m_pendingGoogleTimelineFetch.wait();
        std::cerr << "[LOAD] google timeline fetch done after " << ms_since(t) << "ms\n";
    }

    // Wait for any in-flight thumbnail before resetting (backend pointer still valid here)
    m_interaction.waitForPhotoFetch();

    // Clear all layer data
    for (auto& layer : m_model->layers) {
        layer.entities.clear();
        layer.is_fetching = false;
    }
    m_model->initial_load_complete.store(false);
    m_model->total_expected.store(0);

    m_interaction.resetPickers();

    if (m_backendType == BackendType::Http) {
        // --- GPS fetch (streaming NDJSON export) ---
        auto* gpsBackend = dynamic_cast<HttpBackend*>(m_backend.get());
        if (gpsBackend) {
            m_model->layers[0].startFetch();
            m_pendingGpsFetch = std::async(std::launch::async, [this, gpsBackend]() {
                std::cerr << "[GPS] stream started\n";
                size_t batchNum = 0;
                gpsBackend->streamAllEntities(
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

        // --- Photo fetch (time-range query, type-filtered) ---
        auto* photoBackend = dynamic_cast<HttpBackend*>(m_photoBackend.get());
        if (photoBackend) {
            m_model->layers[1].startFetch();
            m_pendingPhotoFetch = std::async(std::launch::async, [this, photoBackend]() {
                std::cerr << "[PHOTO] fetch started\n";
                // Full time range: epoch (1970) to 2033
                photoBackend->streamAllByType(
                    0.0, 2000000000.0,
                    [this](std::vector<Entity>&& batch) {
                        std::cerr << "[PHOTO] batch size=" << batch.size() << "\n";
                        auto t = Clock::now();
                        std::lock_guard<std::mutex> lock(m_batchMutex);
                        long lockMs = ms_since(t);
                        if (lockMs > 5)
                            std::cerr << "[PHOTO] waited " << lockMs << "ms for batchMutex\n";
                        m_completedBatches.push_back({1, std::move(batch)});
                    });
                std::cerr << "[PHOTO] fetch complete\n";
                m_model->layers[1].endFetch();
            });
        }

        // --- Calendar fetch (time-range query, type-filtered) ---
        auto* calendarBackend = dynamic_cast<HttpBackend*>(m_calendarBackend.get());
        if (calendarBackend) {
            m_model->layers[2].startFetch();
            m_pendingCalendarFetch = std::async(std::launch::async, [this, calendarBackend]() {
                std::cerr << "[CALENDAR] fetch started\n";
                calendarBackend->streamAllByType(
                    0.0, 2000000000.0,
                    [this](std::vector<Entity>&& batch) {
                        std::cerr << "[CALENDAR] batch size=" << batch.size() << "\n";
                        auto t = Clock::now();
                        std::lock_guard<std::mutex> lock(m_batchMutex);
                        long lockMs = ms_since(t);
                        if (lockMs > 5)
                            std::cerr << "[CALENDAR] waited " << lockMs << "ms for batchMutex\n";
                        m_completedBatches.push_back({2, std::move(batch)});
                    });
                std::cerr << "[CALENDAR] fetch complete\n";
                m_model->layers[2].endFetch();
            });
        }

        // --- Google Timeline fetch (time-range query, type-filtered) ---
        auto* gtBackend = dynamic_cast<HttpBackend*>(m_googleTimelineBackend.get());
        if (gtBackend) {
            m_model->layers[3].startFetch();
            m_pendingGoogleTimelineFetch = std::async(std::launch::async, [this, gtBackend]() {
                std::cerr << "[GTIMELINE] fetch started\n";
                gtBackend->streamAllByType(
                    0.0, 2000000000.0,
                    [this](std::vector<Entity>&& batch) {
                        std::cerr << "[GTIMELINE] batch size=" << batch.size() << "\n";
                        auto t = Clock::now();
                        std::lock_guard<std::mutex> lock(m_batchMutex);
                        long lockMs = ms_since(t);
                        if (lockMs > 5)
                            std::cerr << "[GTIMELINE] waited " << lockMs << "ms for batchMutex\n";
                        m_completedBatches.push_back({3, std::move(batch)});
                    });
                std::cerr << "[GTIMELINE] fetch complete\n";
                m_model->layers[3].endFetch();
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
            m_backend->fetchEntities(fullTime, fullSpace,
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
    if (m_backendType == BackendType::Http) {
        auto* httpBackend = dynamic_cast<HttpBackend*>(m_backend.get());
        if (httpBackend) {
            m_serverStats = httpBackend->fetchStats();
            m_hasServerStats = true;
        }
    }
}

void MainScreen::switchBackend(BackendType type)
{
    if (m_backendType == BackendType::Http && m_backend) {
        if (auto* h = dynamic_cast<HttpBackend*>(m_backend.get()))                h->cancelFetch();
        if (auto* h = dynamic_cast<HttpBackend*>(m_photoBackend.get()))           h->cancelFetch();
        if (auto* h = dynamic_cast<HttpBackend*>(m_calendarBackend.get()))        h->cancelFetch();
        if (auto* h = dynamic_cast<HttpBackend*>(m_googleTimelineBackend.get()))  h->cancelFetch();
    }
    if (m_pendingGpsFetch.valid())             m_pendingGpsFetch.wait();
    if (m_pendingPhotoFetch.valid())           m_pendingPhotoFetch.wait();
    if (m_pendingCalendarFetch.valid())        m_pendingCalendarFetch.wait();
    if (m_pendingGoogleTimelineFetch.valid())  m_pendingGoogleTimelineFetch.wait();
    // Must wait for thumbnail fetch before destroying the backend it holds a pointer to
    m_interaction.waitForPhotoFetch();

    m_backendType = type;

    if (type == BackendType::Fake)
    {
        m_backend                = std::make_unique<FakeBackend>(1000);
        m_photoBackend           = nullptr;
        m_calendarBackend        = nullptr;
        m_googleTimelineBackend  = nullptr;
        m_interaction.setPhotoFetcher({});  // no photo thumbnails in fake mode
        m_hasServerStats = false;
    }
    else
    {
        m_backend                = std::make_unique<HttpBackend>(m_backendUrl, "location.gps");
        m_photoBackend           = std::make_unique<HttpBackend>(m_backendUrl, "photo");
        m_calendarBackend        = std::make_unique<HttpBackend>(m_backendUrl, "calendar.event");
        m_googleTimelineBackend  = std::make_unique<HttpBackend>(m_backendUrl, "location.googletimeline");
        auto* pb = dynamic_cast<HttpBackend*>(m_photoBackend.get());
        m_interaction.setPhotoFetcher([pb](const std::string& id) -> std::vector<uint8_t> {
            return pb ? pb->fetchPhotoThumb(id) : std::vector<uint8_t>{};
        });
        fetchServerStats();
    }

    startFullLoad();
}
