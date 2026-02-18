#include "Renderer.h"

#include <algorithm>
#include <cmath>
#include "core/Theme.h"

Renderer::Renderer()
{
   m_lines.init();
   m_tiles.init();
   m_chunkBuildBuf.reserve(PointRenderer::CHUNK_SIZE);
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

void Renderer::rebuildChunk(size_t chunkIndex, const AppModel &model)
{
   m_chunkBuildBuf.clear();

   size_t start = chunkIndex * PointRenderer::CHUNK_SIZE;
   size_t end = std::min(start + PointRenderer::CHUNK_SIZE, model.entities.size());

   for (size_t i = start; i < end; i++) {
      const auto& entity = model.entities[i];
      if (!entity.has_location()) continue;

      m_chunkBuildBuf.push_back({
          Vec2(static_cast<float>(*entity.lon), static_cast<float>(*entity.lat)),
          static_cast<float>(entity.time_mid()),
          entity.render_offset
      });
   }

   m_points.updateChunk(chunkIndex, m_chunkBuildBuf.data(), m_chunkBuildBuf.size());
}

void Renderer::renderEntities(const Camera &camera, const AppModel &model)
{
   size_t entityCount = model.entities.size();

   if (entityCount == 0) return;

   size_t numActiveChunks = (entityCount + PointRenderer::CHUNK_SIZE - 1) / PointRenderer::CHUNK_SIZE;

   // Only rebuild chunks that contain newly added entities
   if (entityCount != m_lastEntityCount) {
      // Ensure GPU buffers exist for all needed chunks
      m_points.ensureChunks(numActiveChunks);

      // If data was cleared and refilled, rebuild from scratch
      size_t firstDirtyChunk = (entityCount > m_lastEntityCount)
         ? m_lastEntityCount / PointRenderer::CHUNK_SIZE
         : 0;

      for (size_t c = firstDirtyChunk; c < numActiveChunks; c++) {
         rebuildChunk(c, model);
      }

      m_lastEntityCount = entityCount;
   }

   float aspectRatio = static_cast<float>(camera.width()) / static_cast<float>(camera.height());
   float timeMin = static_cast<float>(model.time_extent.start);
   float timeMax = static_cast<float>(model.time_extent.end);
   m_points.drawChunked(camera.Transform(), aspectRatio, numActiveChunks, timeMin, timeMax);
}
