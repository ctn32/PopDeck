// Kucuk GDI+ arayuz kutuphanesi: koyu tema, yuvarlak kartlar/dugmeler/anahtarlar/kaydiricilar (mantiksal piksel, DPI olcekli)
#pragma once
#include "winhdr.hpp"
#include "qr.hpp"
#include <memory>

namespace ui {
using namespace Gdiplus;

// ---- Tema renkleri (Python surumuyle ayni)
extern const Color C_BG, C_YUZEY, C_YUZEY2, C_KENAR, C_LISTE, C_YAZI, C_SOLUK, C_VURGU, C_VURGU_ACIK, C_VURGU_SOFT,
    C_VURGU_KENAR, C_YESIL, C_KIRMIZI, C_SARI;
inline COLORREF to_ref(const Color& c) { return RGB(c.GetR(), c.GetG(), c.GetB()); }

// ---- Cizim yardimcilari
void rrect(Graphics& g, float x, float y, float w, float h, float r, const Color* fill, const Color* line, float lw = 1.f);
void text(Graphics& g, const std::string& s, float x, float y, float w, float h, float px, bool bold, const Color& col,
          int halign = 0 /*0 sol 1 orta 2 sag*/, int valign = 1 /*0 ust 1 orta*/, bool wrap = false, bool ellipsis = true);
float text_width(const std::string& s, float px, bool bold);
float text_height(const std::string& s, float px, bool bold, float width);      // sarilmis yukseklik

struct Surface;

struct Widget {
    RectF r;
    bool visible = true, hover = false, pressed = false, clipped = false, hand = false;
    virtual ~Widget() {}
    virtual void draw(Graphics& g) = 0;
    virtual bool hit(float x, float y) const { return visible && r.Contains(x, y); }
    virtual void down(float, float) {}
    virtual void move(float, float) {}
    virtual void up(float, float) {}
    virtual void dbl(float x, float y) { down(x, y); }
    virtual bool wheel(float, float, int) { return false; }
    Surface* surf = nullptr;
};

// Pencere: widget listesi, cift tamponlu cizim, fare yonlendirme
struct Surface {
    HWND hwnd = nullptr;
    float scale = 1.f;
    RectF viewport = RectF(0, 0, 0, 0);                    // clipped widget'larin gorunur alani (mantiksal)
    std::vector<std::unique_ptr<Widget>> owned;
    std::vector<Widget*> items;                            // cizim sirasi
    Widget* capture = nullptr;
    Widget* hot = nullptr;
    bool tracking = false;

    virtual ~Surface() {}
    template <class T> T* add(T* w, bool clipped = false) {
        w->surf = this; w->clipped = clipped;
        owned.emplace_back(w); items.push_back(w);
        return w;
    }
    void clear_widgets() { capture = hot = nullptr; items.clear(); owned.clear(); }
    float logical_w() const;
    float logical_h() const;
    void invalidate() { if (hwnd) InvalidateRect(hwnd, nullptr, FALSE); }

