#pragma once

#include "core/Vec2.h"
#include "core/Mat3.h"
#include "Camera.h"

class AppModel;

/// Tracks the current interaction mode
enum class InteractionMode
{
    None,
    PanningCamera,
};

/// Public state of the interaction system (for rendering feedback)
struct InteractionState
{
    InteractionMode mode = InteractionMode::None;
    Vec2 mouseDownWorld;  // World position where interaction started
};

/// Handles user input and maps it to camera/model updates
class InteractionController
{
public:
    void onMouseDown(AppModel &model, Camera &camera, const Vec2 &px);
    void beginPan(Camera &camera, const Vec2 &px);
    void onMouseUp();
    void onCursorPos(AppModel &model, Camera &camera, const Vec2 &px);
    void onScroll(AppModel &model, Camera &camera, float yoffset, const Vec2 &px);

    const InteractionState &state() const { return m_state; }

private:
    InteractionState m_state;
    Mat3 m_cameraStart;
    Vec2 m_cameraStartCenter;  // Camera center when interaction started
};