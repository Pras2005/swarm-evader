#include "renderer.hpp"

#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

// Compact 5x7 bitmap font for ASCII 32..95 (and lowercase mapped to uppercase)
// Each glyph has 5 columns of 7 bits (bit 0 = top row, bit 6 = bottom row).
struct Glyph {
    char ch;
    uint8_t cols[5];
};

constexpr Glyph kFont[] = {
    {' ', {0x00, 0x00, 0x00, 0x00, 0x00}},
    {'!', {0x00, 0x00, 0x5F, 0x00, 0x00}},
    {'%', {0x23, 0x13, 0x08, 0x64, 0x62}},
    {'(', {0x00, 0x1C, 0x22, 0x41, 0x00}},
    {')', {0x00, 0x41, 0x22, 0x1C, 0x00}},
    {'+', {0x08, 0x08, 0x3E, 0x08, 0x08}},
    {',', {0x00, 0x50, 0x30, 0x00, 0x00}},
    {'-', {0x08, 0x08, 0x08, 0x08, 0x08}},
    {'.', {0x00, 0x60, 0x60, 0x00, 0x00}},
    {'/', {0x20, 0x10, 0x08, 0x04, 0x02}},
    {'0', {0x3E, 0x51, 0x49, 0x45, 0x3E}},
    {'1', {0x00, 0x42, 0x7F, 0x40, 0x00}},
    {'2', {0x42, 0x61, 0x51, 0x49, 0x46}},
    {'3', {0x21, 0x41, 0x45, 0x4B, 0x31}},
    {'4', {0x18, 0x14, 0x12, 0x7F, 0x10}},
    {'5', {0x27, 0x45, 0x45, 0x45, 0x39}},
    {'6', {0x3C, 0x4A, 0x49, 0x49, 0x30}},
    {'7', {0x01, 0x71, 0x09, 0x05, 0x03}},
    {'8', {0x36, 0x49, 0x49, 0x49, 0x36}},
    {'9', {0x06, 0x49, 0x49, 0x29, 0x1E}},
    {':', {0x00, 0x36, 0x36, 0x00, 0x00}},
    {'=', {0x14, 0x14, 0x14, 0x14, 0x14}},
    {'[', {0x00, 0x7F, 0x41, 0x41, 0x00}},
    {']', {0x00, 0x41, 0x41, 0x7F, 0x00}},
    {'|', {0x00, 0x00, 0x7F, 0x00, 0x00}},
    {'A', {0x7E, 0x11, 0x11, 0x11, 0x7E}},
    {'B', {0x7F, 0x49, 0x49, 0x49, 0x36}},
    {'C', {0x3E, 0x41, 0x41, 0x41, 0x22}},
    {'D', {0x7F, 0x41, 0x41, 0x22, 0x1C}},
    {'E', {0x7F, 0x49, 0x49, 0x49, 0x41}},
    {'F', {0x7F, 0x09, 0x09, 0x09, 0x01}},
    {'G', {0x3E, 0x41, 0x49, 0x49, 0x7A}},
    {'H', {0x7F, 0x08, 0x08, 0x08, 0x7F}},
    {'I', {0x00, 0x41, 0x7F, 0x41, 0x00}},
    {'J', {0x20, 0x40, 0x41, 0x3F, 0x01}},
    {'K', {0x7F, 0x08, 0x14, 0x22, 0x41}},
    {'L', {0x7F, 0x40, 0x40, 0x40, 0x40}},
    {'M', {0x7F, 0x02, 0x0C, 0x02, 0x7F}},
    {'N', {0x7F, 0x04, 0x08, 0x10, 0x7F}},
    {'O', {0x3E, 0x41, 0x41, 0x41, 0x3E}},
    {'P', {0x7F, 0x09, 0x09, 0x09, 0x06}},
    {'Q', {0x3E, 0x41, 0x51, 0x21, 0x5E}},
    {'R', {0x7F, 0x09, 0x19, 0x29, 0x46}},
    {'S', {0x46, 0x49, 0x49, 0x49, 0x31}},
    {'T', {0x01, 0x01, 0x7F, 0x01, 0x01}},
    {'U', {0x3F, 0x40, 0x40, 0x40, 0x3F}},
    {'V', {0x1F, 0x20, 0x40, 0x20, 0x1F}},
    {'W', {0x3F, 0x40, 0x38, 0x40, 0x3F}},
    {'X', {0x63, 0x14, 0x08, 0x14, 0x63}},
    {'Y', {0x07, 0x08, 0x70, 0x08, 0x07}},
    {'Z', {0x61, 0x51, 0x49, 0x45, 0x43}},
};

