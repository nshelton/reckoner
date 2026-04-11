#include <catch2/catch_test_macros.hpp>
#include "FetchOrchestrator.h"
#include "FakeBackend.h"
#include "BackendFactory.h"

TEST_CASE("FetchOrchestrator startFullLoad populates model with FakeBackend", "[fetch_orchestrator]") {
    AppModel model;
    FetchOrchestrator orchestrator;

    BackendSet backends;
    backends.gps = std::make_unique<FakeBackend>(200, 0);
    orchestrator.setBackends(std::move(backends), BackendConfig::Type::Fake);

    orchestrator.startFullLoad(model);
    orchestrator.cancelAndWaitAll();  // wait for async to finish

    // Drain batches into model
    bool drained = orchestrator.drainCompletedBatches(model);
    REQUIRE(drained);
    REQUIRE(model.layers[0].entities.size() == 200);
    REQUIRE(model.initial_load_complete.load());
}

TEST_CASE("FetchOrchestrator drainCompletedBatches returns false when empty", "[fetch_orchestrator]") {
    AppModel model;
    FetchOrchestrator orchestrator;

    bool drained = orchestrator.drainCompletedBatches(model);
    REQUIRE_FALSE(drained);
}

TEST_CASE("FetchOrchestrator cancelAndWaitAll is safe with no backends", "[fetch_orchestrator]") {
    FetchOrchestrator orchestrator;
    orchestrator.cancelAndWaitAll();  // should not crash
}

TEST_CASE("FetchOrchestrator cancelAndWaitAll is safe after load", "[fetch_orchestrator]") {
    AppModel model;
    FetchOrchestrator orchestrator;

    BackendSet backends;
    backends.gps = std::make_unique<FakeBackend>(100, 0);
    orchestrator.setBackends(std::move(backends), BackendConfig::Type::Fake);

    orchestrator.startFullLoad(model);
    orchestrator.cancelAndWaitAll();
    orchestrator.cancelAndWaitAll();  // double cancel should be safe
}

TEST_CASE("FetchOrchestrator fetchServerStats returns empty when no backends", "[fetch_orchestrator]") {
    FetchOrchestrator orchestrator;
    auto [stats, ok] = orchestrator.fetchServerStats();
    REQUIRE_FALSE(ok);
    REQUIRE(stats.total_entities == 0);
}

TEST_CASE("FetchOrchestrator setBackends cancels previous", "[fetch_orchestrator]") {
    AppModel model;
    FetchOrchestrator orchestrator;

    BackendSet backends1;
    backends1.gps = std::make_unique<FakeBackend>(100, 0);
    orchestrator.setBackends(std::move(backends1), BackendConfig::Type::Fake);
    orchestrator.startFullLoad(model);
    orchestrator.cancelAndWaitAll();

    BackendSet backends2;
    backends2.gps = std::make_unique<FakeBackend>(50, 0);
    orchestrator.setBackends(std::move(backends2), BackendConfig::Type::Fake);

    orchestrator.startFullLoad(model);
    orchestrator.cancelAndWaitAll();
    orchestrator.drainCompletedBatches(model);

    REQUIRE(model.layers[0].entities.size() == 50);
}

TEST_CASE("FetchOrchestrator multiple drains accumulate entities", "[fetch_orchestrator]") {
    AppModel model;
    FetchOrchestrator orchestrator;

    BackendSet backends;
    backends.gps = std::make_unique<FakeBackend>(100, 0);
    orchestrator.setBackends(std::move(backends), BackendConfig::Type::Fake);

    orchestrator.startFullLoad(model);
    orchestrator.cancelAndWaitAll();

    // First drain
    orchestrator.drainCompletedBatches(model);
    size_t firstCount = model.layers[0].entities.size();
    REQUIRE(firstCount == 100);

    // Second drain should be empty
    bool drained = orchestrator.drainCompletedBatches(model);
    REQUIRE_FALSE(drained);
    REQUIRE(model.layers[0].entities.size() == 100);
}
