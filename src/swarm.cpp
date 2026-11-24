#include "swarm.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>

namespace {
constexpr float kPi = 3.14159265358979323846f;
}

void SpatialGrid::configure(int worldWidth, int worldHeight, float cellSize, int boidCount) {
    cellSize_ = std::max(8.0f, cellSize);
    cols_ = std::max(1, static_cast<int>(std::ceil(static_cast<float>(worldWidth) / cellSize_)));
    rows_ = std::max(1, static_cast<int>(std::ceil(static_cast<float>(worldHeight) / cellSize_)));
    cellHeads_.assign(static_cast<size_t>(cols_) * static_cast<size_t>(rows_), -1);
    next_.assign(static_cast<size_t>(boidCount), -1);
}

int SpatialGrid::cellX(float x) const {
    int cx = static_cast<int>(x / cellSize_);
    return std::clamp(cx, 0, cols_ - 1);
}

int SpatialGrid::cellY(float y) const {
    int cy = static_cast<int>(y / cellSize_);
    return std::clamp(cy, 0, rows_ - 1);
}

void SpatialGrid::build(const std::vector<Boid>& boids) {
    std::fill(cellHeads_.begin(), cellHeads_.end(), -1);
    if (next_.size() != boids.size()) {
        next_.resize(boids.size(), -1);
    }
    for (int i = 0; i < static_cast<int>(boids.size()); ++i) {
        int cx = cellX(boids[i].pos.x);
        int cy = cellY(boids[i].pos.y);
        int cellIdx = cy * cols_ + cx;
        next_[i] = cellHeads_[cellIdx];
        cellHeads_[cellIdx] = i;
    }
}

SwarmSimulation::SwarmSimulation(const SimConfig& cfg, uint32_t seed)
    : cfg_(cfg), rng_(seed) {
    reset(seed);
}

void SwarmSimulation::reset(uint32_t seed) {
    rng_.seed(seed);
    player_.pos = {cfg_.width * 0.5f, cfg_.height * 0.5f};
    player_.vel = {0.0f, 0.0f};
    player_.pulseActive = false;
    player_.pulseTimer = 0.0f;
    player_.score = 0;
    player_.elapsedSeconds = 0.0f;

    boids_.clear();
    boids_.resize(static_cast<size_t>(std::max(0, cfg_.numBoids)));

    std::uniform_real_distribution<float> distX(cfg_.boundaryMargin,
                                                std::max(cfg_.boundaryMargin + 1.0f, cfg_.width - cfg_.boundaryMargin));
    std::uniform_real_distribution<float> distY(cfg_.boundaryMargin,
                                                std::max(cfg_.boundaryMargin + 1.0f, cfg_.height - cfg_.boundaryMargin));
    std::uniform_real_distribution<float> distAngle(0.0f, 2.0f * kPi);
    std::uniform_real_distribution<float> distSpeed(cfg_.minSpeed, cfg_.maxSpeed * 0.75f);

    for (auto& b : boids_) {
        b.pos = {distX(rng_), distY(rng_)};
        float angle = distAngle(rng_);
        float spd = distSpeed(rng_);
        b.vel = {std::cos(angle) * spd, std::sin(angle) * spd};
        b.acc = {0.0f, 0.0f};
        b.panic = 0.0f;
        b.tagCooldown = 0.0f;
        b.trail.fill(b.pos);
        b.trailHead = 0;
    }

    grid_.configure(cfg_.width, cfg_.height, cfg_.perceptionRadius, static_cast<int>(boids_.size()));
    grid_.build(boids_);
}

void SwarmSimulation::resizeBoids(int count) {
    count = std::clamp(count, 10, 10000);
    int oldSize = static_cast<int>(boids_.size());
    cfg_.numBoids = count;
    if (count < oldSize) {
        boids_.resize(static_cast<size_t>(count));
    } else if (count > oldSize) {
        boids_.resize(static_cast<size_t>(count));
        std::uniform_real_distribution<float> distAngle(0.0f, 2.0f * kPi);
        for (int i = oldSize; i < count; ++i) {
            respawnAtEdge(boids_[i]);
            float angle = distAngle(rng_);
            float spd = 0.5f * (cfg_.minSpeed + cfg_.maxSpeed);
            boids_[i].vel = {std::cos(angle) * spd, std::sin(angle) * spd};
            boids_[i].trail.fill(boids_[i].pos);
        }
    }
    grid_.configure(cfg_.width, cfg_.height, cfg_.perceptionRadius, static_cast<int>(boids_.size()));
}

