#pragma once

#include "vec2.hpp"
#include <array>
#include <cstdint>
#include <random>
#include <vector>

struct SimConfig {
    int width = 1280;
    int height = 800;
    int numBoids = 600;

    float perceptionRadius = 56.0f;
    float separationRadius = 24.0f;
    float maxSpeed = 270.0f;
    float minSpeed = 85.0f;
    float maxForce = 460.0f;

    float cohesionWeight = 1.0f;
    float alignmentWeight = 1.25f;
    float separationWeight = 1.85f;
    float avoidWeight = 4.8f;
    float boundaryWeight = 3.5f;

    float playerRadius = 165.0f;
    float pulseRadius = 275.0f;
    float pulseStrength = 950.0f;
    float captureRadius = 15.0f;
    float boundaryMargin = 65.0f;

    bool useSpatialGrid = true;
};

struct Boid {
    Vec2 pos;
    Vec2 vel;
    Vec2 acc;
    float panic = 0.0f;
    float tagCooldown = 0.0f;
    std::array<Vec2, 4> trail{};
    int trailHead = 0;
};

struct PlayerState {
    Vec2 pos{640.0f, 400.0f};
    Vec2 vel{0.0f, 0.0f};
    bool attractMode = false;
    bool pulseActive = false;
    float pulseTimer = 0.0f;
    float pulseDuration = 0.45f;
    Vec2 pulseOrigin{640.0f, 400.0f};
    int score = 0;
    float elapsedSeconds = 0.0f;
};

struct SwarmMetrics {
    float polarization = 0.0f;
    float meanSpeed = 0.0f;
    float meanNearestNeighborDist = 0.0f;
    float meanPlayerDist = 0.0f;
    float panicFraction = 0.0f;
    float avgNeighbors = 0.0f;
    double stepTimeMs = 0.0;
};

class SpatialGrid {
public:
    void configure(int worldWidth, int worldHeight, float cellSize, int boidCount);
    void build(const std::vector<Boid>& boids);

    int cols() const { return cols_; }
    int rows() const { return rows_; }
    float cellSize() const { return cellSize_; }

    int cellX(float x) const;
    int cellY(float y) const;
    int cellHead(int cx, int cy) const { return cellHeads_[cy * cols_ + cx]; }
    int nextBoid(int idx) const { return next_[idx]; }

private:
    int cols_ = 1;
    int rows_ = 1;
    float cellSize_ = 56.0f;
    std::vector<int> cellHeads_;
    std::vector<int> next_;
};

class SwarmSimulation {
public:
    explicit SwarmSimulation(const SimConfig& cfg = SimConfig{}, uint32_t seed = 42);

    void reset(uint32_t seed = 42);
    void resizeBoids(int count);
    void setConfig(const SimConfig& cfg);

    void triggerPulse();
    void step(float dt, bool computeMetrics = true);

    const SimConfig& config() const { return cfg_; }
    SimConfig& mutableConfig() { return cfg_; }
    const std::vector<Boid>& boids() const { return boids_; }
    PlayerState& player() { return player_; }
    const PlayerState& player() const { return player_; }
    const SwarmMetrics& metrics() const { return metrics_; }
    const SpatialGrid& grid() const { return grid_; }

    // Exposed for verification & testing
    std::vector<int> queryNeighborsGrid(int idx) const;
    std::vector<int> queryNeighborsNaive(int idx) const;

private:
    Vec2 computeBoundaryForce(const Vec2& pos) const;
    Vec2 computePlayerForce(const Boid& b, float& outPanic) const;
    void respawnAtEdge(Boid& b);

    SimConfig cfg_;
    std::mt19937 rng_;
    std::vector<Boid> boids_;
    SpatialGrid grid_;
    PlayerState player_;
    SwarmMetrics metrics_;
};
