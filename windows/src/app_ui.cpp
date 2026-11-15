// MainWindow: pencere, arayuz kurulumu, yerlesim, diyaloglar
#include "app.hpp"
#include <deque>
#include <map>
#include <cmath>

MainWindow* g_app = nullptr;
HWND g_main_hwnd = nullptr;

using namespace ui;

// ---------------- ui_post / ui_after ----------------
static std::mutex g_q_m;
static std::deque<std::function<void()>> g_q;
static std::map<UINT_PTR, std::function<void()>> g_after;
static UINT_PTR g_after_id = 100;

void ui_post(std::function<void()> fn) {
    { std::lock_guard<std::mutex> l(g_q_m); g_q.push_back(std::move(fn)); }
    if (g_main_hwnd) PostMessageW(g_main_hwnd, WM_PD_RUN, 0, 0);
}
void ui_after(int ms, std::function<void()> fn) {
    if (!g_main_hwnd) return;
    UINT_PTR id = g_after_id++;
    g_after[id] = std::move(fn);
    SetTimer(g_main_hwnd, id, (UINT)std::max(1, ms), nullptr);
}

static const UINT_PTR T_KAYIT = 1, T_OTO = 2;

static std::string upper_u8(const std::string& s) {
    std::wstring w = W(s);
    if (!w.empty()) CharUpperBuffW(&w[0], (DWORD)w.size());
    return U(w);
}

static float dpi_of(HWND h) {
    typedef UINT(WINAPI * Fn)(HWND);
    static Fn fn = (Fn)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    if (fn && h) { UINT d = fn(h); if (d) return d / 96.f; }
    HDC dc = GetDC(nullptr);
    int d = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(nullptr, dc);
    return d / 96.f;
}

static void dark_title(HWND h) {
    HMODULE m = LoadLibraryW(L"dwmapi.dll");
    if (!m) return;
    typedef HRESULT(WINAPI * Fn)(HWND, DWORD, LPCVOID, DWORD);
    Fn f = (Fn)GetProcAddress(m, "DwmSetWindowAttribute");
    if (f) { BOOL on = TRUE; if (FAILED(f(h, 20, &on, sizeof on))) f(h, 19, &on, sizeof on); }
}

static LRESULT CALLBACK main_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    MainWindow* self = (MainWindow*)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (m == WM_NCCREATE) {
        self = (MainWindow*)((CREATESTRUCTW*)l)->lpCreateParams;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)self);
    }
    if (!self) return DefWindowProcW(h, m, w, l);
    return self->proc(h, m, w, l);
}

MainWindow::MainWindow() : core(deck_path()) {}
MainWindow::~MainWindow() {
    if (edit_font_) DeleteObject(edit_font_);
    if (edit_brush_) DeleteObject(edit_brush_);
    delete logo_;
}

