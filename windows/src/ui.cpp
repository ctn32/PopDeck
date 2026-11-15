#include "ui.hpp"
#include <cmath>
#include <map>

namespace ui {
const Color C_BG(255, 0x13, 0x15, 0x1c), C_YUZEY(255, 0x1a, 0x1d, 0x27), C_YUZEY2(255, 0x22, 0x26, 0x37), C_KENAR(255, 0x2c, 0x31, 0x45),
    C_LISTE(255, 0x16, 0x19, 0x22), C_YAZI(255, 0xe6, 0xe8, 0xee), C_SOLUK(255, 0x8b, 0x92, 0xa5), C_VURGU(255, 0x63, 0x72, 0xe6),
    C_VURGU_ACIK(255, 0xaa, 0xb4, 0xff), C_VURGU_SOFT(255, 0x25, 0x2a, 0x4a), C_VURGU_KENAR(255, 0x3a, 0x43, 0x7d),
    C_YESIL(255, 0x4c, 0xc3, 0x8a), C_KIRMIZI(255, 0xf0, 0x6a, 0x6a), C_SARI(255, 0xff, 0xd4, 0x3b);

static ULONG_PTR g_token = 0;
void init_gdiplus() { GdiplusStartupInput in; GdiplusStartup(&g_token, &in, nullptr); }
void shutdown_gdiplus() { if (g_token) GdiplusShutdown(g_token); }

static Font* get_font(float px, bool bold) {
    static std::map<int, Font*> cache;
    int key = (int)(px * 10) * 2 + (bold ? 1 : 0);
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    Font* f = new Font(L"Segoe UI", px, bold ? FontStyleBold : FontStyleRegular, UnitPixel);
    cache[key] = f;
    return f;
}

static Graphics* measure_g() {
    static HDC dc = CreateCompatibleDC(nullptr);
    static Graphics* g = new Graphics(dc);
    return g;
}

void rrect(Graphics& g, float x, float y, float w, float h, float r, const Color* fill, const Color* line, float lw) {
    GraphicsPath p;
    float d = std::min(r * 2, std::min(w, h));
    p.AddArc(x, y, d, d, 180, 90);
    p.AddArc(x + w - d, y, d, d, 270, 90);
    p.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    p.AddArc(x, y + h - d, d, d, 90, 90);
    p.CloseFigure();
    if (fill) { SolidBrush b(*fill); g.FillPath(&b, &p); }
    if (line) { Pen pen(*line, lw); g.DrawPath(&pen, &p); }
}

void text(Graphics& g, const std::string& s, float x, float y, float w, float h, float px, bool bold, const Color& col,
          int halign, int valign, bool wrap, bool ellipsis) {
    if (s.empty()) return;
    std::wstring ws = W(s);
    StringFormat sf;
    sf.SetAlignment(halign == 1 ? StringAlignmentCenter : halign == 2 ? StringAlignmentFar : StringAlignmentNear);
    sf.SetLineAlignment(valign == 0 ? StringAlignmentNear : StringAlignmentCenter);
    sf.SetTrimming(ellipsis ? StringTrimmingEllipsisCharacter : StringTrimmingNone);
    if (!wrap) sf.SetFormatFlags(StringFormatFlagsNoWrap);
    SolidBrush b(col);
    g.DrawString(ws.c_str(), -1, get_font(px, bold), RectF(x, y, w, h), &sf, &b);
}

float text_width(const std::string& s, float px, bool bold) {
    std::wstring ws = W(s);
    StringFormat sf(StringFormat::GenericTypographic());
    RectF out;
    measure_g()->MeasureString(ws.c_str(), -1, get_font(px, bold), PointF(0, 0), &sf, &out);
    return out.Width;
}
float text_height(const std::string& s, float px, bool bold, float width) {
    std::wstring ws = W(s);
    StringFormat sf;
    RectF out;
    measure_g()->MeasureString(ws.c_str(), -1, get_font(px, bold), RectF(0, 0, width, 10000), &sf, &out);
    return out.Height;
}

// ---------------- Surface ----------------
float Surface::logical_w() const { RECT r; GetClientRect(hwnd, &r); return (r.right - r.left) / scale; }
float Surface::logical_h() const { RECT r; GetClientRect(hwnd, &r); return (r.bottom - r.top) / scale; }
void Surface::paint_bg(Graphics& g, float w, float h) { SolidBrush b(C_BG); g.FillRectangle(&b, 0.f, 0.f, w, h); }

Widget* Surface::find(float x, float y) {
    for (size_t i = items.size(); i > 0; i--) {
        Widget* w = items[i - 1];
        if (!w->visible) continue;
        if (w->clipped && !viewport.Contains(x, y)) continue;
        if (w->hit(x, y)) return w;
    }
    return nullptr;
}

void Surface::paint() {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT cr; GetClientRect(hwnd, &cr);
    int W_ = cr.right - cr.left, H_ = cr.bottom - cr.top;
    if (W_ > 0 && H_ > 0) {
        HDC mem = CreateCompatibleDC(dc);
        HBITMAP bm = CreateCompatibleBitmap(dc, W_, H_);
        HGDIOBJ old = SelectObject(mem, bm);
        {
            Graphics g(mem);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
            g.ScaleTransform(scale, scale);
            paint_bg(g, W_ / scale, H_ / scale);
            for (Widget* w : items) {
                if (!w->visible) continue;
                if (w->clipped) g.SetClip(viewport); else g.ResetClip();
                w->draw(g);
            }
            g.ResetClip();
        }
        BitBlt(dc, 0, 0, W_, H_, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old); DeleteObject(bm); DeleteDC(mem);
    }
    EndPaint(hwnd, &ps);
}

LRESULT Surface::proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    hwnd = h;
    switch (m) {
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: paint(); return 0;
        case WM_SIZE: on_relayout(); invalidate(); return 0;
        case WM_MOUSEMOVE: {
            float x = GET_X_LPARAM(l) / scale, y = GET_Y_LPARAM(l) / scale;
            if (!tracking) { TRACKMOUSEEVENT t = {sizeof t, TME_LEAVE, h, 0}; TrackMouseEvent(&t); tracking = true; }
            if (capture) { capture->move(x, y); return 0; }
            Widget* t = find(x, y);
            if (t != hot) {
                if (hot) hot->hover = false;
                hot = t;
                if (hot) hot->hover = true;
                invalidate();
            }
            if (hot) hot->move(x, y);
            return 0;
        }
        case WM_MOUSELEAVE:
            tracking = false;
            if (hot) { hot->hover = false; hot = nullptr; invalidate(); }
            for (Widget* it : items) if (it->hover) { it->hover = false; }
            return 0;
        case WM_SETCURSOR:
            if (LOWORD(l) == HTCLIENT) { SetCursor(LoadCursor(nullptr, (hot && hot->hand) ? IDC_HAND : IDC_ARROW)); return TRUE; }
            break;
        case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: {
            float x = GET_X_LPARAM(l) / scale, y = GET_Y_LPARAM(l) / scale;
            SetFocus(h);
            Widget* t = find(x, y);
            if (t) {
                capture = t; SetCapture(h);
                if (m == WM_LBUTTONDBLCLK) t->dbl(x, y); else t->down(x, y);
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            float x = GET_X_LPARAM(l) / scale, y = GET_Y_LPARAM(l) / scale;
            Widget* c = capture;
            capture = nullptr;
            if (GetCapture() == h) ReleaseCapture();
            if (c) c->up(x, y);
            invalidate();
            return 0;
        }
        case WM_CAPTURECHANGED: if (capture) { capture->pressed = false; capture = nullptr; invalidate(); } return 0;
        case WM_MOUSEWHEEL: {
            POINT p = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
            ScreenToClient(h, &p);
            float x = p.x / scale, y = p.y / scale;
            int d = GET_WHEEL_DELTA_WPARAM(w);
            Widget* t = find(x, y);
            if (t && t->wheel(x, y, d)) { invalidate(); return 0; }
            wheel_default(x, y, d);
            return 0;
        }
    }
    return other(m, w, l);
}

// ---------------- Widget cizimleri ----------------
void Button::draw(Graphics& g) {
    Color dolgu, ustu, basili, kenar, yazi;
    switch (style) {
        case Primary: dolgu = Color(255, 0x5b, 0x68, 0xd4); ustu = Color(255, 0x68, 0x75, 0xe0); basili = Color(255, 0x4d, 0x59, 0xbd); kenar = dolgu; yazi = Color(255, 255, 255, 255); break;
        case Danger: dolgu = Color(255, 0x2a, 0x1d, 0x24); ustu = Color(255, 0x38, 0x25, 0x2f); basili = Color(255, 0x22, 0x18, 0x20); kenar = Color(255, 0x55, 0x30, 0x39); yazi = Color(255, 0xff, 0x9b, 0x9b); break;
        case Ghost: dolgu = arka; ustu = Color(255, 0x22, 0x26, 0x37); basili = Color(255, 0x1a, 0x1d, 0x2a); kenar = Color(255, 0x30, 0x36, 0x4c); yazi = Color(255, 0xa2, 0xa9, 0xbd); break;
        default: dolgu = Color(255, 0x22, 0x26, 0x37); ustu = Color(255, 0x2b, 0x31, 0x47); basili = Color(255, 0x1a, 0x1d, 0x2a); kenar = Color(255, 0x30, 0x36, 0x4c); yazi = Color(255, 0xdd, 0xe0, 0xe8); break;
    }
    Color z = pressed ? basili : (hover ? ustu : dolgu);
    rrect(g, r.X + 1, r.Y + 1, r.Width - 2, r.Height - 2, 11, &z, &kenar);
    text(g, s, r.X, r.Y + (pressed ? 1.f : 0.f), r.Width, r.Height, px, true, yazi, 1, 1, false, true);
}

void Toggle::draw(Graphics& g) {
    bool on = v && *v;
    Color tr = on ? C_VURGU : Color(255, 0x34, 0x3a, 0x52), kn(255, 0xf4, 0xf5, 0xfb);
    rrect(g, r.X + 1, r.Y + 2, 44, 22, 11, &tr, nullptr);
    float x = r.X + (on ? 34.f : 13.f);
    SolidBrush b(kn);
    g.FillEllipse(&b, x - 8, r.Y + 5, 16.f, 16.f);
}

void Row::draw_label(Graphics& g) { text(g, label, r.X, r.Y, label_w, r.Height, 13, false, C_YAZI, 0, 1, true, false); }

void ToggleRow::draw(Graphics& g) {
    draw_label(g);
    bool on = v && *v;
    Color tr = on ? C_VURGU : Color(255, 0x34, 0x3a, 0x52), kn(255, 0xf4, 0xf5, 0xfb);
    float x0 = r.X + r.Width - 46, y0 = r.Y + (r.Height - 26) / 2;
    rrect(g, x0 + 1, y0 + 2, 44, 22, 11, &tr, nullptr);
    SolidBrush b(kn);
    g.FillEllipse(&b, x0 + (on ? 34.f : 13.f) - 8, y0 + 5, 16.f, 16.f);
}

void SliderRow::draw(Graphics& g) {
    draw_label(g);
    double val = v ? std::min(b, std::max(a, *v)) : a;
    float x1 = r.X + r.Width - track_w - 66 + 11, x2 = r.X + r.Width - 66 - 11, y = r.Y + r.Height / 2;
    float oran = (float)((val - a) / (b - a)), xk = x1 + oran * (x2 - x1);
    Color tr(255, 0x34, 0x3a, 0x52);
    Pen p1(tr, 4); p1.SetStartCap(LineCapRound); p1.SetEndCap(LineCapRound);
    g.DrawLine(&p1, x1, y, x2, y);
    if (xk > x1) { Pen p2(C_VURGU, 4); p2.SetStartCap(LineCapRound); p2.SetEndCap(LineCapRound); g.DrawLine(&p2, x1, y, xk, y); }
    SolidBrush kb(Color(255, 0xf4, 0xf5, 0xfb)); Pen kp(C_VURGU, 2);
    g.FillEllipse(&kb, xk - 8, y - 8, 16.f, 16.f);
    g.DrawEllipse(&kp, xk - 8, y - 8, 16.f, 16.f);
    char buf[40];
    if (step >= 1) snprintf(buf, sizeof buf, "%d", (int)std::lround(val)); else snprintf(buf, sizeof buf, "%.1f", val);
    text(g, std::string(buf) + unit, r.X + r.Width - 60, r.Y, 60, r.Height, 12, true, C_YAZI, 0, 1, false, false);
}
void SliderRow::set_from(float x) {
    float x1 = r.X + r.Width - track_w - 66 + 11, x2 = r.X + r.Width - 66 - 11;
    float o = std::min(1.f, std::max(0.f, (x - x1) / (x2 - x1)));
    double raw = a + o * (b - a);
    double val = std::round(raw / step) * step;
    val = std::min(b, std::max(a, val));
    if (v && *v != val) { *v = val; if (changed) changed(); }
    surf->invalidate();
}

void EditRow::draw(Graphics& g) {
    draw_label(g);
    RectF e = edit_rect();
    rrect(g, e.X, e.Y, e.Width, e.Height, 8, &C_YUZEY2, &C_KENAR);
}

void DropDown::draw(Graphics& g) {
    Color z = hover ? Color(255, 0x2b, 0x31, 0x47) : C_YUZEY2;
    rrect(g, r.X + 1, r.Y + 1, r.Width - 2, r.Height - 2, 9, &z, &C_KENAR);
    std::string t = (sel >= 0 && sel < (int)items.size()) ? items[(size_t)sel] : placeholder;
    text(g, t, r.X + 12, r.Y, r.Width - 36, r.Height, px, false, C_YAZI, 0, 1, false, true);
    Pen p(C_SOLUK, 1.6f);
    float cx = r.X + r.Width - 16, cy = r.Y + r.Height / 2;
    g.DrawLine(&p, cx - 4, cy - 2, cx, cy + 2); g.DrawLine(&p, cx, cy + 2, cx + 4, cy - 2);
}
void DropDown::down(float, float) {
    if (items.empty()) return;
    HMENU m = CreatePopupMenu();
    for (size_t i = 0; i < items.size(); i++) AppendMenuW(m, MF_STRING | ((int)i == sel ? MF_CHECKED : 0), (UINT_PTR)(i + 1), W(items[i]).c_str());
    POINT p = {(LONG)(r.X * surf->scale), (LONG)((r.Y + r.Height) * surf->scale)};
    ClientToScreen(surf->hwnd, &p);
    int id = (int)TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, p.x, p.y, 0, surf->hwnd, nullptr);
    DestroyMenu(m);
    if (id > 0) { sel = id - 1; if (changed) changed(sel); }
    surf->invalidate();
}

