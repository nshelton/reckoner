#pragma once

#include "core/Vec2.h"
#include "Camera.h"
#include "TimelineCamera.h"
#include "EntityPicker.h"
#include <vector>
#include <cstddef>
#include <limits>

class AppModel;

/// Identifies a specific entity within a specific layer; invalid if layerIndex < 0.
struct PickResult {
    int layerIndex{-1};
    int entityIndex{-1};
    bool valid() const { return layerIndex >= 0 && entityIndex >= 0; }
    bool operator==(const PickResult& o) const { return layerIndex == o.layerIndex && entityIndex == o.entityIndex; }
    bool operator!=(const PickResult& o) const { return !(*this == o); }
};

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

    // --- ImGui rendering ---
    void drawDetailsPanel(const AppModel& model);   // non-const: deselect button mutates state

private:
    InteractionState m_state;

    std::vector<EntityPicker> m_pickers;
    std::vector<size_t>       m_lastEntityCounts;
    bool                      m_pickersDirty{true};

    PickResult m_hoveredMap{};
    PickResult m_hoveredTimeline{};
    PickResult m_selected{};

    void ensurePickers(size_t count);
    PickResult pickMap(const Camera& camera, Vec2 localPx, const AppModel& model) const;
    PickResult pickTimeline(const TimelineCamera& camera, float localX, float localY, float panelHeight, const AppModel& model) const;
    void drawEntityTooltip(PickResult pick, const AppModel& model) const;
};
