#pragma once

#include <imgui.h>
#include "AppModel.h"
#include "Backend.h"
#include "BackendFactory.h"
#include "Renderer.h"
#include "TimelineRenderer.h"
#include "core/FpsTracker.h"

/// Actions requested by the controls panel UI.
/// MainScreen inspects these after draw() and performs the actual mutations.
struct ControlsActions {
    bool resetMap{false};
    bool resetTimeline{false};
    bool reloadAllData{false};

    // -1 means no change requested
    int switchBackendType{-1};
    bool applyHttpConfig{false};

    // Rendering changes (-1 = no change)
    int tileMode{-1};
    float pointSize{-1.0f};

    // Timeline overlay toggles (-1 = no change, 0 = off, 1 = on)
    int histogram{-1};
    int solarAltitude{-1};
    int moonAltitude{-1};
};

/// Draws the Controls ImGui window.
/// Pure UI — reads state, returns actions, never mutates backends or renderers.
class ControlsPanel {
public:
    void draw(
        const FpsTracker& fps,
        const AppModel& model,
        const BackendConfig& backendConfig,
        char* backendUrl, size_t backendUrlSize,
        const ServerStats& serverStats, bool hasServerStats,
        const Renderer& renderer,
        const TimelineRenderer& timelineRenderer,
        const Camera& camera,
        const TimelineCamera& timelineCamera,
        ControlsActions& actions
    );
};
