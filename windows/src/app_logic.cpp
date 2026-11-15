// MainWindow: ayarlar, klasor islemleri, tuslar, telefon kumandasi, pencere yonetimi
#include "app.hpp"
#include <cmath>
#include <cstring>

using namespace ui;

static std::string upper_u8(const std::string& s) {
    std::wstring w = W(s);
    if (!w.empty()) CharUpperBuffW(&w[0], (DWORD)w.size());
    return U(w);
}
static std::string edit_text(EditRow* e) {
    if (!e || !e->edit) return "";
    int n = GetWindowTextLengthW(e->edit);
    std::wstring w((size_t)n + 1, L'\0');
    GetWindowTextW(e->edit, &w[0], n + 1);
    w.resize((size_t)n);
    return U(w);
}
static bool parse_num(std::string s, double& out) {
    for (char& c : s) if (c == ',') c = '.';
    s = trim(s);
    if (s.empty()) return false;
    char* end = nullptr;
    double v = strtod(s.c_str(), &end);
    if (end == s.c_str() || *end) return false;
    out = v;
    return true;
}
static double clampd(double v, double a, double b) { return std::max(a, std::min(b, v)); }

// ---------------- ayarlar ----------------
void MainWindow::ayar_degisti() {
    if (!ui_hazir_) return;
    SetTimer(hwnd, 1, 300, nullptr);
    kayit_bekliyor_ = true;
}

void MainWindow::dinleme_guncelle() {
    if (!lbl_dinle_) return;
    lbl_dinle_->s = "●  " + (v_.dinle ? L("Dinleme açık") : L("Dinleme kapalı"));
    lbl_dinle_->col = v_.dinle ? C_YESIL : C_KIRMIZI;
    invalidate();
}

void MainWindow::ozel_tus_yazilari() {
    Genel g = core.ayar.kopya();
    std::string deg = "  " + L("(değiştir)");
    if (kr_arayuz_) kr_arayuz_->btn->s = upper_u8(g.arayuz_tusu) + deg;
    if (kr_cikis_) kr_cikis_->btn->s = upper_u8(g.cikis_tusu) + deg;
    if (kr_kapat_) kr_kapat_->btn->s = upper_u8(g.kapat_tusu) + deg;
    invalidate();
}

void MainWindow::ayarlari_kaydet() {
    kayit_bekliyor_ = false;
    bool web_degisti, onb_degisti, onb_temizle;
    {
        std::lock_guard<std::recursive_mutex> lk(core.ayar.m);
        Genel& g = core.ayar.genel;
        auto onb = [&g] { return std::make_tuple(g.boyut_orani, g.orani_boz, g.max_buyutme, g.fps_siniri, g.onbellek_mb, g.on_yukle); };
        auto onb4 = [&g] { return std::make_tuple(g.boyut_orani, g.orani_boz, g.max_buyutme, g.fps_siniri); };
        double d;
        if (parse_num(edit_text(ed_sure_), d)) g.sure = std::max(0.5, d); else g.sure = std::max(0.5, g.sure);
        g.boyut_orani = clampd(std::round(v_.boyut) / 100.0, 0.1, 1.0);
        if (parse_num(edit_text(ed_fade_), d)) g.fade_in = std::max(0.0, d);
        g.orani_boz = v_.boz; g.tiklama_gecir = v_.tikla; g.ses_aktif = v_.ses;
        g.ses_seviyesi = (int)clampd(std::round(v_.seviye), 0, 100);
        g.ses_gecikme = (int)clampd(std::round(v_.gecikme), 0, 500);
        auto eski = onb(); auto eski4 = onb4();
        g.max_buyutme = clampd(v_.maxb, 1.0, 4.0);
        g.fps_siniri = (int)clampd(std::round(v_.fps), 10, 60);
        g.bos_sure_dk = (int)clampd(std::round(v_.bos), 0, 30);
        g.on_yukle = v_.onyukle;
        g.onbellek_mb = (int)clampd(std::round(v_.onbellek), 200, 3000);
        g.ses_gif_ile_kesil = v_.kes; g.dinleme = v_.dinle; g.gizli_basla = v_.gizli;
        std::pair<bool, int> eski_web(g.web_aktif, g.web_port);
        g.web_aktif = v_.web;
        double p;
        if (parse_num(edit_text(ed_port_), p) && p >= 1024 && p <= 65535) g.web_port = (int)p;
        g.web_pin = utf8_truncate(trim(edit_text(ed_pin_)), 12);
        g.web_pin_yok = false;
        g.web_yukleme = v_.yukle; g.gif_sonuna_kadar = v_.sonuna; g.fw_otomatik = v_.fwoto;
        core.ayar.kaydet();
        web_degisti = eski_web != std::make_pair(g.web_aktif, g.web_port);
        onb_degisti = onb() != eski;
        onb_temizle = onb4() != eski4;
    }
    durum(L("Ayarlar kaydedildi."));
    if (web_degisti) web_yeniden_baslat(); else web_bilgi_guncelle();
    if (onb_degisti) {
        bellek_bosaltildi_ = false;
        if (onb_temizle) oynatici->temizle();
        oynatici->limit_degisti();
        ui_after(600, [this] { onyukle_baslat(); });
        onbellek_etiketi();
    }
}

