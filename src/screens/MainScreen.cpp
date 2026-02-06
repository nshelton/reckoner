#include "MainScreen.h"
#include "app/App.h"
#include <GLFW/glfw3.h>
#include <iostream>

void MainScreen::onAttach(App &app)
{
    m_app = &app;
    std::cout << "MainScreen attached" << std::endl;
}

void MainScreen::onResize(int width, int height)
{
    m_renderer.setSize(width, height);
    m_camera.setSize(width, height);
}

void MainScreen::onUpdate(double /*dt*/)
{
    // Nothing to update yet
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
    // Map Window
    ImGui::Begin("Map");
    {
        ImVec2 contentSize = ImGui::GetContentRegionAvail();

        // Update camera size only if window size changed
        if (contentSize.x != m_lastMapSize.x || contentSize.y != m_lastMapSize.y)
        {
            m_camera.setSize(static_cast<int>(contentSize.x), static_cast<int>(contentSize.y));
            m_lastMapSize = contentSize;
        }

        // Render the map content
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

        // Render the grid
        m_renderer.render(m_camera, m_model, m_interaction.state());

        // Display camera info
        ImGui::SetCursorPos(ImVec2(10, 10));
        ImGui::BeginChild("MapInfo", ImVec2(200, 80), true);
        ImGui::Text("Map View");
        Vec2 center = m_camera.center();
        ImGui::Text("Center: %.1f, %.1f", center.x, center.y);
        ImGui::Text("Zoom: %.2f", m_camera.zoom());
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
    }
    ImGui::End();
}
