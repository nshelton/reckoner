#include "MainScreen.h"
#include "app/App.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <cmath>
#include <future>

void MainScreen::onAttach(App &app)
{
    m_app = &app;

    // Initialize backend based on default type
    switchBackend(m_backendType);
}

void MainScreen::onResize(int width, int height)
{
    // Note: Camera size is set per-window in onGui()
    // Don't set it here as it would override the per-window settings
}

void MainScreen::onUpdate(double dt)
{
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

    // Update spatial extent from camera (for rendering, not fetching)
    updateSpatialExtent();

    // Drain completed entity batches from background thread
    drainCompletedBatches();
}

void MainScreen::onRender()
{
    // Main rendering happens in the ImGui windows
}

void MainScreen::onDetach()
{
    // Cancel any in-progress fetch
    if (m_backendType == BackendType::Http) {
        auto* httpBackend = dynamic_cast<HttpBackend*>(m_backend.get());
        if (httpBackend) httpBackend->cancelFetch();
    }
    if (m_pendingFetch.valid()) m_pendingFetch.wait();

    m_renderer.shutdown();
    m_timelineRenderer.shutdown();
}

void MainScreen::onFilesDropped(const std::vector<std::string> & /*paths*/)
{
    // Not implemented yet
}

void MainScreen::onMouseButton(int button, int action, int /*mods*/, Vec2 px)
{
    // Mouse handling will be done per-window
    Vec2 mouseWorld = m_camera.screenToWorld(px);

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        m_interaction.onMouseDown(m_model, m_camera, mouseWorld);
    }
    else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
    {
        m_interaction.onMouseUp();
    }
}

void MainScreen::onCursorPos(Vec2 px)
{
    Vec2 mouseWorld = m_camera.screenToWorld(px);
    m_interaction.onCursorPos(m_model, m_camera, mouseWorld);
}

void MainScreen::onScroll(double /*xoffset*/, double yoffset, Vec2 px)
{
    m_interaction.onScroll(m_model, m_camera, static_cast<float>(yoffset), px);
}

