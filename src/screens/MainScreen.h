#pragma once

#include "app/Screen.h"
#include <imgui.h>
#include "Camera.h"
#include "Renderer.h"
#include "AppModel.h"
#include "Backend.h"
#include "FakeBackend.h"
#include <memory>
#include <future>

class MainScreen : public IScreen
{
public:
    void onAttach(App &app) override;
    void onResize(int width, int height) override;
    void onUpdate(double dt) override;
    void onRender() override;
    void onDetach() override;
    void onMouseButton(int button, int action, int mods, Vec2 px) override;
    void onCursorPos(Vec2 px) override;
    void onScroll(double xoffset, double yoffset, Vec2 px) override;
    void onGui() override;
    void onPostGuiRender() override;
    void onFilesDropped(const std::vector<std::string>& paths) override;

private:
    App *m_app{nullptr};
    Camera m_camera{};
    Renderer m_renderer{};
    InteractionController m_interaction{};
    AppModel m_model{};

    // Backend
    std::unique_ptr<Backend> m_backend;
    std::future<void> m_pendingFetch;

    // Cached window sizes
    ImVec2 m_lastMapSize{0, 0};
    ImVec2 m_lastTimelineSize{0, 0};

    // Viewport in framebuffer coordinates for drawing after ImGui (so content is visible)
    struct ViewportRect { int x{0}, y{0}, w{0}, h{0}; };
    ViewportRect m_mapViewport;
    ViewportRect m_timelineViewport;
    bool m_mapViewportValid{false};
    bool m_timelineViewportValid{false};

    // Cached extents for change detection
    SpatialExtent m_lastSpatialExtent;
    TimeExtent m_lastTimeExtent;

    // Methods
    void refreshDataIfExtentChanged();
    void fetchData();
};
