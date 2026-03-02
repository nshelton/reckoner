#include "Interaction.h"
#include "AppModel.h"
#include <imgui.h>
#include <ctime>
#include <cmath>
#include <cstdio>
#include <limits>

// ---- Spatial index management ----

void InteractionController::resetPickers()
{
    for (auto& count : m_lastEntityCounts)
        count = std::numeric_limits<size_t>::max();
    m_hoveredMap      = {};
    m_hoveredTimeline = {};
    m_selected        = {};
    m_pickersDirty    = true;
}

void InteractionController::ensurePickers(size_t count)
{
    while (m_pickers.size() < count) {
        m_pickers.emplace_back();
        m_lastEntityCounts.push_back(std::numeric_limits<size_t>::max());
    }
}

void InteractionController::update(const AppModel& model)
{
    if (!m_pickersDirty) return;

    ensurePickers(model.layers.size());

    for (size_t li = 0; li < model.layers.size(); ++li) {
        const auto& entities = model.layers[li].entities;
        size_t n    = entities.size();
        size_t last = m_lastEntityCounts[li];

        if (n < last)
            m_pickers[li].rebuild(entities);
        else if (n > last)
            m_pickers[li].addEntities(entities, last);

        m_lastEntityCounts[li] = n;
    }

    m_pickersDirty = false;
}

// ---- Pick helpers ----

PickResult InteractionController::pickMap(const Camera& camera, Vec2 localPx, const AppModel& model) const
{
    Vec2   worldPos = camera.screenToWorld(localPx);
    double radius   = camera.zoom() * 0.02;

    // Iterate in reverse so higher-indexed layers (higher render priority) win
    for (int li = static_cast<int>(m_pickers.size()) - 1; li >= 0; --li) {
        if (li >= static_cast<int>(model.layers.size())) continue;
        if (!model.layers[li].visible) continue;
        int idx = m_pickers[li].pickMap(worldPos.x, worldPos.y, radius);
        if (idx >= 0) return {li, idx};
    }
    return {};
}

PickResult InteractionController::pickTimeline(const TimelineCamera& camera, float localX, float localY, float panelHeight, const AppModel& model) const
{
    double time         = camera.screenToTime(localX);
    float  renderOffset = 1.0f - 2.0f * (localY / panelHeight);
    double timeRadius   = camera.zoom() * 0.02;
    float  yRadius      = 0.1f;

    for (int li = static_cast<int>(m_pickers.size()) - 1; li >= 0; --li) {
        if (li >= static_cast<int>(model.layers.size())) continue;
        if (!model.layers[li].visible) continue;
        // Subtract the layer's yOffset so the cursor maps into entity render_offset space
        float layerOffset = renderOffset - model.layers[li].yOffset;
        int idx = m_pickers[li].pickTimeline(time, layerOffset, timeRadius, yRadius);
        if (idx >= 0) return {li, idx};
    }
    return {};
}

// ---- Map canvas ----

void InteractionController::onMapDrag(Camera& camera, Vec2 mouseDeltaPx)
{
    m_state.panning = true;
    Vec2 worldDelta = camera.screenToWorld(Vec2(-mouseDeltaPx.x, -mouseDeltaPx.y))
                    - camera.screenToWorld(Vec2(0.0f, 0.0f));
    camera.move(worldDelta);
}

void InteractionController::onMapScroll(Camera& camera, float yoffset, Vec2 localPx)
{
    camera.zoomAtPixel(localPx, yoffset);
}

void InteractionController::onMapHover(const Camera& camera, Vec2 localPx, const AppModel& model)
{
    m_state.panning = false;
    m_hoveredMap = pickMap(camera, localPx, model);
    if (m_hoveredMap.valid())
        drawEntityTooltip(m_hoveredMap, model);
}

void InteractionController::onMapClick(const Camera& camera, Vec2 localPx, const AppModel& model)
{
    PickResult pick = pickMap(camera, localPx, model);
    if (pick.valid())
        m_selected = pick;
}

void InteractionController::onMapDoubleClick(TimelineCamera& timeline, const Camera& camera, Vec2 localPx, const AppModel& model)
{
    PickResult pick = pickMap(camera, localPx, model);
    if (!pick.valid()) return;
    const auto& e = model.layers[pick.layerIndex].entities[pick.entityIndex];
    timeline.setCenter(e.time_mid());
}

