#include "MainScreen.h"
#include "app/App.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <cmath>
#include <future>

void MainScreen::onAttach(App &app)
{
    m_app = &app;
    std::cout << "MainScreen attached" << std::endl;

    // Initialize backend with 1000 points
    m_backend = std::make_unique<FakeBackend>(1000);

    // Cache initial extents
    m_lastSpatialExtent = m_model.spatial_extent;
    m_lastTimeExtent = m_model.time_extent;

    // Do initial fetch
    fetchData();
}

void MainScreen::onResize(int width, int height)
{
    // Note: Camera size is set per-window in onGui()
    // Don't set it here as it would override the per-window settings
}

void MainScreen::onUpdate(double /*dt*/)
{
    // Check if extent changed and trigger refresh
    refreshDataIfExtentChanged();
}

void MainScreen::onRender()
{
    // Main rendering happens in the ImGui windows
}

void MainScreen::onDetach()
{
    m_renderer.shutdown();
}

void MainScreen::onFilesDropped(const std::vector<std::string>& /*paths*/)
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
            ImGuiIO& io = ImGui::GetIO();

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

        ImGuiIO& io = ImGui::GetIO();
        float fbScale = fbWidth / io.DisplaySize.x;

        m_mapViewport.x = static_cast<int>(cursorPos.x * fbScale);
        m_mapViewport.y = static_cast<int>((io.DisplaySize.y - cursorPos.y - contentSize.y) * fbScale);
        m_mapViewport.w = static_cast<int>(contentSize.x * fbScale);
        m_mapViewport.h = static_cast<int>(contentSize.y * fbScale);
        m_mapViewportValid = true;

        // IMPORTANT: Set camera size right before rendering
        m_camera.setSize(static_cast<int>(contentSize.x), static_cast<int>(contentSize.y));

        // Display camera info and controls
        ImGui::SetCursorPos(ImVec2(10, 10));
        ImGui::BeginChild("MapInfo", ImVec2(220, 100), true);
        ImGui::Text("Map View");
        Vec2 center = m_camera.center();
        ImGui::Text("Center: %.1f, %.1f", center.x, center.y);
        ImGui::Text("Zoom: %.2f", m_camera.zoom());
        if (ImGui::Button("Reset View")) {
            m_camera.reset();
        }
        ImGui::EndChild();
    }
    ImGui::End();

    // Timeline Window
    ImGui::Begin("Timeline");
    {
        ImVec2 contentSize = ImGui::GetContentRegionAvail();

        // Placeholder for timeline rendering
        ImGui::Text("Timeline View");
        ImGui::Separator();
        ImGui::Text("Size: %.0f x %.0f", contentSize.x, contentSize.y);

        // TODO: Implement timeline rendering
        // - Time axis with nice intervals
        // - Events as bars or ticks
        // - Pan/zoom in time dimension
    }
    ImGui::End();

    // Controls Window
    ImGui::Begin("Controls");
    {
        ImGui::Text("Spatiotemporal Visualizer");
        ImGui::Separator();

        ImGui::Text("Map Controls:");
        ImGui::BulletText("Left drag: Pan");
        ImGui::BulletText("Scroll: Zoom");

        ImGui::Separator();
        if (ImGui::Button("Reset View"))
        {
            m_camera.reset();
        }

        ImGui::Separator();
        ImGui::Text("Backend Stats:");
        ImGui::Text("Entities: %zu", m_model.entities.size());
        ImGui::Text("Points rendered: %d", m_renderer.totalPoints());

        // Debug: show first entity location if any
        if (!m_model.entities.empty() && m_model.entities[0].has_location()) {
            auto& e = m_model.entities[0];
            ImGui::Text("First entity:");
            ImGui::Text("  lat: %.6f, lon: %.6f", *e.lat, *e.lon);
            Vec2 norm = m_model.spatial_extent.to_normalized(*e.lat, *e.lon);
            ImGui::Text("  norm: %.3f, %.3f", norm.x, norm.y);
            Vec2 world(norm.x * 100.0f, norm.y * 100.0f);
            ImGui::Text("  world: %.1f, %.1f", world.x, world.y);
        }

        if (m_model.is_fetching) {
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "Fetching...");
        } else {
            ImGui::Text("Status: Ready");
        }

        // Latency ring buffer stats
        if (m_model.fetch_latencies.count() > 0) {
            ImGui::Separator();
            ImGui::Text("Fetch Latency (ms):");
            ImGui::Text("  Avg: %.1f", m_model.fetch_latencies.average());
            ImGui::Text("  Min: %.1f", m_model.fetch_latencies.min());
            ImGui::Text("  Max: %.1f", m_model.fetch_latencies.max());
            ImGui::Text("  Samples: %zu", m_model.fetch_latencies.count());
        }

        ImGui::Separator();
        if (ImGui::Button("Refresh Data")) {
            fetchData();
        }
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
}

void MainScreen::refreshDataIfExtentChanged()
{
    // Update spatial extent from camera
    Vec2 center = m_camera.center();
    double zoom = m_camera.zoom();
    double half_width = 50.0 / zoom;  // Our world is 100 units
    double half_height = 50.0 / zoom;

    m_model.spatial_extent.min_lon = -118.3 + (center.x / 100.0) * 0.1 - half_width * 0.001;
    m_model.spatial_extent.max_lon = -118.3 + (center.x / 100.0) * 0.1 + half_width * 0.001;
    m_model.spatial_extent.min_lat = 34.0 + (center.y / 100.0) * 0.1 - half_height * 0.001;
    m_model.spatial_extent.max_lat = 34.0 + (center.y / 100.0) * 0.1 + half_height * 0.001;

    // Check if extent changed significantly (more than 1%)
    bool spatial_changed =
        std::abs(m_model.spatial_extent.min_lat - m_lastSpatialExtent.min_lat) > 0.001 ||
        std::abs(m_model.spatial_extent.max_lat - m_lastSpatialExtent.max_lat) > 0.001 ||
        std::abs(m_model.spatial_extent.min_lon - m_lastSpatialExtent.min_lon) > 0.001 ||
        std::abs(m_model.spatial_extent.max_lon - m_lastSpatialExtent.max_lon) > 0.001;

    bool time_changed =
        std::abs(m_model.time_extent.start - m_lastTimeExtent.start) > 100.0 ||
        std::abs(m_model.time_extent.end - m_lastTimeExtent.end) > 100.0;

    if ((spatial_changed || time_changed) && !m_model.is_fetching) {
        m_lastSpatialExtent = m_model.spatial_extent;
        m_lastTimeExtent = m_model.time_extent;
        fetchData();
    }
}

void MainScreen::fetchData()
{
    if (m_model.is_fetching) return;  // Already fetching

    m_model.startFetch();

    // Launch async fetch
    m_pendingFetch = std::async(std::launch::async, [this]() {
        m_backend->fetchEntities(
            m_model.time_extent,
            m_model.spatial_extent,
            [this](std::vector<Entity>&& entities) {
                // Update model on main thread (will be read in next frame)
                m_model.entities = std::move(entities);
                m_model.endFetch();
            }
        );
    });
}
