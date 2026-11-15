#include "scale.hpp"
#include <thread>
#include <cstring>

static inline u32 lerp32(u32 a, u32 b, u32 w) {      // w: 0..256, bayt basina lerp
    u32 rb = ((((a & 0x00FF00FFu) * (256 - w)) + ((b & 0x00FF00FFu) * w)) >> 8) & 0x00FF00FFu;
    u32 ag = ((((a >> 8) & 0x00FF00FFu) * (256 - w)) + (((b >> 8) & 0x00FF00FFu) * w)) & 0xFF00FF00u;
    return rb | ag;
}

namespace {
struct Map { std::vector<int> i0, i1; std::vector<u32> w; };
Map make_map(int s, int d) {
    Map m; m.i0.resize((size_t)d); m.i1.resize((size_t)d); m.w.resize((size_t)d);
    double sc = (double)s / d;
    for (int i = 0; i < d; i++) {
        double f = (i + 0.5) * sc - 0.5;
        if (f < 0) f = 0;
        int a = (int)f;
        if (a >= s - 1) { a = s - 1; m.i0[(size_t)i] = a; m.i1[(size_t)i] = a; m.w[(size_t)i] = 0; continue; }
        m.i0[(size_t)i] = a; m.i1[(size_t)i] = a + 1; m.w[(size_t)i] = (u32)((f - a) * 256.0 + 0.5);
        if (m.w[(size_t)i] > 256) m.w[(size_t)i] = 256;
    }
    return m;
}
void rows(const u32* src, int sw, u32* dst, int dw, const Map& mx, const Map& my, int y0, int y1) {
    for (int y = y0; y < y1; y++) {
        const u32* r0 = src + (size_t)my.i0[(size_t)y] * sw;
        const u32* r1 = src + (size_t)my.i1[(size_t)y] * sw;
        u32 wy = my.w[(size_t)y];
        u32* o = dst + (size_t)y * dw;
        for (int x = 0; x < dw; x++) {
            int a = mx.i0[(size_t)x], b = mx.i1[(size_t)x];
            u32 wx = mx.w[(size_t)x];
            u32 top = lerp32(r0[a], r0[b], wx);
            u32 bot = (r0 == r1) ? top : lerp32(r1[a], r1[b], wx);
            o[x] = wy ? lerp32(top, bot, wy) : top;
        }
    }
}
}

void scale_bilinear(const u8* src, int sw, int sh, u8* dst, int dw, int dh) {
    if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return;
    if (sw == dw && sh == dh) { memcpy(dst, src, (size_t)sw * sh * 4); return; }
    Map mx = make_map(sw, dw), my = make_map(sh, dh);
    const u32* s = (const u32*)src; u32* d = (u32*)dst;
    unsigned nt = 1;
    if ((size_t)dw * dh > 400000) { nt = std::thread::hardware_concurrency(); if (nt < 1) nt = 1; if (nt > 8) nt = 8; if ((int)nt > dh) nt = 1; }
    if (nt <= 1) { rows(s, sw, d, dw, mx, my, 0, dh); return; }
    std::vector<std::thread> th;
    for (unsigned t = 1; t < nt; t++) {
        int y0 = (int)((long long)dh * t / nt), y1 = (int)((long long)dh * (t + 1) / nt);
        th.emplace_back([=, &mx, &my] { rows(s, sw, d, dw, mx, my, y0, y1); });
    }
    rows(s, sw, d, dw, mx, my, 0, (int)((long long)dh / nt));
    for (auto& t : th) t.join();
}

void premultiply_swap_rb(const u8* in, u8* out, size_t n) {
    for (size_t i = 0; i < n; i++) {
        u32 a = in[i * 4 + 3];
        u8 r = in[i * 4], g = in[i * 4 + 1], b = in[i * 4 + 2];
        if (a != 255) { r = (u8)((r * a + 127) / 255); g = (u8)((g * a + 127) / 255); b = (u8)((b * a + 127) / 255); }
        out[i * 4] = b; out[i * 4 + 1] = g; out[i * 4 + 2] = r; out[i * 4 + 3] = (u8)a;
    }
}
void unpremultiply_swap_rb(const u8* in, u8* out, size_t n) {
    for (size_t i = 0; i < n; i++) {
        u32 a = in[i * 4 + 3];
        u32 b = in[i * 4], g = in[i * 4 + 1], r = in[i * 4 + 2];
        if (a != 255 && a != 0) { r = (r * 255 + a / 2) / a; g = (g * 255 + a / 2) / a; b = (b * 255 + a / 2) / a; if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255; }
        out[i * 4] = (u8)r; out[i * 4 + 1] = (u8)g; out[i * 4 + 2] = (u8)b; out[i * 4 + 3] = (u8)a;
    }
}
