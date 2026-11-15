// PopDeck cekirdegi: ayarlar + deck + web kumandasi mantigi (arayuzden bagimsiz, tasinabilir)
#pragma once
#include "util.hpp"
#include "json.hpp"
#include "settings.hpp"
#include "deck.hpp"
#include "files.hpp"
#include "http.hpp"
#include <atomic>
#include <map>
#include <list>

struct Cmd { std::string tur, a, b; bool kes = false; };

bool name_has_invalid(const std::string& s);              // \ / : * ? " < > |
std::string name_sanitize(const std::string& s);
std::string utf8_truncate(const std::string& s, size_t max_chars);

// Yukleme: akistan gelen dosyayi dogrulayip kaydeder. (ok, dosya_adi | hata)
bool yukleme_kaydet(const fs::path& klasor_yolu, const std::string& ad,
                    const std::function<size_t(u8*, size_t)>& read, long long uzunluk, std::string& sonuc);
extern const int YUKLEME_LIMIT_MB;
std::vector<std::string> yukleme_uzantilari();

class Cekirdek {
public:
    Ayarlar ayar;
    DeckDurumu deck;
    std::function<void(const Cmd&)> post;                  // arayuz thread'ine komut gonder
    std::atomic<bool> mesgul{false}, oturum_var{false};   // oynatici durumu (arayuz gunceller)

    explicit Cekirdek(const fs::path& deck_yolu) : deck(deck_yolu) {}

    // ---- web kumandasi
    bool web_yetki(const std::string& ip, const std::string& verilen, std::string& mesaj, int& kod);
    Json web_klasor_listesi();
    Json web_gorsel_listesi();
    bool web_oge(const std::string& ad, const std::string& dosya, std::string& yol, std::string& tur);
    bool web_deck_islem(const Json& veri, std::string& hata);
    bool web_yukle(std::string klasor, const std::string& dosya, bool yeni, const std::function<size_t(u8*, size_t)>& read,
                   long long uzunluk, std::string& sonuc, std::string& klasor_out);
    bool web_telefon(std::string ad, const std::string& dosya, Json& out, std::string& hata);   // telefonda oynatma: secimi yapar
    bool web_dosya_yolu(const std::string& ad, const std::string& dosya, std::string& yol);   // klasordeki herhangi gorsel/ses
    bool web_tetik(std::string ad, const std::string& dosya, std::string& sonuc);
    bool kucuk_resim(const std::string& yol, std::vector<u8>& out);
    HttpResponse web_isle(HttpRequest& rq);                 // tum rotalar
    std::string web_sayfasi();

    struct Oge { std::string dosya, yol, tur; };
    std::vector<Oge> klasor_ogeleri(const std::string& ad);

private:
    std::mutex wm;
    std::map<std::string, std::pair<int, double>> web_hatalar;   // IP -> (yanlis sayisi, kilit bitisi)
    double web_son = 0, web_son_ses = 0;
    std::mutex km;
    std::list<std::pair<std::string, std::vector<u8>>> kucuk_lru;
    std::map<std::string, std::list<std::pair<std::string, std::vector<u8>>>::iterator> kucuk_idx;
};
