#include "gif.hpp"
#include <cstring>

bool gif_check_header(const u8* d, size_t n, int* w, int* h) {
    if (n < 13 || (memcmp(d, "GIF87a", 6) != 0 && memcmp(d, "GIF89a", 6) != 0)) return false;
    if (w) *w = d[6] | (d[7] << 8);
    if (h) *h = d[8] | (d[9] << 8);
    return true;
}

namespace {
struct Rd {
    const u8* d; size_t n, p = 0;
    bool has(size_t k) const { return p + k <= n; }
    int u8_() { return p < n ? d[p++] : -1; }
    int u16() { if (!has(2)) { p = n; return -1; } int v = d[p] | (d[p + 1] << 8); p += 2; return v; }
};

// Alt-bloklari birlestirir
bool read_sub_blocks(Rd& r, std::vector<u8>& out) {
    for (;;) {
        int len = r.u8_();
        if (len < 0) return false;
        if (len == 0) return true;
        if (!r.has((size_t)len)) return false;
        out.insert(out.end(), r.d + r.p, r.d + r.p + len);
        r.p += (size_t)len;
    }
}
bool skip_sub_blocks(Rd& r) {
    for (;;) {
        int len = r.u8_();
        if (len < 0) return false;
        if (len == 0) return true;
        if (!r.has((size_t)len)) return false;
        r.p += (size_t)len;
    }
}

bool lzw(const std::vector<u8>& data, int min_code, std::vector<u8>& out, size_t npix) {
    if (min_code < 1 || min_code > 11) return false;
    const int clear = 1 << min_code, eoi = clear + 1;
    int next = eoi + 1, size = min_code + 1;
    static thread_local short prefix[4096];
    static thread_local u8 suffix[4096];
    std::vector<u8> stack;
    stack.reserve(4097);
    out.clear();
    out.reserve(npix);
    size_t bitpos = 0, total_bits = data.size() * 8;
    int old = -1; u8 first = 0;
    while (out.size() < npix) {
        if (bitpos + (size_t)size > total_bits) break;
        int code = 0;
        for (int i = 0; i < size; i++, bitpos++) code |= ((data[bitpos >> 3] >> (bitpos & 7)) & 1) << i;
        if (code == clear) { size = min_code + 1; next = eoi + 1; old = -1; continue; }
        if (code == eoi) break;
        if (old == -1) {
            if (code >= clear) return !out.empty();
            out.push_back((u8)code); old = code; first = (u8)code; continue;
        }
        int in = code;
        stack.clear();
        if (code >= next) {
            if (code > next) break;                       // bozuk
            stack.push_back(first); code = old;
        }
        while (code >= clear) {
            if (code >= 4096) return !out.empty();
            stack.push_back(suffix[code]); code = prefix[code];
        }
        first = (u8)code;
        stack.push_back(first);
        if (next < 4096) {
            prefix[next] = (short)old; suffix[next] = first; next++;
            if (next == (1 << size) && size < 12) size++;
        }
        old = in;
        for (size_t i = stack.size(); i > 0 && out.size() < npix; i--) out.push_back(stack[i - 1]);
    }
    return true;
}
}

