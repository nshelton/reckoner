#include "HttpBackend.h"
#include "core/EnvLoader.h"
#include <iostream>
#include <unordered_set>

HttpBackend::HttpBackend(const std::string& base_url, const std::string& entity_type)
    : m_entityType(entity_type)
{
    // Load API key from env file (try project root, then parent dir for build/)
    auto env = EnvLoader::load(".env");
    std::string api_key = EnvLoader::get(env, "API_KEY");
    if (api_key.empty()) {
        env = EnvLoader::load("../.env");
        api_key = EnvLoader::get(env, "API_KEY");
    }

    if (api_key.empty()) {
        std::cerr << "Warning: No API_KEY found in .env or ../.env" << std::endl;
    }

    m_api = std::make_unique<BackendAPI>(base_url, api_key);
}

HttpBackend::HttpBackend(const std::string& base_url, const std::string& api_key, const std::string& entity_type)
    : m_api(std::make_unique<BackendAPI>(base_url, api_key))
    , m_entityType(entity_type)
{
}

void HttpBackend::fetchEntities(
    const TimeExtent& time,
    const SpatialExtent& space,
    std::function<void(std::vector<Entity>&&)> callback
) {
    try {
        std::vector<Entity> entities = m_api->fetch_bbox(m_entityType, time, space);
        callback(std::move(entities));
    } catch (const std::exception& e) {
        std::cerr << "HttpBackend: Fetch failed: " << e.what() << std::endl;
        callback(std::vector<Entity>{});
    }
}

void HttpBackend::fetchAllEntities(
    const TimeExtent& time,
    const SpatialExtent& space,
    std::function<void(std::vector<Entity>&&)> batch_callback
) {
    static constexpr int PAGE_LIMIT = 50000;

    m_cancelled.store(false);
    TimeExtent window = time;

    // Track IDs from the last batch to deduplicate across page boundaries
    std::unordered_set<std::string> last_batch_ids;

    while (!m_cancelled.load()) {
        std::vector<Entity> batch;
        try {
            batch = m_api->fetch_bbox(m_entityType, window, space, PAGE_LIMIT, "t_start_asc");
        } catch (const std::exception& e) {
            std::cerr << "HttpBackend: Paginated fetch failed: " << e.what() << std::endl;
            break;
        }

        if (batch.empty()) break;

        bool is_last_page = static_cast<int>(batch.size()) < PAGE_LIMIT/2;

        // Deduplicate: remove entities we already sent in the previous batch
        if (!last_batch_ids.empty()) {
            batch.erase(
                std::remove_if(batch.begin(), batch.end(),
                    [&](const Entity& e) { return last_batch_ids.count(e.id) > 0; }),
                batch.end());
        }

        // Remember IDs near the boundary (same timestamp as last entity)
        last_batch_ids.clear();
        if (!is_last_page && !batch.empty()) {
            double last_time = batch.back().time_start;
            // Advance window start to last entity's timestamp
            window.start = last_time;
            // Collect IDs with the same timestamp for dedup
            for (auto it = batch.rbegin(); it != batch.rend(); ++it) {
                if (it->time_start != last_time) break;
                last_batch_ids.insert(it->id);
            }
        }

        batch_callback(std::move(batch));

        if (is_last_page) break;
    }
}
