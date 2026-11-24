#include "renderer.hpp"
#include "swarm.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

void applyPreset(SimConfig& cfg, int preset) {
    if (preset == 1) {
        // Calm cohesive flocking
        cfg.cohesionWeight = 1.35f;
        cfg.alignmentWeight = 1.65f;
        cfg.separationWeight = 1.60f;
        cfg.avoidWeight = 3.20f;
        cfg.maxSpeed = 210.0f;
        cfg.playerRadius = 140.0f;
    } else if (preset == 2) {
        // High-reactivity predator evasion (default)
        cfg.cohesionWeight = 1.0f;
        cfg.alignmentWeight = 1.25f;
        cfg.separationWeight = 1.85f;
        cfg.avoidWeight = 4.80f;
        cfg.maxSpeed = 270.0f;
        cfg.playerRadius = 165.0f;
    } else if (preset == 3) {
        // Dense agile murmuration
        cfg.cohesionWeight = 1.25f;
        cfg.alignmentWeight = 1.80f;
        cfg.separationWeight = 2.15f;
        cfg.avoidWeight = 5.50f;
        cfg.maxSpeed = 320.0f;
        cfg.playerRadius = 190.0f;
    }
}

int runBenchmark(const std::string& outDir) {
    std::filesystem::create_directories(outDir);

    // 1. Scaling benchmark: Naive O(N^2) vs Spatial Hash Grid O(N)
    {
        std::ofstream out(outDir + "/scaling.csv");
        out << "N,grid_ms,naive_ms,speedup,avg_neighbors\n";
        std::vector<int> counts = {100, 250, 500, 1000, 1500, 2000, 3000, 4000};
        std::cout << "Running scaling benchmark...\n";
        for (int n : counts) {
            SimConfig cfg;
            cfg.numBoids = n;
            cfg.useSpatialGrid = true;
            SwarmSimulation simGrid(cfg, 123);
            // Warmup
            for (int w = 0; w < 5; ++w) simGrid.step(1.0f / 60.0f, true);

            double gridSum = 0.0;
            double avgNeighbors = 0.0;
            constexpr int kTrials = 20;
            for (int t = 0; t < kTrials; ++t) {
                simGrid.step(1.0f / 60.0f, true);
                gridSum += simGrid.metrics().stepTimeMs;
                avgNeighbors += simGrid.metrics().avgNeighbors;
            }
            double gridMs = gridSum / kTrials;
            avgNeighbors /= kTrials;

            cfg.useSpatialGrid = false;
            SwarmSimulation simNaive(cfg, 123);
            for (int w = 0; w < 2; ++w) simNaive.step(1.0f / 60.0f, false);
            int naiveTrials = (n <= 2000) ? 10 : 5;
            double naiveSum = 0.0;
            for (int t = 0; t < naiveTrials; ++t) {
                simNaive.step(1.0f / 60.0f, false);
                naiveSum += simNaive.metrics().stepTimeMs;
            }
            double naiveMs = naiveSum / naiveTrials;
            double speedup = naiveMs / std::max(gridMs, 1e-6);

            out << n << "," << std::fixed << std::setprecision(4) << gridMs << ","
                << naiveMs << "," << speedup << "," << avgNeighbors << "\n";
            std::cout << "  N=" << n << " | Grid=" << gridMs << " ms | Naive=" << naiveMs
                      << " ms | Speedup=" << speedup << "x\n";
        }
    }

    // 2. Emergent dynamics time-series: Unperturbed vs Active Predator Sweep
    {
        std::ofstream out(outDir + "/dynamics.csv");
        out << "time_s,pol_free,pol_pred,panic_pred,pdist_pred,nndist_free,nndist_pred\n";

        SimConfig cfgFree;
        cfgFree.numBoids = 600;
        cfgFree.avoidWeight = 0.0f;
        SwarmSimulation simFree(cfgFree, 77);

        SimConfig cfgPred;
        cfgPred.numBoids = 600;
        SwarmSimulation simPred(cfgPred, 77);

        constexpr float dt = 1.0f / 60.0f;
        constexpr int totalSteps = 600; // 10 seconds
        for (int s = 0; s < totalSteps; ++s) {
            float t = s * dt;
            // Circular/figure-8 predator sweep through the arena
            simPred.player().pos = {
                640.0f + 360.0f * std::cos(t * 1.15f),
                400.0f + 220.0f * std::sin(t * 2.30f)
            };
            simFree.step(dt, true);
            simPred.step(dt, true);

            if (s % 6 == 0) { // 10 Hz output (100 data points)
                const auto& mf = simFree.metrics();
                const auto& mp = simPred.metrics();
                out << std::fixed << std::setprecision(3) << t << ","
                    << mf.polarization << "," << mp.polarization << ","
                    << mp.panicFraction << "," << mp.meanPlayerDist << ","
                    << mf.meanNearestNeighborDist << "," << mp.meanNearestNeighborDist << "\n";
            }
        }
        std::cout << "Saved dynamics time-series to " << outDir << "/dynamics.csv\n";
    }

    // 3. Avoidance Weight Parameter Sweep
    {
        std::ofstream out(outDir + "/weights_sweep.csv");
        out << "avoid_weight,mean_pol,mean_pdist,mean_panic,tags\n";
        std::vector<float> weights = {0.0f, 1.0f, 2.0f, 3.5f, 4.8f, 6.5f, 8.5f};
        constexpr float dt = 1.0f / 60.0f;
        for (float w : weights) {
            SimConfig cfg;
            cfg.numBoids = 600;
            cfg.avoidWeight = w;
            SwarmSimulation sim(cfg, 99);

            double polSum = 0.0, pdistSum = 0.0, panicSum = 0.0;
            constexpr int steps = 480;
            for (int s = 0; s < steps; ++s) {
                float t = s * dt;
                sim.player().pos = {
                    640.0f + 340.0f * std::cos(t * 1.2f),
                    400.0f + 200.0f * std::sin(t * 1.8f)
                };
                sim.step(dt, true);
                if (s >= 120) {
                    polSum += sim.metrics().polarization;
                    pdistSum += sim.metrics().meanPlayerDist;
                    panicSum += sim.metrics().panicFraction;
                }
            }
            double denom = steps - 120;
            out << std::fixed << std::setprecision(3) << w << ","
                << (polSum / denom) << "," << (pdistSum / denom) << ","
                << (panicSum / denom) << "," << sim.player().score << "\n";
        }
        std::cout << "Saved parameter sweep to " << outDir << "/weights_sweep.csv\n";
    }

    return 0;
}

