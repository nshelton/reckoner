#include "tiles/TileCache.h"
#include "tiles/MvtDecoder.h"
#include <curl/curl.h>
#include <iostream>
#include <algorithm>

static size_t curlWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* buffer = static_cast<std::vector<uint8_t>*>(userp);
    size_t totalSize = size * nmemb;
    buffer->insert(buffer->end(), static_cast<uint8_t*>(contents),
                   static_cast<uint8_t*>(contents) + totalSize);
    return totalSize;
}

TileCache::~TileCache() {
    for (auto& f : m_inFlight) {
        if (f.valid()) f.wait();
    }
}

TileCache::FetchResult TileCache::downloadAndDecode(const TileKey& key) {
    FetchResult result;
    result.key = key;

    CURL* curl = curl_easy_init();
    if (!curl) return result;

    // Versatiles OSM vector tiles (free, no API key)
    char url[256];
    snprintf(url, sizeof(url),
             "https://tiles.versatiles.org/tiles/osm/%d/%d/%d.pbf",
             key.z, key.x, key.y);

    std::vector<uint8_t> rawData;
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &rawData);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "reckoner/1.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, ""); // Accept gzip/deflate

    CURLcode res = curl_easy_perform(curl);

    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || httpCode != 200) {
        return result;
    }

    // Decode MVT protobuf into line segments
    result.lines = MvtDecoder::decode(rawData.data(), rawData.size(),
                                       key.x, key.y, key.z);
    result.success = true;
    return result;
}

void TileCache::fetchTileAsync(const TileKey& key) {
    // Clean up completed futures
    m_inFlight.erase(
        std::remove_if(m_inFlight.begin(), m_inFlight.end(),
            [](const std::future<void>& f) {
                return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
            }),
        m_inFlight.end());

    m_tiles[key].state = TileState::Fetching;

    m_inFlight.push_back(std::async(std::launch::async, [this, key]() {
        auto result = downloadAndDecode(key);
        {
            std::lock_guard<std::mutex> lock(m_resultMutex);
            m_completedFetches.push_back(std::move(result));
        }
    }));
}

void TileCache::processCompletedFetches() {
    m_frameCounter++;

    std::deque<FetchResult> results;
    {
        std::lock_guard<std::mutex> lock(m_resultMutex);
        results.swap(m_completedFetches);
    }

    for (auto& result : results) {
        auto& entry = m_tiles[result.key];
        if (result.success) {
            entry.lines = std::move(result.lines);
            entry.state = TileState::Ready;
            entry.lastUsedFrame = m_frameCounter;
        } else {
            entry.state = TileState::Failed;
        }
    }
}

const std::vector<TileLine>* TileCache::requestTile(const TileKey& key) {
    auto it = m_tiles.find(key);
    if (it != m_tiles.end()) {
        it->second.lastUsedFrame = m_frameCounter;
        if (it->second.state == TileState::Ready) {
            return &it->second.lines;
        }
        return nullptr;
    }

    // Count active fetches
    int activeCount = 0;
    for (const auto& [k, e] : m_tiles) {
        if (e.state == TileState::Fetching) activeCount++;
    }

    if (activeCount < 4) {
        fetchTileAsync(key);
    }

    return nullptr;
}

void TileCache::evictOldTiles(int maxTiles) {
    int count = 0;
    for (const auto& [k, e] : m_tiles) {
        if (e.state == TileState::Ready) count++;
    }

    if (count <= maxTiles) return;

    std::vector<std::pair<TileKey, uint64_t>> candidates;
    for (const auto& [k, e] : m_tiles) {
        if (e.state == TileState::Ready) {
            candidates.push_back({k, e.lastUsedFrame});
        }
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });

    int toEvict = count - maxTiles;
    for (int i = 0; i < toEvict && i < static_cast<int>(candidates.size()); i++) {
        m_tiles.erase(candidates[i].first);
    }
}

int TileCache::cachedCount() const {
    int count = 0;
    for (const auto& [k, e] : m_tiles) {
        if (e.state == TileState::Ready) count++;
    }
    return count;
}

int TileCache::pendingCount() const {
    int count = 0;
    for (const auto& [k, e] : m_tiles) {
        if (e.state == TileState::Fetching) count++;
    }
    return count;
}
