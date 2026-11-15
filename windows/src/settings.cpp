#include "settings.hpp"
#include "json.hpp"
#include "files.hpp"

fs::path g_main_dir;
fs::path settings_path() { return g_main_dir / "ayarlar.json"; }
fs::path deck_path() { return g_main_dir / "deck.json"; }

static int ival(const Json& v, int d) { return v.is_num() ? (int)v.n : (v.is_bool() ? (int)v.b : d); }
static bool bval(const Json& v, bool d) { return v.is_bool() ? v.b : (v.is_num() ? v.n != 0 : d); }
static double dval(const Json& v, double d) { return v.is_num() ? v.n : d; }
static std::string sval(const Json& v, const std::string& d) { return v.is_str() ? v.s : d; }

void Ayarlar::yukle() {
    std::lock_guard<std::recursive_mutex> l(m);
    std::string txt;
    Json j;
    if (!read_file(settings_path(), txt) || !Json::parse(txt, j) || !j.is_obj()) return;
    if (const Json* g = j.get("genel")) {
        Genel& x = genel;
        for (auto& kv : g->o) {
            const std::string& k = kv.first; const Json& v = kv.second;
            if (k == "sure") x.sure = dval(v, x.sure);
            else if (k == "boyut_orani") x.boyut_orani = dval(v, x.boyut_orani);
            else if (k == "orani_boz") x.orani_boz = bval(v, x.orani_boz);
            else if (k == "fade_in") x.fade_in = dval(v, x.fade_in);
            else if (k == "tiklama_gecir") x.tiklama_gecir = bval(v, x.tiklama_gecir);
            else if (k == "ses_aktif") x.ses_aktif = bval(v, x.ses_aktif);
            else if (k == "ses_seviyesi") x.ses_seviyesi = ival(v, x.ses_seviyesi);
            else if (k == "ses_gif_ile_kesil") x.ses_gif_ile_kesil = bval(v, x.ses_gif_ile_kesil);
            else if (k == "dinleme") x.dinleme = bval(v, x.dinleme);
            else if (k == "gizli_basla") x.gizli_basla = bval(v, x.gizli_basla);
            else if (k == "arayuz_tusu") x.arayuz_tusu = sval(v, x.arayuz_tusu);
            else if (k == "cikis_tusu") x.cikis_tusu = sval(v, x.cikis_tusu);
            else if (k == "kapat_tusu") x.kapat_tusu = sval(v, x.kapat_tusu);
            else if (k == "dil") x.dil = sval(v, "");
            else if (k == "ses_gecikme") x.ses_gecikme = ival(v, x.ses_gecikme);
            else if (k == "on_yukle") x.on_yukle = bval(v, x.on_yukle);
            else if (k == "onbellek_mb") x.onbellek_mb = ival(v, x.onbellek_mb);
            else if (k == "max_buyutme") x.max_buyutme = dval(v, x.max_buyutme);
            else if (k == "fps_siniri") x.fps_siniri = ival(v, x.fps_siniri);
            else if (k == "bos_sure_dk") x.bos_sure_dk = ival(v, x.bos_sure_dk);
            else if (k == "ayar_surumu") x.ayar_surumu = ival(v, x.ayar_surumu);
            else if (k == "gif_sonuna_kadar") x.gif_sonuna_kadar = bval(v, x.gif_sonuna_kadar);
            else if (k == "fw_otomatik") x.fw_otomatik = bval(v, x.fw_otomatik);
            else if (k == "web_aktif") x.web_aktif = bval(v, x.web_aktif);
            else if (k == "web_port") x.web_port = ival(v, x.web_port);
            else if (k == "web_pin") { if (v.t == Json::Null) { x.web_pin_yok = true; x.web_pin.clear(); } else { x.web_pin_yok = false; x.web_pin = v.is_str() ? v.s : (v.is_num() ? S((long long)v.n) : ""); } }
            else if (k == "web_yukleme") x.web_yukleme = bval(v, x.web_yukleme);
        }
        if (const Json* sv = g->get("ayar_surumu")) { if (ival(*sv, 0) < 2) { x.onbellek_mb = std::min(x.onbellek_mb ? x.onbellek_mb : 400, 400); x.ayar_surumu = 2; } }
        else { x.onbellek_mb = std::min(x.onbellek_mb ? x.onbellek_mb : 400, 400); x.ayar_surumu = 2; }
    }
    if (const Json* k = j.get("klasorler")) {
        tuslar.clear();
        for (auto& kv : k->o) tuslar.push_back({kv.first, kv.second.is_str() ? kv.second.s : ""});
    }
}

