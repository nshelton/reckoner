# Spatiotemporal Entity Visualizer — Design Document

## Overview

A C++ application for visualizing entities that exist in both space and time. The core insight: **a layer lives in both dimensions simultaneously**. Users can pan/zoom a map and a timeline independently, and layers render into both views.

**Key design principle: Layer = Type.** Each layer corresponds to exactly one entity type from the backend (e.g., `"location.gps"`, `"photo"`, `"calendar.event"`). These concepts are synonymous.

### Goals
- Modular layer system where adding a new type = adding a new layer
- Clean separation between data fetching, state management, and rendering
- Start simple (grid map, basic timeline), swap in better implementations later
- Use imgui for controls, OpenGL for rendering

### Non-Goals (for now)
- Vector tile maps (start with lat/lon grid, upgrade to raster tiles later)
- Complex spatial queries (points first, tracks/polygons later)

---

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                      Views                          │
│  ┌──────────────┐          ┌──────────────┐        │
│  │   Timeline   │          │     Map      │        │
│  │  (TimeExtent)│          │(SpatialExtent)│        │
│  └──────┬───────┘          └───────┬──────┘        │
└─────────┼──────────────────────────┼───────────────┘
          │   OnExtentChanged        │
          ▼                          ▼
┌─────────────────────────────────────────────────────┐
│                    AppModel                         │
│  ┌─────────────────────────────────────────────┐   │
│  │ vector<unique_ptr<Layer>> layers            │   │
│  │ unique_ptr<MapBackground> map_background    │   │
│  │ TimeExtent, SpatialExtent                   │   │
│  │ Entity* selected_entity                     │   │
│  └─────────────────────────────────────────────┘   │
└────────────────────────┬────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────┐
│                 Layer Renderers                     │
│   RenderMap() | RenderTimeline() | RenderGUI()     │
│   HitTestMap() | HitTestTimeline()                 │
└─────────────────────────────────────────────────────┘
```

---

## Backend API Integration

The frontend consumes a FastAPI + PostgreSQL + PostGIS backend. Key details:

### API Endpoints

**Spatial + Time Query** (for layers with location data):
```
POST /v1/query/bbox
{
  "types": ["location.gps"],           // single type per layer
  "bbox": [min_lon, min_lat, max_lon, max_lat],
  "time": { "start": "ISO8601", "end": "ISO8601" },
  "limit": 5000
}
```

**Time-only Query** (for layers without location data):
```
POST /v1/query/time
{
  "types": ["calendar.event"],
  "start": "ISO8601",
  "end": "ISO8601",
  "limit": 2000,
  "resample": { "method": "none" }      // or "uniform_time" with "n": 2000
}
```

### Response Format
```json
{
  "entities": [
    {
      "id": "uuid",
      "type": "location.gps",
      "t_start": "2024-01-01T00:00:00Z",
      "t_end": null,
      "lat": 34.0522,
      "lon": -118.2437,
      "name": "optional",
      "color": "#FF00FF",
      "render_offset": 0.0,
      "payload": { "accuracy_m": 12.3 }
    }
  ]
}
```

### Data Mapping: Backend → Frontend

| Backend Field | Frontend Field | Notes |
|---------------|----------------|-------|
| `t_start` | `time_start` | Parse ISO 8601 → unix timestamp (double) |
| `t_end` | `time_end` | If null, set equal to `time_start` (instant) |
| `lat`, `lon` | `lat`, `lon` | Optional — some entities have no location |
| `type` | (implicit) | Determined by which layer owns the entity |
| `color` | `color` | Optional per-entity override of layer color |
| `render_offset` | `render_offset` | Vertical offset for timeline stacking |
| `payload` | `payload` | Type-specific JSON data |

### Timestamp Parsing

Backend uses ISO 8601 strings. Frontend uses unix timestamps (doubles).

```cpp
#include <chrono>
#include <iomanip>
#include <sstream>

