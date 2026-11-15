#include "player.hpp"
#include "scale.hpp"
#include <cmath>

static const wchar_t* kCls = L"PopDeckOverlay";

static void register_class() {
    static bool done = false;
    if (done) return;
    done = true;
    WNDCLASSEXW wc; memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kCls;
    RegisterClassExW(&wc);
}

Player::Player(const PlayParams& p) : p_(p) {
    ev_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    static std::once_flag once;
    std::call_once(once, register_class);
    th_ = std::thread([this] { run(); });
}

Player::~Player() {
    stop();
    CloseHandle(ev_);
}

void Player::stop() {
    stop_ = true;
    SetEvent(ev_);
    if (th_.joinable()) th_.join();
}

void Player::run() {
    const ImgData& im = *p_.img;
    HWND hw = nullptr;
    HDC scr = nullptr, mem = nullptr;
    HBITMAP dib = nullptr;
    HGDIOBJ old = nullptr;
    void* bits = nullptr;
    int dw = 1, dh = 1;
    do {
        display_size(im.w, im.h, p_.sw, p_.sh, p_.oran, p_.boz, p_.maxb, dw, dh);
        const int x = (p_.sw - dw) / 2, y = (p_.sh - dh) / 2;
        DWORD ex = WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
        if (p_.click_through) ex |= WS_EX_TRANSPARENT;
        hw = CreateWindowExW(ex, kCls, L"", WS_POPUP, x, y, dw, dh, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (!hw) break;
        scr = GetDC(nullptr);
        mem = CreateCompatibleDC(scr);
        BITMAPINFO bi; memset(&bi, 0, sizeof bi);
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = dw; bi.bmiHeader.biHeight = -dh;       // yukaridan asagi
        bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
        dib = CreateDIBSection(scr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!dib || !bits) break;
        old = SelectObject(mem, dib);

        auto render = [&](int idx) { scale_bilinear(im.frames[(size_t)idx].bgra.data(), im.w, im.h, (u8*)bits, dw, dh); };
        auto show = [&](int alpha) {
            POINT pos = {x, y}, src = {0, 0};
            SIZE sz = {dw, dh};
            BLENDFUNCTION bf; bf.BlendOp = AC_SRC_OVER; bf.BlendFlags = 0; bf.SourceConstantAlpha = (BYTE)alpha; bf.AlphaFormat = AC_SRC_ALPHA;
            UpdateLayeredWindow(hw, scr, &pos, &sz, mem, &src, 0, &bf, ULW_ALPHA);
        };

        const int n = (int)im.frames.size();
        std::vector<double> starts((size_t)n);
        double total = 0;
        for (int i = 0; i < n; i++) { starts[(size_t)i] = total; total += std::max(1, im.frames[(size_t)i].ms); }
        const double end_ms = (p_.tek_tur && n > 1) ? total : std::max(0.5, p_.sure_s) * 1000.0;
        const double fade_ms = p_.fade_in > 0 ? p_.fade_in * 1000.0 : 0;

        render(0);
        int shown = 0, last_alpha = fade_ms > 0 ? 0 : 255;
        show(last_alpha);
        ShowWindow(hw, SW_SHOWNOACTIVATE);
        const double t0 = mono_ms();
        if (p_.on_first) p_.on_first();

        for (;;) {
            MSG msg;
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
            if (stop_) break;
            const double now = mono_ms() - t0;
            if (now >= end_ms) break;
            int idx = 0;
            double dong = 0;
            if (n > 1) {
                dong = std::fmod(now, total);
                idx = (int)(std::upper_bound(starts.begin(), starts.end(), dong) - starts.begin()) - 1;
                if (idx < 0) idx = 0;
                if (idx >= n) idx = n - 1;
            }
            bool changed = idx != shown;
            if (changed) { render(idx); shown = idx; }
            int alpha = 255;
            bool fading = false;
            if (fade_ms > 0 && now < fade_ms) { alpha = (int)(255.0 * now / fade_ms); fading = true; }
            if (changed || alpha != last_alpha) { show(alpha); last_alpha = alpha; }
            double wait;
            if (fading) wait = 15;
            else if (n > 1) wait = starts[(size_t)idx] + std::max(1, im.frames[(size_t)idx].ms) - dong;
            else wait = end_ms - now;
            wait = std::min(wait, end_ms - now);
            if (wait < 1) wait = 1;
            MsgWaitForMultipleObjects(1, &ev_, FALSE, (DWORD)wait, QS_ALLINPUT);
        }
    } while (false);
    if (old && mem) SelectObject(mem, old);
    if (dib) DeleteObject(dib);
    if (mem) DeleteDC(mem);
    if (scr) ReleaseDC(nullptr, scr);
    if (hw) DestroyWindow(hw);
    if (p_.on_done) p_.on_done();
}
