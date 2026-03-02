#include "tiles/RasterTileCache.h"
#include <curl/curl.h>
#include <algorithm>
#include <string>

// stb_image — PNG/JPEG/WebP decoder. Define implementation exactly once here.
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static size_t curlWrite(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* buf = static_cast<std::vector<uint8_t>*>(userp);
    size_t total = size * nmemb;
    buf->insert(buf->end(),
                static_cast<uint8_t*>(contents),
                static_cast<uint8_t*>(contents) + total);
    return total;
}

RasterTileCache::RasterTileCache(std::string urlTemplate)
    : m_urlTemplate(std::move(urlTemplate)) {}

RasterTileCache::~RasterTileCache() {
    for (auto& f : m_inFlight) { if (f.valid()) f.wait(); }
    for (auto* h : m_curlPool) curl_easy_cleanup(h);
}

CURL* RasterTileCache::acquireCurl() {
    std::lock_guard<std::mutex> lock(m_curlPoolMutex);
    if (!m_curlPool.empty()) {
        CURL* h = m_curlPool.back();
        m_curlPool.pop_back();
        return h;
    }
    CURL* h = curl_easy_init();
    if (h) {
        curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, curlWrite);
        curl_easy_setopt(h, CURLOPT_USERAGENT,     "reckoner/1.0");
        curl_easy_setopt(h, CURLOPT_TIMEOUT,       10L);
        curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(h, CURLOPT_ACCEPT_ENCODING, "");
    }
    return h;
}

void RasterTileCache::releaseCurl(CURL* h) {
    std::lock_guard<std::mutex> lock(m_curlPoolMutex);
    m_curlPool.push_back(h);
}

static std::string replaceAll(std::string s,
                               const std::string& from,
                               const std::string& to) {
    size_t pos = s.find(from);
    if (pos != std::string::npos) s.replace(pos, from.size(), to);
    return s;
}

RasterTileCache::FetchResult RasterTileCache::downloadAndDecode(const TileKey& key) {
    FetchResult result;
    result.key = key;

    CURL* curl = acquireCurl();
    if (!curl) return result;

    // Substitute {z}, {x}, {y} in the URL template
    std::string url = m_urlTemplate;
    url = replaceAll(url, "{z}", std::to_string(key.z));
    url = replaceAll(url, "{x}", std::to_string(key.x));
    url = replaceAll(url, "{y}", std::to_string(key.y));

    std::vector<uint8_t> raw;
    curl_easy_setopt(curl, CURLOPT_URL,       url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &raw);

    CURLcode res  = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    releaseCurl(curl);

    if (res != CURLE_OK || httpCode != 200 || raw.empty())
        return result;

    // Decode image into RGBA with stb_image.
    // Flip vertically so texture origin matches OpenGL (bottom-left).
    int w, h, ch;
    stbi_set_flip_vertically_on_load(1);
    uint8_t* data = stbi_load_from_memory(raw.data(),
                                           static_cast<int>(raw.size()),
                                           &w, &h, &ch, 4);
    if (!data) return result;

    result.pixels.assign(data, data + w * h * 4);
    result.width   = w;
    result.height  = h;
    result.success = true;
    stbi_image_free(data);
    return result;
}

void RasterTileCache::fetchTileAsync(const TileKey& key) {
    // Remove finished futures
    m_inFlight.erase(
        std::remove_if(m_inFlight.begin(), m_inFlight.end(),
            [](const std::future<void>& f) {
                return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
            }),
        m_inFlight.end());

    m_tiles[key].state = TileState::Fetching;
    m_inFlight.push_back(std::async(std::launch::async, [this, key]() {
        auto result = downloadAndDecode(key);
        std::lock_guard<std::mutex> lock(m_resultMutex);
        m_completedFetches.push_back(std::move(result));
    }));
}

void RasterTileCache::processCompletedFetches() {
    m_frameCounter++;

    std::deque<FetchResult> results;
    {
        std::lock_guard<std::mutex> lock(m_resultMutex);
        results.swap(m_completedFetches);
    }

    for (auto& r : results) {
        auto& entry = m_tiles[r.key];
        if (r.success) {
            entry.pixels = std::move(r.pixels);
            entry.width  = r.width;
            entry.height = r.height;
            entry.state  = TileState::Ready;
            entry.lastUsedFrame = m_frameCounter;
        } else {
            entry.state = TileState::Failed;
        }
    }

    drainPendingQueue();
}

const RasterTileEntry* RasterTileCache::requestTile(const TileKey& key) {
    auto it = m_tiles.find(key);
    if (it != m_tiles.end()) {
        it->second.lastUsedFrame = m_frameCounter;
        if (it->second.state == TileState::Ready)
            return &it->second;
        return nullptr;
    }

    m_tiles[key].state = TileState::Empty;
    m_pendingQueue.push_back(key);
    drainPendingQueue();
    return nullptr;
}

void RasterTileCache::drainPendingQueue() {
    int active = 0;
    for (const auto& [k, e] : m_tiles)
        if (e.state == TileState::Fetching) active++;

    while (!m_pendingQueue.empty() && active < MaxConcurrentFetches) {
        TileKey key = m_pendingQueue.front();
        m_pendingQueue.pop_front();
        if (m_tiles[key].state != TileState::Empty) continue;
        fetchTileAsync(key);
        active++;
    }
}

void RasterTileCache::evictOldTiles(int maxTiles) {
    int count = 0;
    for (const auto& [k, e] : m_tiles)
        if (e.state == TileState::Ready) count++;

    if (count <= maxTiles) return;

    std::vector<std::pair<TileKey, uint64_t>> candidates;
    for (const auto& [k, e] : m_tiles)
        if (e.state == TileState::Ready)
            candidates.push_back({k, e.lastUsedFrame});

    std::sort(candidates.begin(), candidates.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });

    int toEvict = count - maxTiles;
    for (int i = 0; i < toEvict && i < (int)candidates.size(); i++)
        m_tiles.erase(candidates[i].first);
}

int RasterTileCache::cachedCount() const {
    int n = 0;
    for (const auto& [k, e] : m_tiles)
        if (e.state == TileState::Ready) n++;
    return n;
}

int RasterTileCache::pendingCount() const {
    int n = 0;
    for (const auto& [k, e] : m_tiles)
        if (e.state == TileState::Fetching) n++;
    return n;
}