void MainScreen::onGui()
{
    m_mapViewportValid = false;
    m_timelineViewportValid = false;

    // Map Window
    ImGui::Begin("Map");
    {
        ImVec2 contentSize = ImGui::GetContentRegionAvail();

        // Cache size for change detection
        if (contentSize.x != m_lastMapSize.x || contentSize.y != m_lastMapSize.y)
        {
            m_lastMapSize = contentSize;
        }

        // Get the position where we'll render
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();

        // Create an invisible button to capture mouse input
        ImGui::InvisibleButton("MapCanvas", contentSize);

        // Handle mouse input for this window
        if (ImGui::IsItemHovered())
        {
            ImGuiIO &io = ImGui::GetIO();

            // Pan with left mouse drag
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                ImVec2 delta = io.MouseDelta;
                // Convert screen delta to world delta
                Vec2 worldDelta = m_camera.screenToWorld(Vec2(-delta.x, -delta.y)) -
                                  m_camera.screenToWorld(Vec2(0, 0));
                m_camera.move(worldDelta);
            }

            // Zoom with scroll wheel
            if (io.MouseWheel != 0.0f)
            {
                ImVec2 mousePos = ImGui::GetMousePos();
                Vec2 pixelPos(mousePos.x - cursorPos.x, mousePos.y - cursorPos.y);
                m_camera.zoomAtPixel(pixelPos, io.MouseWheel);
            }
        }

        // Set up OpenGL viewport and scissor for this ImGui window
        int fbWidth, fbHeight;
        glfwGetFramebufferSize(m_app->window(), &fbWidth, &fbHeight);

        ImGuiIO &io = ImGui::GetIO();
        float fbScale = fbWidth / io.DisplaySize.x;

        m_mapViewport.x = static_cast<int>(cursorPos.x * fbScale);
        m_mapViewport.y = static_cast<int>((io.DisplaySize.y - cursorPos.y - contentSize.y) * fbScale);
        m_mapViewport.w = static_cast<int>(contentSize.x * fbScale);
        m_mapViewport.h = static_cast<int>(contentSize.y * fbScale);
        m_mapViewportValid = true;

        // IMPORTANT: Set camera size right before rendering
        m_camera.setSize(static_cast<int>(contentSize.x), static_cast<int>(contentSize.y));
    }
    ImGui::End();

    // Timeline Window
    ImGui::Begin("Timeline");
    {
        ImVec2 contentSize = ImGui::GetContentRegionAvail();
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();

        ImGui::InvisibleButton("TimelineCanvas", contentSize);

        if (ImGui::IsItemHovered())
        {
            ImGuiIO &tio = ImGui::GetIO();

            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                m_timelineCamera.panByPixels(tio.MouseDelta.x);
            }

            if (tio.MouseWheel != 0.0f)
            {
                ImVec2 mousePos = ImGui::GetMousePos();
                float localX = mousePos.x - cursorPos.x;
                m_timelineCamera.zoomAtPixel(localX, tio.MouseWheel);
            }
        }

        // Compute framebuffer viewport
        int fbWidth, fbHeight;
        glfwGetFramebufferSize(m_app->window(), &fbWidth, &fbHeight);
        ImGuiIO &tio = ImGui::GetIO();
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

    // Controls Window
    ImGui::Begin("Controls");
    {


        ImGui::Text("%.1f FPS  (%.2f ms)", m_fps, m_frameMs);

        ImGui::Separator();
        if (ImGui::Button("Reset Map"))
        {
            m_camera.reset();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset Timeline"))
        {
            m_timelineCamera.reset();
        }

        ImGui::Separator();
        ImGui::Text("Backend Configuration:");

        // Backend type selector
        const char *backend_types[] = {"Fake Data", "HTTP Backend"};
        int current_type = static_cast<int>(m_backendType);
        if (ImGui::Combo("Backend Type", &current_type, backend_types, 2))
        {
            switchBackend(static_cast<BackendType>(current_type));
        }

        // HTTP backend configuration
        if (m_backendType == BackendType::Http)
        {

            ImGui::InputText("Backend URL", m_backendUrl, sizeof(m_backendUrl));
            ImGui::InputText("Entity Type", m_entityType, sizeof(m_entityType));
            if (ImGui::Button("Apply HTTP Config"))
            {
                switchBackend(BackendType::Http);
            }
        }

        ImGui::Separator();
        ImGui::Text("View Extent:");
        ImGui::Text("Lon: %.6f to %.6f",
            m_model.spatial_extent.min_lon,
            m_model.spatial_extent.max_lon);
        ImGui::Text("Lat: %.6f to %.6f",
            m_model.spatial_extent.min_lat,
            m_model.spatial_extent.max_lat);

        ImGui::Separator();
        ImGui::Text("Server Stats:");
        if (m_hasServerStats)
        {
            ImGui::Text("Total entities: %d", m_serverStats.total_entities);
            for (const auto& [type, count] : m_serverStats.entities_by_type) {
                ImGui::Text("  %s: %d", type.c_str(), count);
            }
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

        // Loading progress
        ImGui::Separator();
        size_t loaded = m_model.entities.size();
        size_t total = m_model.total_expected.load();

        if (m_model.is_fetching)
        {
            if (total > 0) {
                float progress = static_cast<float>(loaded) / static_cast<float>(total);
                ImGui::ProgressBar(progress, ImVec2(-1, 0));
                ImGui::Text("Loading: %zu / %zu entities", loaded, total);
            } else {
                ImGui::ProgressBar(0.0f, ImVec2(-1, 0));
                ImGui::Text("Loading: %zu entities...", loaded);
            }
        }
        else if (m_model.initial_load_complete.load())
        {
            ImGui::Text("Loaded: %zu entities", loaded);
        }
        else
        {
            ImGui::Text("Entities: %zu", loaded);
        }

        ImGui::Text("Points rendered: %d", m_renderer.totalPoints());

        // Latency ring buffer stats
        if (m_model.fetch_latencies.count() > 0)
        {
            ImGui::Separator();
            ImGui::Text("Fetch Latency (ms):");
            ImGui::Text("  Avg: %.1f", m_model.fetch_latencies.average());
            ImGui::Text("  Min: %.1f", m_model.fetch_latencies.min());
            ImGui::Text("  Max: %.1f", m_model.fetch_latencies.max());
            ImGui::Text("  Samples: %zu", m_model.fetch_latencies.count());
        }

        ImGui::Separator();
        ImGui::Text("Rendering:");
        bool tilesEnabled = m_renderer.tilesEnabled();
        if (ImGui::Checkbox("Show Map Tiles", &tilesEnabled)) {
            m_renderer.setTilesEnabled(tilesEnabled);
        }
        float pointSize = m_renderer.pointSize();
        if (ImGui::SliderFloat("Point Size", &pointSize, 0.01f, 1.0f, "%.1f"))
        {
            m_renderer.setPointSize(pointSize);
        }

        ImGui::Separator();
        if (ImGui::Button("Reload All Data"))
        {
            startFullLoad();
        }

        ImGui::Separator();
        ImGui::Text("Map: center (%.4f, %.4f) zoom %.4f",
                     m_camera.center().x, m_camera.center().y, m_camera.zoom());
        ImGui::Text("Timeline: zoom %.0fs", m_timelineCamera.zoom());
    }
    ImGui::End();
}

void MainScreen::onPostGuiRender()
{
    if (m_mapViewportValid)
    {
        int fbWidth, fbHeight;
        glfwGetFramebufferSize(m_app->window(), &fbWidth, &fbHeight);

        // Set viewport and enable scissor test to clip to this region
        glViewport(m_mapViewport.x, m_mapViewport.y, m_mapViewport.w, m_mapViewport.h);
        glScissor(m_mapViewport.x, m_mapViewport.y, m_mapViewport.w, m_mapViewport.h);
        glEnable(GL_SCISSOR_TEST);

        // Clear background to dark gray
        glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Render the grid and entities
        m_renderer.render(m_camera, m_model, m_interaction.state());

        // Disable scissor test
        glDisable(GL_SCISSOR_TEST);

        // Reset viewport to full window
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

        m_timelineRenderer.render(m_timelineCamera, m_model);

        glDisable(GL_SCISSOR_TEST);
        glViewport(0, 0, fbWidth, fbHeight);
    }
}

void MainScreen::updateSpatialExtent()
{
    // Camera now works directly in lat/lon coordinates!
    Vec2 center = m_camera.center();
    double zoom = m_camera.zoom();
    double aspect = static_cast<double>(m_camera.width()) / static_cast<double>(m_camera.height());

    m_model.spatial_extent.min_lon = center.x - aspect * zoom;
    m_model.spatial_extent.max_lon = center.x + aspect * zoom;
    m_model.spatial_extent.min_lat = center.y - zoom;
    m_model.spatial_extent.max_lat = center.y + zoom;

    // Sync time extent from timeline camera
    TimeExtent visibleTime = m_timelineCamera.getTimeExtent();
    m_model.time_extent = visibleTime;
}

void MainScreen::drainCompletedBatches()
{
    std::deque<std::vector<Entity>> batches;
    {
        std::lock_guard<std::mutex> lock(m_batchMutex);
        batches.swap(m_completedBatches);
    }

    for (auto& batch : batches) {
        m_model.entities.reserve(m_model.entities.size() + batch.size());
        for (auto& entity : batch) {
            m_model.entities.push_back(std::move(entity));
        }
    }
}

void MainScreen::startFullLoad()
{
    // Cancel any in-progress fetch
    if (m_backendType == BackendType::Http) {
        auto* httpBackend = dynamic_cast<HttpBackend*>(m_backend.get());
        if (httpBackend) httpBackend->cancelFetch();
    }
    if (m_pendingFetch.valid()) m_pendingFetch.wait();

    // Clear existing data
    m_model.entities.clear();
    m_model.initial_load_complete.store(false);
    m_model.is_fetching = true;
    m_model.startFetch();

    // Use full time range for loading everything
    TimeExtent fullTime = m_model.time_extent;
    // Use a huge spatial extent to get everything
    SpatialExtent fullSpace;
    fullSpace.min_lat = -90.0;
    fullSpace.max_lat = 90.0;
    fullSpace.min_lon = -180.0;
    fullSpace.max_lon = 180.0;

    if (m_backendType == BackendType::Http) {
        auto* httpBackend = dynamic_cast<HttpBackend*>(m_backend.get());
        if (!httpBackend) return;

        m_pendingFetch = std::async(std::launch::async, [this, httpBackend, fullTime, fullSpace]() {
            httpBackend->fetchAllEntities(fullTime, fullSpace,
                [this](std::vector<Entity>&& batch) {
                    std::lock_guard<std::mutex> lock(m_batchMutex);
                    m_completedBatches.push_back(std::move(batch));
                });
            m_model.endFetch();
            m_model.initial_load_complete.store(true);
        });
    } else {
        // Fake backend: single fetch
        m_pendingFetch = std::async(std::launch::async, [this, fullTime, fullSpace]() {
            m_backend->fetchEntities(fullTime, fullSpace,
                [this](std::vector<Entity>&& batch) {
                    std::lock_guard<std::mutex> lock(m_batchMutex);
                    m_completedBatches.push_back(std::move(batch));
                });
            m_model.endFetch();
            m_model.initial_load_complete.store(true);
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
            m_model.total_expected.store(static_cast<size_t>(m_serverStats.total_entities));
        }
    }
}

void MainScreen::switchBackend(BackendType type)
{
    // Cancel any in-progress fetch
    if (m_backendType == BackendType::Http && m_backend) {
        auto* httpBackend = dynamic_cast<HttpBackend*>(m_backend.get());
        if (httpBackend) httpBackend->cancelFetch();
    }
    if (m_pendingFetch.valid()) m_pendingFetch.wait();

    m_backendType = type;

    if (type == BackendType::Fake)
    {
        m_backend = std::make_unique<FakeBackend>(1000);
        m_hasServerStats = false;
    }
    else
    {
        m_backend = std::make_unique<HttpBackend>(m_backendUrl, m_entityType);
        fetchServerStats();
    }

    // Start loading all data
    startFullLoad();
}