int TabBar::part(float x) const {
    int n = (int)titles.size();
    if (n == 0) return 0;
    int p = (int)((x - r.X) / (r.Width / n));
    return std::max(0, std::min(n - 1, p));
}
void TabBar::draw(Graphics& g) {
    rrect(g, r.X + 1, r.Y + 1, r.Width - 2, r.Height - 2, 15, &C_YUZEY, &C_KENAR);
    int n = (int)titles.size();
    if (!n) return;
    float parca = (r.Width - 10) / n;
    for (int i = 0; i < n; i++) {
        float x1 = r.X + 5 + i * parca, x2 = x1 + parca;
        if (i == sel) rrect(g, x1 + 2, r.Y + 6, x2 - x1 - 4, r.Height - 12, 11, &C_VURGU_SOFT, &C_VURGU_KENAR);
        else if (i == over && hover) { Color c(255, 0x20, 0x24, 0x3a); rrect(g, x1 + 2, r.Y + 6, x2 - x1 - 4, r.Height - 12, 11, &c, nullptr); }
        const Color& col = i == sel ? C_VURGU_ACIK : ((i == over && hover) ? C_YAZI : C_SOLUK);
        text(g, titles[(size_t)i], x1, r.Y, x2 - x1, r.Height, 13, true, col, 1, 1, false, true);
    }
}

