#pragma once

#include "tiles/MvtDecoder.h"
#include <unordered_map>
#include <vector>
#include <mutex>
#include <future>
#include <deque>
#include <cstdint>

typedef void CURL;

struct TileKey {
    int z, x, y;
    bool operator==(const TileKey& o) const { return z == o.z && x == o.x && y == o.y; }
};

struct TileKeyHash {
    size_t operator()(const TileKey& k) const {
        return std::hash<int>()(k.z) ^ (std::hash<int>()(k.x) << 10) ^ (std::hash<int>()(k.y) << 20);
    }
};

enum class TileState { Empty, Fetching, Ready, Failed };

struct TileEntry {
    TileState state = TileState::Empty;
    std::vector<TileLine> lines;
    uint64_t lastUsedFrame = 0;
};

class TileCache {
public:
    TileCache();
    ~TileCache();

    /// Process completed async fetches (call each frame from main thread)
    void processCompletedFetches();

    /// Request a tile. Returns pointer to cached lines if ready, nullptr otherwise.
    /// Kicks off async fetch if tile not yet requested.
    const std::vector<TileLine>* requestTile(const TileKey& key);

    /// Evict old tiles to stay under maxTiles
    void evictOldTiles(int maxTiles = 256);

    int cachedCount() const;
    int pendingCount() const;

private:
    struct FetchResult {
        TileKey key;
        std::vector<TileLine> lines;
        bool success = false;
    };

    std::unordered_map<TileKey, TileEntry, TileKeyHash> m_tiles;
    uint64_t m_frameCounter = 0;

    std::mutex m_resultMutex;
    std::deque<FetchResult> m_completedFetches;
    std::vector<std::future<void>> m_inFlight;
    std::deque<TileKey> m_pendingQueue;

    // Pool of reusable CURL handles — each keeps its TCP+TLS connection alive
    std::mutex m_curlPoolMutex;
    std::vector<CURL*> m_curlPool;
    static constexpr int MaxConcurrentFetches = 10;

    void fetchTileAsync(const TileKey& key);
    void drainPendingQueue();
    FetchResult downloadAndDecode(const TileKey& key);
    CURL* acquireCurl();
    void releaseCurl(CURL* curl);
};