void SwarmSimulation::setConfig(const SimConfig& cfg) {
    bool countChanged = cfg.numBoids != cfg_.numBoids;
    cfg_ = cfg;
    if (countChanged) {
        resizeBoids(cfg_.numBoids);
    } else {
        grid_.configure(cfg_.width, cfg_.height, cfg_.perceptionRadius, static_cast<int>(boids_.size()));
    }
}

void SwarmSimulation::triggerPulse() {
    player_.pulseActive = true;
    player_.pulseTimer = player_.pulseDuration;
    player_.pulseOrigin = player_.pos;
}

void SwarmSimulation::respawnAtEdge(Boid& b) {
    std::uniform_int_distribution<int> sideDist(0, 3);
    std::uniform_real_distribution<float> uDist(0.05f, 0.95f);
    int side = sideDist(rng_);
    float w = static_cast<float>(cfg_.width);
    float h = static_cast<float>(cfg_.height);
    float pad = 18.0f;

    switch (side) {
        case 0: // left
            b.pos = {pad, uDist(rng_) * h};
            b.vel = {cfg_.minSpeed * 1.3f, 0.0f};
            break;
        case 1: // right
            b.pos = {w - pad, uDist(rng_) * h};
            b.vel = {-cfg_.minSpeed * 1.3f, 0.0f};
            break;
        case 2: // top
            b.pos = {uDist(rng_) * w, pad};
            b.vel = {0.0f, cfg_.minSpeed * 1.3f};
            break;
        default: // bottom
            b.pos = {uDist(rng_) * w, h - pad};
            b.vel = {0.0f, -cfg_.minSpeed * 1.3f};
            break;
    }
    b.panic = 0.0f;
    b.tagCooldown = 1.0f;
    b.trail.fill(b.pos);
}

Vec2 SwarmSimulation::computeBoundaryForce(const Vec2& pos) const {
    Vec2 steer{0.0f, 0.0f};
    float m = cfg_.boundaryMargin;
    float w = static_cast<float>(cfg_.width);
    float h = static_cast<float>(cfg_.height);

    if (pos.x < m) {
        float t = (m - pos.x) / m;
        steer.x += t * cfg_.maxForce;
    } else if (pos.x > w - m) {
        float t = (pos.x - (w - m)) / m;
        steer.x -= t * cfg_.maxForce;
    }

    if (pos.y < m) {
        float t = (m - pos.y) / m;
        steer.y += t * cfg_.maxForce;
    } else if (pos.y > h - m) {
        float t = (pos.y - (h - m)) / m;
        steer.y -= t * cfg_.maxForce;
    }

    return steer;
}

Vec2 SwarmSimulation::computePlayerForce(const Boid& b, float& outPanic) const {
    Vec2 total{0.0f, 0.0f};
    outPanic = 0.0f;

    Vec2 diff = b.pos - player_.pos;
    float dSq = diff.lengthSq();
    float r = cfg_.playerRadius;

    if (dSq < r * r && dSq > 1e-6f) {
        float d = std::sqrt(dSq);
        float falloff = 1.0f - (d / r);
        // Smooth quadratic ramp increases urgency rapidly near the player
        float urgency = falloff * (0.4f + 1.6f * falloff);
        Vec2 dir = diff / d;
        if (player_.attractMode) {
            Vec2 desired = (-1.0f * dir) * cfg_.maxSpeed;
            Vec2 steer = (desired - b.vel).limit(cfg_.maxForce);
            total += steer * (urgency * 0.65f);
            outPanic = std::min(1.0f, falloff * 0.6f);
        } else {
            Vec2 desired = dir * cfg_.maxSpeed;
            Vec2 steer = (desired - b.vel).limit(cfg_.maxForce * 1.4f);
            total += steer * urgency;
            outPanic = std::min(1.0f, falloff * 1.35f);
        }
    }

    if (player_.pulseActive) {
        float progress = 1.0f - (player_.pulseTimer / player_.pulseDuration);
        float waveR = cfg_.pulseRadius * progress;
        Vec2 pDiff = b.pos - player_.pulseOrigin;
        float pdSq = pDiff.lengthSq();
        float band = 65.0f;
        if (pdSq > 1e-6f && pdSq < (waveR + band) * (waveR + band)) {
            float pd = std::sqrt(pdSq);
            float distToWave = std::abs(pd - waveR);
            if (distToWave < band) {
                float w = (1.0f - distToWave / band) * (1.0f - 0.5f * progress);
                total += (pDiff / pd) * (cfg_.pulseStrength * w);
                outPanic = 1.0f;
            }
        }
    }

    return total;
}

