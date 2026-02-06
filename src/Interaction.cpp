#include "Interaction.h"
#include "AppModel.h"
#include "Camera.h"
#include <cmath>

void InteractionController::onMouseDown(AppModel &model, Camera &camera, const Vec2 &mouseWorld)
{
    m_state.mouseDownWorld = mouseWorld;
    m_state.mode = InteractionMode::PanningCamera;
    m_cameraStart = camera.Transform();
    m_cameraStartCenterMm = camera.center();
}

void InteractionController::beginPan(Camera &camera, const Vec2 &mouseWorld)
{
    m_state.mouseDownWorld = mouseWorld;
    m_state.mode = InteractionMode::PanningCamera;
    m_cameraStart = camera.Transform();
    m_cameraStartCenterMm = camera.center();
}

void InteractionController::onCursorPos(AppModel &model, Camera &camera, const Vec2 &mouseWorld)
{
    if (m_state.mode == InteractionMode::PanningCamera)
    {
        // Pan in world space
        Vec2 delta = mouseWorld - m_state.mouseDownWorld;
        camera.move(delta);
    }
}

void InteractionController::onScroll(AppModel &model, Camera &camera, float yoffset, const Vec2 &px)
{
    camera.zoomAtPixel(px, yoffset);
}

void InteractionController::onMouseUp()
{
    m_state.mode = InteractionMode::None;
}