const uint8_t* lookupGlyph(char c) {
    if (c >= 'a' && c <= 'z') {
        c = static_cast<char>(c - 'a' + 'A');
    }
    for (const auto& g : kFont) {
        if (g.ch == c) return g.cols;
    }
    return kFont[0].cols;
}

uint32_t lerpColor(uint32_t c0, uint32_t c1, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    float r0 = static_cast<float>((c0 >> 16) & 0xFF);
    float g0 = static_cast<float>((c0 >> 8) & 0xFF);
    float b0 = static_cast<float>(c0 & 0xFF);
    float r1 = static_cast<float>((c1 >> 16) & 0xFF);
    float g1 = static_cast<float>((c1 >> 8) & 0xFF);
    float b1 = static_cast<float>(c1 & 0xFF);
    uint32_t r = static_cast<uint32_t>(r0 + (r1 - r0) * t);
    uint32_t g = static_cast<uint32_t>(g0 + (g1 - g0) * t);
    uint32_t b = static_cast<uint32_t>(b0 + (b1 - b0) * t);
    return (r << 16) | (g << 8) | b;
}

uint32_t blendPixel(uint32_t dst, uint32_t src, float alpha) {
    uint32_t a = static_cast<uint32_t>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f);
    uint32_t inv = 255u - a;
    uint32_t r = (((src >> 16) & 0xFF) * a + ((dst >> 16) & 0xFF) * inv) >> 8;
    uint32_t g = (((src >> 8) & 0xFF) * a + ((dst >> 8) & 0xFF) * inv) >> 8;
    uint32_t b = ((src & 0xFF) * a + (dst & 0xFF) * inv) >> 8;
    return (r << 16) | (g << 8) | b;
}

float edgeFunction(const Vec2& a, const Vec2& b, const Vec2& c) {
    return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
}

} // namespace

X11Renderer::X11Renderer(int width, int height, const std::string& title, bool headless)
    : width_(width), height_(height), headless_(headless) {
    framebuffer_.assign(static_cast<size_t>(width_) * static_cast<size_t>(height_), 0x0D1117u);
    if (headless_) {
        return;
    }

    display_ = XOpenDisplay(nullptr);
    if (!display_) {
        return;
    }

    screen_ = DefaultScreen(display_);
    int depth = DefaultDepth(display_, screen_);
    Visual* visual = DefaultVisual(display_, screen_);

    window_ = XCreateSimpleWindow(
        display_, RootWindow(display_, screen_), 80, 60,
        static_cast<unsigned int>(width_), static_cast<unsigned int>(height_), 0,
        BlackPixel(display_, screen_), 0x0D1117);

    XStoreName(display_, window_, title.c_str());

    // Lock window size to match framebuffer dimensions cleanly
    XSizeHints* hints = XAllocSizeHints();
    if (hints) {
        hints->flags = PMinSize | PMaxSize;
        hints->min_width = hints->max_width = width_;
        hints->min_height = hints->max_height = height_;
        XSetWMNormalHints(display_, window_, hints);
        XFree(hints);
    }

    XSelectInput(display_, window_,
                 ExposureMask | KeyPressMask | KeyReleaseMask |
                     PointerMotionMask | ButtonPressMask | StructureNotifyMask);

    wmDeleteMessage_ = XInternAtom(display_, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display_, window_, &wmDeleteMessage_, 1);

    gc_ = XCreateGC(display_, window_, 0, nullptr);
    XMapWindow(display_, window_);

    char* rawData = reinterpret_cast<char*>(framebuffer_.data());
    ximage_ = XCreateImage(display_, visual, static_cast<unsigned int>(depth), ZPixmap, 0,
                           rawData, static_cast<unsigned int>(width_),
                           static_cast<unsigned int>(height_), 32,
                           width_ * static_cast<int>(sizeof(uint32_t)));
}

