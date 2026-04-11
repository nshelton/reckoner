#include "MainScreen.h"
#include "app/App.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <cmath>
#include <chrono>

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

    m_fpsTracker.tick(dt);

    auto t_update = Clock::now();

    { auto t = Clock::now(); updateSpatialExtent();              log_slow("updateSpatialExtent", t); }
    {
        auto t = Clock::now();
        if (m_fetchOrchestrator.drainCompletedBatches(*m_model))
            m_interaction.markPickersDirty();
        log_slow("drainCompletedBatches", t);
    }
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
    m_fetchOrchestrator.cancelAndWaitAll();
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
    ControlsActions actions;
    m_controlsPanel.draw(
        m_fpsTracker, *m_model, m_backendConfig,
        m_backendUrl, sizeof(m_backendUrl),
        m_serverStats, m_hasServerStats,
        m_renderer, m_timelineRenderer,
        m_camera, m_timelineCamera,
        actions);
    applyControlsActions(actions);

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

void MainScreen::applyControlsActions(const ControlsActions& actions)
{
    if (actions.resetMap) m_camera.reset();
    if (actions.resetTimeline) m_timelineCamera.reset();

    if (actions.switchBackendType >= 0)
        switchBackend(static_cast<BackendConfig::Type>(actions.switchBackendType));
    if (actions.applyHttpConfig)
        switchBackend(BackendConfig::Type::Http);

    if (actions.tileMode >= 0)
        m_renderer.setTileMode(static_cast<TileMode>(actions.tileMode));
    if (actions.pointSize >= 0.0f)
        m_renderer.setPointSize(actions.pointSize);

    if (actions.histogram >= 0)
        m_timelineRenderer.setHistogramEnabled(actions.histogram != 0);
    if (actions.solarAltitude >= 0)
        m_timelineRenderer.setSolarAltitudeEnabled(actions.solarAltitude != 0);
    if (actions.moonAltitude >= 0)
        m_timelineRenderer.setMoonAltitudeEnabled(actions.moonAltitude != 0);

    if (actions.reloadAllData) {
        m_fetchOrchestrator.cancelAndWaitAll();
        m_interaction.resetPickers();
        m_fetchOrchestrator.startFullLoad(*m_model);
    }
}

void MainScreen::switchBackend(BackendConfig::Type type)
{
    m_fetchOrchestrator.cancelAndWaitAll();
    m_interaction.waitForPhotoFetch();

    m_backendConfig.type = type;
    m_fetchOrchestrator.setBackends(createBackends(m_backendConfig, m_backendUrl), type);

    auto& backends = m_fetchOrchestrator.backends();
    if (backends.photo) {
        Backend* pb = backends.photo.get();
        m_interaction.setPhotoFetcher([pb](const std::string& id) -> std::vector<uint8_t> {
            return pb->fetchPhotoThumb(id);
        });
    } else {
        m_interaction.setPhotoFetcher({});
    }

    if (type == BackendConfig::Type::Http) {
        auto [stats, ok] = m_fetchOrchestrator.fetchServerStats();
        m_serverStats = stats;
        m_hasServerStats = ok;
    } else {
        m_hasServerStats = false;
    }

    m_interaction.resetPickers();
    m_fetchOrchestrator.startFullLoad(*m_model);
}
