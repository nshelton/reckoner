#include "Renderer.h"

#include <algorithm>
#include <cmath>
#include "core/Theme.h"

Renderer::Renderer()
{
   m_lines.init();
}

void Renderer::render(const Camera &camera, const AppModel &model, const InteractionState &uiState)
{
   m_lines.clear();
   renderGrid(camera, model);
   m_lines.draw(camera.Transform());
}

void Renderer::shutdown()
{
   m_lines.shutdown();
}

void Renderer::renderGrid(const Camera &camera, const AppModel &model)
{
   // Simple test grid in world coordinates (0-100 range)
   // TODO: Replace with proper lat/lon grid based on SpatialExtent
   Color gridCol = Color(0.3f, 0.3f, 0.3f, 1.0f);

   // Draw a 100x100 grid with lines every 10 units
   for (float x = 0.0f; x <= 100.0f; x += 10.0f)
   {
      m_lines.addLine(Vec2(x, 0.0f), Vec2(x, 100.0f), gridCol);
   }
   for (float y = 0.0f; y <= 100.0f; y += 10.0f)
   {
      m_lines.addLine(Vec2(0.0f, y), Vec2(100.0f, y), gridCol);
   }
}
