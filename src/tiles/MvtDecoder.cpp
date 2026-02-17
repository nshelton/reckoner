#include "tiles/MvtDecoder.h"
#include "tiles/TileMath.h"
#include <cstring>
#include <unordered_map>

// --- Minimal protobuf wire format decoder ---

namespace {

struct PbReader {
    const uint8_t* data;
    size_t pos;
    size_t end;

    PbReader(const uint8_t* d, size_t len) : data(d), pos(0), end(len) {}
    PbReader(const uint8_t* d, size_t offset, size_t len) : data(d), pos(offset), end(offset + len) {}

    bool hasMore() const { return pos < end; }

    uint64_t readVarint() {
        uint64_t result = 0;
        int shift = 0;
        while (pos < end) {
            uint8_t b = data[pos++];
            result |= static_cast<uint64_t>(b & 0x7F) << shift;
            if ((b & 0x80) == 0) break;
            shift += 7;
        }
        return result;
    }

    int32_t readSVarint() {
        uint64_t v = readVarint();
        return static_cast<int32_t>((v >> 1) ^ -(static_cast<int64_t>(v) & 1));
    }

    // Read field tag, returns {field_number, wire_type}
    std::pair<uint32_t, uint32_t> readTag() {
        uint64_t v = readVarint();
        return {static_cast<uint32_t>(v >> 3), static_cast<uint32_t>(v & 0x7)};
    }

    // Skip a field based on wire type
    void skip(uint32_t wireType) {
        switch (wireType) {
            case 0: readVarint(); break;                    // varint
            case 1: pos += 8; break;                        // 64-bit
            case 2: { uint64_t len = readVarint(); pos += len; break; } // length-delimited
            case 5: pos += 4; break;                        // 32-bit
            default: pos = end; break;
        }
    }

    // Read length-delimited bytes, returns sub-reader
    PbReader readBytes() {
        uint64_t len = readVarint();
        PbReader sub(data, pos, len);
        pos += len;
        return sub;
    }

    // Read packed uint32 array
    std::vector<uint32_t> readPackedUint32() {
        uint64_t len = readVarint();
        size_t endPos = pos + len;
        std::vector<uint32_t> result;
        while (pos < endPos) {
            result.push_back(static_cast<uint32_t>(readVarint()));
        }
        return result;
    }

    std::string readString() {
        uint64_t len = readVarint();
        std::string s(reinterpret_cast<const char*>(data + pos), len);
        pos += len;
        return s;
    }
};

// MVT geometry types
enum GeomType { UNKNOWN = 0, POINT = 1, LINESTRING = 2, POLYGON = 3 };

struct MvtFeature {
    GeomType type = UNKNOWN;
    std::vector<uint32_t> geometry;
    std::vector<uint32_t> tags;
};

struct MvtLayer {
    std::string name;
    uint32_t extent = 4096;
    std::vector<MvtFeature> features;
    std::vector<std::string> keys;
};

Color colorForLayer(const std::string& name) {
    // Versatiles layer names
    if (name == "streets")          return Color(0.45f, 0.45f, 0.45f, 0.2f);
    if (name == "bridges")          return Color(0.45f, 0.45f, 0.45f, 0.2f);
    if (name == "street_polygons")  return Color(0.4f, 0.4f, 0.4f, 0.2f);
    if (name == "buildings")        return Color(0.35f, 0.35f, 0.35f, 0.2f);
    if (name == "water_polygons")   return Color(0.2f, 0.3f, 0.5f, 0.2f);
    if (name == "land")             return Color(0.3f, 0.35f, 0.3f, 0.2f);
    // OpenMapTiles layer names (fallback)
    if (name == "transportation") return Color(0.45f, 0.45f, 0.45f, 0.2f);
    if (name == "boundary")       return Color(0.6f, 0.5f, 0.3f, 0.2f);
    if (name == "water")          return Color(0.2f, 0.3f, 0.5f, 0.2f);
    if (name == "waterway")       return Color(0.2f, 0.3f, 0.5f, 0.2f);
    if (name == "building")       return Color(0.35f, 0.35f, 0.35f, 0.2f);
    return Color(0.4f, 0.4f, 0.4f, 0.2f);
}

// Decode geometry commands into line segments (in tile-local coords 0..extent)
void decodeGeometry(const std::vector<uint32_t>& geom, GeomType type,
                    std::vector<std::pair<Vec2, Vec2>>& lines) {
    int32_t cx = 0, cy = 0;
    size_t i = 0;

    Vec2 moveTo(0, 0);
    Vec2 lastPos(0, 0);
    bool hasLast = false;

    while (i < geom.size()) {
        uint32_t cmdInt = geom[i++];
        uint32_t cmdId = cmdInt & 0x7;
        uint32_t count = cmdInt >> 3;

        if (cmdId == 1) { // MoveTo
            for (uint32_t j = 0; j < count && i + 1 < geom.size(); j++) {
                int32_t dx = static_cast<int32_t>((geom[i] >> 1) ^ -(static_cast<int64_t>(geom[i]) & 1)); i++;
                int32_t dy = static_cast<int32_t>((geom[i] >> 1) ^ -(static_cast<int64_t>(geom[i]) & 1)); i++;
                cx += dx;
                cy += dy;
            }
            lastPos = Vec2(static_cast<float>(cx), static_cast<float>(cy));
            moveTo = lastPos;
            hasLast = true;
        } else if (cmdId == 2) { // LineTo
            for (uint32_t j = 0; j < count && i + 1 < geom.size(); j++) {
                int32_t dx = static_cast<int32_t>((geom[i] >> 1) ^ -(static_cast<int64_t>(geom[i]) & 1)); i++;
                int32_t dy = static_cast<int32_t>((geom[i] >> 1) ^ -(static_cast<int64_t>(geom[i]) & 1)); i++;
                cx += dx;
                cy += dy;
                Vec2 newPos(static_cast<float>(cx), static_cast<float>(cy));
                if (hasLast) {
                    lines.push_back({lastPos, newPos});
                }
                lastPos = newPos;
                hasLast = true;
            }
        } else if (cmdId == 7) { // ClosePath
            if (hasLast) {
                lines.push_back({lastPos, moveTo});
                lastPos = moveTo;
            }
        }
    }
}

} // anonymous namespace