std::vector<int> SwarmSimulation::queryNeighborsGrid(int idx) const {
    std::vector<int> result;
    const Boid& self = boids_[idx];
    float rSq = cfg_.perceptionRadius * cfg_.perceptionRadius;
    int cx = grid_.cellX(self.pos.x);
    int cy = grid_.cellY(self.pos.y);

    for (int dy = -1; dy <= 1; ++dy) {
        int ny = cy + dy;
        if (ny < 0 || ny >= grid_.rows()) continue;
        for (int dx = -1; dx <= 1; ++dx) {
            int nx = cx + dx;
            if (nx < 0 || nx >= grid_.cols()) continue;
            int other = grid_.cellHead(nx, ny);
            while (other != -1) {
                if (other != idx && self.pos.distSq(boids_[other].pos) <= rSq) {
                    result.push_back(other);
                }
                other = grid_.nextBoid(other);
            }
        }
    }
    return result;
}

std::vector<int> SwarmSimulation::queryNeighborsNaive(int idx) const {
    std::vector<int> result;
    const Boid& self = boids_[idx];
    float rSq = cfg_.perceptionRadius * cfg_.perceptionRadius;
    for (int j = 0; j < static_cast<int>(boids_.size()); ++j) {
        if (j != idx && self.pos.distSq(boids_[j].pos) <= rSq) {
            result.push_back(j);
        }
    }
    return result;
}