double parse_iso8601(const std::string& s) {
    std::tm tm = {};
    std::istringstream ss(s);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return (double)timegm(&tm);
}

std::string to_iso8601(double timestamp) {
    std::time_t t = (std::time_t)timestamp;
    std::tm* tm = std::gmtime(&t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", tm);
    return buf;
}
```

---

## Core Types

### Extents

```cpp
struct TimeExtent {
    double start;  // unix timestamp
    double end;
    
    double duration() const { return end - start; }
    bool contains(double t) const { return t >= start && t <= end; }
    double to_normalized(double t) const { return (t - start) / duration(); }
};

struct SpatialExtent {
    double min_lat, max_lat;
    double min_lon, max_lon;
    
    Vec2 to_normalized(double lat, double lon) const {
        return {
            (lon - min_lon) / (max_lon - min_lon),
            (lat - min_lat) / (max_lat - min_lat)
        };
    }
    
    // For backend bbox queries: [min_lon, min_lat, max_lon, max_lat]
    std::array<double, 4> to_bbox() const {
        return {min_lon, min_lat, max_lon, max_lat};
    }
};
```

### Entity

Entities always have a time extent. Point events have `time_start == time_end`. Location is optional.

```cpp
struct Entity {
    std::string id;
    
    // Time — always an extent, instants have start == end
    double time_start;
    double time_end;
    
    // Space — optional, some entities have no location
    std::optional<double> lat;
    std::optional<double> lon;
    
    // Display
    std::optional<std::string> name;
    std::optional<std::string> color;  // "#RRGGBB", overrides layer color
    float render_offset = 0.0f;        // vertical offset for timeline stacking
    
    // Type-specific data from backend
    nlohmann::json payload;
    
    // Helpers
    bool is_instant() const { return time_start == time_end; }
    double duration() const { return time_end - time_start; }
    double time_mid() const { return (time_start + time_end) / 2; }
    
    bool has_location() const { return lat.has_value() && lon.has_value(); }
    
    bool time_overlaps(const TimeExtent& extent) const {
        return time_start <= extent.end && time_end >= extent.start;
    }
    
    bool time_contains(double t) const {
        return t >= time_start && t <= time_end;
    }
    
    double spatial_distance_sq(double lat_, double lon_) const {
        if (!has_location()) return std::numeric_limits<double>::max();
        double dlat = *lat - lat_;
        double dlon = *lon - lon_;
        return dlat * dlat + dlon * dlon;
    }
};

// Color resolution: entity color > layer color
Vec4 GetEntityColor(const Entity& e, const Layer& layer) {
    if (e.color.has_value())
        return ParseHexColor(*e.color);
    return layer.color;
}
```

---

## Layer System

### Core Principle: Layer = Type

Each layer corresponds to exactly one backend entity type. The layer name *is* the type string.

```cpp
// Adding a layer = adding a type
model.AddLayer(std::make_unique<BackendLayer>("location.gps", base_url, true));
model.AddLayer(std::make_unique<BackendLayer>("photo", base_url, true));
model.AddLayer(std::make_unique<BackendLayer>("calendar.event", base_url, false));
model.AddLayer(std::make_unique<SunLayer>());  // computed, no backend
```

### Base Interface

```cpp
struct Layer {
    std::string name;      // == type for BackendLayers
    bool visible = true;
    float alpha = 1.0f;
    Vec4 color = {1, 1, 1, 1};
    
    virtual ~Layer() = default;
    
    // Called when view changes — layer decides if it needs to fetch/recompute
    virtual void OnExtentChanged(const TimeExtent& time, const SpatialExtent& space) = 0;
    
    // Rendering — receives current extent, renders in normalized [0,1] space
    virtual void RenderMap(const SpatialExtent& extent) = 0;
    virtual void RenderTimeline(const TimeExtent& extent) = 0;
    
    // imgui controls for this layer
    virtual void RenderGUI() = 0;
    
    // Hit testing — returns closest entity within radius, or empty
    virtual HitTestResult HitTestMap(const HitTestContext& ctx) { return {}; }
    virtual HitTestResult HitTestTimeline(const HitTestContext& ctx) { return {}; }
    
    // Optional: aggregation for zoomed-out views
    virtual bool HasTimelineHistogram() const { return false; }
    virtual std::vector<float> GetTimelineHistogram(const TimeExtent& extent, int bins) { return {}; }
};
```

### Layer Types

1. **BackendLayer** — fetches entities from the backend API (one type per layer)
2. **ComputedLayer** — generates data from math (e.g., sun/moon positions)
3. **StaticLayer** — doesn't change with extent (rare)

### BackendLayer

```cpp
class BackendLayer : public Layer {
protected:
    std::string type_;           // e.g., "location.gps" — this IS the layer
    std::string base_url_;
    bool has_spatial_data_;      // determines which endpoint to use
    
    std::vector<Entity> entities_;
    std::future<std::vector<Entity>> pending_request_;
    
    TimeExtent cached_time_extent_;
    SpatialExtent cached_spatial_extent_;
    
public:
    BackendLayer(std::string type, std::string base_url, bool has_spatial_data)
        : type_(std::move(type))
        , base_url_(std::move(base_url))
        , has_spatial_data_(has_spatial_data)
    {
        this->name = type_;  // layer name == type
    }
    
    void OnExtentChanged(const TimeExtent& time, const SpatialExtent& space) override {
        if (NeedsRefetch(time, space)) {
            if (has_spatial_data_) {
                // POST /v1/query/bbox
                pending_request_ = std::async(std::launch::async, [=]() {
                    return FetchViaBbox(type_, time, space);
                });
            } else {
                // POST /v1/query/time (ignore spatial extent)
                pending_request_ = std::async(std::launch::async, [=]() {
                    return FetchViaTime(type_, time);
                });
            }
            cached_time_extent_ = time;
            cached_spatial_extent_ = space;
        }
    }
    
    void Update() {
        if (pending_request_.valid() &&
            pending_request_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            entities_ = pending_request_.get();
        }
    }
    
    void RenderMap(const SpatialExtent& extent) override {
        if (!visible || !has_spatial_data_) return;
        for (const auto& e : entities_) {
            if (!e.has_location()) continue;
            Vec2 pos = extent.to_normalized(*e.lat, *e.lon);
            Vec4 c = GetEntityColor(e, *this);
            c.w *= alpha;
            DrawPoint(pos, c);
        }
    }
    
    void RenderTimeline(const TimeExtent& extent) override {
        if (!visible) return;
        for (const auto& e : entities_) {
            if (!e.time_overlaps(extent)) continue;
            Vec4 c = GetEntityColor(e, *this);
            c.w *= alpha;
            
            float y_offset = e.render_offset;  // use for vertical stacking
            
            if (e.is_instant()) {
                float x = extent.to_normalized(e.time_start);
                DrawTimelineTick(x, y_offset, c);
            } else {
                float x0 = std::max(0.0f, (float)extent.to_normalized(e.time_start));
                float x1 = std::min(1.0f, (float)extent.to_normalized(e.time_end));
                DrawTimelineBar(x0, x1, y_offset, c);
            }
        }
    }
    
    void RenderGUI() override {
        ImGui::Checkbox("Visible", &visible);
        ImGui::SliderFloat("Alpha", &alpha, 0.0f, 1.0f);
        ImGui::ColorEdit4("Color", &color.x);
        ImGui::Text("Entities: %zu", entities_.size());
    }
    
    // Hit testing implementations...
};
```

### Backend Request Builders

```cpp
std::vector<Entity> FetchViaBbox(
    const std::string& type,
    const TimeExtent& time,
    const SpatialExtent& space
) {
    nlohmann::json request = {
        {"types", {type}},  // single-element array
        {"bbox", space.to_bbox()},
        {"time", {
            {"start", to_iso8601(time.start)},
            {"end", to_iso8601(time.end)}
        }},
        {"limit", 5000}
    };
    
    auto response = HttpPost(base_url_ + "/v1/query/bbox", request);
    return ParseEntities(response["entities"]);
}

std::vector<Entity> FetchViaTime(
    const std::string& type,
    const TimeExtent& time
) {
    nlohmann::json request = {
        {"types", {type}},
        {"start", to_iso8601(time.start)},
        {"end", to_iso8601(time.end)},
        {"limit", 2000},
        {"resample", {{"method", "none"}}}
    };
    
    auto response = HttpPost(base_url_ + "/v1/query/time", request);
    return ParseEntities(response["entities"]);
}

std::vector<Entity> ParseEntities(const nlohmann::json& arr) {
    std::vector<Entity> result;
    for (const auto& j : arr) {
        Entity e;
        e.id = j["id"];
        e.time_start = parse_iso8601(j["t_start"]);
        e.time_end = j["t_end"].is_null() ? e.time_start : parse_iso8601(j["t_end"]);
        
        if (!j["lat"].is_null()) e.lat = j["lat"];
        if (!j["lon"].is_null()) e.lon = j["lon"];
        if (!j["name"].is_null()) e.name = j["name"];
        if (!j["color"].is_null()) e.color = j["color"];
        if (!j["render_offset"].is_null()) e.render_offset = j["render_offset"];
        if (!j["payload"].is_null()) e.payload = j["payload"];
        
        result.push_back(std::move(e));
    }
    return result;
}
```

### ComputedLayer Example (Sun/Moon)

```cpp
class SunLayer : public Layer {
    struct SunEvent { double timestamp; bool is_sunrise; };
    std::vector<SunEvent> events_;
    
public:
    SunLayer() { 
        name = "sun";  // not a backend type, but still a unique name
        color = {1.0f, 0.8f, 0.2f, 1.0f}; 
    }
    
    void OnExtentChanged(const TimeExtent& time, const SpatialExtent& space) override {
        double lat = (space.min_lat + space.max_lat) / 2;
        double lon = (space.min_lon + space.max_lon) / 2;
        
        events_.clear();
        for (double day = floor(time.start / 86400) * 86400; day < time.end; day += 86400) {
            auto [sunrise, sunset] = ComputeSunTimes(day, lat, lon);
            if (sunrise >= time.start && sunrise <= time.end)
                events_.push_back({sunrise, true});
            if (sunset >= time.start && sunset <= time.end)
                events_.push_back({sunset, false});
        }
    }
    
    void RenderMap(const SpatialExtent& extent) override {
        // Optional: render current sun position or day/night terminator
    }
    
    void RenderTimeline(const TimeExtent& extent) override {
        if (!visible) return;
        for (const auto& e : events_) {
            float x = extent.to_normalized(e.timestamp);
            Vec4 c = e.is_sunrise ? Vec4{1, 0.9, 0.3, alpha} : Vec4{0.8, 0.4, 0.1, alpha};
            DrawTimelineMark(x, c);
        }
    }
    
    void RenderGUI() override {
        ImGui::Checkbox("Visible", &visible);
        ImGui::SliderFloat("Alpha", &alpha, 0.0f, 1.0f);
    }
};
```

---

## Hit Testing

### Context

```cpp
struct HitTestResult {
    Entity* entity = nullptr;
    Layer* layer = nullptr;
    float distance = 0;
    
    explicit operator bool() const { return entity != nullptr; }
};

struct HitTestContext {
    Vec2 screen_pos;
    Vec2 screen_size;
    TimeExtent time_extent;
    SpatialExtent spatial_extent;
    float radius_px = 10.0f;
    
    Vec2 to_map_normalized() const {
        return { screen_pos.x / screen_size.x, screen_pos.y / screen_size.y };
    }
    
    std::pair<double, double> to_latlon() const {
        Vec2 n = to_map_normalized();
        double lon = spatial_extent.min_lon + n.x * (spatial_extent.max_lon - spatial_extent.min_lon);
        double lat = spatial_extent.min_lat + n.y * (spatial_extent.max_lat - spatial_extent.min_lat);
        return {lat, lon};
    }
    
    double to_timestamp() const {
        float nx = screen_pos.x / screen_size.x;
        return time_extent.start + nx * time_extent.duration();
    }
    
    double radius_in_degrees() const {
        double deg_per_px = (spatial_extent.max_lon - spatial_extent.min_lon) / screen_size.x;
        return radius_px * deg_per_px;
    }
    
    double radius_in_time() const {
        double time_per_px = time_extent.duration() / screen_size.x;
        return radius_px * time_per_px;
    }
};
```

### Timeline Hit Testing with Duration

```cpp
HitTestResult HitTestTimeline(const HitTestContext& ctx) override {
    if (!visible) return {};
    
    double hit_time = ctx.to_timestamp();
    double radius_time = ctx.radius_in_time();
    
    HitTestResult best;
    best.distance = std::numeric_limits<float>::max();
    
    for (auto& e : entities_) {
        float dist;
        if (e.is_instant()) {
            dist = std::abs(hit_time - e.time_start);
        } else if (hit_time >= e.time_start && hit_time <= e.time_end) {
            dist = 0;  // inside the event
        } else {
            dist = std::min(
                std::abs(hit_time - e.time_start),
                std::abs(hit_time - e.time_end)
            );
        }
        
        if (dist < radius_time && dist < best.distance) {
            best.entity = &e;
            best.layer = this;
            best.distance = dist;
        }
    }
    return best;
}
```

### Map Hit Testing

```cpp
HitTestResult HitTestMap(const HitTestContext& ctx) override {
    if (!visible || !has_spatial_data_) return {};
    
    auto [hit_lat, hit_lon] = ctx.to_latlon();
    double radius_deg = ctx.radius_in_degrees();
    double radius_sq = radius_deg * radius_deg;
    
    HitTestResult best;
    best.distance = std::numeric_limits<float>::max();
    
    for (auto& e : entities_) {
        double dist_sq = e.spatial_distance_sq(hit_lat, hit_lon);
        if (dist_sq < radius_sq && dist_sq < best.distance) {
            best.entity = &e;
            best.layer = this;
            best.distance = (float)dist_sq;
        }
    }
    
    return best;
}
```

---

## Map Background

Abstracted so we can swap implementations:

```cpp
struct MapBackground {
    virtual ~MapBackground() = default;
    virtual void Render(const SpatialExtent& extent) = 0;
    virtual void RenderGUI() {}
};

// Start with this
class GridMapBackground : public MapBackground {
public:
    void Render(const SpatialExtent& extent) override {
        double lat_span = extent.max_lat - extent.min_lat;
        double lon_span = extent.max_lon - extent.min_lon;
        
        double lat_step = ComputeNiceStep(lat_span);
        double lon_step = ComputeNiceStep(lon_span);
        
        // Draw latitude lines
        for (double lat = floor(extent.min_lat / lat_step) * lat_step;
             lat <= extent.max_lat;
             lat += lat_step) {
            float y = (lat - extent.min_lat) / lat_span;
            DrawHLine(y, {0.3, 0.3, 0.3, 1.0});
            // Label with imgui or custom text rendering
        }
        
        // Draw longitude lines
        for (double lon = floor(extent.min_lon / lon_step) * lon_step;
             lon <= extent.max_lon;
             lon += lon_step) {
            float x = (lon - extent.min_lon) / lon_span;
            DrawVLine(x, {0.3, 0.3, 0.3, 1.0});
        }
    }
    
private:
    double ComputeNiceStep(double span) {
        // Return a "nice" step size (1, 2, 5, 10, 20, etc.)
        double rough_step = span / 8;  // aim for ~8 grid lines
        double magnitude = pow(10, floor(log10(rough_step)));
        double normalized = rough_step / magnitude;
        
        if (normalized < 1.5) return magnitude;
        if (normalized < 3.5) return 2 * magnitude;
        if (normalized < 7.5) return 5 * magnitude;
        return 10 * magnitude;
    }
};

// Upgrade to this later
class RasterTileMapBackground : public MapBackground {
    TileCache cache_;
public:
    void Render(const SpatialExtent& extent) override {
        // Fetch and render OSM/Mapbox PNG tiles
    }
};
```

### Raster Tile Math (for later)

```cpp
int lon_to_tile_x(double lon, int z) {
    return (int)((lon + 180.0) / 360.0 * (1 << z));
}

int lat_to_tile_y(double lat, int z) {
    double lat_rad = lat * M_PI / 180.0;
    return (int)((1.0 - asinh(tan(lat_rad)) / M_PI) / 2.0 * (1 << z));
}

// Tile URL: https://tile.openstreetmap.org/{z}/{x}/{y}.png
```

---

## AppModel

Central state management:

```cpp
class AppModel {
    std::vector<std::unique_ptr<Layer>> layers_;
    std::unique_ptr<MapBackground> map_background_;
    
    TimeExtent time_extent_;
    SpatialExtent spatial_extent_;
    
    Entity* selected_entity_ = nullptr;
    Layer* selected_layer_ = nullptr;
    
public:
    AppModel() {
        map_background_ = std::make_unique<GridMapBackground>();
    }
    
    void AddLayer(std::unique_ptr<Layer> layer) {
        layers_.push_back(std::move(layer));
    }
    
    void SetTimeExtent(const TimeExtent& e) {
        time_extent_ = e;
        for (auto& layer : layers_)
            layer->OnExtentChanged(time_extent_, spatial_extent_);
    }
    
    void SetSpatialExtent(const SpatialExtent& e) {
        spatial_extent_ = e;
        for (auto& layer : layers_)
            layer->OnExtentChanged(time_extent_, spatial_extent_);
    }
    
    void Update() {
        for (auto& layer : layers_) {
            if (auto* bl = dynamic_cast<BackendLayer*>(layer.get()))
                bl->Update();
        }
    }
    
    void RenderMap() {
        map_background_->Render(spatial_extent_);
        for (auto& layer : layers_)
            layer->RenderMap(spatial_extent_);
    }
    
    void RenderTimeline() {
        // Render timeline background, axis, ticks first
        RenderTimelineAxis(time_extent_);
        for (auto& layer : layers_)
            layer->RenderTimeline(time_extent_);
    }
    
    void RenderLayerGUI() {
        for (auto& layer : layers_) {
            if (ImGui::TreeNode(layer->name.c_str())) {
                layer->RenderGUI();
                ImGui::TreePop();
            }
        }
    }
    
    HitTestResult HitTestMap(Vec2 screen_pos, Vec2 screen_size) {
        HitTestContext ctx;
        ctx.screen_pos = screen_pos;
        ctx.screen_size = screen_size;
        ctx.time_extent = time_extent_;
        ctx.spatial_extent = spatial_extent_;
        
        HitTestResult best;
        best.distance = std::numeric_limits<float>::max();
        
        // Test layers in reverse order (top layer = last added = first priority)
        for (auto it = layers_.rbegin(); it != layers_.rend(); ++it) {
            auto result = (*it)->HitTestMap(ctx);
            if (result && result.distance < best.distance)
                best = result;
        }
        return best;
    }
    
    HitTestResult HitTestTimeline(Vec2 screen_pos, Vec2 screen_size) {
        HitTestContext ctx;
        ctx.screen_pos = screen_pos;
        ctx.screen_size = screen_size;
        ctx.time_extent = time_extent_;
        ctx.spatial_extent = spatial_extent_;
        
        HitTestResult best;
        best.distance = std::numeric_limits<float>::max();
        
        for (auto it = layers_.rbegin(); it != layers_.rend(); ++it) {
            auto result = (*it)->HitTestTimeline(ctx);
            if (result && result.distance < best.distance)
                best = result;
        }
        return best;
    }
    
    void Select(HitTestResult hit) {
        selected_entity_ = hit.entity;
        selected_layer_ = hit.layer;
    }
    
    void ClearSelection() {
        selected_entity_ = nullptr;
        selected_layer_ = nullptr;
    }
    
    Entity* GetSelectedEntity() { return selected_entity_; }
    Layer* GetSelectedLayer() { return selected_layer_; }
};
```

---

## Timeline Rendering

### Tick Mark Math

Given a TimeExtent and pixel width, compute nice tick positions:

```cpp
struct TickLevel {
    double interval;      // seconds between ticks
    const char* format;   // strftime format for labels
};

TickLevel ChooseTickLevel(double duration_seconds, double target_ticks = 10) {
    static const TickLevel levels[] = {
        {1,           "%H:%M:%S"},      // 1 second
        {5,           "%H:%M:%S"},      // 5 seconds
        {15,          "%H:%M:%S"},      // 15 seconds
        {60,          "%H:%M"},         // 1 minute
        {300,         "%H:%M"},         // 5 minutes
        {900,         "%H:%M"},         // 15 minutes
        {3600,        "%H:%M"},         // 1 hour
        {21600,       "%b %d %H:%M"},   // 6 hours
        {86400,       "%b %d"},         // 1 day
        {604800,      "%b %d"},         // 1 week
        {2592000,     "%b %Y"},         // 30 days
        {31536000,    "%Y"},            // 1 year
    };
    
    double ideal_interval = duration_seconds / target_ticks;
    
    for (const auto& level : levels) {
        if (level.interval >= ideal_interval)
            return level;
    }
    return levels[std::size(levels) - 1];
}

std::vector<double> ComputeTickPositions(const TimeExtent& extent, double interval) {
    std::vector<double> ticks;
    double first = ceil(extent.start / interval) * interval;
    for (double t = first; t <= extent.end; t += interval) {
        ticks.push_back(t);
    }
    return ticks;
}
```

---

## Future Considerations

### Downsampling (Backend Support)

The backend supports `resample: { "method": "uniform_time", "n": N }` for dense point series like GPS. Use this when zoomed out:

```cpp
void OnExtentChanged(const TimeExtent& time, const SpatialExtent& space) override {
    int estimated_points = EstimatePointCount(time);
    
    nlohmann::json request = /* ... */;
    
    if (estimated_points > 10000) {
        request["resample"] = {
            {"method", "uniform_time"},
            {"n", 2000}
        };
    }
    
    // fetch...
}
```

### Timeline Histogram

When zoomed out with many overlapping points, render a histogram in the background. This is per-layer logic — add to Layer interface if needed.

### Spatial Entities (Tracks/Areas)

If needed, extend Entity:
```cpp
std::vector<std::pair<double, double>> path;  // empty for points
SpatialExtent bounding_box() const;
```

---

## Task Breakdown

### Foundation
- [ ] Define core types: Entity, TimeExtent, SpatialExtent
- [ ] Define Layer interface
- [ ] Implement AppModel skeleton
- [ ] HTTP client wrapper (curl/httplib)
- [ ] JSON parsing utilities

### Timeline
- [ ] Tick mark math (compute nice intervals)
- [ ] Timeline axis rendering
- [ ] Timeline interaction (pan/zoom → new TimeExtent)

### Map
- [ ] GridMapBackground
- [ ] Map interaction (pan/zoom → new SpatialExtent)
- [ ] (Later) RasterTileMapBackground

### Layers
- [ ] BackendLayer base class with bbox/time fetch
- [ ] SunLayer (computed, no backend)
- [ ] First real data layer (location.gps)

### Interaction
- [ ] Hit testing (map + timeline)
- [ ] Selection state and rendering
- [ ] Entity info panel (imgui)

### Polish
- [ ] Loading states for async fetches
- [ ] Request debouncing / hysteresis
- [ ] Error handling
- [ ] Timeline histogram for dense data
