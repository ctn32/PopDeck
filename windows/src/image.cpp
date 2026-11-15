#include "image.hpp"
#include "gif.hpp"
#include "scale.hpp"
#include <cstring>

Codec* g_codec = nullptr;

void apply_orientation(Rgba& im, int o) {
    if (o < 2 || o > 8 || im.w <= 0 || im.h <= 0) return;
    const int w = im.w, h = im.h;
    const bool swap = o >= 5;
    const int nw = swap ? h : w, nh = swap ? w : h;
    std::vector<u8> out((size_t)nw * nh * 4);
    for (int y = 0; y < nh; y++) for (int x = 0; x < nw; x++) {
        int sx, sy;
        switch (o) {
            case 2: sx = w - 1 - x; sy = y; break;
            case 3: sx = w - 1 - x; sy = h - 1 - y; break;
            case 4: sx = x; sy = h - 1 - y; break;
            case 5: sx = y; sy = x; break;
            case 6: sx = y; sy = h - 1 - x; break;
            case 7: sx = w - 1 - y; sy = h - 1 - x; break;
            default: sx = w - 1 - y; sy = x; break;
        }
        memcpy(&out[((size_t)y * nw + x) * 4], &im.px[((size_t)sy * w + sx) * 4], 4);
    }
    im.px.swap(out); im.w = nw; im.h = nh;
}

static bool is_gif_data(const std::string& d) { return d.size() > 13 && (d.compare(0, 6, "GIF89a") == 0 || d.compare(0, 6, "GIF87a") == 0); }

bool decode_first_frame(const std::string& path, Rgba& out) {
    std::string d;
    if (read_file(path_u8(path), d) && is_gif_data(d)) {
        RawImage ri;
        if (gif_decode((const u8*)d.data(), d.size(), ri, true) && !ri.frames.empty()) {
            out.w = ri.w; out.h = ri.h; out.px = std::move(ri.frames[0].rgba);
            return true;
        }
    }
    if (!g_codec) return false;
    int orient = 1;
    if (!g_codec->decode(path, out, orient)) return false;
    apply_orientation(out, orient);
    return true;
}

bool load_image(const std::string& path, int fps_limit, std::shared_ptr<ImgData>& out, std::string* err, const std::atomic<bool>* cancel) {
    auto data = std::make_shared<ImgData>();
    auto fail = [&](const std::string& m) { if (err) *err = m; return false; };
    std::string d;
    if (!read_file(path_u8(path), d)) return fail("dosya okunamadi");
    bool gif_ok = false;
    if (is_gif_data(d)) {
        int n_est = 0;
        (void)n_est;
        const double min_sure = (fps_limit < 60 && fps_limit > 0) ? 1000.0 / fps_limit : 0.0;
        struct Pend { std::vector<u8> bgra; double sure = 0; u64 hash = 0; bool has = false; } pend;
        bool multi = false;
        auto push_pending = [&]() {
            if (!pend.has) return;
            ImgFrame f; f.bgra = std::move(pend.bgra); f.ms = (int)(pend.sure + 0.5);
            data->frames.push_back(std::move(f));
            pend.has = false;
        };
        std::vector<u8> tmp;
        size_t budget = (size_t)3 << 30, used = 0;
        bool cancelled = false, oom = false;
        try {
            gif_ok = gif_stream((const u8*)d.data(), d.size(), false, [&](const u8* rgba, int w, int h, int delay) {
                if (cancel && cancel->load()) { cancelled = true; return false; }
                data->w = w; data->h = h;
                double sure = (double)std::max(20, delay ? delay : 100);
                if (pend.has && pend.sure < min_sure) { pend.sure += sure; multi = true; return true; }
                size_t n = (size_t)w * h;
                tmp.resize(n * 4);
                premultiply_swap_rb(rgba, tmp.data(), n);
                u64 hs = 1469598103934665603ull;                     // FNV-1a (8 bayt adimlarla)
                const u64* p = (const u64*)tmp.data();
                for (size_t i = 0; i < n / 2; i++) { hs ^= p[i]; hs *= 1099511628211ull; }
                if (pend.has && hs == pend.hash && memcmp(pend.bgra.data(), tmp.data(), n * 4) == 0) { pend.sure += sure; return true; }
                push_pending();
                used += n * 4;
                if (used > budget) { oom = true; return false; }
                pend.bgra = tmp; pend.sure = sure; pend.hash = hs; pend.has = true;
                return true;
            });
        } catch (const std::bad_alloc&) { return fail("bellek yetersiz"); }
        (void)multi;
        if (cancelled) return fail("iptal");
        if (oom) return fail("cok buyuk");
        push_pending();
        if (!gif_ok || data->frames.empty()) { data->frames.clear(); gif_ok = false; }
    }
    if (!gif_ok) {
        Rgba im;
        if (!decode_first_frame(path, im) || im.w <= 0 || im.h <= 0) return fail("gorsel acilamadi");
        data->w = im.w; data->h = im.h;
        ImgFrame f; f.bgra.resize((size_t)im.w * im.h * 4); f.ms = 0;
        premultiply_swap_rb(im.px.data(), f.bgra.data(), (size_t)im.w * im.h);
        data->frames.push_back(std::move(f));
    }
    out = data;
    return true;
}

void resize_rgba(const Rgba& in, int nw, int nh, Rgba& out) {
    std::vector<u8> a((size_t)in.w * in.h * 4), b((size_t)nw * nh * 4);
    premultiply_swap_rb(in.px.data(), a.data(), (size_t)in.w * in.h);
    scale_bilinear(a.data(), in.w, in.h, b.data(), nw, nh);
    out.w = nw; out.h = nh; out.px.resize((size_t)nw * nh * 4);
    unpremultiply_swap_rb(b.data(), out.px.data(), (size_t)nw * nh);
}

bool make_thumbnail_png(const std::string& path, int size, std::vector<u8>& out) {
    Rgba im;
    if (!decode_first_frame(path, im) || !g_codec) return false;
    int nw = im.w, nh = im.h;
    if (nw > size || nh > size) {
        double k = std::min((double)size / nw, (double)size / nh);
        nw = std::max(1, (int)(nw * k + 0.5)); nh = std::max(1, (int)(nh * k + 0.5));
        Rgba sm;
        resize_rgba(im, nw, nh, sm);
        return g_codec->encode_png(sm, out);
    }
    return g_codec->encode_png(im, out);
}

void display_size(int w, int h, int sw, int sh, double oran, bool boz, double maxb, int& dw, int& dh) {
    double hw, hh;
    if (boz) { hw = sw * oran; hh = sh * oran; }                        // gerilip ekrani tam doldurur
    else {
        double o = std::min(sw * oran / w, sh * oran / h);
        hw = w * o; hh = h * o;
        if (maxb > 0 && maxb < 4.0) { hw = std::min(hw, w * maxb); hh = std::min(hh, h * maxb); }
    }
    dw = std::max(1, (int)(hw + 0.5)); dh = std::max(1, (int)(hh + 0.5));
}
