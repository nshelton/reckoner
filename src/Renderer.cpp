#include "Renderer.h"

#include <algorithm>
#include <cmath>
#include "core/Theme.h"

static constexpr float kPi = 3.14159265358979323846f;

static const char* rasterUrlForMode(TileMode mode) {
    switch (mode) {
        case TileMode::OSM:
            return "https://tile.openstreetmap.org/{z}/{x}/{y}.png";
        case TileMode::CartoDB_Light:
            return "https://basemaps.cartocdn.com/light_all/{z}/{x}/{y}.png";
        case TileMode::CartoDB_Dark:
            return "https://basemaps.cartocdn.com/dark_all/{z}/{x}/{y}.png";
        default:
            return nullptr;
    }
}

Renderer::Renderer()
{
   m_lines.init();
   m_tiles.init();
   m_rasterTiles.init();
   m_chunkBuildBuf.reserve(PointRenderer::CHUNK_SIZE);
}

void Renderer::setTileMode(TileMode mode) {
    m_tileMode = mode;
    const char* url = rasterUrlForMode(mode);
    if (url) {
        m_rasterTiles.setUrlTemplate(url);
    }
}

void Renderer::setPointSize(float size) {
    m_pointSize = size;
    for (auto& pr : m_layerPoints)
        if (pr) pr->setPointSize(size);
}

int Renderer::totalPoints() const {
    int total = 0;
    for (const auto& pr : m_layerPoints)
        if (pr) total += static_cast<int>(pr->pointCount());
    return total;
}

PointRenderer* Renderer::layerRenderer(size_t layerIndex) {
    if (layerIndex < m_layerPoints.size())
        return m_layerPoints[layerIndex].get();
    return nullptr;
}

void Renderer::ensureLayerRenderer(size_t layerIndex) {
    while (m_layerPoints.size() <= layerIndex) {
        auto pr = std::make_unique<PointRenderer>();
        pr->setPointSize(m_pointSize);
        m_layerPoints.push_back(std::move(pr));
        m_layerEntityCounts.push_back(0);
    }
}

void Renderer::render(const Camera &camera, const AppModel &model, const InteractionState &uiState)
{
   m_lines.clear();

   // Tile background layer (drawn before entities)
   if (m_tileMode == TileMode::Vector) {
      m_tiles.render(camera);
   } else if (m_tileMode == TileMode::OSM ||
              m_tileMode == TileMode::CartoDB_Light ||
              m_tileMode == TileMode::CartoDB_Dark) {
      m_rasterTiles.render(camera);
   }

   renderGrid(camera, model);
   m_lines.draw(camera.Transform());

   // Render entities as points, one layer at a time
   renderEntities(camera, model);
}

void Renderer::shutdown()
{
   m_lines.shutdown();
   m_tiles.shutdown();
   m_rasterTiles.shutdown();
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

void Renderer::rebuildLayerChunk(size_t layerIndex, size_t chunkIndex, const Layer &layer)
{
   m_chunkBuildBuf.clear();

   size_t start = chunkIndex * PointRenderer::CHUNK_SIZE;
   size_t end = std::min(start + PointRenderer::CHUNK_SIZE, layer.entities.size());

   for (size_t i = start; i < end; i++) {
      const auto& entity = layer.entities[i];

      // Entities without GPS use a sentinel far outside any map view.
      // On the map: the sentinel projects off-screen and is GPU-clipped (invisible).
      // On the timeline: the sentinel is treated as "out of map view" (gray/muted pass).
      Vec2 geo = entity.has_location()
          ? Vec2(static_cast<float>(*entity.lon), static_cast<float>(*entity.lat))
          : Vec2(-9999.0f, -9999.0f);

      m_chunkBuildBuf.push_back({
          geo,
          static_cast<float>(entity.time_mid()),
          entity.render_offset
      });
   }

   m_layerPoints[layerIndex]->updateChunk(chunkIndex, m_chunkBuildBuf.data(), m_chunkBuildBuf.size());
}

void Renderer::drawMapHighlight(const Camera &camera, double lon, double lat)
{
    m_lines.clear();
    m_lines.setLineWidth(2.0f);

    Color c(1.0f, 0.95f, 0.2f, 0.9f);

    // Compute world-space radii so the ring is a fixed pixel size on screen.
    // The camera maps lat range [bottom, top] (size = 2*zoom) to height pixels.
    const float pixelRadius = 12.0f;
    float lat_r = pixelRadius * 2.0f * camera.zoom() / static_cast<float>(camera.height());
    float latRad = static_cast<float>(lat) * kPi / 180.0f;
    float cosLat = std::max(0.001f, std::cos(latRad));
    float lon_r  = lat_r / cosLat;

    constexpr int N = 24;
    for (int i = 0; i < N; ++i) {
        float a0 = 2.0f * kPi * static_cast<float>(i)     / static_cast<float>(N);
        float a1 = 2.0f * kPi * static_cast<float>(i + 1) / static_cast<float>(N);
        Vec2 p0(static_cast<float>(lon) + lon_r * std::cos(a0),
                static_cast<float>(lat) + lat_r * std::sin(a0));
        Vec2 p1(static_cast<float>(lon) + lon_r * std::cos(a1),
                static_cast<float>(lat) + lat_r * std::sin(a1));
        m_lines.addLine(p0, p1, c);
    }

    m_lines.draw(camera.Transform());
    m_lines.setLineWidth(1.0f);
}

void Renderer::renderEntities(const Camera &camera, const AppModel &model)
{
   float aspectRatio = static_cast<float>(camera.width()) / static_cast<float>(camera.height());
   float timeMin = static_cast<float>(model.time_extent.start);
   float timeMax = static_cast<float>(model.time_extent.end);

   for (size_t li = 0; li < model.layers.size(); ++li) {
      const Layer& layer = model.layers[li];
      if (!layer.visible) continue;

      size_t entityCount = layer.entities.size();
      if (entityCount == 0) continue;

      ensureLayerRenderer(li);
      PointRenderer& pr = *m_layerPoints[li];

      size_t numActiveChunks = (entityCount + PointRenderer::CHUNK_SIZE - 1) / PointRenderer::CHUNK_SIZE;

      // Only rebuild chunks that contain newly added entities
      if (entityCount != m_layerEntityCounts[li]) {
         pr.ensureChunks(numActiveChunks);

         size_t firstDirtyChunk = (entityCount > m_layerEntityCounts[li])
            ? m_layerEntityCounts[li] / PointRenderer::CHUNK_SIZE
            : 0;

         for (size_t c = firstDirtyChunk; c < numActiveChunks; c++) {
            rebuildLayerChunk(li, c, layer);
         }

         m_layerEntityCounts[li] = entityCount;
      }

      pr.drawChunked(camera.Transform(), aspectRatio, numActiveChunks, timeMin, timeMax,
                     layer.colorMode,
                     layer.color.r, layer.color.g, layer.color.b, layer.color.a,
                     layer.shape);
   }
}
