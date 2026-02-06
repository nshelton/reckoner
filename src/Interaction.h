#pragma once

#include <optional>
#include "core/Vec2.h"
#include "core/Mat3.h"
#include "Camera.h"

#define HANDLE_HITBOX_RADIUS 10.0f

enum class InteractionMode
{
    None,
    PanningCamera,
};

struct InteractionState
{
    InteractionMode mode = InteractionMode::None;
    Mat3 dragEntityStartTransform; // cached for drags
    Vec2 mouseDownWorld; // cached for drags
};

class AppModel;

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
    Vec2 m_cameraStartCenterMm;
};