int runHeadlessSmoke(int frames) {
    SwarmSimulation sim(SimConfig{}, 42);
    constexpr float dt = 1.0f / 60.0f;
    for (int i = 0; i < frames; ++i) {
        float t = i * dt;
        sim.player().pos = {640.0f + 250.0f * std::cos(t), 400.0f + 160.0f * std::sin(t)};
        sim.step(dt, true);
    }
    const auto& m = sim.metrics();
    std::cout << "Headless run complete (" << frames << " frames): "
              << "polarization=" << m.polarization
              << " meanSpeed=" << m.meanSpeed
              << " nnDist=" << m.meanNearestNeighborDist
              << " stepMs=" << m.stepTimeMs
              << " score=" << sim.player().score << "\n";
    return 0;
}

} // namespace

int runCaptureScreenshots(const std::string& outDir) {
    std::filesystem::create_directories(outDir);
    SimConfig cfg;
    cfg.numBoids = 600;
    SwarmSimulation sim(cfg, 42);
    X11Renderer renderer(cfg.width, cfg.height, "Capture", true);

    constexpr float dt = 1.0f / 60.0f;
    // Let flock form coherent streams first, then sweep predator through center and fire pulse
    for (int i = 0; i < 240; ++i) {
        float t = i * dt;
        sim.player().pos = {
            640.0f + 260.0f * std::cos(t * 1.1f),
            400.0f + 150.0f * std::sin(t * 1.8f)
        };
        if (i == 228) {
            sim.triggerPulse();
        }
        sim.step(dt, true);
    }
    renderer.render(sim, false, false, 60.0);
    renderer.savePPM(outDir + "/shot_evasion.ppm");

    // Advance slightly and capture spatial grid debug overlay
    for (int i = 0; i < 45; ++i) {
        float t = (240 + i) * dt;
        sim.player().pos = {
            640.0f + 260.0f * std::cos(t * 1.1f),
            400.0f + 150.0f * std::sin(t * 1.8f)
        };
        sim.step(dt, true);
    }
    renderer.render(sim, true, false, 60.0);
    renderer.savePPM(outDir + "/shot_debug.ppm");

    std::cout << "Captured screenshots to " << outDir << "\n";
    return 0;
}