void InteractionController::onMapUnhovered()
{
    m_state.panning = false;
    m_hoveredMap = {};
}

// ---- Timeline canvas ----

void InteractionController::onTimelineDrag(TimelineCamera& camera, float deltaX)
{
    camera.panByPixels(deltaX);
}

void InteractionController::onTimelineScroll(TimelineCamera& camera, float yoffset, float localX)
{
    camera.zoomAtPixel(localX, yoffset);
}

void InteractionController::onTimelineHover(const TimelineCamera& camera, float localX, float localY, float panelHeight, const AppModel& model)
{
    m_hoveredTimeline = pickTimeline(camera, localX, localY, panelHeight, model);
    if (m_hoveredTimeline.valid())
        drawEntityTooltip(m_hoveredTimeline, model);
}

void InteractionController::onTimelineClick(const TimelineCamera& camera, float localX, float localY, float panelHeight, const AppModel& model)
{
    PickResult pick = pickTimeline(camera, localX, localY, panelHeight, model);
    if (pick.valid())
        m_selected = pick;
}

void InteractionController::onTimelineDoubleClick(Camera& map, const TimelineCamera& camera, float localX, float localY, float panelHeight, const AppModel& model)
{
    PickResult pick = pickTimeline(camera, localX, localY, panelHeight, model);
    if (!pick.valid()) return;
    const auto& e = model.layers[pick.layerIndex].entities[pick.entityIndex];
    if (e.has_location())
        map.setCenter({static_cast<float>(*e.lon), static_cast<float>(*e.lat)});
}

void InteractionController::onTimelineUnhovered()
{
    m_hoveredTimeline = {};
}

// ---- Formatting helpers ----

namespace {

static std::string fmtTimestamp(double t, bool includeSeconds = true)
{
    std::time_t tt = static_cast<std::time_t>(t);
    if (std::tm* tm = std::gmtime(&tt)) {
        char buf[64];
        std::strftime(buf, sizeof(buf),
            includeSeconds ? "%Y-%m-%d  %H:%M:%S UTC"
                           : "%Y-%m-%d  %H:%M UTC",
            tm);
        return buf;
    }
    return "(invalid)";
}

static std::string fmtDuration(double seconds)
{
    if (seconds <= 0.0) return "instant";
    long s = static_cast<long>(seconds);
    if (s < 60)          return std::to_string(s) + "s";
    long m = s / 60; s %= 60;
    if (m < 60)          return std::to_string(m) + "m " + std::to_string(s) + "s";
    long h = m / 60; m %= 60;
    if (h < 24)          return std::to_string(h) + "h " + std::to_string(m) + "m";
    long d = h / 24; h %= 24;
    return std::to_string(d) + "d " + std::to_string(h) + "h";
}

static std::string fmtLat(double lat)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.6f° %c", std::abs(lat), lat >= 0.0 ? 'N' : 'S');
    return buf;
}

static std::string fmtLon(double lon)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.6f° %c", std::abs(lon), lon >= 0.0 ? 'E' : 'W');
    return buf;
}

} // namespace

// ---- ImGui rendering ----

void InteractionController::drawEntityTooltip(PickResult pick, const AppModel& model) const
{
    if (!pick.valid()) return;
    const auto& layer = model.layers[pick.layerIndex];
    const auto& e     = layer.entities[pick.entityIndex];

    ImGui::BeginTooltip();

    ImVec4 col(layer.color.r, layer.color.g, layer.color.b, 1.0f);
    ImGui::TextColored(col, "[%s]", layer.name.c_str());
    if (e.name) {
        ImGui::SameLine();
        ImGui::TextUnformatted(e.name->c_str());
    }

    if (e.has_location())
        ImGui::Text("lat %.5f  lon %.5f", *e.lat, *e.lon);

    std::time_t t = static_cast<std::time_t>(e.time_mid());
    if (std::tm* tm_info = std::gmtime(&t)) {
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S UTC", tm_info);
        ImGui::Text("%s", buf);
    }

    ImGui::TextDisabled("click to select");
    ImGui::EndTooltip();
}

