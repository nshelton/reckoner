#pragma once

#include "app/Screen.h"
#include <imgui.h>
#include "Camera.h"
#include "Renderer.h"
#include "TimelineCamera.h"
#include "TimelineRenderer.h"
#include "AppModel.h"
#include "Backend.h"
#include "FakeBackend.h"
#include "HttpBackend.h"
#include <memory>
#include <future>
#include <string>
#include <mutex>
#include <deque>

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
    TimelineCamera m_timelineCamera{};
    TimelineRenderer m_timelineRenderer{};
    AppModel m_model{};

    // Backend
    std::unique_ptr<Backend> m_backend;
    std::future<void> m_pendingFetch;

    // Thread-safe batch delivery (background thread → main thread)
    std::mutex m_batchMutex;
    std::deque<std::vector<Entity>> m_completedBatches;

    // Backend configuration
    enum class BackendType { Fake, Http };
    BackendType m_backendType = BackendType::Http;
    char m_backendUrl[256] = "http://n3k0.local:8000";
    char m_entityType[128] = "location.gps";

    // Cached window sizes
    ImVec2 m_lastMapSize{0, 0};
    ImVec2 m_lastTimelineSize{0, 0};

    // Viewport in framebuffer coordinates for drawing after ImGui (so content is visible)
    struct ViewportRect { int x{0}, y{0}, w{0}, h{0}; };
    ViewportRect m_mapViewport;
    ViewportRect m_timelineViewport;
    bool m_mapViewportValid{false};
    bool m_timelineViewportValid{false};

    // FPS tracking
    double m_frameTimeAccum{0.0};
    int m_frameCount{0};
    double m_fps{0.0};
    double m_frameMs{0.0};

    // Server stats
    ServerStats m_serverStats;
    bool m_hasServerStats{false};

    // Methods
    void updateSpatialExtent();
    void startFullLoad();
    void drainCompletedBatches();
    void fetchServerStats();
    void switchBackend(BackendType type);
};
