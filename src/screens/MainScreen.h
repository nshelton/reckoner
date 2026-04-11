#pragma once

#include "app/Screen.h"
#include <imgui.h>
#include "Camera.h"
#include "Renderer.h"
#include "Interaction.h"
#include "TimelineCamera.h"
#include "TimelineRenderer.h"
#include "AppModel.h"
#include "FetchOrchestrator.h"
#include "gui/ControlsPanel.h"
#include "core/FpsTracker.h"
#include <string>
#include <vector>

class MainScreen : public IScreen
{
public:
    explicit MainScreen(AppModel* model);

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
    TimelineCamera m_timelineCamera{};
    TimelineRenderer m_timelineRenderer{};
    AppModel* m_model{nullptr};

    // Fetch orchestration — owns backends, futures, and batch queue
    BackendConfig m_backendConfig{BackendConfig::Type::Http};
    FetchOrchestrator m_fetchOrchestrator;

    // Backend URL buffer for ImGui input
    char m_backendUrl[256] = "http://n3k0.local:8000";

    // Cached window sizes
    ImVec2 m_lastMapSize{0, 0};
    ImVec2 m_lastTimelineSize{0, 0};

    // Viewport in framebuffer coordinates for drawing after ImGui (so content is visible)
    struct ViewportRect { int x{0}, y{0}, w{0}, h{0}; };
    ViewportRect m_mapViewport;
    ViewportRect m_timelineViewport;
    bool m_mapViewportValid{false};
    bool m_timelineViewportValid{false};

    FpsTracker m_fpsTracker;
    ControlsPanel m_controlsPanel;

    // Server stats
    ServerStats m_serverStats;
    bool m_hasServerStats{false};

    // Methods
    void updateSpatialExtent();
    void switchBackend(BackendConfig::Type type);
    void applyControlsActions(const ControlsActions& actions);
};
