#pragma once

#include "core/Vec2.h"
#include "core/PickingLogic.h"
#include "Camera.h"
#include "TimelineCamera.h"
#include "EntityPicker.h"
#include <vector>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <future>
#include <functional>

class AppModel;

/// Public state of the interaction system (passed to renderers for visual feedback)
struct InteractionState {
    bool panning{false};
};

/// Manages all user interaction: camera control for map and timeline,
/// entity picking across all layers, hover state, and selection.
class InteractionController
{
public:
    // --- Spatial index management ---
    void markPickersDirty() { m_pickersDirty = true; }
    void resetPickers();                   ///< Clear indices + hover/selection (call on data reload)
    void update(const AppModel& model);    ///< Incremental picker rebuild if dirty

    // --- Map canvas interactions (call when map InvisibleButton is hovered) ---
    void onMapDrag(Camera& camera, Vec2 mouseDeltaPx);
    void onMapScroll(Camera& camera, float yoffset, Vec2 localPx);
    void onMapHover(const Camera& camera, Vec2 localPx, const AppModel& model);
    void onMapClick(const Camera& camera, Vec2 localPx, const AppModel& model);
    void onMapDoubleClick(TimelineCamera& timeline, const Camera& camera, Vec2 localPx, const AppModel& model);
    void onMapUnhovered();

    // --- Timeline canvas interactions ---
    void onTimelineDrag(TimelineCamera& camera, float deltaX);
    void onTimelineScroll(TimelineCamera& camera, float yoffset, float localX);
    void onTimelineHover(const TimelineCamera& camera, float localX, float localY, float panelHeight, const AppModel& model);
    void onTimelineClick(const TimelineCamera& camera, float localX, float localY, float panelHeight, const AppModel& model);
    void onTimelineDoubleClick(Camera& map, const TimelineCamera& camera, float localX, float localY, float panelHeight, const AppModel& model);
    void onTimelineUnhovered();

    // --- State ---
    PickResult hoveredMap()      const { return m_hoveredMap; }
    PickResult hoveredTimeline() const { return m_hoveredTimeline; }
    PickResult selected()        const { return m_selected; }
    const InteractionState& state() const { return m_state; }

    // --- Photo thumbnail loading ---
    /// Set by MainScreen after constructing the photo backend.
    /// The fetcher is a blocking function called on a background thread.
    /// Clear it (pass {}) before destroying the backend it captures.
    void setPhotoFetcher(std::function<std::vector<uint8_t>(const std::string& entityId)> f) {
        m_photoFetcher = std::move(f);
    }

    /// Call each frame from onUpdate — non-blocking check; uploads texture to GL if ready.
    void drainPhotoTexture();

    /// Block until the current thumbnail fetch completes. Call before destroying
    /// the backend that the fetcher function captures.
    void waitForPhotoFetch();

    /// Block on any pending fetch, then delete the GL texture. Call from onDetach.
    void shutdown();

    // --- ImGui rendering ---
    void drawDetailsPanel(const AppModel& model);   // non-const: deselect button + photo fetch

private:
    InteractionState m_state;

    // Pickers
    std::vector<EntityPicker> m_pickers;
    std::vector<size_t>       m_lastEntityCounts;
    bool                      m_pickersDirty{true};

    // Hover / selection
    PickResult m_hoveredMap{};
    PickResult m_hoveredTimeline{};
    PickResult m_selected{};

    // Photo thumbnail
    struct PhotoTexture {
        unsigned int texture{0};   // GLuint — unsigned int avoids GL header in .h
        int          texW{0};
        int          texH{0};
        bool         loading{false};
        std::string  forEntityId;  // entity whose texture is loaded/loading
        std::future<std::vector<uint8_t>> pendingFetch;
    };
    PhotoTexture m_photoTexture;
    std::function<std::vector<uint8_t>(const std::string&)> m_photoFetcher;

    void maybeStartPhotoFetch(const AppModel& model);
    void clearPhotoTexture();   // deletes GL texture, resets struct (call from main thread)

    // Internal pick helpers
    void ensurePickers(size_t count);
    PickResult pickMap(const Camera& camera, Vec2 localPx, const AppModel& model) const;
    PickResult pickTimeline(const TimelineCamera& camera, float localX, float localY, float panelHeight, const AppModel& model) const;
    void drawEntityTooltip(PickResult pick, const AppModel& model) const;
};
