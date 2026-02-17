#include "Renderer.h"

#include <algorithm>
#include <cmath>
#include "core/Theme.h"

Renderer::Renderer()
{
   m_lines.init();
   m_tiles.init();
}

void Renderer::render(const Camera &camera, const AppModel &model, const InteractionState &uiState)
{
   m_lines.clear();

   // Vector tile lines (drawn first as background layer)
   if (m_tilesEnabled) {
      m_tiles.render(camera, m_lines);
   }

   renderGrid(camera, model);
   m_lines.draw(camera.Transform());

   // Render entities as points
   renderEntities(camera, model);
}

void Renderer::shutdown()
{
   m_lines.shutdown();
   m_tiles.shutdown();
}

void Renderer::renderGrid(const Camera &camera, const AppModel &model)
{
   // Draw lat/lon grid based on the spatial extent
   Color gridCol = Color(0.5f, 0.5f, 0.5f, 1.0f);  // Gray grid lines

   const auto& extent = model.spatial_extent;

   // Determine grid spacing based on view size
   double lon_span = extent.max_lon - extent.min_lon;
   double lat_span = extent.max_lat - extent.min_lat;

   // Choose nice grid spacing (0.01, 0.05, 0.1, 0.5, 1.0, 5.0 degrees)
   auto getNiceStep = [](double span) -> double {
      double target = span / 8.0; // Aim for ~8 grid lines
      if (target < 0.01) return 0.01;
      if (target < 0.05) return 0.05;
      if (target < 0.1) return 0.1;
      if (target < 0.5) return 0.5;
      if (target < 1.0) return 1.0;
      return 5.0;
   };

   double lon_step = getNiceStep(lon_span);
   double lat_step = getNiceStep(lat_span);

   // Draw longitude lines (vertical)
   double lon_start = std::floor(extent.min_lon / lon_step) * lon_step;
   for (double lon = lon_start; lon <= extent.max_lon; lon += lon_step)
   {
      m_lines.addLine(Vec2(lon, extent.min_lat), Vec2(lon, extent.max_lat), gridCol);
   }

   // Draw latitude lines (horizontal)
   double lat_start = std::floor(extent.min_lat / lat_step) * lat_step;
   for (double lat = lat_start; lat <= extent.max_lat; lat += lat_step)
   {
      m_lines.addLine(Vec2(extent.min_lon, lat), Vec2(extent.max_lon, lat), gridCol);
   }
}

void Renderer::renderEntities(const Camera &camera, const AppModel &model)
{
   float aspectRatio = static_cast<float>(camera.width()) / static_cast<float>(camera.height());
   m_points.begin(camera.Transform(), aspectRatio);

   int rendered_count = 0;
   // Render entities that have location data
   // Entities are already in lat/lon coordinates - use them directly!
   for (const auto& entity : model.entities)
   {
      if (!entity.has_location()) continue;

      // Use lat/lon coordinates directly (longitude=x, latitude=y)
      Vec2 geoPos(*entity.lon, *entity.lat);

      // Render as a bright red point
      Color pointColor(1.0f, 0.0f, 0.0f, 0.3f);
      m_points.addPoint(geoPos, pointColor, m_pointSize);
      rendered_count++;
   }

   // Debug output on first render
   static bool first_render = true;
   if (first_render && rendered_count > 0) {
      std::cout << "PointRenderer: Rendering " << rendered_count << " points in lat/lon coordinates" << std::endl;
      first_render = false;
   }

   m_points.end();
}
