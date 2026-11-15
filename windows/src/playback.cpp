#include "playback.hpp"
#include "lang.hpp"
#include "winsys.hpp"
#include "uipost.hpp"

Oynatici::Oynatici(Cekirdek& c, std::function<void(const std::string&)> on_oynat) : c_(c), on_oynat_(on_oynat) {
    ekran_yenile();
    son_kullanim_ = now_s();
}

void Oynatici::ekran_yenile() {
    ekran_w_ = GetSystemMetrics(SM_CXSCREEN);
    ekran_h_ = GetSystemMetrics(SM_CYSCREEN);
}

std::string Oynatici::anahtar_yap(const std::string& yol, const Genel& g) const {
    std::error_code ec;
    auto ft = fs::last_write_time(path_u8(yol), ec);
    long long mt = ec ? 0 : (long long)ft.time_since_epoch().count();
    return yol + "|" + std::to_string(mt) + "|" + std::to_string(g.fps_siniri);
}

size_t Oynatici::limit_bayt() const { return (size_t)std::max(1, c_.ayar.kopya().onbellek_mb) * 1024 * 1024; }
size_t Oynatici::toplam_bayt() const { size_t t = 0; for (auto& g : onbellek_) t += g.veri->bytes(); return t; }

Oynatici::Giris* Oynatici::bul(const std::string& anahtar) {
    for (auto& g : onbellek_) if (g.anahtar == anahtar) return &g;
    return nullptr;
}

bool Oynatici::onbellege_ekle(const std::string& anahtar, const std::string& yol, std::shared_ptr<ImgData> veri, bool onyuk) {
    size_t limit = limit_bayt();
    if (veri->bytes() > limit) return false;
    if (onyuk && toplam_bayt() + veri->bytes() > limit) return false;
    for (size_t i = 0; i < onbellek_.size(); i++) if (onbellek_[i].anahtar == anahtar) { onbellek_.erase(onbellek_.begin() + (long)i); break; }
    onbellek_.push_back({anahtar, yol, veri});
    while (onbellek_.size() > 1 && toplam_bayt() > limit) onbellek_.erase(onbellek_.begin());     // en eskiyi at
    return true;
}

void Oynatici::tetikle(const std::string& klasor, const std::string& yol_in, bool kes) {
    if (c_.mesgul) {
        if (kes && oturum_) kapat(oturum_->id);
        else return;
    }
    iptal_ = false;
    std::vector<std::string> gorseller, sesler;
    klasor_dosyalari(klasor, gorseller, sesler);
    if (gorseller.empty()) { uyari_goster(L("Lütfen '{0}' klasörüne bir görsel yükleyin!", {klasor})); return; }
    Genel g = c_.ayar.kopya();
    std::string yol = yol_in;
    if (!yol.empty() && std::find(gorseller.begin(), gorseller.end(), yol) != gorseller.end()) RASTGELE.bildir(klasor, yol);
    else yol = RASTGELE.sec(klasor, gorseller);                     // akilli rastgele secim
    std::string ses = g.ses_aktif ? ses_bul(yol, gorseller, sesler) : "";
    son_kullanim_ = now_s();
    std::string anahtar = anahtar_yap(yol, g);
    if (Giris* gi = bul(anahtar)) {                                 // hazirsa aninda ac
        Giris kopya = *gi;
        onbellek_.erase(onbellek_.begin() + (gi - onbellek_.data()));
        onbellek_.push_back(kopya);                                 // en yeniye tasi
        baslat(kopya.veri, ses, yol);
    } else {
        c_.mesgul = true;
        isler_.erase(anahtar);
        is_baslat(anahtar, yol, ses, false);
    }
}

void Oynatici::is_baslat(const std::string& anahtar, const std::string& yol, const std::string& ses, bool onyuk) {
    auto is = std::make_shared<Is>();
    is->anahtar = anahtar; is->yol = yol; is->ses = ses; is->onyuk = onyuk;
    isler_[anahtar] = is;
    int fps = c_.ayar.kopya().fps_siniri;
    if (onyuk) onyuk_is_ = is;
    std::thread([this, is, fps] {
        std::shared_ptr<ImgData> d;
        std::string err;
        bool ok = false;
        try { ok = load_image(is->yol, fps, d, &err, &is->iptal); } catch (const std::exception& e) { err = e.what(); }
        ui_post([this, is, d, ok, err] { is_bitti(is, ok ? d : nullptr, ok ? "" : (err.empty() ? "hata" : err)); });
    }).detach();
}

