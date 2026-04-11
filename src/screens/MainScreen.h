#pragma once

#include "app/Screen.h"
#include <imgui.h>
#include "Camera.h"
#include "Renderer.h"
#include "Interaction.h"
#include "TimelineCamera.h"
#include "TimelineRenderer.h"
#include "AppModel.h"
#include "BackendFactory.h"
#include <memory>
#include <future>
#include <string>
#include <mutex>
#include <deque>
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

    // Backends — one per layer via BackendFactory
    BackendConfig m_backendConfig{BackendConfig::Type::Http};
    BackendSet m_backends;
    std::future<void> m_pendingGpsFetch;
    std::future<void> m_pendingPhotoFetch;
    std::future<void> m_pendingCalendarFetch;
    std::future<void> m_pendingGoogleTimelineFetch;

    // Thread-safe tagged batch delivery (background thread → main thread)
    struct PendingBatch {
        int layerIndex;
        std::vector<Entity> entities;
    };
    std::mutex m_batchMutex;
    std::deque<PendingBatch> m_completedBatches;

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
    void cancelAndWaitAll();
    void switchBackend(BackendConfig::Type type);
};