// ---------------- klasor listesi ----------------
void MainWindow::listeyi_yenile(const std::string& sec) {
    if (core.ayar.klasor_esitle()) {
        core.ayar.kaydet();
        if (ui_hazir_) tuslari_kaydet();
    }
    std::string hedef = sec;
    if (hedef.empty() && list_->sel >= 0 && list_->sel < (int)list_->items.size()) hedef = list_->items[(size_t)list_->sel].ad;
    list_->items.clear();
    int idx = -1;
    for (auto& kv : core.ayar.tuslar_kopya()) {
        std::vector<std::string> g, s;
        klasor_dosyalari(kv.first, g, s);
        FolderList::Item it;
        it.ad = kv.first;
        it.atanmis = !kv.second.empty();
        it.tus = it.atanmis ? upper_u8(kv.second) : L("— atanmadı —");
        it.icerik = L("{0} görsel, {1} ses", {S((long long)g.size()), S((long long)s.size())});
        if (kv.first == hedef) idx = (int)list_->items.size();
        list_->items.push_back(it);
    }
    list_->sel = idx;
    list_->clamp();
    invalidate();
}

std::string MainWindow::secili(bool sor) {
    if (list_->sel < 0 || list_->sel >= (int)list_->items.size()) {
        if (sor) msg(L("Klasör seç"), L("Önce listeden bir klasör seç."), MB_OK | MB_ICONINFORMATION);
        return "";
    }
    return list_->items[(size_t)list_->sel].ad;
}

bool MainWindow::ad_dogrula(std::string& ad, const std::string& eski) {
    ad = trim(ad);
    if (ad.empty()) return false;
    if (name_has_invalid(ad) || starts_with(ad, ".") || starts_with(ad, "_") || ends_with(ad, ".")) {
        msg(L("Geçersiz ad"), L("Klasör adı geçersiz. Şu karakterler olamaz: \\ / : * ? < > | ve ad '.' ya da '_' ile başlayamaz."), MB_OK | MB_ICONWARNING);
        return false;
    }
    if (lower_u8(ad) != lower_u8(eski)) {
        for (auto& kv : core.ayar.tuslar_kopya())
            if (lower_u8(kv.first) == lower_u8(ad)) { msg(L("Zaten var"), L("'{0}' adlı bir klasör zaten var.", {ad}), MB_OK | MB_ICONWARNING); return false; }
    }
    return true;
}

void MainWindow::yeni_klasor() {
    std::string ad;
    if (!metin_sor(L("Yeni Klasör"), L("Klasör adı (örn: Komik, Alkış, Korku):"), "", ad)) return;
    if (!ad_dogrula(ad, "")) return;
    std::error_code ec;
    if (!fs::create_directories(g_main_dir / path_u8(ad), ec) && ec) {
        msg(L("Hata"), L("Klasör oluşturulamadı:\n{0}", {ec.message()}), MB_OK | MB_ICONERROR);
        return;
    }
    core.ayar.tus_koy(ad, "");
    core.ayar.kaydet();
    listeyi_yenile(ad);
    durum(L("'{0}' oluşturuldu.", {ad}));
    if (msg(L("Tuş ata"), L("'{0}' için şimdi tuş atansın mı?", {ad}), MB_YESNO | MB_ICONQUESTION) == IDYES) tus_ata();
}

