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
   renderPage(camera, model);
   m_lines.draw(camera.Transform());
}

void Renderer::shutdown()
{
   m_lines.shutdown();
}

void Renderer::renderPage(const Camera &camera, const AppModel &model)
{
   // Simple grid in world coordinates (0-100 range for now)
   // This will be replaced with proper lat/lon rendering later
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

void Renderer::drawRect(const Vec2 &min, const Vec2 &max, const Color &col)
{
   m_lines.addLine(Vec2(min.x, min.y), Vec2(max.x, min.y), col);
   m_lines.addLine(Vec2(max.x, min.y), Vec2(max.x, max.y), col);
   m_lines.addLine(Vec2(max.x, max.y), Vec2(min.x, max.y), col);
   m_lines.addLine(Vec2(min.x, max.y), Vec2(min.x, min.y), col);
}

void Renderer::drawHandle(const Vec2 &center, float sizeMm, const Color &col)
{
   Vec2 half(sizeMm * 0.5f, sizeMm * 0.5f);
   drawRect(center - half, center + half, col);
}

void Renderer::drawCircle(const Vec2 &center, float radiusMm, const Color &col)
{
   const int segments = 16;
   if (segments < 3)
      return;
   float twoPi = 6.28318530718f;
   Vec2 prev = Vec2(center.x + radiusMm, center.y);
   for (int i = 1; i <= segments; ++i)
   {
      float t = (float)i / (float)segments;
      float ang = t * twoPi;
      Vec2 cur = Vec2(center.x + radiusMm * cosf(ang), center.y + radiusMm * sinf(ang));
      m_lines.addLine(prev, cur, col);
      prev = cur;
   }
}
