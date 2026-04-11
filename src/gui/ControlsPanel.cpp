#include "ControlsPanel.h"

void ControlsPanel::draw(
    const FpsTracker& fps,
    const AppModel& model,
    const BackendConfig& backendConfig,
    char* backendUrl, size_t backendUrlSize,
    const ServerStats& serverStats, bool hasServerStats,
    const Renderer& renderer,
    const TimelineRenderer& timelineRenderer,
    const Camera& camera,
    const TimelineCamera& timelineCamera,
    ControlsActions& actions)
{
    ImGui::Begin("Controls");

    ImGui::Text("%.1f FPS  (%.2f ms)", fps.fps(), fps.frameMs());

    ImGui::Separator();
    if (ImGui::Button("Reset Map")) actions.resetMap = true;
    ImGui::SameLine();
    if (ImGui::Button("Reset Timeline")) actions.resetTimeline = true;

    ImGui::Separator();
    ImGui::Text("Backend Configuration:");

    const char* backend_types[] = {"Fake Data", "HTTP Backend"};
    int current_type = static_cast<int>(backendConfig.type);
    if (ImGui::Combo("Backend Type", &current_type, backend_types, 2))
        actions.switchBackendType = current_type;

    if (backendConfig.type == BackendConfig::Type::Http) {
        ImGui::InputText("Backend URL", backendUrl, backendUrlSize);
        if (ImGui::Button("Apply HTTP Config"))
            actions.applyHttpConfig = true;
    }

    ImGui::Separator();
    ImGui::Text("View Extent:");
    ImGui::Text("Lon: %.6f to %.6f",
        model.spatial_extent.min_lon, model.spatial_extent.max_lon);
    ImGui::Text("Lat: %.6f to %.6f",
        model.spatial_extent.min_lat, model.spatial_extent.max_lat);

    ImGui::Separator();
    ImGui::Text("Server Stats:");
    if (hasServerStats) {
        ImGui::Text("Total entities: %d", serverStats.total_entities);
        for (const auto& [type, count] : serverStats.entities_by_type)
            ImGui::Text("  %s: %d", type.c_str(), count);
        if (!serverStats.oldest_time.empty()) {
            ImGui::Text("Coverage: %s", serverStats.oldest_time.substr(0, 10).c_str());
            ImGui::Text("      to: %s", serverStats.newest_time.substr(0, 10).c_str());
        }
        ImGui::Text("DB size: %.1f MB", serverStats.db_size_mb);
    } else {
        ImGui::TextDisabled("No stats available");
    }

    ImGui::Separator();
    ImGui::Text("Layers:");
    // Note: layer visibility/alpha are mutated directly via ImGui widgets
    // (checkbox and slider bind to the layer's fields). This is the standard
    // ImGui immediate-mode pattern and doesn't need action indirection.
    for (auto& layer : const_cast<AppModel&>(model).layers) {
        ImGui::PushID(layer.name.c_str());
        bool vis = layer.visible;
        if (ImGui::Checkbox("##vis", &vis))
            layer.visible = vis;
        ImGui::SameLine();
        ImVec4 col(layer.color.r, layer.color.g, layer.color.b, 1.0f);
        ImGui::ColorButton("##col", col,
            ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder,
            ImVec2(14, 14));
        ImGui::SameLine();
        size_t count = layer.entities.size();
        if (layer.is_fetching)
            ImGui::Text("%s: %zu (loading...)", layer.name.c_str(), count);
        else
            ImGui::Text("%s: %zu", layer.name.c_str(), count);
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##alpha", &layer.color.a, 0.0f, 1.0f, "alpha %.3f");
        ImGui::PopID();
    }

    {
        const auto& gpsLayer = model.layers[0];
        size_t loaded = gpsLayer.entities.size();
        size_t total  = model.total_expected.load();
        if (gpsLayer.is_fetching) {
            if (total > 0) {
                float progress = static_cast<float>(loaded) / static_cast<float>(total);
                ImGui::ProgressBar(progress, ImVec2(-1, 0));
            } else {
                ImGui::ProgressBar(-1.0f, ImVec2(-1, 0));
            }
        } else if (model.initial_load_complete.load()) {
            ImGui::Text("Loaded: %zu GPS  /  %zu photos  /  %zu calendar",
                model.layers[0].entities.size(),
                model.layers[1].entities.size(),
                model.layers[2].entities.size());
        }
    }

    ImGui::Text("Points rendered: %d", renderer.totalPoints());

    if (model.fetch_latencies.count() > 0) {
        ImGui::Separator();
        ImGui::Text("Fetch Latency (ms):");
        ImGui::Text("  Avg: %.1f", model.fetch_latencies.average());
        ImGui::Text("  Min: %.1f", model.fetch_latencies.min());
        ImGui::Text("  Max: %.1f", model.fetch_latencies.max());
        ImGui::Text("  Samples: %zu", model.fetch_latencies.count());
    }

    ImGui::Separator();
    ImGui::Text("Timeline Overlays:");
    bool histogramEnabled = timelineRenderer.histogramEnabled();
    if (ImGui::Checkbox("Histogram", &histogramEnabled))
        actions.histogram = histogramEnabled ? 1 : 0;
    bool solarAltEnabled = timelineRenderer.solarAltitudeEnabled();
    if (ImGui::Checkbox("Solar Altitude", &solarAltEnabled))
        actions.solarAltitude = solarAltEnabled ? 1 : 0;
    bool moonAltEnabled = timelineRenderer.moonAltitudeEnabled();
    if (ImGui::Checkbox("Moon Altitude", &moonAltEnabled))
        actions.moonAltitude = moonAltEnabled ? 1 : 0;

    ImGui::Separator();
    ImGui::Text("Rendering:");

    static const char* kTileModeLabels[] = {
        "None",
        "Vector (Versatiles)",
        "OSM Standard (labeled)",
        "CartoDB Positron (labeled)",
        "CartoDB Dark Matter (labeled)",
    };
    int tileMode = static_cast<int>(renderer.tileMode());
    if (ImGui::Combo("Map Tiles", &tileMode, kTileModeLabels, IM_ARRAYSIZE(kTileModeLabels)))
        actions.tileMode = tileMode;

    float pointSize = renderer.pointSize();
    if (ImGui::SliderFloat("Point Size", &pointSize, 0.01f, 1.0f, "%.1f"))
        actions.pointSize = pointSize;

    ImGui::Separator();
    if (ImGui::Button("Reload All Data"))
        actions.reloadAllData = true;

    ImGui::Separator();
    ImGui::Text("Map: center (%.4f, %.4f) zoom %.4f",
                 camera.center().x, camera.center().y, camera.zoom());
    ImGui::Text("Timeline: zoom %.0fs", timelineCamera.zoom());

    ImGui::End();
}