void Oynatici::onyukle_tetikle(const std::vector<std::string>& yollar) {
    Genel g = c_.ayar.kopya();
    if (onyuk_dur_ || !g.on_yukle) return;
    for (auto& yol : yollar) {
        std::error_code ec;
        if (!fs::exists(path_u8(yol), ec)) continue;
        std::string a = anahtar_yap(yol, g);
        if (bul(a) || sigmayanlar_.count(a) || isler_.count(a) || onyuk_bekleyen_.count(a)) continue;
        onyuk_kuyrugu_.push_back({a, yol});
        onyuk_bekleyen_.insert(a);
    }
    onyuk_ilerlet();
}

void Oynatici::onyuk_ilerlet() {
    while (!onyuk_is_ && !onyuk_kuyrugu_.empty() && !onyuk_dur_) {
        auto p = onyuk_kuyrugu_.front(); onyuk_kuyrugu_.pop_front();
        onyuk_bekleyen_.erase(p.first);
        if (bul(p.first) || isler_.count(p.first)) continue;
        is_baslat(p.first, p.second, "", true);
    }
    if (onyuk_dur_) { onyuk_kuyrugu_.clear(); onyuk_bekleyen_.clear(); }
}

void Oynatici::is_bitti(std::shared_ptr<Is> is, std::shared_ptr<ImgData> veri, const std::string& hata) {
    auto it = isler_.find(is->anahtar);
    if (it != isler_.end() && it->second == is) isler_.erase(it);
    if (is->onyuk) {                                                // ----- on yukleme isi -----
        if (onyuk_is_ == is) onyuk_is_.reset();
        if (veri && !is->iptal) {
            if (!onbellege_ekle(is->anahtar, is->yol, veri, true)) { sigmayanlar_.insert(is->anahtar); onyuk_dur_ = true; }
        } else if (!hata.empty() && !is->iptal) {
            log_line("On yukleme atlandi: " + is->yol + " " + hata);
            sigmayanlar_.insert(is->anahtar);
        }
        onyuk_ilerlet();
        return;
    }
    if (!veri) {                                                    // kullanicinin tetikledigi is
        log_line("Gorsel acilamadi: " + is->yol + " " + hata);
        c_.mesgul = false;
        return;
    }
    onbellege_ekle(is->anahtar, is->yol, veri, false);              // sigmazsa yine de bu sefer oynatilir
    if (iptal_) { iptal_ = false; c_.mesgul = false; return; }
    baslat(veri, is->ses, is->yol);
}

void Oynatici::baslat(std::shared_ptr<ImgData> veri, const std::string& ses, const std::string& yol) {
    Genel g = c_.ayar.kopya();
    c_.mesgul = true;
    son_kullanim_ = now_s();
    std::string alias = (!ses.empty() && g.ses_aktif) ? SES.hazirla(ses, g.ses_seviyesi) : "";   // sesi ONCEDEN hazirla
    auto o = std::make_unique<Oturum>();
    o->id = ++sayac_;
    o->ses = alias;
    unsigned id = o->id;
    PlayParams p;
    p.img = veri;
    p.sw = ekran_w_; p.sh = ekran_h_;
    p.oran = g.boyut_orani; p.maxb = g.max_buyutme; p.fade_in = g.fade_in; p.sure_s = g.sure;
    p.boz = g.orani_boz; p.click_through = g.tiklama_gecir;
    p.tek_tur = g.gif_sonuna_kadar && veri->frames.size() > 1;
    int gecikme = std::max(0, g.ses_gecikme);
    p.on_first = [this, id, alias, gecikme] {
        ui_post([this, id, alias, gecikme] {
            if (!oturum_ || oturum_->id != id || alias.empty()) return;
            if (gecikme > 0) ui_after(gecikme, [this, id, alias] { if (oturum_ && oturum_->id == id) SES.oynat(alias); });
            else SES.oynat(alias);
        });
    };
    p.on_done = [this, id] { ui_post([this, id] { kapat(id); }); };
    oturum_ = std::move(o);
    c_.oturum_var = true;
    oturum_->player.reset(new Player(p));
    if (on_oynat_ && !yol.empty()) { try { on_oynat_(yol); } catch (...) {} }
}