void MainWindow::yeniden_adlandir() {
    std::string eski = secili();
    if (eski.empty()) return;
    std::string yeni;
    if (!metin_sor(L("Adını Değiştir"), L("Yeni ad:"), eski, yeni)) return;
    if (!ad_dogrula(yeni, eski) || yeni == eski) return;
    std::error_code ec;
    fs::rename(g_main_dir / path_u8(eski), g_main_dir / path_u8(yeni), ec);
    if (ec) { msg(L("Hata"), L("Ad değiştirilemedi:\n{0}", {ec.message()}), MB_OK | MB_ICONERROR); return; }
    core.deck.klasor_adi_degisti(eski, yeni);
    {
        std::lock_guard<std::recursive_mutex> lk(core.ayar.m);
        for (auto& kv : core.ayar.tuslar) if (kv.first == eski) kv.first = yeni;
        core.ayar.kaydet();
    }
    listeyi_yenile(yeni);
    tuslari_kaydet();
}

void MainWindow::klasor_sil() {
    std::string ad = secili();
    if (ad.empty()) return;
    std::vector<std::string> g, s;
    klasor_dosyalari(ad, g, s);
    if (msg(L("Klasörü sil"), L("'{0}' klasörü ve içindeki TÜM dosyalar ({1} görsel, {2} ses) kalıcı olarak silinecek.\n\nEmin misin?",
                                 {ad, S((long long)g.size()), S((long long)s.size())}), MB_YESNO | MB_ICONWARNING) != IDYES) return;
    std::error_code ec;
    fs::remove_all(g_main_dir / path_u8(ad), ec);
    if (ec) { msg(L("Hata"), L("Silinemedi:\n{0}", {ec.message()}), MB_OK | MB_ICONERROR); return; }
    {
        std::lock_guard<std::recursive_mutex> lk(core.ayar.m);
        auto& t = core.ayar.tuslar;
        t.erase(std::remove_if(t.begin(), t.end(), [&](const std::pair<std::string, std::string>& kv) { return kv.first == ad; }), t.end());
        core.ayar.kaydet();
    }
    list_->sel = -1;
    listeyi_yenile();
    tuslari_kaydet();
    durum(L("'{0}' silindi.", {ad}));
}

void MainWindow::klasor_ac_() {
    std::string ad = secili();
    if (ad.empty()) return;
    if (!klasor_ac(path_str(g_main_dir / path_u8(ad)))) msg(L("Hata"), L("Klasör açılamadı:\n{0}", {ad}), MB_OK | MB_ICONERROR);
}

void MainWindow::dosya_ekle() {
    std::string ad = secili();
    if (ad.empty()) return;
    std::string filtre;
    for (auto& e : g_image_exts) filtre += "*" + e + ";";
    filtre += "*.mp3;*.wav";
    auto dosyalar = dosya_sec(hwnd, L("'{0}' klasörüne eklenecek dosyaları seç", {ad}), L("Görsel ve ses dosyaları"), filtre);
    int sayac = 0;
    for (auto& d : dosyalar) {
        fs::path hedef = g_main_dir / path_u8(ad) / path_u8(base_name(d));
        std::error_code ec;
        if (fs::exists(hedef, ec)) {
            fs::path taban = hedef.parent_path() / hedef.stem();
            std::string uz = path_str(hedef.extension());
            int i = 2;
            fs::path y;
            do { y = path_u8(path_str(taban) + "_" + std::to_string(i++) + uz); } while (fs::exists(y, ec));
            hedef = y;
        }
        fs::copy_file(path_u8(d), hedef, ec);
        if (!ec) sayac++; else log_line("Kopyalanamadi: " + d + " " + ec.message());
    }
    listeyi_yenile();
    durum(L("{0} dosya '{1}' klasörüne eklendi.", {S(sayac), ad}));
}

