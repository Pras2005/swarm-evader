#pragma once

#include "swarm.hpp"
#include <X11/Xlib.h>
#include <cstdint>
#include <string>
#include <vector>

struct InputState {
    bool quit = false;
    bool paused = false;
    bool toggleDebug = false;
    bool resetRequested = false;
    bool pulseRequested = false;
    bool toggleAttract = false;
    bool screenshotRequested = false;
    int deltaBoids = 0;
    int presetRequested = 0;

    bool hasMouseTarget = false;
    Vec2 mousePos{640.0f, 400.0f};
    bool keyUp = false;
    bool keyDown = false;
    bool keyLeft = false;
    bool keyRight = false;
};

class X11Renderer {
public:
    X11Renderer(int width, int height, const std::string& title, bool headless = false);
    ~X11Renderer();

    X11Renderer(const X11Renderer&) = delete;
    X11Renderer& operator=(const X11Renderer&) = delete;

    bool isValid() const { return headless_ || (display_ != nullptr && window_ != 0); }
    void pollEvents(InputState& input);
    void render(const SwarmSimulation& sim, bool showDebug, bool paused, double fps);
    bool savePPM(const std::string& path) const;

private:
    void clearBuffer(uint32_t color);
    void putPixelAlpha(int x, int y, uint32_t rgb, float alpha);
    void drawLine(float x0, float y0, float x1, float y1, uint32_t rgb, float alpha = 1.0f);
    void drawCircle(float cx, float cy, float r, uint32_t rgb, float alpha = 1.0f);
    void fillCircle(float cx, float cy, float r, uint32_t rgb, float alpha = 1.0f);
    void fillTriangle(Vec2 p0, Vec2 p1, Vec2 p2, uint32_t rgb, float alpha = 1.0f);
    void fillRect(int x, int y, int w, int h, uint32_t rgb, float alpha = 1.0f);
    void drawChar(int x, int y, char c, uint32_t rgb, int scale = 2);
    void drawText(int x, int y, const std::string& text, uint32_t rgb, int scale = 2);

    int width_ = 1280;
    int height_ = 800;
    bool headless_ = false;
    Display* display_ = nullptr;
    int screen_ = 0;
    Window window_ = 0;
    GC gc_ = nullptr;
    Atom wmDeleteMessage_ = 0;
    XImage* ximage_ = nullptr;
    std::vector<uint32_t> framebuffer_;
};