void InteractionController::drawDetailsPanel(const AppModel& model)
{
    ImGui::Begin("Details");

    if (!m_selected.valid() ||
        m_selected.layerIndex >= static_cast<int>(model.layers.size()))
    {
        ImGui::TextDisabled("No entity selected.");
        ImGui::TextDisabled("Click an entity to view details.");
        ImGui::End();
        return;
    }

    const auto& layer = model.layers[m_selected.layerIndex];
    if (m_selected.entityIndex >= static_cast<int>(layer.entities.size())) {
        ImGui::TextDisabled("(entity no longer available)");
        if (ImGui::SmallButton("Clear")) m_selected = {};
        ImGui::End();
        return;
    }

    const auto& e     = layer.entities[m_selected.entityIndex];
    ImVec4      col(layer.color.r, layer.color.g, layer.color.b, 1.0f);

    // ── Header: type badge | name | [×] ──────────────────────────────────────
    ImGui::TextColored(col, "%s", layer.name.c_str());
    if (e.name && !e.name->empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("·");
        ImGui::SameLine();
        ImGui::TextUnformatted(e.name->c_str());
    }
    {
        const char* deselLabel = "×";
        float deselW = ImGui::CalcTextSize(deselLabel).x
                     + ImGui::GetStyle().FramePadding.x * 2.0f;
        float posX = ImGui::GetWindowContentRegionMax().x - deselW;
        if (posX > ImGui::GetCursorPosX())
            ImGui::SameLine(posX);
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 0.3f, 0.3f, 0.4f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1, 0.3f, 0.3f, 0.7f));
        if (ImGui::SmallButton(deselLabel))
            m_selected = {};
        ImGui::PopStyleColor(3);
    }

    ImGui::Separator();

    // ── Time ─────────────────────────────────────────────────────────────────
    ImGui::TextDisabled("TIME");

    constexpr float kLabelW = 70.0f;
    if (ImGui::BeginTable("##time", 2, ImGuiTableFlags_None)) {
        ImGui::TableSetupColumn("##lbl", ImGuiTableColumnFlags_WidthFixed, kLabelW);
        ImGui::TableSetupColumn("##val", ImGuiTableColumnFlags_WidthStretch);

        if (e.is_instant()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextDisabled("When");
            ImGui::TableNextColumn(); ImGui::Text("%s", fmtTimestamp(e.time_start).c_str());

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TableNextColumn(); ImGui::TextDisabled("instant");
        } else {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextDisabled("Start");
            ImGui::TableNextColumn(); ImGui::Text("%s", fmtTimestamp(e.time_start).c_str());

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextDisabled("End");
            ImGui::TableNextColumn(); ImGui::Text("%s", fmtTimestamp(e.time_end).c_str());

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextDisabled("Duration");
            ImGui::TableNextColumn(); ImGui::Text("%s", fmtDuration(e.duration()).c_str());
        }

        ImGui::EndTable();
    }

    // ── Location ─────────────────────────────────────────────────────────────
    if (e.has_location()) {
        ImGui::Separator();
        ImGui::TextDisabled("LOCATION");

        if (ImGui::BeginTable("##loc", 2, ImGuiTableFlags_None)) {
            ImGui::TableSetupColumn("##lbl", ImGuiTableColumnFlags_WidthFixed, kLabelW);
            ImGui::TableSetupColumn("##val", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextDisabled("Lat");
            ImGui::TableNextColumn(); ImGui::Text("%s", fmtLat(*e.lat).c_str());

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextDisabled("Lon");
            ImGui::TableNextColumn(); ImGui::Text("%s", fmtLon(*e.lon).c_str());

            ImGui::EndTable();
        }

        if (ImGui::SmallButton("Copy coordinates")) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%.6f, %.6f", *e.lat, *e.lon);
            ImGui::SetClipboardText(buf);
        }
    }

    // ── Entity ID ─────────────────────────────────────────────────────────────
    ImGui::Separator();
    ImGui::TextDisabled("ID");
    ImGui::SameLine();
    ImGui::TextUnformatted(e.id.c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton("Copy##id"))
        ImGui::SetClipboardText(e.id.c_str());

    ImGui::End();
}