void MainWindow::test_et() {
    std::string ad = secili();
    if (!ad.empty()) oynatici->tetikle(ad);
}

// ---------------- tus atama ----------------
std::string MainWindow::tus_cakisma(const std::string& tus, const std::string& haric_tur, const std::string& haric_ad) {
    std::string t = lower_u8(tus);
    for (auto& kv : core.ayar.tuslar_kopya())
        if (!(haric_tur == "klasor" && kv.first == haric_ad) && !kv.second.empty() && lower_u8(kv.second) == t)
            return L("'{0}' klasörü", {kv.first});
    Genel g = core.ayar.kopya();
    struct { const char* ad; const std::string* v; } oz[3] = {{"arayuz_tusu", &g.arayuz_tusu}, {"cikis_tusu", &g.cikis_tusu}, {"kapat_tusu", &g.kapat_tusu}};
    for (auto& o : oz)
        if (!(haric_tur == "ozel" && haric_ad == o.ad) && !o.v->empty() && lower_u8(*o.v) == t) return L("özel tuşlar");
    return "";
}

void MainWindow::tus_ata() {
    std::string ad = secili();
    if (ad.empty()) return;
    tus_yakala(L("'{0}' klasörü için tuş seç", {ad}), [this, ad](const std::string& tus) {
        std::string k = tus_cakisma(tus, "klasor", ad);
        if (!k.empty()) { msg(L("Tuş kullanımda"), L("'{0}' zaten {1} için atanmış.", {upper_u8(tus), k}), MB_OK | MB_ICONWARNING); return; }
        core.ayar.tus_koy(ad, tus);
        core.ayar.kaydet();
        listeyi_yenile(ad);
        tuslari_kaydet();
        durum("'" + ad + "' → " + upper_u8(tus));
    });
}

void MainWindow::tus_kaldir() {
    std::string ad = secili();
    if (ad.empty()) return;
    core.ayar.tus_koy(ad, "");
    core.ayar.kaydet();
    listeyi_yenile(ad);
    tuslari_kaydet();
}

void MainWindow::ozel_tus_ata(const std::string& anahtar) {
    tus_yakala(L("Yeni tuşu seç"), [this, anahtar](const std::string& tus) {
        std::string k = tus_cakisma(tus, "ozel", anahtar);
        if (!k.empty()) { msg(L("Tuş kullanımda"), L("'{0}' zaten {1} için atanmış.", {upper_u8(tus), k}), MB_OK | MB_ICONWARNING); return; }
        {
            std::lock_guard<std::recursive_mutex> lk(core.ayar.m);
            if (anahtar == "arayuz_tusu") core.ayar.genel.arayuz_tusu = tus;
            else if (anahtar == "cikis_tusu") core.ayar.genel.cikis_tusu = tus;
            else if (anahtar == "kapat_tusu") core.ayar.genel.kapat_tusu = tus;
            core.ayar.kaydet();
        }
        ozel_tus_yazilari();
        tuslari_kaydet();
    });
}

void MainWindow::tuslari_kaydet() {
    std::vector<KeyHook::Binding> b;
    std::vector<std::string> hatalar;
    auto ekle = [&](const std::string& tus, const Cmd& c) {
        if (tus.empty()) return;
        HotKey hk = parse_hotkey(tus);
        if (!hk.ok) { hatalar.push_back(tus); return; }
        b.push_back({hk, c});
    };
    for (auto& kv : core.ayar.tuslar_kopya()) ekle(kv.second, Cmd{"tetik", kv.first, "", false});
    Genel g = core.ayar.kopya();
    ekle(g.arayuz_tusu, Cmd{"arayuz", "", "", false});
    ekle(g.cikis_tusu, Cmd{"cik", "", "", false});
    ekle(g.kapat_tusu, Cmd{"durdur", "", "", false});
    hook.set_bindings(b);
    if (!hatalar.empty()) durum(L("Bu tuşlar kaydedilemedi: {0}", {join(hatalar, ", ")}));
}

