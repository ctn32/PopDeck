// Gosterim yoneticisi (arayuz thread'inde calisir): onbellek, on yukleme, oturum, uyari
#pragma once
#include "winhdr.hpp"
#include "core.hpp"
#include "image.hpp"
#include "player.hpp"
#include <set>
#include <deque>
#include <memory>

class Oynatici {
public:
    Oynatici(Cekirdek& c, std::function<void(const std::string&)> on_oynat);
    void tetikle(const std::string& klasor, const std::string& yol = "", bool kes = false);
    void durdur();
    bool mesgul() const { return c_.mesgul; }
    bool oynuyor() const { return oturum_ != nullptr; }
    void uyari_goster(const std::string& mesaj);

    // onbellek
    void onyukle_tetikle(const std::vector<std::string>& yollar);
    void eskileri_at(const std::set<std::string>& gecerli);
    void temizle();
    bool bosta_mi(int dakika) const;
    void bosalt();
    void limit_degisti();
    void ozet(size_t toplam, int& mb, int& limit_mb, int& hazir) const;
    void ekran_yenile();

private:
    struct Is {
        std::string anahtar, yol, ses;
        bool onyuk = false;
        std::atomic<bool> iptal{false};
    };
    struct Oturum { unsigned id; std::unique_ptr<Player> player; std::string ses; };
    struct Giris { std::string anahtar, yol; std::shared_ptr<ImgData> veri; };

    std::string anahtar_yap(const std::string& yol, const Genel& g) const;
    size_t limit_bayt() const;
    size_t toplam_bayt() const;
    Giris* bul(const std::string& anahtar);
    bool onbellege_ekle(const std::string& anahtar, const std::string& yol, std::shared_ptr<ImgData> veri, bool onyuk);
    void is_baslat(const std::string& anahtar, const std::string& yol, const std::string& ses, bool onyuk);
    void is_bitti(std::shared_ptr<Is> is, std::shared_ptr<ImgData> veri, const std::string& hata);
    void onyuk_ilerlet();
    void baslat(std::shared_ptr<ImgData> veri, const std::string& ses, const std::string& yol);
    void kapat(unsigned id);

    Cekirdek& c_;
    std::function<void(const std::string&)> on_oynat_;
    int ekran_w_, ekran_h_;
    std::vector<Giris> onbellek_;                       // en eski basta
    std::map<std::string, std::shared_ptr<Is>> isler_;
    std::deque<std::pair<std::string, std::string>> onyuk_kuyrugu_;
    std::set<std::string> onyuk_bekleyen_, sigmayanlar_;
    std::shared_ptr<Is> onyuk_is_;
    bool onyuk_dur_ = false, iptal_ = false;
    double son_kullanim_ = 0;
    std::unique_ptr<Oturum> oturum_;
    unsigned sayac_ = 0;
    HWND uyari_ = nullptr;
};