    virtual void on_relayout() {}                          // boyut/olcek degisti
    virtual void paint_bg(Graphics& g, float w, float h);
    virtual LRESULT other(UINT m, WPARAM w, LPARAM l) { return DefWindowProcW(hwnd, m, w, l); }
    virtual void wheel_default(float, float, int) {}
    LRESULT proc(HWND h, UINT m, WPARAM w, LPARAM l);
    Widget* find(float x, float y);
    void paint();
};

// ---- Widget'lar
struct Label : Widget {
    std::string s; float px = 13; bool bold = false, wrap = false; Color col = C_YAZI; int align = 0, valign = 1;
    void draw(Graphics& g) override { text(g, s, r.X, r.Y, r.Width, r.Height, px, bold, col, align, valign, wrap, !wrap); }
    bool hit(float, float) const override { return false; }
};

struct Button : Widget {
    enum Style { Normal, Primary, Danger, Ghost };
    std::string s; Style style = Normal; std::function<void()> cb; float px = 13;
    Color arka = C_BG;
    Button() { hand = true; }
    void draw(Graphics& g) override;
    void down(float, float) override { pressed = true; surf->invalidate(); }
    void up(float x, float y) override { bool ok = pressed && r.Contains(x, y); pressed = false; surf->invalidate(); if (ok && cb) cb(); }
};

struct Toggle : Widget {
    bool* v = nullptr; std::function<void()> changed;
    Toggle() { hand = true; r.Width = 46; r.Height = 26; }
    void draw(Graphics& g) override;
    void down(float, float) override { if (v) { *v = !*v; if (changed) changed(); surf->invalidate(); } }
};

struct Row : Widget {                  // "etiket solda + kontrol sagda" satirlari icin ortak taban
    std::string label; float label_w = 250;
    void draw_label(Graphics& g);
};
struct ToggleRow : Row {
    bool* v = nullptr; std::function<void()> changed;
    ToggleRow() { hand = true; }
    void draw(Graphics& g) override;
    void down(float, float) override { if (v) { *v = !*v; if (changed) changed(); surf->invalidate(); } }
};
struct SliderRow : Row {
    double* v = nullptr; double a = 0, b = 100, step = 1; std::string unit; float track_w = 150; bool drag = false;
    std::function<void()> changed;
    SliderRow() { hand = true; }
    void draw(Graphics& g) override;
    void set_from(float x);
    void down(float x, float) override { drag = true; set_from(x); }
    void move(float x, float) override { if (drag) set_from(x); }
    void up(float, float) override { drag = false; }
    bool hit(float x, float y) const override { return visible && r.Contains(x, y) && x >= r.X + r.Width - track_w - 70; }
};
struct EditRow : Row {                 // gercek EDIT kontrolu surface icinde yonetilir
    HWND edit = nullptr; float edit_w = 110;
    RectF edit_rect() const { return RectF(r.X + r.Width - edit_w, r.Y + (r.Height - 32) / 2, edit_w, 32); }
    void draw(Graphics& g) override;
    bool hit(float, float) const override { return false; }
};
struct KeyRow : Row {
    Button* btn = nullptr;             // Surface sahiplenir
    void draw(Graphics& g) override { draw_label(g); }
    bool hit(float, float) const override { return false; }
};

struct DropDown : Widget {
    std::vector<std::string> items; int sel = -1; std::string placeholder; std::function<void(int)> changed; float px = 13;
    Color arka = C_BG;
    DropDown() { hand = true; }
    void draw(Graphics& g) override;
    void down(float, float) override;
};

struct TabBar : Widget {
    std::vector<std::string> titles; int sel = 0, over = -1; std::function<void(int)> changed;
    TabBar() { hand = true; }
    void draw(Graphics& g) override;
    int part(float x) const;
    void down(float x, float) override { sel = part(x); surf->invalidate(); if (changed) changed(sel); }
    void move(float x, float y) override { int p = part(x); if (p != over) { over = p; surf->invalidate(); } }
};

struct Card : Widget {                 // yuvarlak kart (baslikli)
    std::string title;
    void draw(Graphics& g) override;
    bool hit(float, float) const override { return false; }
};

struct Panel : Widget {
    Color fill = C_LISTE;
    void draw(Graphics& g) override { rrect(g, r.X, r.Y, r.Width, r.Height, 12, &fill, &C_KENAR); }
    bool hit(float, float) const override { return false; }
};

struct FolderList : Widget {
    struct Item { std::string ad, tus, icerik; bool atanmis = false; };
    std::vector<Item> items; int sel = -1, over = -1; float scroll = 0; std::string empty_text;
    std::function<void(int)> on_select, on_dbl;
    static constexpr float ROW = 64;
    FolderList() { hand = true; }
    void draw(Graphics& g) override;
    void down(float x, float y) override;
    void dbl(float x, float y) override;
    void move(float x, float y) override;
    bool wheel(float, float, int d) override;
    int row_at(float y) const;
    float content_h() const { return (float)items.size() * ROW + 6; }
    void clamp() { float m = std::max(0.f, content_h() - r.Height); scroll = std::max(0.f, std::min(scroll, m)); }
};

struct QrView : Widget {
    QrCode qr; bool has = false; std::string message;
    void draw(Graphics& g) override;
    bool hit(float, float) const override { return false; }
};

void init_gdiplus();
void shutdown_gdiplus();
}