void Card::draw(Graphics& g) {
    rrect(g, r.X + 1, r.Y + 1, r.Width - 2, r.Height - 2, 16, &C_YUZEY, &C_KENAR);
    text(g, title, r.X + 18, r.Y + 14, r.Width - 36, 22, 15, true, C_VURGU_ACIK, 0, 1, false, true);
}

static std::string first_char_upper(const std::string& s) {
    if (s.empty()) return "?";
    unsigned char c = (unsigned char)s[0];
    size_t n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : 4;
    std::wstring w = W(s.substr(0, std::min(n, s.size())));
    if (!w.empty()) CharUpperBuffW(&w[0], (DWORD)w.size());
    return U(w);
}

int FolderList::row_at(float y) const { return (int)((y - r.Y + scroll - 3) / ROW); }
void FolderList::down(float, float y) {
    int i = row_at(y);
    if (i >= 0 && i < (int)items.size()) { sel = i; if (on_select) on_select(i); surf->invalidate(); }
}
void FolderList::dbl(float x, float y) { down(x, y); int i = row_at(y); if (i >= 0 && i < (int)items.size() && on_dbl) on_dbl(i); }
void FolderList::move(float, float y) { int i = row_at(y); if (i < 0 || i >= (int)items.size()) i = -1; if (i != over) { over = i; surf->invalidate(); } }
bool FolderList::wheel(float, float, int d) {
    if (content_h() <= r.Height) return false;
    scroll -= d / 120.f * 48.f; clamp();
    return true;
}
void FolderList::draw(Graphics& g) {
    rrect(g, r.X, r.Y, r.Width, r.Height, 12, &C_LISTE, &C_KENAR);
    g.SetClip(RectF(r.X + 1, r.Y + 1, r.Width - 2, r.Height - 2));
    if (items.empty()) {
        text(g, empty_text, r.X, r.Y, r.Width, r.Height, 13, false, C_SOLUK, 1, 1, true, false);
    }
    clamp();
    for (size_t i = 0; i < items.size(); i++) {
        float y = r.Y + 3 + (float)i * ROW - scroll;
        if (y + ROW < r.Y || y > r.Y + r.Height) continue;
        const Item& it = items[i];
        float x = r.X + 8, w = r.Width - 16 - (content_h() > r.Height ? 8 : 0);
        bool sel_ = (int)i == sel, ov = (int)i == over && hover;
        Color zemin = sel_ ? C_VURGU_SOFT : (ov ? Color(255, 0x1f, 0x23, 0x31) : C_YUZEY);
        const Color& kenar = sel_ ? C_VURGU_KENAR : C_KENAR;
        rrect(g, x, y + 3, w, ROW - 6, 14, &zemin, &kenar, sel_ ? 2.f : 1.f);
        Color dai = sel_ ? Color(255, 0x2a, 0x30, 0x50) : Color(255, 0x26, 0x2b, 0x40);
        SolidBrush db(dai);
        g.FillEllipse(&db, x + 14, y + 14, 36.f, 36.f);
        std::string ilk = first_char_upper(it.ad);
        text(g, ilk, x + 14, y + 14, 36, 36, 17, true, C_VURGU_ACIK, 1, 1, false, false);
        float rozet_g = std::max(96.f, text_width(it.tus, 12, true) + 30);
        float x2 = x + w - 18, x1 = x2 - rozet_g;
        if (it.atanmis) { rrect(g, x1, y + 17, rozet_g, 26, 13, &C_VURGU_SOFT, &C_VURGU_KENAR); text(g, it.tus, x1, y + 17, rozet_g, 26, 12, true, C_VURGU_ACIK, 1, 1, false, true); }
        else { rrect(g, x1, y + 17, rozet_g, 26, 13, &C_YUZEY, &C_KENAR); text(g, it.tus, x1, y + 17, rozet_g, 26, 12, false, C_SOLUK, 1, 1, false, true); }
        float azami = std::max(60.f, x1 - (x + 66) - 12);
        text(g, it.ad, x + 66, y + 10, azami, 22, 15, true, C_YAZI, 0, 1, false, true);
        text(g, it.icerik, x + 66, y + 33, azami, 18, 12, false, C_SOLUK, 0, 1, false, true);
    }
    if (content_h() > r.Height) {                       // kaydirma cubugu
        float oran = r.Height / content_h(), bh = std::max(30.f, r.Height * oran);
        float by = r.Y + (r.Height - bh) * (scroll / std::max(1.f, content_h() - r.Height));
        Color c(255, 0x3a, 0x40, 0x58);
        rrect(g, r.X + r.Width - 9, by, 5, bh, 2.5f, &c, nullptr);
    }
    g.ResetClip();
}

void QrView::draw(Graphics& g) {
    if (!has) { text(g, message, r.X, r.Y, r.Width, r.Height, 12, false, C_SOLUK, 1, 1, true, false); return; }
    Color beyaz(255, 255, 255, 255), siyah(255, 0, 0, 0);
    SolidBrush bw(beyaz), bk(siyah);
    g.SetSmoothingMode(SmoothingModeNone);
    g.FillRectangle(&bw, r.X, r.Y, r.Width, r.Height);
    int border = 2, n = qr.size + border * 2;
    float cell = std::floor(std::min(r.Width, r.Height) / n * 10) / 10;
    float ox = r.X + (r.Width - cell * n) / 2, oy = r.Y + (r.Height - cell * n) / 2;
    for (int y = 0; y < qr.size; y++) for (int x = 0; x < qr.size; x++)
        if (qr.m[(size_t)y][(size_t)x]) g.FillRectangle(&bk, ox + (x + border) * cell, oy + (y + border) * cell, cell + 0.4f, cell + 0.4f);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
}
}