std::vector<TileLine> MvtDecoder::decode(const uint8_t* data, size_t size,
                                          int tileX, int tileY, int zoom) {
    std::vector<TileLine> result;

    // Layers we care about (both Versatiles and OpenMapTiles names)
    static const std::unordered_map<std::string, bool> wantedLayers = {
        // Versatiles
        {"streets", true},
        {"bridges", true},
        {"street_polygons", true},
        {"buildings", true},
        {"water_polygons", true},
        {"land", true},
        // OpenMapTiles
        {"transportation", true},
        {"boundary", true},
        {"water", true},
        {"waterway", true},
        {"building", true},
    };

    PbReader tile(data, size);

    while (tile.hasMore()) {
        auto [field, wire] = tile.readTag();
        if (field == 3 && wire == 2) { // Layer
            PbReader layerReader = tile.readBytes();

            MvtLayer layer;

            // First pass: get layer name and extent
            PbReader probe(layerReader.data, layerReader.pos, layerReader.end - layerReader.pos);
            while (probe.hasMore()) {
                auto [f, w] = probe.readTag();
                if (f == 1 && w == 2) { layer.name = probe.readString(); }
                else if (f == 5 && w == 0) { layer.extent = static_cast<uint32_t>(probe.readVarint()); }
                else { probe.skip(w); }
            }

            // Skip layers we don't care about
            if (wantedLayers.find(layer.name) == wantedLayers.end()) {
                continue;
            }

            Color layerColor = colorForLayer(layer.name);

            // Second pass: extract features
            PbReader featurePass(layerReader.data, layerReader.pos, layerReader.end - layerReader.pos);
            while (featurePass.hasMore()) {
                auto [f, w] = featurePass.readTag();
                if (f == 2 && w == 2) { // Feature
                    PbReader featReader = featurePass.readBytes();
                    MvtFeature feat;

                    while (featReader.hasMore()) {
                        auto [ff, fw] = featReader.readTag();
                        if (ff == 3 && fw == 0) {
                            feat.type = static_cast<GeomType>(featReader.readVarint());
                        } else if (ff == 4 && fw == 2) {
                            feat.geometry = featReader.readPackedUint32();
                        } else {
                            featReader.skip(fw);
                        }
                    }

                    // Only process lines and polygons
                    if (feat.type == LINESTRING || feat.type == POLYGON) {
                        std::vector<std::pair<Vec2, Vec2>> localLines;
                        decodeGeometry(feat.geometry, feat.type, localLines);

                        // Convert tile-local coords to lat/lon
                        double ext = static_cast<double>(layer.extent);
                        for (auto& [a, b] : localLines) {
                            double lonA = TileMath::tileXToLon(tileX + a.x / ext, zoom);
                            double latA = TileMath::tileYToLat(tileY + a.y / ext, zoom);
                            double lonB = TileMath::tileXToLon(tileX + b.x / ext, zoom);
                            double latB = TileMath::tileYToLat(tileY + b.y / ext, zoom);

                            result.push_back({
                                Vec2(static_cast<float>(lonA), static_cast<float>(latA)),
                                Vec2(static_cast<float>(lonB), static_cast<float>(latB)),
                                layerColor
                            });
                        }
                    }
                } else {
                    featurePass.skip(w);
                }
            }
        } else {
            tile.skip(wire);
        }
    }

    return result;
}
