#pragma once

#include <cmath>

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Vec2() = default;
    constexpr Vec2(float x_, float y_) : x(x_), y(y_) {}

    constexpr Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    constexpr Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    constexpr Vec2 operator*(float s) const { return {x * s, y * s}; }
    constexpr Vec2 operator/(float s) const { return {x / s, y / s}; }

    constexpr Vec2& operator+=(const Vec2& o) {
        x += o.x;
        y += o.y;
        return *this;
    }
    constexpr Vec2& operator-=(const Vec2& o) {
        x -= o.x;
        y -= o.y;
        return *this;
    }
    constexpr Vec2& operator*=(float s) {
        x *= s;
        y *= s;
        return *this;
    }

    constexpr float lengthSq() const { return x * x + y * y; }
    float length() const { return std::sqrt(lengthSq()); }

    float distSq(const Vec2& o) const {
        float dx = x - o.x;
        float dy = y - o.y;
        return dx * dx + dy * dy;
    }

    float dist(const Vec2& o) const { return std::sqrt(distSq(o)); }

    constexpr float dot(const Vec2& o) const { return x * o.x + y * o.y; }

    Vec2 normalized() const {
        float lenSq = lengthSq();
        if (lenSq <= 1e-12f) {
            return {0.0f, 0.0f};
        }
        float invLen = 1.0f / std::sqrt(lenSq);
        return {x * invLen, y * invLen};
    }

    Vec2 limit(float maxMag) const {
        float lenSq = lengthSq();
        if (lenSq > maxMag * maxMag && lenSq > 1e-12f) {
            float scale = maxMag / std::sqrt(lenSq);
            return {x * scale, y * scale};
        }
        return *this;
    }

    Vec2 setMag(float mag) const {
        float lenSq = lengthSq();
        if (lenSq <= 1e-12f) {
            return {0.0f, 0.0f};
        }
        float scale = mag / std::sqrt(lenSq);
        return {x * scale, y * scale};
    }
};

inline constexpr Vec2 operator*(float s, const Vec2& v) {
    return {v.x * s, v.y * s};
}