void Oynatici::durdur() {
    if (oturum_) {
        std::string ses = oturum_->ses;
        kapat(oturum_->id);
        SES.durdur(ses);
    } else if (c_.mesgul) iptal_ = true;                            // hala yukleniyor: hazir olunca gosterme
}

void Oynatici::kapat(unsigned id) {
    if (!oturum_ || oturum_->id != id) return;
    std::unique_ptr<Oturum> o = std::move(oturum_);
    c_.oturum_var = false;
    c_.mesgul = false;
    if (c_.ayar.kopya().ses_gif_ile_kesil) SES.durdur(o->ses);
    o.reset();                                                      // Player: pencereyi kapatir, thread'i bekler
    son_kullanim_ = now_s();
}

void Oynatici::eskileri_at(const std::set<std::string>& gecerli) {
    onbellek_.erase(std::remove_if(onbellek_.begin(), onbellek_.end(), [&](const Giris& g) { return !gecerli.count(g.yol); }), onbellek_.end());
}

void Oynatici::temizle() {
    onbellek_.clear();
    for (auto& kv : isler_) if (kv.second->onyuk) kv.second->iptal = true;
    onyuk_kuyrugu_.clear(); onyuk_bekleyen_.clear(); sigmayanlar_.clear();
    onyuk_dur_ = false;
}

bool Oynatici::bosta_mi(int dakika) const {
    return !c_.mesgul && isler_.empty() && !onbellek_.empty() && now_s() - son_kullanim_ > dakika * 60.0;
}
void Oynatici::bosalt() { temizle(); onyuk_dur_ = true; }
void Oynatici::limit_degisti() {
    onyuk_dur_ = false;
    sigmayanlar_.clear();
    size_t limit = limit_bayt();
    while (!onbellek_.empty() && toplam_bayt() > limit) onbellek_.erase(onbellek_.begin());
}
void Oynatici::ozet(size_t toplam, int& mb, int& limit_mb, int& hazir) const {
    (void)toplam;
    mb = (int)(toplam_bayt() / 1048576.0 + 0.5);
    limit_mb = (int)(limit_bayt() / 1048576.0 + 0.5);
    hazir = (int)onbellek_.size();
}

// ---------------- uyari penceresi ----------------
static std::wstring g_uyari_metin;
static LRESULT CALLBACK uyari_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_PAINT) {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        RECT r; GetClientRect(h, &r);
        HBRUSH b = CreateSolidBrush(RGB(30, 30, 30)); FillRect(dc, &r, b); DeleteObject(b);
        HFONT f = CreateFontW(-30, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        HGDIOBJ o = SelectObject(dc, f);
        SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(255, 77, 77));
        DrawTextW(dc, g_uyari_metin.c_str(), -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, o); DeleteObject(f);
        EndPaint(h, &ps);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

void Oynatici::uyari_goster(const std::string& mesaj) {
    log_line(mesaj);
    if (uyari_) return;
    static bool reg = false;
    if (!reg) {
        reg = true;
        WNDCLASSEXW wc; memset(&wc, 0, sizeof wc);
        wc.cbSize = sizeof wc; wc.lpfnWndProc = uyari_proc; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"PopDeckUyari";
        RegisterClassExW(&wc);
    }
    g_uyari_metin = W(mesaj);
    HDC dc = GetDC(nullptr);
    HFONT f = CreateFontW(-30, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HGDIOBJ o = SelectObject(dc, f);
    RECT tr = {0, 0, 0, 0};
    DrawTextW(dc, g_uyari_metin.c_str(), -1, &tr, DT_CALCRECT | DT_SINGLELINE);
    SelectObject(dc, o); DeleteObject(f); ReleaseDC(nullptr, dc);
    int w = (tr.right - tr.left) + 60, h = (tr.bottom - tr.top) + 40;
    uyari_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"PopDeckUyari", L"", WS_POPUP,
                             (ekran_w_ - w) / 2, (ekran_h_ - h) / 2, w, h, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!uyari_) return;
    ShowWindow(uyari_, SW_SHOWNOACTIVATE);
    UpdateWindow(uyari_);
    ui_after(3000, [this] { if (uyari_) { DestroyWindow(uyari_); uyari_ = nullptr; } });
}