X11Renderer::~X11Renderer() {
    if (ximage_) {
        // Prevent XDestroyImage from freeing vector-owned memory
        ximage_->data = nullptr;
        XDestroyImage(ximage_);
        ximage_ = nullptr;
    }
    if (display_) {
        if (gc_) XFreeGC(display_, gc_);
        if (window_) XDestroyWindow(display_, window_);
        XCloseDisplay(display_);
    }
}

void X11Renderer::pollEvents(InputState& input) {
    input.toggleDebug = false;
    input.resetRequested = false;
    input.pulseRequested = false;
    input.toggleAttract = false;
    input.screenshotRequested = false;
    input.deltaBoids = 0;
    input.presetRequested = 0;

    while (display_ && XPending(display_) > 0) {
        XEvent ev;
        XNextEvent(display_, &ev);

        switch (ev.type) {
            case ClientMessage:
                if (static_cast<Atom>(ev.xclient.data.l[0]) == wmDeleteMessage_) {
                    input.quit = true;
                }
                break;

            case MotionNotify:
                input.hasMouseTarget = true;
                input.mousePos = {static_cast<float>(ev.xmotion.x),
                                  static_cast<float>(ev.xmotion.y)};
                break;

            case ButtonPress:
                input.hasMouseTarget = true;
                input.mousePos = {static_cast<float>(ev.xbutton.x),
                                  static_cast<float>(ev.xbutton.y)};
                if (ev.xbutton.button == Button1) {
                    input.pulseRequested = true;
                } else if (ev.xbutton.button == Button3) {
                    input.toggleAttract = true;
                }
                break;

            case KeyPress: {
                KeySym ks = XLookupKeysym(&ev.xkey, 0);
                if (ks == XK_Escape || ks == XK_q || ks == XK_Q) {
                    input.quit = true;
                } else if (ks == XK_p || ks == XK_P) {
                    input.paused = !input.paused;
                } else if (ks == XK_g || ks == XK_G) {
                    input.toggleDebug = true;
                } else if (ks == XK_r || ks == XK_R) {
                    input.resetRequested = true;
                } else if (ks == XK_space) {
                    input.pulseRequested = true;
                } else if (ks == XK_e || ks == XK_E) {
                    input.toggleAttract = true;
                } else if (ks == XK_plus || ks == XK_equal || ks == XK_KP_Add) {
                    input.deltaBoids += 150;
                } else if (ks == XK_minus || ks == XK_KP_Subtract) {
                    input.deltaBoids -= 150;
                } else if (ks == XK_1) {
                    input.presetRequested = 1;
                } else if (ks == XK_2) {
                    input.presetRequested = 2;
                } else if (ks == XK_3) {
                    input.presetRequested = 3;
                } else if (ks == XK_F12) {
                    input.screenshotRequested = true;
                } else if (ks == XK_w || ks == XK_W || ks == XK_Up) {
                    input.keyUp = true;
                    input.hasMouseTarget = false;
                } else if (ks == XK_s || ks == XK_S || ks == XK_Down) {
                    input.keyDown = true;
                    input.hasMouseTarget = false;
                } else if (ks == XK_a || ks == XK_A || ks == XK_Left) {
                    input.keyLeft = true;
                    input.hasMouseTarget = false;
                } else if (ks == XK_d || ks == XK_D || ks == XK_Right) {
                    input.keyRight = true;
                    input.hasMouseTarget = false;
                }
                break;
            }

            case KeyRelease: {
                KeySym ks = XLookupKeysym(&ev.xkey, 0);
                if (ks == XK_w || ks == XK_W || ks == XK_Up) input.keyUp = false;
                if (ks == XK_s || ks == XK_S || ks == XK_Down) input.keyDown = false;
                if (ks == XK_a || ks == XK_A || ks == XK_Left) input.keyLeft = false;
                if (ks == XK_d || ks == XK_D || ks == XK_Right) input.keyRight = false;
                break;
            }

            default:
                break;
        }
    }
}

void X11Renderer::clearBuffer(uint32_t color) {
    std::fill(framebuffer_.begin(), framebuffer_.end(), color);
}

void X11Renderer::putPixelAlpha(int x, int y, uint32_t rgb, float alpha) {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return;
    size_t idx = static_cast<size_t>(y) * static_cast<size_t>(width_) + static_cast<size_t>(x);
    if (alpha >= 0.999f) {
        framebuffer_[idx] = rgb;
    } else if (alpha > 0.005f) {
        framebuffer_[idx] = blendPixel(framebuffer_[idx], rgb, alpha);
    }
}

