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

   // Render entities as points
   renderEntities(camera, model);
}

void Renderer::shutdown()
{
   m_lines.shutdown();
}

void Renderer::renderGrid(const Camera &camera, const AppModel &model)
{
   // Simple test grid in world coordinates (0-100 range)
   // TODO: Replace with proper lat/lon grid based on SpatialExtent
   Color gridCol = Color(0.5f, 0.5f, 0.5f, 1.0f);  // Brighter gray

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

void Renderer::renderEntities(const Camera &camera, const AppModel &model)
{
   m_points.begin(camera.Transform());

   int rendered_count = 0;
   // Render entities that have location data
   for (const auto& entity : model.entities)
   {
      if (!entity.has_location()) continue;

      // Convert lat/lon to normalized [0,1] space
      Vec2 normalized = model.spatial_extent.to_normalized(*entity.lat, *entity.lon);

      // Scale to world space (matching our 100x100 grid)
      Vec2 worldPos(normalized.x * 100.0f, normalized.y * 100.0f);

      // Render as a bright red point (easier to see)
      Color pointColor(1.0f, 0.0f, 0.0f, 1.0f);
      m_points.addPoint(worldPos, pointColor, m_pointSize);  // Larger size
      rendered_count++;
   }

   // Debug output on first render
   static bool first_render = true;
   if (first_render && rendered_count > 0) {
      std::cout << "PointRenderer: Rendering " << rendered_count << " points" << std::endl;
      first_render = false;
   }

   m_points.end();
}