void SwarmSimulation::step(float dt, bool computeMetrics) {
    auto tStart = std::chrono::steady_clock::now();

    dt = std::clamp(dt, 0.0005f, 0.05f);
    player_.elapsedSeconds += dt;
    if (player_.pulseActive) {
        player_.pulseTimer -= dt;
        if (player_.pulseTimer <= 0.0f) {
            player_.pulseActive = false;
            player_.pulseTimer = 0.0f;
        }
    }

    const int n = static_cast<int>(boids_.size());
    if (n == 0) {
        metrics_ = SwarmMetrics{};
        return;
    }

    if (cfg_.useSpatialGrid) {
        grid_.configure(cfg_.width, cfg_.height, cfg_.perceptionRadius, n);
        grid_.build(boids_);
    }

    const float percSq = cfg_.perceptionRadius * cfg_.perceptionRadius;
    const float sepSq = cfg_.separationRadius * cfg_.separationRadius;
    const float captureSq = cfg_.captureRadius * cfg_.captureRadius;

    double totalNeighborCount = 0.0;
    double totalNearestDist = 0.0;
    int nearestSamples = 0;

    auto accumulateNeighbor = [&](int selfIdx, int otherIdx, Vec2& cohSum, Vec2& alignSum,
                                  Vec2& sepSum, int& percCount, int& sepCount, float& minDistSq) {
        const Boid& self = boids_[selfIdx];
        const Boid& other = boids_[otherIdx];
        Vec2 diff = other.pos - self.pos;
        float dSq = diff.lengthSq();
        if (dSq > percSq) return;

        if (dSq < minDistSq) {
            minDistSq = dSq;
        }
        cohSum += other.pos;
        alignSum += other.vel;
        ++percCount;

        if (dSq < sepSq && dSq > 1e-6f) {
            float invD = 1.0f / std::sqrt(dSq);
            // Repel stronger at closer distance
            sepSum -= (diff * invD) * (1.0f - std::sqrt(dSq) / cfg_.separationRadius);
            ++sepCount;
        }
    };

    for (int i = 0; i < n; ++i) {
        const Boid& self = boids_[i];
        Vec2 cohSum{0.0f, 0.0f};
        Vec2 alignSum{0.0f, 0.0f};
        Vec2 sepSum{0.0f, 0.0f};
        int percCount = 0;
        int sepCount = 0;
        float minDistSq = std::numeric_limits<float>::max();

        if (cfg_.useSpatialGrid) {
            int cx = grid_.cellX(self.pos.x);
            int cy = grid_.cellY(self.pos.y);
            for (int dy = -1; dy <= 1; ++dy) {
                int ny = cy + dy;
                if (ny < 0 || ny >= grid_.rows()) continue;
                for (int dx = -1; dx <= 1; ++dx) {
                    int nx = cx + dx;
                    if (nx < 0 || nx >= grid_.cols()) continue;
                    int other = grid_.cellHead(nx, ny);
                    while (other != -1) {
                        if (other != i) {
                            accumulateNeighbor(i, other, cohSum, alignSum, sepSum,
                                               percCount, sepCount, minDistSq);
                        }
                        other = grid_.nextBoid(other);
                    }
                }
            }
        } else {
            for (int j = 0; j < n; ++j) {
                if (j != i) {
                    accumulateNeighbor(i, j, cohSum, alignSum, sepSum,
                                       percCount, sepCount, minDistSq);
                }
            }
        }

        totalNeighborCount += percCount;
        if (percCount > 0) {
            totalNearestDist += std::sqrt(minDistSq);
            ++nearestSamples;
        }

        Vec2 totalForce{0.0f, 0.0f};

        if (percCount > 0) {
            // Cohesion: steer toward local center of mass
            Vec2 center = cohSum / static_cast<float>(percCount);
            Vec2 cohDesired = (center - self.pos).setMag(cfg_.maxSpeed);
            Vec2 cohSteer = (cohDesired - self.vel).limit(cfg_.maxForce);
            totalForce += cohSteer * cfg_.cohesionWeight;

            // Alignment: steer toward average neighbor velocity
            Vec2 avgVel = (alignSum / static_cast<float>(percCount)).setMag(cfg_.maxSpeed);
            Vec2 alignSteer = (avgVel - self.vel).limit(cfg_.maxForce);
            totalForce += alignSteer * cfg_.alignmentWeight;
        }

        if (sepCount > 0) {
            Vec2 sepDesired = sepSum.setMag(cfg_.maxSpeed);
            Vec2 sepSteer = (sepDesired - self.vel).limit(cfg_.maxForce);
            totalForce += sepSteer * cfg_.separationWeight;
        }

        float instantPanic = 0.0f;
        Vec2 avoidForce = computePlayerForce(self, instantPanic);
        totalForce += avoidForce * cfg_.avoidWeight;
        totalForce += computeBoundaryForce(self.pos) * cfg_.boundaryWeight;

        boids_[i].acc = totalForce;
        // Exponential smoothing for panic visual & metric state
        boids_[i].panic = boids_[i].panic * 0.85f + instantPanic * 0.15f;
        if (boids_[i].panic < instantPanic) {
            boids_[i].panic = instantPanic;
        }
    }

    // Integrate positions and velocities
    Vec2 unitVelSum{0.0f, 0.0f};
    double speedSum = 0.0;
    double playerDistSum = 0.0;
    int panicCount = 0;

    const float w = static_cast<float>(cfg_.width);
    const float h = static_cast<float>(cfg_.height);

    for (int i = 0; i < n; ++i) {
        Boid& b = boids_[i];
        b.trailHead = (b.trailHead + 1) % static_cast<int>(b.trail.size());
        b.trail[b.trailHead] = b.pos;

        b.vel += b.acc * dt;
        float spd = b.vel.length();
        if (spd > cfg_.maxSpeed) {
            b.vel = (b.vel / spd) * cfg_.maxSpeed;
            spd = cfg_.maxSpeed;
        } else if (spd < cfg_.minSpeed) {
            if (spd > 1e-4f) {
                b.vel = (b.vel / spd) * cfg_.minSpeed;
            } else {
                b.vel = {cfg_.minSpeed, 0.0f};
            }
            spd = cfg_.minSpeed;
        }

        b.pos += b.vel * dt;

        // Hard clamp safety net inside screen bounds
        if (b.pos.x < 4.0f) {
            b.pos.x = 4.0f;
            b.vel.x = std::abs(b.vel.x);
        } else if (b.pos.x > w - 4.0f) {
            b.pos.x = w - 4.0f;
            b.vel.x = -std::abs(b.vel.x);
        }
        if (b.pos.y < 4.0f) {
            b.pos.y = 4.0f;
            b.vel.y = std::abs(b.vel.y);
        } else if (b.pos.y > h - 4.0f) {
            b.pos.y = h - 4.0f;
            b.vel.y = -std::abs(b.vel.y);
        }

        if (b.tagCooldown > 0.0f) {
            b.tagCooldown = std::max(0.0f, b.tagCooldown - dt);
        } else if (!player_.attractMode && b.pos.distSq(player_.pos) < captureSq) {
            ++player_.score;
            respawnAtEdge(b);
            spd = b.vel.length();
        }

        if (computeMetrics) {
            unitVelSum += b.vel / std::max(spd, 1e-4f);
            speedSum += spd;
            playerDistSum += b.pos.dist(player_.pos);
            if (b.panic > 0.25f) {
                ++panicCount;
            }
        }
    }

    auto tEnd = std::chrono::steady_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

    if (computeMetrics) {
        metrics_.polarization = unitVelSum.length() / static_cast<float>(n);
        metrics_.meanSpeed = static_cast<float>(speedSum / n);
        metrics_.meanNearestNeighborDist =
            nearestSamples > 0 ? static_cast<float>(totalNearestDist / nearestSamples) : cfg_.perceptionRadius;
        metrics_.meanPlayerDist = static_cast<float>(playerDistSum / n);
        metrics_.panicFraction = static_cast<float>(panicCount) / static_cast<float>(n);
        metrics_.avgNeighbors = static_cast<float>(totalNeighborCount / n);
    }
    metrics_.stepTimeMs = elapsedMs;
}