void X11Renderer::drawLine(float x0, float y0, float x1, float y1, uint32_t rgb, float alpha) {
    float dx = x1 - x0;
    float dy = y1 - y0;
    int steps = static_cast<int>(std::ceil(std::max(std::abs(dx), std::abs(dy))));
    if (steps <= 0) {
        putPixelAlpha(static_cast<int>(x0), static_cast<int>(y0), rgb, alpha);
        return;
    }
    float sx = dx / static_cast<float>(steps);
    float sy = dy / static_cast<float>(steps);
    float x = x0;
    float y = y0;
    for (int i = 0; i <= steps; ++i) {
        putPixelAlpha(static_cast<int>(x), static_cast<int>(y), rgb, alpha);
        x += sx;
        y += sy;
    }
}

void X11Renderer::drawCircle(float cx, float cy, float r, uint32_t rgb, float alpha) {
    if (r < 1.0f) return;
    int segments = std::clamp(static_cast<int>(r * 0.85f), 24, 160);
    float step = 6.283185307f / static_cast<float>(segments);
    float px = cx + r;
    float py = cy;
    for (int i = 1; i <= segments; ++i) {
        float ang = step * static_cast<float>(i);
        float nx = cx + std::cos(ang) * r;
        float ny = cy + std::sin(ang) * r;
        drawLine(px, py, nx, ny, rgb, alpha);
        px = nx;
        py = ny;
    }
}

void X11Renderer::fillCircle(float cx, float cy, float r, uint32_t rgb, float alpha) {
    int minX = std::max(0, static_cast<int>(std::floor(cx - r)));
    int maxX = std::min(width_ - 1, static_cast<int>(std::ceil(cx + r)));
    int minY = std::max(0, static_cast<int>(std::floor(cy - r)));
    int maxY = std::min(height_ - 1, static_cast<int>(std::ceil(cy + r)));
    float rSq = r * r;
    for (int y = minY; y <= maxY; ++y) {
        float dy = static_cast<float>(y) + 0.5f - cy;
        for (int x = minX; x <= maxX; ++x) {
            float dx = static_cast<float>(x) + 0.5f - cx;
            if (dx * dx + dy * dy <= rSq) {
                putPixelAlpha(x, y, rgb, alpha);
            }
        }
    }
}

void X11Renderer::fillTriangle(Vec2 p0, Vec2 p1, Vec2 p2, uint32_t rgb, float alpha) {
    int minX = std::max(0, static_cast<int>(std::floor(std::min({p0.x, p1.x, p2.x}))));
    int maxX = std::min(width_ - 1, static_cast<int>(std::ceil(std::max({p0.x, p1.x, p2.x}))));
    int minY = std::max(0, static_cast<int>(std::floor(std::min({p0.y, p1.y, p2.y}))));
    int maxY = std::min(height_ - 1, static_cast<int>(std::ceil(std::max({p0.y, p1.y, p2.y}))));

    float area = edgeFunction(p0, p1, p2);
    if (std::abs(area) < 1e-4f) return;
    float sign = area > 0.0f ? 1.0f : -1.0f;

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            Vec2 p{static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f};
            float w0 = edgeFunction(p1, p2, p) * sign;
            float w1 = edgeFunction(p2, p0, p) * sign;
            float w2 = edgeFunction(p0, p1, p) * sign;
            if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                putPixelAlpha(x, y, rgb, alpha);
            }
        }
    }
}

void X11Renderer::fillRect(int x, int y, int w, int h, uint32_t rgb, float alpha) {
    int x0 = std::max(0, x);
    int y0 = std::max(0, y);
    int x1 = std::min(width_, x + w);
    int y1 = std::min(height_, y + h);
    for (int py = y0; py < y1; ++py) {
        for (int px = x0; px < x1; ++px) {
            putPixelAlpha(px, py, rgb, alpha);
        }
    }
}

void X11Renderer::drawChar(int x, int y, char c, uint32_t rgb, int scale) {
    const uint8_t* cols = lookupGlyph(c);
    for (int col = 0; col < 5; ++col) {
        uint8_t bits = cols[col];
        for (int row = 0; row < 7; ++row) {
            if ((bits >> row) & 1) {
                fillRect(x + col * scale, y + row * scale, scale, scale, rgb, 1.0f);
            }
        }
    }
}