// ---------------- komutlar ----------------
void MainWindow::komut_isle(const Cmd& c) {
    if (kapaniyor_) return;
    const std::string& t = c.tur;
    if (t == "tetik") {
        if (!yakalama_ && v_.dinle) oynatici->tetikle(c.a);
    } else if (t == "web_tetik") {
        if (!yakalama_) { durum(L("Telefondan tetiklendi: {0}", {c.a})); oynatici->tetikle(c.a, c.b, c.kes); }
    } else if (t == "durdur") {
        if (!yakalama_) { oynatici->durdur(); SES.durdur(SES.sb); durum(L("Durduruldu.")); }
    } else if (t == "yuklendi") {
        durum(L("Yüklendi: {0} → {1}", {c.b, c.a}));
        listeyi_yenile();
        onyukle_baslat();
    } else if (t == "web_ses") {
        if (!yakalama_) {
            SES.soundboard(c.b, core.ayar.kopya().ses_seviyesi);
            core.deck.son_ekle(c.a + "/" + base_name(c.b));
            durum(L("Soundboard: {0}", {base_name(c.b)}));
        }
    } else if (t == "arayuz") {
        if (IsWindowVisible(hwnd)) gizle(); else goster();
    } else if (t == "cik") {
        tamamen_kapat();
    }
}

// ---------------- onbellek ----------------
void MainWindow::tum_dosyalar(std::vector<std::string>& gorseller, std::vector<std::string>& sesler) {
    for (auto& kv : core.ayar.tuslar_kopya()) {
        std::vector<std::string> g, s;
        klasor_dosyalari(kv.first, g, s);
        gorseller.insert(gorseller.end(), g.begin(), g.end());
        sesler.insert(sesler.end(), s.begin(), s.end());
    }
}

void MainWindow::onyukle_baslat() {
    if (kapaniyor_) return;
    std::vector<std::string> g, s;
    tum_dosyalar(g, s);
    oynatici->eskileri_at(std::set<std::string>(g.begin(), g.end()));
    if (core.ayar.kopya().on_yukle && !bellek_bosaltildi_) {
        oynatici->onyukle_tetikle(g);
        ses_kuyrugu_.clear();
        size_t yer = SesOnbellegi::ADET > SES.sayi() ? SesOnbellegi::ADET - SES.sayi() : 0;
        for (auto& y : s) { if (ses_kuyrugu_.size() >= yer) break; if (!SES.acik_mi(y)) ses_kuyrugu_.push_back(y); }
        if (!ses_kuyrugu_.empty() && !ses_isitiliyor_) { ses_isitiliyor_ = true; ui_after(80, [this] { ses_isit_adimi(); }); }
    }
}

void MainWindow::ses_isit_adimi() {
    if (kapaniyor_) return;
    if (!ses_kuyrugu_.empty() && !core.mesgul) { std::string y = ses_kuyrugu_.front(); ses_kuyrugu_.erase(ses_kuyrugu_.begin()); SES.isit(y); }
    if (!ses_kuyrugu_.empty()) ui_after(80, [this] { ses_isit_adimi(); });
    else ses_isitiliyor_ = false;
}

void MainWindow::onbellek_etiketi() {
    if (!lbl_onb_) return;
    std::vector<std::string> g, s;
    tum_dosyalar(g, s);
    int mb, limit, hazir;
    oynatici->ozet(g.size(), mb, limit, hazir);
    std::string t = L("Önbellek: {0} MB / {1} MB • {2}/{3} görsel hazır", {S(mb), S(limit), S(hazir), S((long long)g.size())});
    double ram = islem_bellegi_mb();
    if (ram > 0) t += " • " + L("Program belleği: {0} MB", {S((long long)std::llround(ram))});
    lbl_onb_->s = t;
    invalidate();
}

void MainWindow::onbellegi_temizle() {
    oynatici->temizle();
    SES.hepsini_kapat();
    bellek_bosaltildi_ = false;
    bellek_kirp();
    durum(L("Önbellek temizlendi."));
    onbellek_etiketi();
    ui_after(800, [this] { onyukle_baslat(); });
}

