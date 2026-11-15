// Windows'a ozgu sistem islevleri: ses (MCI), bellek, guvenlik duvari, tek kopya, panoya kopyalama...
#pragma once
#include "winhdr.hpp"
#include <map>
#include <list>

struct Codec;
Codec* win_codec_create();

std::string exe_path();
std::string exe_dir();
std::string sistem_dili();                                   // "tr" "en" ...
bool tek_kopya_mi(const std::string& mesaj);                 // false: baska kopya calisiyor (mesaj gosterilir)
double islem_bellegi_mb();                                   // <0: olculemedi
void bellek_kirp();
void panoya_kopyala(HWND h, const std::string& metin);
bool klasor_ac(const std::string& yol);
std::vector<std::string> dosya_sec(HWND h, const std::string& baslik, const std::string& filtre_ad, const std::string& filtre_uz);
void klavye_yedeklerini_kur();                               // tus adi <-> VK (noktalama, Turkce harfler)

// Ses: Windows MCI (mp3/wav). Tum cagrilar ARAYUZ thread'inden yapilmalidir.
class SesOnbellegi {
public:
    static const size_t ADET = 32;
    bool isit(const std::string& yol);
    std::string hazirla(const std::string& yol, int seviye);   // takma ad ("" = acilamadi)
    void oynat(const std::string& alias);
    void durdur(const std::string& alias);
    void soundboard(const std::string& yol, int seviye);
    void hepsini_kapat();
    bool acik_mi(const std::string& yol) const { return acik.count(yol) != 0; }
    size_t sayi() const { return acik.size(); }
    std::string sb;
private:
    std::string ac(const std::string& yol);
    std::map<std::string, std::string> acik;
    std::list<std::string> sira;                               // en eski basta
    int sayac = 0;
};
extern SesOnbellegi SES;

// Guvenlik duvari
int fw_kural_var(int port);                                  // 1 var, 0 yok, -1 kontrol edilemedi
bool fw_izin_baslat(int port);                               // gerekirse UAC ile; baslatildiysa true
