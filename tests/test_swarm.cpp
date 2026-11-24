#include "../src/swarm.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

namespace {

void testVec2Math() {
    Vec2 a{3.0f, 4.0f};
    assert(std::abs(a.length() - 5.0f) < 1e-5f);
    Vec2 n = a.normalized();
    assert(std::abs(n.length() - 1.0f) < 1e-5f);
    Vec2 zero{0.0f, 0.0f};
    assert(zero.normalized().length() == 0.0f);
    Vec2 limited = a.limit(2.0f);
    assert(std::abs(limited.length() - 2.0f) < 1e-5f);
}

void testSpatialGridEquivalence() {
    SimConfig cfg;
    cfg.numBoids = 300;
    cfg.perceptionRadius = 60.0f;
    SwarmSimulation sim(cfg, 12345);
    // Advance a few steps so boids move naturally
    for (int i = 0; i < 10; ++i) {
        sim.step(1.0f / 60.0f, true);
    }

    for (int idx = 0; idx < cfg.numBoids; idx += 7) {
        auto gridNb = sim.queryNeighborsGrid(idx);
        auto naiveNb = sim.queryNeighborsNaive(idx);
        std::sort(gridNb.begin(), gridNb.end());
        std::sort(naiveNb.begin(), naiveNb.end());
        assert(gridNb == naiveNb);
    }
}

void testPlayerAvoidanceDirection() {
    SimConfig cfg;
    cfg.numBoids = 1;
    cfg.cohesionWeight = 0.0f;
    cfg.alignmentWeight = 0.0f;
    cfg.separationWeight = 0.0f;
    cfg.boundaryWeight = 0.0f;
    cfg.avoidWeight = 5.0f;
    cfg.playerRadius = 200.0f;

    SwarmSimulation sim(cfg, 1);
    sim.player().pos = {600.0f, 400.0f};
    sim.player().attractMode = false;

    // Step multiple times and verify boid moves away from player when placed nearby
    // First place boid to the right of player via step observation:
    // Actually check distance after stepping from initial random position inside radius if near,
    // or verify panic & avoidance behavior in a larger flock around player.
    SimConfig flockCfg;
    flockCfg.numBoids = 250;
    flockCfg.avoidWeight = 6.0f;
    SwarmSimulation flockSim(flockCfg, 55);
    flockSim.player().pos = {640.0f, 400.0f};

    float initialPlayerDist = 0.0f;
    for (const auto& b : flockSim.boids()) {
        initialPlayerDist += b.pos.dist(flockSim.player().pos);
    }
    initialPlayerDist /= static_cast<float>(flockCfg.numBoids);

    for (int i = 0; i < 45; ++i) {
        flockSim.step(1.0f / 60.0f, true);
    }
    assert(flockSim.metrics().meanPlayerDist > initialPlayerDist);
}

void testStabilityAndBounds() {
    SimConfig cfg;
    cfg.numBoids = 400;
    SwarmSimulation sim(cfg, 999);
    sim.triggerPulse();

    for (int i = 0; i < 180; ++i) {
        sim.step(1.0f / 60.0f, true);
        const auto& m = sim.metrics();
        assert(std::isfinite(m.polarization));
        assert(m.polarization >= 0.0f && m.polarization <= 1.0001f);
        assert(std::isfinite(m.meanSpeed));
        assert(m.meanSpeed >= cfg.minSpeed - 1e-2f && m.meanSpeed <= cfg.maxSpeed + 1e-2f);
    }

    for (const auto& b : sim.boids()) {
        assert(std::isfinite(b.pos.x) && std::isfinite(b.pos.y));
        assert(b.pos.x >= 0.0f && b.pos.x <= static_cast<float>(cfg.width));
        assert(b.pos.y >= 0.0f && b.pos.y <= static_cast<float>(cfg.height));
    }
}

} // namespace

int main() {
    testVec2Math();
    testSpatialGridEquivalence();
    testPlayerAvoidanceDirection();
    testStabilityAndBounds();
    std::cout << "All swarm unit tests passed!\n";
    return 0;
}
