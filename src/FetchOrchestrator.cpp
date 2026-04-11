#include "FetchOrchestrator.h"
#include <iostream>
#include <chrono>
#include <iterator>

namespace {
    using Clock = std::chrono::steady_clock;

    inline long ms_since(Clock::time_point t0) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count();
    }
}

FetchOrchestrator::~FetchOrchestrator()
{
    cancelAndWaitAll();
}

void FetchOrchestrator::setBackends(BackendSet backends, BackendConfig::Type type)
{
    m_backendType = type;
    m_backends = std::move(backends);
    std::lock_guard<std::mutex> lock(m_batchMutex);
    m_completedBatches.clear();
}

void FetchOrchestrator::startFullLoad(AppModel& model)
{
    std::cerr << "[LOAD] startFullLoad begin\n";

    {
        std::lock_guard<std::mutex> lock(m_batchMutex);
        m_completedBatches.clear();
    }

    for (auto& layer : model.layers) {
        layer.entities.clear();
        layer.is_fetching = false;
    }
    model.initial_load_complete.store(false);
    model.total_expected.store(0);

    if (m_backendType == BackendConfig::Type::Http) {
        if (m_backends.gps) {
            auto* gps = m_backends.gps.get();
            model.layers[0].startFetch();
            m_pendingGpsFetch = std::async(std::launch::async, [this, &model, gps]() {
                std::cerr << "[GPS] stream started\n";
                size_t batchNum = 0;
                gps->streamAllEntities(
                    [&model](size_t total) {
                        std::cerr << "[GPS] total expected: " << total << "\n";
                        model.total_expected.store(total);
                    },
                    [this, &batchNum](std::vector<Entity>&& batch) {
                        ++batchNum;
                        std::cerr << "[GPS] batch " << batchNum
                                  << " size=" << batch.size() << "\n";
                        auto t = Clock::now();
                        std::lock_guard<std::mutex> lock(m_batchMutex);
                        long lockMs = ms_since(t);
                        if (lockMs > 5)
                            std::cerr << "[GPS] waited " << lockMs << "ms for batchMutex\n";
                        m_completedBatches.push_back({0, std::move(batch)});
                    });
                std::cerr << "[GPS] stream complete, " << batchNum << " batches\n";
                model.fetch_latencies.push(static_cast<float>(
                    ms_since(model.layers[0].last_fetch_start)));
                model.layers[0].endFetch();
                model.initial_load_complete.store(true);
            });
        }

        struct TypeFetch {
            int layerIndex;
            Backend* backend;
            std::future<void>* future;
            const char* tag;
        };
        TypeFetch typeFetches[] = {
            {1, m_backends.photo.get(),          &m_pendingPhotoFetch,           "PHOTO"},
            {2, m_backends.calendar.get(),       &m_pendingCalendarFetch,        "CALENDAR"},
            {3, m_backends.googleTimeline.get(),  &m_pendingGoogleTimelineFetch, "GTIMELINE"},
        };

        for (auto& tf : typeFetches) {
            if (!tf.backend) continue;
            int li = tf.layerIndex;
            Backend* be = tf.backend;
            const char* tag = tf.tag;
            model.layers[li].startFetch();
            *tf.future = std::async(std::launch::async, [this, &model, be, li, tag]() {
                std::cerr << "[" << tag << "] fetch started\n";
                be->streamAllByType(
                    0.0, 2000000000.0,
                    [this, li, tag](std::vector<Entity>&& batch) {
                        std::cerr << "[" << tag << "] batch size=" << batch.size() << "\n";
                        auto t = Clock::now();
                        std::lock_guard<std::mutex> lock(m_batchMutex);
                        long lockMs = ms_since(t);
                        if (lockMs > 5)
                            std::cerr << "[" << tag << "] waited " << lockMs << "ms for batchMutex\n";
                        m_completedBatches.push_back({li, std::move(batch)});
                    });
                std::cerr << "[" << tag << "] fetch complete\n";
                model.layers[li].endFetch();
            });
        }
    } else {
        model.layers[0].startFetch();
        TimeExtent fullTime = model.time_extent;
        SpatialExtent fullSpace;
        fullSpace.min_lat = -90.0;
        fullSpace.max_lat =  90.0;
        fullSpace.min_lon = -180.0;
        fullSpace.max_lon =  180.0;

        m_pendingGpsFetch = std::async(std::launch::async, [this, &model, fullTime, fullSpace]() {
            m_backends.gps->fetchEntities(fullTime, fullSpace,
                [this](std::vector<Entity>&& batch) {
                    std::lock_guard<std::mutex> lock(m_batchMutex);
                    m_completedBatches.push_back({0, std::move(batch)});
                });
            model.layers[0].endFetch();
            model.initial_load_complete.store(true);
        });
    }
}

bool FetchOrchestrator::drainCompletedBatches(AppModel& model)
{
    std::deque<PendingBatch> batches;
    {
        std::lock_guard<std::mutex> lock(m_batchMutex);
        batches.swap(m_completedBatches);
    }

    for (auto& pb : batches) {
        if (pb.layerIndex < 0 || pb.layerIndex >= static_cast<int>(model.layers.size())) continue;
        auto& layerEntities = model.layers[pb.layerIndex].entities;
        layerEntities.reserve(layerEntities.size() + pb.entities.size());
        layerEntities.insert(layerEntities.end(),
            std::make_move_iterator(pb.entities.begin()),
            std::make_move_iterator(pb.entities.end()));
    }

    return !batches.empty();
}

void FetchOrchestrator::cancelAndWaitAll()
{
    m_backends.cancelAll();
    if (m_pendingGpsFetch.valid())             m_pendingGpsFetch.wait();
    if (m_pendingPhotoFetch.valid())           m_pendingPhotoFetch.wait();
    if (m_pendingCalendarFetch.valid())        m_pendingCalendarFetch.wait();
    if (m_pendingGoogleTimelineFetch.valid())  m_pendingGoogleTimelineFetch.wait();
}

std::pair<ServerStats, bool> FetchOrchestrator::fetchServerStats()
{
    if (m_backends.gps) {
        ServerStats stats = m_backends.gps->fetchStats();
        return {stats, stats.total_entities > 0};
    }
    return {{}, false};
}
