#pragma once

#include "BackendFactory.h"
#include "AppModel.h"
#include <future>
#include <mutex>
#include <deque>
#include <vector>

/// Owns backends and async fetch lifecycle.
/// Thread-safe batch queue delivers entities from background threads to main thread.
class FetchOrchestrator {
public:
    FetchOrchestrator() = default;
    ~FetchOrchestrator();

    /// Replace backends and discard any stale queued batches.
    /// Caller must cancel/wait before calling this.
    void setBackends(BackendSet backends, BackendConfig::Type type);

    const BackendSet& backends() const { return m_backends; }

    void startFullLoad(AppModel& model);
    bool drainCompletedBatches(AppModel& model);
    void cancelAndWaitAll();
    std::pair<ServerStats, bool> fetchServerStats();

private:
    struct PendingBatch {
        int layerIndex;
        std::vector<Entity> entities;
    };

    BackendConfig::Type m_backendType{BackendConfig::Type::Http};
    BackendSet m_backends;
    std::future<void> m_pendingGpsFetch;
    std::future<void> m_pendingPhotoFetch;
    std::future<void> m_pendingCalendarFetch;
    std::future<void> m_pendingGoogleTimelineFetch;

    std::mutex m_batchMutex;
    std::deque<PendingBatch> m_completedBatches;
};