// ---------------- olusturma ----------------
bool MainWindow::create() {
    g_app = this;
    ilk_kurulum();

    HINSTANCE hi = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc; memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc; wc.style = CS_DBLCLKS; wc.lpfnWndProc = main_proc; wc.hInstance = hi;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = (HICON)LoadImageW(hi, MAKEINTRESOURCEW(1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
    wc.hIconSm = (HICON)LoadImageW(hi, MAKEINTRESOURCEW(1), IMAGE_ICON, 16, 16, 0);
    wc.hbrBackground = nullptr; wc.lpszClassName = L"PopDeckMain";
    RegisterClassExW(&wc);

    scale = dpi_of(nullptr);
    RECT rc = {0, 0, dp(980), dp(740)};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    int ww = rc.right - rc.left, wh = rc.bottom - rc.top;
    int sx = GetSystemMetrics(SM_CXSCREEN), sy = GetSystemMetrics(SM_CYSCREEN);
    HWND h = CreateWindowExW(0, L"PopDeckMain", L"PopDeck", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, (sx - ww) / 2, std::max(0, (sy - wh) / 2),
                             ww, wh, nullptr, nullptr, hi, this);
    if (!h) return false;
    hwnd = h; g_main_hwnd = h;
    scale = dpi_of(h);
    dark_title(h);
    edit_brush_ = CreateSolidBrush(to_ref(C_YUZEY2));
    HICON ic = (HICON)LoadImageW(hi, MAKEINTRESOURCEW(1), IMAGE_ICON, 96, 96, 0);
    if (ic) { logo_ = Gdiplus::Bitmap::FromHICON(ic); DestroyIcon(ic); }

    oynatici = std::make_unique<Oynatici>(core, [this](const std::string& yol) {
        core.deck.son_ekle(base_name(dir_name(yol)) + "/" + base_name(yol));
    });
    core.post = [](const Cmd& c) { ui_post([c] { if (g_app) g_app->komut_isle(c); }); };
    hook.on_cmd = [](const Cmd& c) { ui_post([c] { if (g_app) g_app->komut_isle(c); }); };
    hook.on_capture = [this](const std::string& tus) { ui_post([this, tus] { yakalama_bitir(tus); }); };
    hook.start();

    font_olustur();
    vars_yukle();
    build_ui();
    ui_kurulu_ = true;
    layout();
    listeyi_yenile();
    tuslari_kaydet();
    ui_hazir_ = true;
    web_yeniden_baslat();
    SetTimer(h, T_OTO, 3000, nullptr);
    ui_after(2000, [this] { onyukle_baslat(); });

    if (!core.ayar.kopya().gizli_basla) { ShowWindow(h, SW_SHOW); UpdateWindow(h); }
    return true;
}

void MainWindow::ilk_kurulum() {
    std::error_code ec;
    fs::create_directories(g_main_dir, ec);
    if (!fs::exists(settings_path(), ec) && klasor_listesi().empty()) {
        fs::create_directories(g_main_dir / "Genel", ec);
        core.ayar.tus_koy("Genel", "f9");
        core.ayar.kaydet();
    }
    core.ayar.yukle();
    core.ayar.klasor_esitle();
    {
        std::lock_guard<std::recursive_mutex> lk(core.ayar.m);
        Genel& g = core.ayar.genel;
        if (g.web_pin_yok) {
            std::string pin;
            for (int i = 0; i < 4; i++) pin += (char)('0' + (rng()() % 10));
            g.web_pin = pin; g.web_pin_yok = false;
        }
        if (g.dil.empty()) g.dil = sistem_dili();
        lang_set(g.dil);
    }
    core.ayar.kaydet();
}

void MainWindow::vars_yukle() {
    Genel g = core.ayar.kopya();
    v_.sure = g.sure; v_.boyut = std::round(g.boyut_orani * 100); v_.fade = g.fade_in; v_.seviye = g.ses_seviyesi;
    v_.gecikme = g.ses_gecikme; v_.onbellek = g.onbellek_mb; v_.maxb = g.max_buyutme; v_.fps = g.fps_siniri; v_.bos = g.bos_sure_dk;
    v_.boz = g.orani_boz; v_.tikla = g.tiklama_gecir; v_.ses = g.ses_aktif; v_.kes = g.ses_gif_ile_kesil; v_.dinle = g.dinleme;
    v_.gizli = g.gizli_basla; v_.web = g.web_aktif; v_.sonuna = g.gif_sonuna_kadar; v_.fwoto = g.fw_otomatik;
    v_.yukle = g.web_yukleme; v_.onyukle = g.on_yukle;
}

void MainWindow::font_olustur() {
    if (edit_font_) DeleteObject(edit_font_);
    edit_font_ = CreateFontW(-dp(15), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
}

HWND MainWindow::make_edit(bool number) {
    DWORD st = WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL | (number ? ES_NUMBER : 0);
    HWND e = CreateWindowExW(0, L"EDIT", L"", st, 0, 0, 10, 10, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
    SendMessageW(e, WM_SETFONT, (WPARAM)edit_font_, TRUE);
    return e;
}

// ---------------- widget uretimi ----------------
static void fmt_num(char* buf, size_t n, double v) { snprintf(buf, n, "%g", v); }

ToggleRow* MainWindow::toggle_row(int page, const std::string& label, bool* v) {
    auto* r = add(new ToggleRow(), true);
    r->label = label; r->v = v; r->changed = [this] { ayar_degisti(); };
    page_widgets_[page].push_back(r);
    return r;
}
SliderRow* MainWindow::slider_row(int page, const std::string& label, double* v, double a, double b, double step, const std::string& unit) {
    auto* r = add(new SliderRow(), true);
    r->label = label; r->v = v; r->a = a; r->b = b; r->step = step; r->unit = unit; r->changed = [this] { ayar_degisti(); };
    page_widgets_[page].push_back(r);
    return r;
}
EditRow* MainWindow::edit_row(int page, const std::string& label, const std::string& init, bool number, float w) {
    auto* r = add(new EditRow(), true);
    r->label = label; r->edit_w = w;
    r->edit = make_edit(number);
    SetWindowTextW(r->edit, W(init).c_str());
    edits_.push_back(r);
    page_widgets_[page].push_back(r);
    return r;
}
KeyRow* MainWindow::key_row(int page, const std::string& label, const std::string& anahtar) {
    auto* r = add(new KeyRow(), true);
    r->label = label;
    r->btn = add(new Button(), true);
    r->btn->arka = C_YUZEY;
    r->btn->cb = [this, anahtar] { ozel_tus_ata(anahtar); };
    page_widgets_[page].push_back(r);
    page_widgets_[page].push_back(r->btn);
    return r;
}
Button* MainWindow::button(int page, const std::string& text, Button::Style st, std::function<void()> cb, bool clipped) {
    auto* b = add(new Button(), clipped);
    b->s = text; b->style = st; b->cb = std::move(cb);
    if (page >= 0) page_widgets_[page].push_back(b);
    return b;
}
Label* MainWindow::label(int page, const std::string& text, float px, bool bold, const Color& col, bool clipped, bool wrap) {
    auto* l = add(new Label(), clipped);
    l->s = text; l->px = px; l->bold = bold; l->col = col; l->wrap = wrap;
    if (page >= 0) page_widgets_[page].push_back(l);
    return l;
}
MainWindow::CardL& MainWindow::card(int page, int kolon, const std::string& baslik) {
    auto* c = add(new Card(), true);
    c->title = baslik;
    page_widgets_[page].push_back(c);
    cards_.push_back(CardL{c, {}, {}, page, kolon});
    return cards_.back();
}

void MainWindow::build_ui() {
    Genel g = core.ayar.kopya();
    cards_.clear(); edits_.clear(); klasor_btn_.clear();
    for (auto& p : page_widgets_) p.clear();
    ed_sure_ = ed_fade_ = ed_port_ = ed_pin_ = nullptr;
    // ----- ust bar
    lbl_title_ = label(-1, "PopDeck", 20, true, C_YAZI, false);
    lbl_sub_ = label(-1, L("GIF • PNG • Ses kumandası"), 12, false, C_SOLUK, false);
    lbl_dinle_ = label(-1, "", 12, true, C_YESIL, false);
    lbl_dil_ = label(-1, L("Dil:"), 12, false, C_SOLUK, false);
    dd_dil_ = add(new DropDown());
    dd_dil_->items = g_lang_names; dd_dil_->sel = lang_index(lang_cur());
    dd_dil_->changed = [this](int i) { dil_secildi(i); };
    tabs_ = add(new TabBar());
    tabs_->titles = {L("Klasörler"), L("Ayarlar"), L("Telefon Kumandası")};
    tabs_->sel = sayfa_;
    tabs_->changed = [this](int i) { sayfa_goster(i); };

    // ----- sayfa 0
    lbl_info_ = label(0, L("Her klasöre bir tuş ata. O tuşa basınca o klasörden rastgele görsel çıkar.\n"
                            "Sesi olan görselle AYNI İSİMDE mp3/wav koy (komik.gif + komik.mp3). Ana klasör: {0}", {path_str(g_main_dir)}),
                      12, false, C_SOLUK, false, true);
    lbl_info_->valign = 0;
    list_ = add(new FolderList());
    page_widgets_[0].push_back(list_);
    list_->empty_text = L("Henüz klasör yok. «Yeni Klasör» ile başla.");
    list_->on_dbl = [this](int) { tus_ata(); };
    struct D { const char* t; void (MainWindow::*f)(); Button::Style s; };
    D d[8] = {{"Yeni Klasör", &MainWindow::yeni_klasor, Button::Primary}, {"Tuş Ata", &MainWindow::tus_ata, Button::Primary},
              {"Dosya Ekle...", &MainWindow::dosya_ekle, Button::Normal}, {"Test Et", &MainWindow::test_et, Button::Normal},
              {"Adını Değiştir", &MainWindow::yeniden_adlandir, Button::Normal}, {"Tuşu Kaldır", &MainWindow::tus_kaldir, Button::Normal},
              {"Klasörü Aç", &MainWindow::klasor_ac_, Button::Normal}, {"Klasörü Sil", &MainWindow::klasor_sil, Button::Danger}};
    for (auto& x : d) {
        auto f = x.f;
        klasor_btn_.push_back(button(0, L(x.t), x.s, [this, f] { (this->*f)(); }, false));
    }

    // ----- sayfa 1
    CardL* c;
    c = &card(1, 0, L("Görünüm"));
    c->rows.push_back(toggle_row(1, L("GIF'i sonuna kadar oynat (süre sınırı yok)"), &v_.sonuna));
    char buf[32];
    fmt_num(buf, sizeof buf, g.sure);
    ed_sure_ = edit_row(1, L("PNG / sabit görsel süresi (sn):"), buf, false, 90); c->rows.push_back(ed_sure_);
    c->rows.push_back(slider_row(1, L("Boyut (% ekran):"), &v_.boyut, 10, 100, 1, "%"));
    fmt_num(buf, sizeof buf, g.fade_in);
    ed_fade_ = edit_row(1, L("Belirme / fade-in (sn):"), buf, false, 90); c->rows.push_back(ed_fade_);
    c->rows.push_back(toggle_row(1, L("Oranı bozup tüm ekranı doldur"), &v_.boz));
    c->rows.push_back(toggle_row(1, L("Tıklamalar arkadaki oyuna geçsin"), &v_.tikla));
    c->rows.push_back(toggle_row(1, L("Program açılınca arayüzü gizle"), &v_.gizli));
    c->hs.assign(c->rows.size(), 44.f);

    c = &card(1, 0, L("Özel Tuşlar"));
    kr_arayuz_ = key_row(1, L("Arayüzü göster / gizle:"), "arayuz_tusu");
    kr_cikis_ = key_row(1, L("Programı tamamen kapat:"), "cikis_tusu");
    kr_kapat_ = key_row(1, L("Ekrandaki GIF'i kapat:"), "kapat_tusu");
    c->rows = {kr_arayuz_, kr_cikis_, kr_kapat_};
    c->hs.assign(3, 44.f);
    ozel_tus_yazilari();

    c = &card(1, 1, L("Ses"));
    c->rows.push_back(toggle_row(1, L("Ses çal"), &v_.ses));
    c->rows.push_back(slider_row(1, L("Ses seviyesi:"), &v_.seviye, 0, 100, 1, "%"));
    c->rows.push_back(slider_row(1, L("Ses senkron gecikmesi (ms):"), &v_.gecikme, 0, 500, 10, " ms"));
    lbl_ses_note_ = label(1, L("Ses görselden erken başlıyorsa gecikmeyi artır."), 12, false, C_SOLUK, true, true);
    lbl_ses_note_->valign = 0;
    c->rows.push_back(lbl_ses_note_);
    c->rows.push_back(toggle_row(1, L("Süre bitince ses de dursun"), &v_.kes));
    c->hs.assign(c->rows.size(), 44.f);
    c->hs[3] = 30.f;

    c = &card(1, 1, L("Performans ve Önbellek"));
    c->rows.push_back(toggle_row(1, L("Önceden yükle (ilk gösterim anında)"), &v_.onyukle));
    c->rows.push_back(slider_row(1, L("Önbellek limiti:"), &v_.onbellek, 200, 3000, 100, " MB"));
    c->rows.push_back(slider_row(1, L("Maksimum büyütme (4x = sınırsız):"), &v_.maxb, 1, 4, 0.5, "x"));
    c->rows.push_back(slider_row(1, L("Akıcılık sınırı (FPS, 60 = sınırsız):"), &v_.fps, 10, 60, 5, ""));
    c->rows.push_back(slider_row(1, L("Boştayken belleği boşalt (dk, 0 = kapalı):"), &v_.bos, 0, 30, 1, ""));
    lbl_onb_ = label(1, "", 12, false, C_SOLUK, true, true);
    lbl_onb_->valign = 0;
    c->rows.push_back(lbl_onb_);
    c->rows.push_back(button(1, L("Önbelleği temizle"), Button::Ghost, [this] { onbellegi_temizle(); }, true));
    ((Button*)c->rows.back())->arka = C_YUZEY;
    c->hs.assign(c->rows.size(), 44.f);
    c->hs[5] = 40.f;
    c->hs[6] = 46.f;

    // ----- sayfa 2 (elle yerlesim)
    c = &card(2, 0, L("Telefon Kumandası (aynı Wi-Fi)"));
    tr_web_ = toggle_row(2, L("Telefon kumandası açık"), &v_.web);
    ed_port_ = edit_row(2, L("Port:"), std::to_string(g.web_port), true, 90);
    ed_pin_ = edit_row(2, L("PIN (boş = şifresiz):"), g.web_pin, false, 110);
    SendMessageW(ed_pin_->edit, EM_SETLIMITTEXT, 12, 0);
    lbl_adres_ = label(2, L("Telefonda açılacak adres:"), 13, false, C_YAZI, true);
    dd_url_ = add(new DropDown(), true);
    page_widgets_[2].push_back(dd_url_);
    dd_url_->changed = [this](int i) { if (i >= 0 && i < (int)dd_url_->items.size()) { secili_url_ = dd_url_->items[(size_t)i]; web_bilgi_guncelle(); } };
    btn_kopyala_ = button(2, L("Kopyala"), Button::Normal, [this] {
        if (dd_url_ && dd_url_->sel >= 0 && dd_url_->sel < (int)dd_url_->items.size()) {
            panoya_kopyala(hwnd, dd_url_->items[(size_t)dd_url_->sel]);
            durum(L("Adres kopyalandı."));
        }
    }, true);
    btn_kopyala_->arka = C_YUZEY;
    btn_fw_ = button(2, L("Güvenlik duvarına izin ver (otomatik)"), Button::Primary, [this] { guvenlik_duvari_ac(); }, true);
    lbl_fw_ = label(2, L("Güvenlik duvarı: kontrol ediliyor..."), 12, true, C_SOLUK, true);
    tr_fwoto_ = toggle_row(2, L("İzin eksikse açılışta otomatik iste"), &v_.fwoto);
    tr_yukle_ = toggle_row(2, L("Telefondan dosya yüklemeye izin ver"), &v_.yukle);
    lbl_note_tel_ = label(2, L("1) Telefon ve bilgisayar AYNI Wi-Fi'ye bağlı olmalı (mobil veri/VPN kapalı).\n"
                                "2) QR kodu okut ya da adresi telefonun tarayıcısına http:// ile yaz.\n"
                                "3) İzin yoksa program açılışta kendisi sorar; ya da düğmeye bas ve onaya Evet de.\n"
                                "4) Adres yanlışsa listeden başka adres seç (QR da değişir)."), 12, false, C_SOLUK, true, true);
    lbl_note_tel_->valign = 0;
    qrv_ = add(new QrView(), true);
    page_widgets_[2].push_back(qrv_);
    lbl_qr_cap_ = label(2, L("Telefon kamerasıyla okut"), 12, false, C_SOLUK, true);
    lbl_qr_cap_->align = 1;

    // ----- alt bar
    tg_dinle_ = add(new Toggle());
    tg_dinle_->v = &v_.dinle;
    tg_dinle_->changed = [this] { dinleme_guncelle(); ayar_degisti(); };
    lbl_dinle_yazi_ = label(-1, L("Tuş dinleme"), 13, false, C_YAZI, false);
    btn_gizle_ = button(-1, L("Arka Plana Gizle"), Button::Normal, [this] { gizle(); }, false);
    btn_kapat_ = button(-1, L("Programı Kapat"), Button::Danger, [this] { tamamen_kapat(); }, false);
    lbl_durum_ = label(-1, L("Hazır."), 12, false, C_SOLUK, false);

    dinleme_guncelle();
    sayfa_goster(sayfa_);
}

void MainWindow::sayfa_goster(int i) {
    sayfa_ = i;
    if (tabs_) tabs_->sel = i;
    for (int p = 0; p < 3; p++)
        for (Widget* w : page_widgets_[p]) w->visible = (p == i);
    layout();
    invalidate();
}

// ---------------- yerlesim ----------------
void MainWindow::on_relayout() { if (ui_kurulu_) layout(); }

void MainWindow::layout() {
    if (!ui_kurulu_ || !hwnd) return;
    float W_ = logical_w(), H_ = logical_h();
    if (W_ < 100 || H_ < 100) return;
    const float M = 24;
    // ust bar
    lbl_title_->r = RectF(M + 62, 16, 300, 32);
    lbl_sub_->r = RectF(M + 62, 48, 400, 20);
    float dx = W_ - M - 150;
    dd_dil_->r = RectF(dx, 26, 150, 34);
    lbl_dil_->r = RectF(dx - 40, 26, 36, 34); lbl_dil_->align = 2;
    lbl_dinle_->r = RectF(dx - 40 - 200, 26, 190, 34); lbl_dinle_->align = 2;
    tabs_->r = RectF(M, 88, W_ - 2 * M, 48);
    // alt bar
    float by = H_ - 90;
    tg_dinle_->r = RectF(M, by + 7, 46, 26);
    lbl_dinle_yazi_->r = RectF(M + 56, by, 200, 40);
    btn_kapat_->r = RectF(W_ - M - 190, by, 190, 40);
    btn_gizle_->r = RectF(W_ - M - 190 - 10 - 190, by, 190, 40);
    lbl_durum_->r = RectF(M + 2, H_ - 36, W_ - 2 * M, 22);
    float y0 = 150, y1 = by - 12;
    viewport = RectF(M, y0, W_ - 2 * M, y1 - y0);

    if (sayfa_ == 0) {
        lbl_info_->r = RectF(M, y0, W_ - 2 * M, 40);
        float bh = 40, gap = 10;
        float gen = 172;
        for (auto* b : klasor_btn_) gen = std::max(gen, text_width(b->s, 13, true) + 44);
        float tw = 4 * gen + 3 * 12, bx = (W_ - tw) / 2;
        float row2 = y1 - bh - 2, row1 = row2 - gap - bh;
        for (int i = 0; i < 8; i++)
            klasor_btn_[(size_t)i]->r = RectF(bx + (float)(i % 4) * (gen + 12), (i < 4 ? row1 : row2), gen, bh);
        list_->r = RectF(M, y0 + 46, W_ - 2 * M, std::max(80.f, row1 - 14 - (y0 + 46)));
        list_->clamp();
    } else if (sayfa_ == 1) {
        float cw = (W_ - 2 * M - 18) / 2;
        float ys[2] = {y0 - scroll_[1], y0 - scroll_[1]};
        for (auto& cl : cards_) {
            if (cl.sayfa != 1) continue;
            float x = M + (float)cl.kolon * (cw + 18), y = ys[cl.kolon];
            float h = 44 + 16;
            for (float v : cl.hs) h += v;
            cl.card->r = RectF(x, y, cw, h);
            float ry = y + 44, rw = cw - 36, rx = x + 18;
            for (size_t i = 0; i < cl.rows.size(); i++) {
                Widget* w = cl.rows[i];
                float rh = cl.hs[i];
                w->r = RectF(rx, ry, rw, rh);
                if (auto* t = dynamic_cast<Label*>(w)) { (void)t; if (w == lbl_onb_) w->r.Height = 36; }
                if (auto* k = dynamic_cast<KeyRow*>(w)) { k->label_w = rw - 210; k->btn->r = RectF(rx + rw - 200, ry + 5, 200, 34); }
                else if (auto* e = dynamic_cast<EditRow*>(w)) e->label_w = rw - e->edit_w - 10;
                else if (auto* s = dynamic_cast<SliderRow*>(w)) s->label_w = std::max(80.f, rw - s->track_w - 80);
                else if (auto* tg = dynamic_cast<ToggleRow*>(w)) tg->label_w = rw - 60;
                else if (auto* bt = dynamic_cast<Button*>(w)) bt->r = RectF(rx + rw - 190, ry + 3, 190, 40);
                ry += rh;
            }
            ys[cl.kolon] = y + h + 16;
        }
        content_h_[1] = std::max(ys[0], ys[1]) + scroll_[1] - y0;
    } else {
        float cw = W_ - 2 * M, y = y0 - scroll_[2];
        float qrw = 220, lw = std::max(260.f, cw - 36 - qrw - 36);
        float x = M + 18, ry = y + 44;
        tr_web_->r = RectF(x, ry, lw, 44); tr_web_->label_w = lw - 60; ry += 44;
        ed_port_->r = RectF(x, ry, lw, 44); ed_port_->label_w = lw - ed_port_->edit_w - 10; ry += 44;
        ed_pin_->r = RectF(x, ry, lw, 44); ed_pin_->label_w = lw - ed_pin_->edit_w - 10; ry += 44;
        lbl_adres_->r = RectF(x, ry + 10, lw, 24); ry += 40;
        dd_url_->r = RectF(x, ry, lw - 120, 36);
        btn_kopyala_->r = RectF(x + lw - 110, ry, 110, 36); ry += 52;
        float fwb = text_width(btn_fw_->s, 13, true) + 44;
        btn_fw_->r = RectF(x, ry, fwb, 40);
        lbl_fw_->r = RectF(x + fwb + 14, ry, std::max(100.f, lw - fwb - 14), 40); ry += 52;
        tr_fwoto_->r = RectF(x, ry, lw, 44); tr_fwoto_->label_w = lw - 60; ry += 44;
        tr_yukle_->r = RectF(x, ry, lw, 44); tr_yukle_->label_w = lw - 60; ry += 44;
        float nh = text_height(lbl_note_tel_->s, 12, false, lw) + 10;
        lbl_note_tel_->r = RectF(x, ry + 8, lw, nh); ry += nh + 12;
        float qx = M + cw - 18 - 200;
        qrv_->r = RectF(qx, y + 54, 200, 200);
        lbl_qr_cap_->r = RectF(qx - 10, y + 262, 220, 22);
        float h = std::max(ry + 16 - y, 54 + 200 + 40 + 16.f);
        for (auto& cl : cards_) if (cl.sayfa == 2) cl.card->r = RectF(M, y, cw, h);
        content_h_[2] = h + scroll_[2] * 0 + 0;
    }
    // kaydirma sinirlari
    if (sayfa_ > 0) {
        float mx = std::max(0.f, content_h_[sayfa_] - (y1 - y0));
        if (scroll_[sayfa_] > mx) { scroll_[sayfa_] = mx; layout(); return; }
    }
    place_edits();
}

void MainWindow::place_edits() {
    for (EditRow* e : edits_) {
        bool on = e->visible && sayfa_ > 0;
        RectF er = e->edit_rect();
        RectF in(er.X + 8, er.Y + 6, er.Width - 16, er.Height - 12);
        if (on && !viewport.Contains(er)) on = false;
        SetWindowPos(e->edit, nullptr, dp(in.X), dp(in.Y), dp(in.Width), dp(in.Height),
                     SWP_NOZORDER | SWP_NOACTIVATE | (on ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
    }
}

void MainWindow::wheel_default(float, float, int d) {
    if (sayfa_ == 0) return;
    float mx = std::max(0.f, content_h_[sayfa_] - viewport.Height);
    scroll_[sayfa_] = std::max(0.f, std::min(mx, scroll_[sayfa_] - d / 120.f * 60.f));
    layout();
    invalidate();
}

void MainWindow::paint_bg(Gdiplus::Graphics& g, float w, float h) {
    Surface::paint_bg(g, w, h);
    if (logo_) g.DrawImage(logo_, Gdiplus::RectF(24, 16, 48, 48));
}

void MainWindow::durum(const std::string& s) {
    if (lbl_durum_) { lbl_durum_->s = s; invalidate(); }
}

int MainWindow::msg(const std::string& baslik, const std::string& metin, UINT flags) {
    return MessageBoxW(hwnd, W(metin).c_str(), W(baslik).c_str(), flags);
}

void MainWindow::dpi_guncelle(float yeni) {
    scale = yeni;
    font_olustur();
    for (EditRow* e : edits_) SendMessageW(e->edit, WM_SETFONT, (WPARAM)edit_font_, TRUE);
    layout();
    invalidate();
}

// ---------------- pencere mesajlari ----------------
LRESULT MainWindow::other(UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_PD_RUN: {
            for (int i = 0; i < 64; i++) {
                std::function<void()> fn;
                { std::lock_guard<std::mutex> lk(g_q_m); if (g_q.empty()) break; fn = std::move(g_q.front()); g_q.pop_front(); }
                if (fn) fn();
                if (kapaniyor_) return 0;
            }
            { std::lock_guard<std::mutex> lk(g_q_m); if (!g_q.empty()) PostMessageW(hwnd, WM_PD_RUN, 0, 0); }
            return 0;
        }
        case WM_TIMER: {
            UINT_PTR id = w;
            if (id >= 100) {
                auto it = g_after.find(id);
                KillTimer(hwnd, id);
                if (it != g_after.end()) { auto fn = std::move(it->second); g_after.erase(it); if (!kapaniyor_) fn(); }
            } else if (id == T_KAYIT) { KillTimer(hwnd, T_KAYIT); kayit_bekliyor_ = false; ayarlari_kaydet(); }
            else if (id == T_OTO) otomatik_yenile();
            return 0;
        }
        case WM_COMMAND:
            if (HIWORD(w) == EN_CHANGE && l) ayar_degisti();
            return 0;
        case WM_CTLCOLOREDIT: {
            HDC dc = (HDC)w;
            SetTextColor(dc, to_ref(C_YAZI)); SetBkColor(dc, to_ref(C_YUZEY2));
            return (LRESULT)edit_brush_;
        }
        case WM_GETMINMAXINFO: {
            MINMAXINFO* mm = (MINMAXINFO*)l;
            RECT rc = {0, 0, dp(900), dp(640)};
            AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
            mm->ptMinTrackSize.x = rc.right - rc.left; mm->ptMinTrackSize.y = rc.bottom - rc.top;
            return 0;
        }
        case WM_DPICHANGED: {
            RECT* r = (RECT*)l;
            SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
            dpi_guncelle(LOWORD(w) / 96.f);
            return 0;
        }
        case WM_DISPLAYCHANGE: if (oynatici) oynatici->ekran_yenile(); return 0;
        case WM_CLOSE: pencere_kapatilirken(); return 0;
        case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, m, w, l);
}

// ---------------- metin giris diyalogu ----------------
struct DlgState { bool done = false, ok = false; HWND edit = nullptr; std::wstring text; HBRUSH bg = nullptr, ebg = nullptr; };

static LRESULT CALLBACK dlg_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    DlgState* st = (DlgState*)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (m == WM_NCCREATE) { st = (DlgState*)((CREATESTRUCTW*)l)->lpCreateParams; SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)st); }
    if (!st) return DefWindowProcW(h, m, w, l);
    switch (m) {
        case WM_COMMAND:
            if (LOWORD(w) == IDOK || LOWORD(w) == IDCANCEL) {
                if (LOWORD(w) == IDOK) {
                    wchar_t buf[1024]; GetWindowTextW(st->edit, buf, 1024); st->text = buf; st->ok = true;
                }
                st->done = true;
            }
            return 0;
        case WM_CLOSE: st->done = true; return 0;
        case WM_CTLCOLORSTATIC: SetTextColor((HDC)w, to_ref(C_YAZI)); SetBkColor((HDC)w, to_ref(C_BG)); return (LRESULT)st->bg;
        case WM_CTLCOLOREDIT: SetTextColor((HDC)w, to_ref(C_YAZI)); SetBkColor((HDC)w, to_ref(C_YUZEY2)); return (LRESULT)st->ebg;
        case WM_ERASEBKGND: { RECT r; GetClientRect(h, &r); FillRect((HDC)w, &r, st->bg); return 1; }
    }
    return DefWindowProcW(h, m, w, l);
}

static void reg_dlg_class(const wchar_t* name, WNDPROC p) {
    WNDCLASSEXW wc; memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc; wc.lpfnWndProc = p; wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.lpszClassName = name;
    wc.hIcon = (HICON)LoadImageW(wc.hInstance, MAKEINTRESOURCEW(1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
    RegisterClassExW(&wc);
}

bool MainWindow::metin_sor(const std::string& baslik, const std::string& mesaj, const std::string& ilk, std::string& sonuc) {
    static bool reg = false;
    if (!reg) { reg_dlg_class(L"PopDeckDlg", dlg_proc); reg = true; }
    DlgState st;
    st.bg = CreateSolidBrush(to_ref(C_BG)); st.ebg = CreateSolidBrush(to_ref(C_YUZEY2));
    int W_ = dp(470), H_ = dp(230);
    RECT mr; GetWindowRect(hwnd, &mr);
    int x = mr.left + dp(120), y = mr.top + dp(120);
    HINSTANCE hi = GetModuleHandleW(nullptr);
    HWND d = CreateWindowExW(WS_EX_DLGMODALFRAME, L"PopDeckDlg", W(baslik).c_str(), WS_POPUP | WS_CAPTION | WS_SYSMENU, x, y, W_, H_, hwnd, nullptr, hi, &st);
    dark_title(d);
    RECT cr; GetClientRect(d, &cr);
    int cw = cr.right;
    auto mk = [&](const wchar_t* cls, const std::wstring& t, DWORD style, int px, int py, int pw, int ph, int id) {
        HWND c = CreateWindowExW(0, cls, t.c_str(), WS_CHILD | WS_VISIBLE | style, dp((float)px), dp((float)py), dp((float)pw), dp((float)ph), d, (HMENU)(INT_PTR)id, hi, nullptr);
        SendMessageW(c, WM_SETFONT, (WPARAM)edit_font_, TRUE);
        return c;
    };
    (void)cw;
    mk(L"STATIC", W(mesaj), 0, 22, 18, 420, 60, 0);
    st.edit = mk(L"EDIT", W(ilk), WS_TABSTOP | ES_AUTOHSCROLL | WS_BORDER, 22, 84, 420, 28, 100);
    mk(L"BUTTON", W(L("İptal")), WS_TABSTOP | BS_PUSHBUTTON, 220, 140, 105, 34, IDCANCEL);
    mk(L"BUTTON", W(L("Tamam")), WS_TABSTOP | BS_DEFPUSHBUTTON, 337, 140, 105, 34, IDOK);
    // istemci alani ~ 470x230 mantiksal olacak sekilde pencereyi ayarla
    RECT want = {0, 0, dp(464), dp(190)};
    AdjustWindowRectEx(&want, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME);
    SetWindowPos(d, nullptr, x, y, want.right - want.left, want.bottom - want.top, SWP_NOZORDER);
    EnableWindow(hwnd, FALSE);
    ShowWindow(d, SW_SHOW);
    SetForegroundWindow(d);
    SetFocus(st.edit);
    SendMessageW(st.edit, EM_SETSEL, 0, -1);
    MSG mg;
    while (!st.done && GetMessageW(&mg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(d, &mg)) { TranslateMessage(&mg); DispatchMessageW(&mg); }
    }
    EnableWindow(hwnd, TRUE);
    DestroyWindow(d);
    SetForegroundWindow(hwnd);
    DeleteObject(st.bg); DeleteObject(st.ebg);
    if (!st.ok) return false;
    sonuc = U(st.text);
    return true;
}

// ---------------- tus yakalama penceresi ----------------
static HBRUSH g_cap_bg = nullptr;
LRESULT CALLBACK cap_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_COMMAND: if (LOWORD(w) == IDCANCEL && g_app) g_app->yakalama_bitir(""); return 0;
        case WM_CLOSE: if (g_app) g_app->yakalama_bitir(""); return 0;
        case WM_CTLCOLORSTATIC: SetTextColor((HDC)w, to_ref(C_YAZI)); SetBkColor((HDC)w, to_ref(C_BG)); return (LRESULT)g_cap_bg;
        case WM_ERASEBKGND: { RECT r; GetClientRect(h, &r); FillRect((HDC)w, &r, g_cap_bg); return 1; }
    }
    return DefWindowProcW(h, m, w, l);
}

void MainWindow::tus_yakala(const std::string& mesaj, std::function<void(const std::string&)> sonuc) {
    if (yakalama_) return;
    static bool reg = false;
    if (!reg) { reg_dlg_class(L"PopDeckCap", cap_proc); g_cap_bg = CreateSolidBrush(to_ref(C_BG)); reg = true; }
    yakalama_ = true;
    cap_fn_ = std::move(sonuc);
    HINSTANCE hi = GetModuleHandleW(nullptr);
    RECT mr; GetWindowRect(hwnd, &mr);
    DWORD st = WS_POPUP | WS_CAPTION | WS_SYSMENU;
    RECT want = {0, 0, dp(430), dp(210)};
    AdjustWindowRectEx(&want, st, FALSE, WS_EX_TOPMOST);
    cap_hwnd_ = CreateWindowExW(WS_EX_TOPMOST, L"PopDeckCap", W(L("Tuş ata")).c_str(), st, mr.left + dp(150), mr.top + dp(120),
                                want.right - want.left, want.bottom - want.top, hwnd, nullptr, hi, nullptr);
    dark_title(cap_hwnd_);
    std::string txt = mesaj + "\n\n" + L("Şimdi atamak istediğin tuşa (ya da CTRL+G gibi bir kombinasyona) bas.\nESC = iptal");
    HWND s = CreateWindowExW(0, L"STATIC", W(txt).c_str(), WS_CHILD | WS_VISIBLE | SS_CENTER, dp(20), dp(20), dp(390), dp(110), cap_hwnd_, nullptr, hi, nullptr);
    SendMessageW(s, WM_SETFONT, (WPARAM)edit_font_, TRUE);
    HWND b = CreateWindowExW(0, L"BUTTON", W(L("İptal")).c_str(), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, dp(150), dp(150), dp(130), dp(36), cap_hwnd_, (HMENU)(INT_PTR)IDCANCEL, hi, nullptr);
    SendMessageW(b, WM_SETFONT, (WPARAM)edit_font_, TRUE);
    ShowWindow(cap_hwnd_, SW_SHOW);
    hook.begin_capture();
}

void MainWindow::yakalama_bitir(const std::string& tus) {
    if (!yakalama_) return;
    yakalama_ = false;
    hook.end_capture();
    if (cap_hwnd_) { DestroyWindow(cap_hwnd_); cap_hwnd_ = nullptr; }
    auto fn = std::move(cap_fn_);
    cap_fn_ = nullptr;
    if (!tus.empty() && tus != "esc" && fn) fn(tus);
}