int main(int argc, char** argv) {
    if (argc >= 2 && std::string(argv[1]) == "--benchmark") {
        std::string outDir = (argc >= 3) ? argv[2] : "report/data";
        return runBenchmark(outDir);
    }
    if (argc >= 2 && std::string(argv[1]) == "--capture") {
        std::string outDir = (argc >= 3) ? argv[2] : "report/figures";
        return runCaptureScreenshots(outDir);
    }
    if (argc >= 2 && std::string(argv[1]) == "--headless") {
        int frames = (argc >= 3) ? std::max(1, std::stoi(argv[2])) : 180;
        return runHeadlessSmoke(frames);
    }

    SimConfig cfg;
    SwarmSimulation sim(cfg, 42);
    X11Renderer renderer(cfg.width, cfg.height, "Swarm Evader - Reynolds Boids & Predator Avoidance");
    if (!renderer.isValid()) {
        std::cerr << "Error: Could not open X11 display. Use --headless or --benchmark.\n";
        return 1;
    }

    InputState input;
    bool showDebug = false;
    auto prevTime = std::chrono::steady_clock::now();
    double smoothedFps = 60.0;

    while (!input.quit) {
        auto now = std::chrono::steady_clock::now();
        double rawDt = std::chrono::duration<double>(now - prevTime).count();
        prevTime = now;
        float dt = static_cast<float>(std::clamp(rawDt, 0.001, 0.04));
        if (rawDt > 1e-5) {
            smoothedFps = smoothedFps * 0.92 + (1.0 / rawDt) * 0.08;
        }

        renderer.pollEvents(input);

        if (input.toggleDebug) showDebug = !showDebug;
        if (input.resetRequested) sim.reset(42);
        if (input.pulseRequested) sim.triggerPulse();
        if (input.toggleAttract) sim.player().attractMode = !sim.player().attractMode;
        if (input.deltaBoids != 0) {
            sim.resizeBoids(sim.config().numBoids + input.deltaBoids);
        }
        if (input.presetRequested != 0) {
            SimConfig updated = sim.config();
            applyPreset(updated, input.presetRequested);
            sim.setConfig(updated);
        }

        // Player control: smooth follow to mouse OR direct WASD/Arrow velocity
        auto& player = sim.player();
        if (input.hasMouseTarget) {
            Vec2 diff = input.mousePos - player.pos;
            player.pos += diff * std::min(1.0f, dt * 16.0f);
        } else {
            Vec2 move{0.0f, 0.0f};
            if (input.keyUp) move.y -= 1.0f;
            if (input.keyDown) move.y += 1.0f;
            if (input.keyLeft) move.x -= 1.0f;
            if (input.keyRight) move.x += 1.0f;
            if (move.lengthSq() > 0.0f) {
                player.pos += move.normalized() * (420.0f * dt);
            }
        }
        player.pos.x = std::clamp(player.pos.x, 10.0f, static_cast<float>(cfg.width) - 10.0f);
        player.pos.y = std::clamp(player.pos.y, 10.0f, static_cast<float>(cfg.height) - 10.0f);

        if (!input.paused) {
            sim.step(dt, true);
        }

        renderer.render(sim, showDebug, input.paused, smoothedFps);
        if (input.screenshotRequested) {
            renderer.savePPM("screenshot.ppm");
        }

        // Cap around ~60-90 Hz to keep frame pacing smooth and CPU light
        auto frameEnd = std::chrono::steady_clock::now();
        auto frameElapsed = std::chrono::duration<double, std::milli>(frameEnd - now).count();
        if (frameElapsed < 14.0) {
            std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(14.0 - frameElapsed));
        }
    }

    return 0;
}
