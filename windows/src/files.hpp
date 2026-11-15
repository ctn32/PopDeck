// main klasoru: klasor/dosya listeleri, ses eslestirme, akilli rastgele
#pragma once
#include "util.hpp"
#include <map>
#include <set>

extern fs::path g_main_dir;
extern std::vector<std::string> g_image_exts;       // ".gif", ".png", ...
extern const char* const AUDIO_EXTS[];              // ".mp3", ".wav"
bool is_image_name(const std::string& lower_name);
bool is_audio_name(const std::string& lower_name);
std::string strip_ext(const std::string& path);     // son uzantiyi atar
std::string ext_of(const std::string& path);        // ".gif" (kucuk harf)
std::string base_name(const std::string& path);     // son yol parcasi
std::string dir_name(const std::string& path);
std::string join_path(const std::string& a, const std::string& b);

std::vector<std::string> klasor_listesi();
void klasor_dosyalari(const std::string& ad, std::vector<std::string>& gorseller, std::vector<std::string>& sesler);

// Karistirilmis torba: art arda ayni oge cikmaz
class AkilliRastgele {
public:
    std::string sec(const std::string& anahtar, const std::vector<std::string>& liste);
    void bildir(const std::string& anahtar, const std::string& oge);
private:
    std::mutex m;
    std::map<std::string, std::vector<std::string>> torba;
    std::map<std::string, std::set<std::string>> kume;
    std::map<std::string, std::string> son;
};
extern AkilliRastgele RASTGELE;

std::string ses_bul(const std::string& gorsel, const std::vector<std::string>& gorseller, const std::vector<std::string>& sesler);