void X11Renderer::drawText(int x, int y, const std::string& text, uint32_t rgb, int scale) {
    int cursor = x;
    int step = 6 * scale;
    for (char c : text) {
        drawChar(cursor, y, c, rgb, scale);
        cursor += step;
    }
}

void X11Renderer::render(const SwarmSimulation& sim, bool showDebug, bool paused, double fps) {
    if (!isValid() || (!headless_ && !ximage_)) return;

    // Deep slate background
    clearBuffer(0x0B101B);

    const auto& cfg = sim.config();
    const auto& player = sim.player();
    const auto& boids = sim.boids();
    const auto& metrics = sim.metrics();

    // Subtle boundary margin guide
    fillRect(static_cast<int>(cfg.boundaryMargin), static_cast<int>(cfg.boundaryMargin),
             width_ - 2 * static_cast<int>(cfg.boundaryMargin), 1, 0x1E293B, 0.55f);
    fillRect(static_cast<int>(cfg.boundaryMargin), height_ - static_cast<int>(cfg.boundaryMargin),
             width_ - 2 * static_cast<int>(cfg.boundaryMargin), 1, 0x1E293B, 0.55f);
    fillRect(static_cast<int>(cfg.boundaryMargin), static_cast<int>(cfg.boundaryMargin),
             1, height_ - 2 * static_cast<int>(cfg.boundaryMargin), 0x1E293B, 0.55f);
    fillRect(width_ - static_cast<int>(cfg.boundaryMargin), static_cast<int>(cfg.boundaryMargin),
             1, height_ - 2 * static_cast<int>(cfg.boundaryMargin), 0x1E293B, 0.55f);

    // Optional spatial grid & neighborhood debug overlay
    if (showDebug) {
        const auto& grid = sim.grid();
        float cs = grid.cellSize();
        for (int c = 1; c < grid.cols(); ++c) {
            int gx = static_cast<int>(c * cs);
            fillRect(gx, 0, 1, height_, 0x1E3A5F, 0.45f);
        }
        for (int r = 1; r < grid.rows(); ++r) {
            int gy = static_cast<int>(r * cs);
            fillRect(0, gy, width_, 1, 0x1E3A5F, 0.45f);
        }
        if (!boids.empty()) {
            drawCircle(boids[0].pos.x, boids[0].pos.y, cfg.perceptionRadius, 0x38BDF8, 0.55f);
            drawCircle(boids[0].pos.x, boids[0].pos.y, cfg.separationRadius, 0xF97316, 0.55f);
            auto neighbors = sim.queryNeighborsGrid(0);
            for (int nb : neighbors) {
                drawLine(boids[0].pos.x, boids[0].pos.y, boids[nb].pos.x, boids[nb].pos.y,
                         0x38BDF8, 0.40f);
            }
        }
    }

    // Player perception / avoidance aura
    uint32_t auraColor = player.attractMode ? 0x34D399 : 0xF43F5E;
    fillCircle(player.pos.x, player.pos.y, cfg.playerRadius, auraColor, 0.05f);
    drawCircle(player.pos.x, player.pos.y, cfg.playerRadius, auraColor, 0.35f);

    // Expanding shockwave pulse
    if (player.pulseActive) {
        float progress = 1.0f - (player.pulseTimer / player.pulseDuration);
        float waveR = cfg.pulseRadius * progress;
        drawCircle(player.pulseOrigin.x, player.pulseOrigin.y, waveR, 0xFBBF24,
                   0.85f * (1.0f - progress));
        drawCircle(player.pulseOrigin.x, player.pulseOrigin.y, std::max(1.0f, waveR - 3.0f),
                   0xFEF08A, 0.45f * (1.0f - progress));
    }

    // Render boids with short velocity trails and panic-mapped coloring
    constexpr uint32_t kCalmColor = 0x38BDF8;   // cyan-blue
    constexpr uint32_t kMidColor = 0xA855F7;    // violet
    constexpr uint32_t kPanicColor = 0xFB7185;  // coral-red
    constexpr uint32_t kAttractColor = 0x34D399;

    for (const auto& b : boids) {
        uint32_t col = player.attractMode
                           ? lerpColor(kCalmColor, kAttractColor, b.panic)
                           : (b.panic < 0.5f ? lerpColor(kCalmColor, kMidColor, b.panic * 2.0f)
                                             : lerpColor(kMidColor, kPanicColor, (b.panic - 0.5f) * 2.0f));

        // Short motion trail
        int prevIdx = b.trailHead;
        const Vec2& pPrev = b.trail[prevIdx];
        drawLine(pPrev.x, pPrev.y, b.pos.x, b.pos.y, col, 0.32f);

        Vec2 dir = b.vel.normalized();
        if (dir.lengthSq() < 1e-4f) dir = {1.0f, 0.0f};
        Vec2 perp{-dir.y, dir.x};

        float len = 7.5f + b.panic * 2.5f;
        float wing = 3.4f;
        Vec2 tip = b.pos + dir * len;
        Vec2 left = b.pos - dir * (len * 0.55f) + perp * wing;
        Vec2 right = b.pos - dir * (len * 0.55f) - perp * wing;
        fillTriangle(tip, left, right, col, 0.94f);
    }

    // Render Player (predator/controller point)
    fillCircle(player.pos.x, player.pos.y, cfg.captureRadius, auraColor, 0.25f);
    fillCircle(player.pos.x, player.pos.y, 7.5f, 0xFFFFFF, 0.95f);
    drawCircle(player.pos.x, player.pos.y, 11.0f, auraColor, 0.9f);

    // Top HUD bar
    fillRect(12, 12, 680, 74, 0x0F172A, 0.82f);
    fillRect(12, 12, 680, 2, auraColor, 0.85f);

    char line1[128];
    std::snprintf(line1, sizeof(line1),
                  "SWARM EVADER  |  N:%d  FPS:%.0f  STEP:%.2fMS  TAGGED:%d",
                  static_cast<int>(boids.size()), fps, metrics.stepTimeMs, player.score);
    drawText(22, 22, line1, 0xF8FAFC, 2);

    char line2[128];
    std::snprintf(line2, sizeof(line2),
                  "POLARIZATION:%.2f  PANIC:%3.0f%%  NN-DIST:%.1fPX  MODE:%s",
                  metrics.polarization, metrics.panicFraction * 100.0f,
                  metrics.meanNearestNeighborDist,
                  player.attractMode ? "ATTRACT" : "REPEL");
    drawText(22, 46, line2, 0x94A3B8, 2);

    if (paused) {
        fillRect(width_ / 2 - 90, height_ / 2 - 22, 180, 44, 0x0F172A, 0.9f);
        drawText(width_ / 2 - 54, height_ / 2 - 7, "PAUSED", 0xFBBF24, 3);
    }

    // Bottom controls strip
    fillRect(12, height_ - 36, width_ - 24, 24, 0x0F172A, 0.78f);
    drawText(20, height_ - 29,
             "MOUSE/WASD:MOVE  LMB/SPACE:PULSE  RMB/E:ATTRACT/REPEL  +/-:BOIDS  1-3:PRESET  G:GRID  P:PAUSE  R:RESET  Q:QUIT",
             0x94A3B8, 1);

    if (!headless_ && display_ && ximage_) {
        XPutImage(display_, window_, gc_, ximage_, 0, 0, 0, 0,
                  static_cast<unsigned int>(width_), static_cast<unsigned int>(height_));
        XFlush(display_);
    }
}

bool X11Renderer::savePPM(const std::string& path) const {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    std::fprintf(f, "P6\n%d %d\n255\n", width_, height_);
    std::vector<uint8_t> row(static_cast<size_t>(width_) * 3u);
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            uint32_t px = framebuffer_[static_cast<size_t>(y) * static_cast<size_t>(width_) + static_cast<size_t>(x)];
            row[static_cast<size_t>(x) * 3u + 0] = static_cast<uint8_t>((px >> 16) & 0xFF);
            row[static_cast<size_t>(x) * 3u + 1] = static_cast<uint8_t>((px >> 8) & 0xFF);
            row[static_cast<size_t>(x) * 3u + 2] = static_cast<uint8_t>(px & 0xFF);
        }
        std::fwrite(row.data(), 1, row.size(), f);
    }
    std::fclose(f);
    return true;
}