void Ayarlar::kaydet() {
    std::lock_guard<std::recursive_mutex> l(m);
    const Genel& x = genel;
    Json g = Json::object();
    g.set("sure", Json::number(x.sure));
    g.set("boyut_orani", Json::number(x.boyut_orani));
    g.set("orani_boz", Json::boolean(x.orani_boz));
    g.set("fade_in", Json::number(x.fade_in));
    g.set("tiklama_gecir", Json::boolean(x.tiklama_gecir));
    g.set("ses_aktif", Json::boolean(x.ses_aktif));
    g.set("ses_seviyesi", Json::integer(x.ses_seviyesi));
    g.set("ses_gif_ile_kesil", Json::boolean(x.ses_gif_ile_kesil));
    g.set("dinleme", Json::boolean(x.dinleme));
    g.set("gizli_basla", Json::boolean(x.gizli_basla));
    g.set("arayuz_tusu", Json::str(x.arayuz_tusu));
    g.set("cikis_tusu", Json::str(x.cikis_tusu));
    g.set("kapat_tusu", Json::str(x.kapat_tusu));
    g.set("dil", x.dil.empty() ? Json::null() : Json::str(x.dil));
    g.set("ses_gecikme", Json::integer(x.ses_gecikme));
    g.set("on_yukle", Json::boolean(x.on_yukle));
    g.set("onbellek_mb", Json::integer(x.onbellek_mb));
    g.set("max_buyutme", Json::number(x.max_buyutme));
    g.set("fps_siniri", Json::integer(x.fps_siniri));
    g.set("bos_sure_dk", Json::integer(x.bos_sure_dk));
    g.set("ayar_surumu", Json::integer(x.ayar_surumu));
    g.set("gif_sonuna_kadar", Json::boolean(x.gif_sonuna_kadar));
    g.set("fw_otomatik", Json::boolean(x.fw_otomatik));
    g.set("web_aktif", Json::boolean(x.web_aktif));
    g.set("web_port", Json::integer(x.web_port));
    g.set("web_pin", x.web_pin_yok ? Json::null() : Json::str(x.web_pin));
    g.set("web_yukleme", Json::boolean(x.web_yukleme));
    Json k = Json::object();
    for (auto& t : tuslar) k.set(t.first, Json::str(t.second));
    Json root = Json::object();
    root.set("genel", g);
    root.set("klasorler", k);
    std::error_code ec;
    fs::create_directories(g_main_dir, ec);
    if (!write_file_atomic(settings_path(), root.dump(2))) log_line("Ayarlar kaydedilemedi");
}

bool Ayarlar::klasor_esitle() {
    std::lock_guard<std::recursive_mutex> l(m);
    std::vector<std::string> mevcut = klasor_listesi();
    auto eski = tuslar;
    std::vector<std::pair<std::string, std::string>> yeni;
    for (auto& t : tuslar) if (std::find(mevcut.begin(), mevcut.end(), t.first) != mevcut.end()) yeni.push_back(t);
    for (auto& ad : mevcut) {
        bool var = false;
        for (auto& y : yeni) if (y.first == ad) { var = true; break; }
        if (!var) yeni.push_back({ad, ""});
    }
    tuslar = yeni;
    return tuslar != eski;
}

bool Ayarlar::klasor_var(const std::string& ad) const {
    std::lock_guard<std::recursive_mutex> l(m);
    for (auto& t : tuslar) if (t.first == ad) return true;
    return false;
}
std::string Ayarlar::tus_of(const std::string& ad) const {
    std::lock_guard<std::recursive_mutex> l(m);
    for (auto& t : tuslar) if (t.first == ad) return t.second;
    return "";
}
void Ayarlar::tus_koy(const std::string& ad, const std::string& tus) {
    std::lock_guard<std::recursive_mutex> l(m);
    for (auto& t : tuslar) if (t.first == ad) { t.second = tus; return; }
    tuslar.push_back({ad, tus});
}