void MainWindow::otomatik_yenile() {
    if (kapaniyor_) return;
    if (IsWindowVisible(hwnd) && !yakalama_) { listeyi_yenile(); onbellek_etiketi(); }
    int dk = core.ayar.kopya().bos_sure_dk;
    if (dk > 0 && oynatici->bosta_mi(dk)) {
        oynatici->bosalt();
        SES.hepsini_kapat();
        bellek_bosaltildi_ = true;
        bellek_kirp();
        durum(L("Bellek boşaltıldı (boşta)."));
    }
    onyukle_baslat();
}

// ---------------- telefon kumandasi ----------------
void MainWindow::web_yeniden_baslat() {
    Genel g = core.ayar.kopya();
    web.stop();
    bool basladi = false;
    if (g.web_aktif) {
        basladi = web.start(g.web_port, [this](HttpRequest& r) { return core.web_isle(r); });
        if (!basladi) durum(L("Telefon kumandası başlatılamadı: port kullanımda olabilir, başka port dene."));
    }
    web_bilgi_guncelle();
    if (basladi) ui_after(1500, [this] { fw_kontrol_et(); });
}

void MainWindow::web_bilgi_guncelle() {
    if (!dd_url_) return;
    Genel g = core.ayar.kopya();
    if (!g.web_aktif || !web.running()) {
        dd_url_->items.clear(); dd_url_->sel = -1;
        dd_url_->placeholder = !g.web_aktif ? L("Kapalı") : L("Başlatılamadı (port kullanımda?)");
        qr_goster("");
        invalidate();
        return;
    }
    std::vector<std::string> urller;
    for (auto& ip : yerel_ip_listesi()) urller.push_back("http://" + ip + ":" + std::to_string(g.web_port));
    if (urller.empty()) urller.push_back("http://127.0.0.1:" + std::to_string(g.web_port));
    if (std::find(urller.begin(), urller.end(), secili_url_) == urller.end()) secili_url_ = urller[0];
    dd_url_->items = urller;
    dd_url_->sel = (int)(std::find(urller.begin(), urller.end(), secili_url_) - urller.begin());
    qr_goster(secili_url_ + (g.web_pin.empty() ? "" : "/?pin=" + url_encode(g.web_pin)));
    invalidate();
}

void MainWindow::qr_goster(const std::string& metin) {
    if (metin == son_qr_) return;
    son_qr_ = metin;
    if (!qrv_) return;
    if (metin.empty()) { qrv_->has = false; qrv_->message = ""; return; }
    if (qr_encode(metin, true, qrv_->qr) || qr_encode(metin, false, qrv_->qr)) qrv_->has = true;
    else { qrv_->has = false; qrv_->message = L("QR yok (qrcode kurulu değil)"); }
}

void MainWindow::fw_etiketi() {
    if (!lbl_fw_) return;
    if (fw_var_ == 1) { lbl_fw_->s = L("Güvenlik duvarı: izin var ✓"); lbl_fw_->col = C_YESIL; }
    else if (fw_var_ == 0) { lbl_fw_->s = L("Güvenlik duvarı: izin YOK ✗"); lbl_fw_->col = C_KIRMIZI; }
    else { lbl_fw_->s = L("Güvenlik duvarı: kontrol edilemedi"); lbl_fw_->col = C_SOLUK; }
    invalidate();
}

void MainWindow::fw_durum_guncelle() {
    fw_var_ = fw_kural_var(core.ayar.kopya().web_port);
    fw_etiketi();
}

void MainWindow::fw_kontrol_et() {
    if (kapaniyor_) return;
    fw_durum_guncelle();
    Genel g = core.ayar.kopya();
    if (fw_var_ == 0 && g.web_aktif && web.running() && g.fw_otomatik && !fw_soruluyor_ && IsWindowVisible(hwnd)) {
        fw_soruluyor_ = true;
        int r = msg(L("Güvenlik duvarı izni"),
                    L("Telefon kumandasının çalışması için Windows güvenlik duvarında izin gerekiyor.\n\n"
                      "Şimdi otomatik eklensin mi?\n"
                      "(Evet'e basınca Windows bir onay penceresi gösterir, orada da Evet de.)"), MB_YESNO | MB_ICONQUESTION);
        fw_soruluyor_ = false;
        if (r == IDYES) guvenlik_duvari_ac();
        else { v_.fwoto = false; ayar_degisti(); invalidate(); }
    }
}

