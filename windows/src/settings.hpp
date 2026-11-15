// Ayarlar (main/settings.json) - Python surumuyle ayni dosya bicimi
#pragma once
#include "util.hpp"
#include <utility>

struct Genel {
    double sure = 3.0;
    double boyut_orani = 1.0;
    bool orani_boz = false;
    double fade_in = 0.4;
    bool tiklama_gecir = true;
    bool ses_aktif = true;
    int ses_seviyesi = 100;
    bool ses_gif_ile_kesil = true;
    bool dinleme = true;
    bool gizli_basla = false;
    std::string arayuz_tusu = "ctrl+shift+f11";
    std::string cikis_tusu = "ctrl+shift+f12";
    std::string kapat_tusu = "f10";
    std::string dil;                       // bos = otomatik (Windows dili)
    int ses_gecikme = 0;
    bool on_yukle = true;
    int onbellek_mb = 400;
    double max_buyutme = 2.0;
    int fps_siniri = 30;
    int bos_sure_dk = 10;
    int ayar_surumu = 2;
    bool gif_sonuna_kadar = true;
    bool fw_otomatik = true;
    bool web_aktif = true;
    int web_port = 8080;
    bool web_pin_yok = true;               // true: henuz PIN uretilmedi (null)
    std::string web_pin;
    bool web_yukleme = true;
};

extern fs::path g_main_dir;                // exe_yani/main
fs::path settings_path();
fs::path deck_path();

class Ayarlar {
public:
    mutable std::recursive_mutex m;
    Genel genel;
    std::vector<std::pair<std::string, std::string>> tuslar;   // klasor adi -> tus ("" = atanmamis), sirali

    void yukle();
    void kaydet();
    bool klasor_esitle();                  // main icindeki klasorlerle esitler; degisiklik varsa true
    Genel kopya() const { std::lock_guard<std::recursive_mutex> l(m); return genel; }
    std::vector<std::pair<std::string, std::string>> tuslar_kopya() const { std::lock_guard<std::recursive_mutex> l(m); return tuslar; }
    bool klasor_var(const std::string& ad) const;
    std::string tus_of(const std::string& ad) const;
    void tus_koy(const std::string& ad, const std::string& tus);
};