bool gif_stream(const u8* data, size_t size, bool first_only, const GifFrameCb& cb) {
    if (!gif_check_header(data, size)) return false;
    int frame_count = 0;
    Rd r{data, size};
    r.p = 6;
    int W = r.u16(), H = r.u16();
    int flags = r.u8_();
    r.u8_(); r.u8_();                                        // arka plan indeksi, en-boy
    if (W <= 0 || H <= 0 || (size_t)W * H > ((size_t)1 << 26)) return false;
    std::vector<u8> gct;
    if (flags & 0x80) {
        size_t cnt = (size_t)3 << ((flags & 7) + 1);
        if (!r.has(cnt)) return false;
        gct.assign(r.d + r.p, r.d + r.p + cnt);
        r.p += cnt;
    }
    const size_t fb = (size_t)W * H * 4;
    std::vector<u8> canvas(fb, 0), saved;
    int g_disp = 0, g_delay = 0, g_trans = -1;
    int prev_disp = 0, px = 0, py = 0, pw = 0, ph = 0;
    bool have_prev = false;

    while (r.has(1)) {
        int b = r.u8_();
        if (b == 0x3B) break;
        if (b == 0x21) {
            int label = r.u8_();
            if (label == 0xF9) {
                int len = r.u8_();
                if (len >= 4 && r.has((size_t)len)) {
                    int f = r.d[r.p];
                    g_disp = (f >> 2) & 7;
                    g_delay = (r.d[r.p + 1] | (r.d[r.p + 2] << 8)) * 10;
                    g_trans = (f & 1) ? r.d[r.p + 3] : -1;
                    r.p += (size_t)len;
                    if (!skip_sub_blocks(r)) break;
                } else break;
            } else if (!skip_sub_blocks(r)) break;
            continue;
        }
        if (b != 0x2C) continue;
        int ix = r.u16(), iy = r.u16(), iw = r.u16(), ih = r.u16(), f = r.u8_();
        if (f < 0 || iw < 0 || ih < 0) break;
        std::vector<u8> lct;
        if (f & 0x80) {
            size_t cnt = (size_t)3 << ((f & 7) + 1);
            if (!r.has(cnt)) break;
            lct.assign(r.d + r.p, r.d + r.p + cnt);
            r.p += cnt;
        }
        int minc = r.u8_();
        std::vector<u8> comp;
        if (!read_sub_blocks(r, comp)) break;
        std::vector<u8> idx;
        if (iw > 0 && ih > 0 && !lzw(comp, minc, idx, (size_t)iw * ih)) break;
        idx.resize((size_t)iw * ih, 0);
        const std::vector<u8>& pal = lct.empty() ? gct : lct;

        // onceki karenin disposal'i
        if (have_prev) {
            if (prev_disp == 2) {
                for (int y = py; y < py + ph && y < H; y++) if (y >= 0)
                    for (int x = px; x < px + pw && x < W; x++) if (x >= 0) memset(&canvas[((size_t)y * W + x) * 4], 0, 4);
            } else if (prev_disp == 3 && saved.size() == fb) canvas = saved;
        }
        if (g_disp == 3) saved = canvas; else saved.clear();

        // satir sirasi (interlace)
        std::vector<int> rows((size_t)ih);
        if (f & 0x40) {
            int k = 0;
            static const int st[4] = {0, 4, 2, 1}, sp[4] = {8, 8, 4, 2};
            for (int pass = 0; pass < 4; pass++) for (int y = st[pass]; y < ih; y += sp[pass]) rows[(size_t)k++] = y;
        } else for (int y = 0; y < ih; y++) rows[(size_t)y] = y;

        if (!pal.empty()) {
            for (int sy = 0; sy < ih; sy++) {
                int y = iy + rows[(size_t)sy];
                if (y < 0 || y >= H) continue;
                for (int sx = 0; sx < iw; sx++) {
                    int x = ix + sx;
                    if (x < 0 || x >= W) continue;
                    int c = idx[(size_t)sy * iw + sx];
                    if (c == g_trans) continue;
                    if ((size_t)c * 3 + 2 >= pal.size()) continue;
                    u8* o = &canvas[((size_t)y * W + x) * 4];
                    o[0] = pal[(size_t)c * 3]; o[1] = pal[(size_t)c * 3 + 1]; o[2] = pal[(size_t)c * 3 + 2]; o[3] = 255;
                }
            }
        }
        frame_count++;
        if (!cb(canvas.data(), W, H, g_delay)) return frame_count > 0;
        prev_disp = g_disp; px = ix; py = iy; pw = iw; ph = ih; have_prev = true;
        g_disp = 0; g_delay = 0; g_trans = -1;
        if (first_only) break;
    }
    return frame_count > 0;
}

bool gif_decode(const u8* data, size_t size, RawImage& out, bool first_only, size_t max_bytes) {
    out = RawImage();
    size_t total = 0;
    bool ok = gif_stream(data, size, first_only, [&](const u8* rgba, int w, int h, int delay) {
        out.w = w; out.h = h;
        total += (size_t)w * h * 4;
        if (total > max_bytes) return false;
        RawFrame fr;
        fr.rgba.assign(rgba, rgba + (size_t)w * h * 4);
        fr.delay_ms = delay;
        out.frames.push_back(std::move(fr));
        return true;
    });
    return ok && !out.frames.empty();
}