void MainWindow::guvenlik_duvari_ac() {
    int port = core.ayar.kopya().web_port;
    if (!fw_izin_baslat(port)) {
        msg(L("Güvenlik duvarı"), L("İzin eklenemedi (onay penceresine Evet demedin olabilir).\n\n"
                                      "Alternatif: firewall_ac.bat dosyasına sağ tıkla → 'Yönetici olarak çalıştır'."), MB_OK | MB_ICONWARNING);
        fw_durum_guncelle();
        return;
    }
    durum(L("Güvenlik duvarı izni ekleniyor..."));
    ui_after(1200, [this, port] { fw_dogrula(0, port); });
}

void MainWindow::fw_dogrula(int deneme, int port) {
    if (kapaniyor_) return;
    if (fw_kural_var(port) == 1) {
        fw_durum_guncelle();
        durum(L("Güvenlik duvarı izni verildi ✓"));
        msg(L("Güvenlik duvarı"), L("{0} portu için izin eklendi.\nŞimdi telefondan sayfayı yenile.", {S(port)}), MB_OK | MB_ICONINFORMATION);
    } else if (deneme < 25) {
        ui_after(1000, [this, deneme, port] { fw_dogrula(deneme + 1, port); });
    } else {
        fw_durum_guncelle();
        durum(L("Güvenlik duvarı izni doğrulanamadı (onay verilmedi olabilir)."));
    }
}

// ---------------- dil / pencere ----------------
void MainWindow::dil_secildi(int idx) {
    if (idx < 0 || idx >= (int)g_lang_codes.size()) return;
    std::string kod = g_lang_codes[(size_t)idx];
    if (kod == lang_cur()) return;
    {
        std::lock_guard<std::recursive_mutex> lk(core.ayar.m);
        core.ayar.genel.dil = kod;
        core.ayar.kaydet();
    }
    lang_set(kod);
    ui_after(40, [this] { arayuzu_yeniden_kur(); });
}

void MainWindow::arayuzu_yeniden_kur() {
    ui_hazir_ = false;
    KillTimer(hwnd, 1);
    std::string sec = secili(false);
    for (EditRow* e : edits_) DestroyWindow(e->edit);
    edits_.clear();
    clear_widgets();
    son_qr_ = "~";
    build_ui();
    layout();
    listeyi_yenile(sec);
    fw_etiketi();
    web_bilgi_guncelle();
    onbellek_etiketi();
    ui_hazir_ = true;
    invalidate();
}

void MainWindow::goster() {
    listeyi_yenile();
    ShowWindow(hwnd, IsIconic(hwnd) ? SW_RESTORE : SW_SHOW);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    SetForegroundWindow(hwnd);
}
void MainWindow::gizle() { ShowWindow(hwnd, SW_HIDE); }

void MainWindow::pencere_kapatilirken() {
    int r = msg(L("Kapat"), L("Arka planda çalışmaya devam etsin mi?\n\n"
                               "Evet: Arayüz gizlenir, tuşlar çalışmaya devam eder.\n"
                               "(Geri açmak için: {0})\n"
                               "Hayır: Program tamamen kapanır.", {upper_u8(core.ayar.kopya().arayuz_tusu)}), MB_YESNOCANCEL | MB_ICONQUESTION);
    if (r == IDYES) gizle();
    else if (r == IDNO) tamamen_kapat();
}

void MainWindow::tamamen_kapat() {
    if (kapaniyor_) return;
    kapaniyor_ = true;
    if (kayit_bekliyor_) { ui_hazir_ = true; ayarlari_kaydet(); }
    hook.stop();
    web.stop();
    if (oynatici) oynatici->durdur();
    SES.hepsini_kapat();
    DestroyWindow(hwnd);
}
