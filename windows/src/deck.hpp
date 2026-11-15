// Telefon Deck'inin kalici durumu: favoriler, siralamalar, son oynatilanlar (main/deck.json)
#pragma once
#include "util.hpp"
#include <map>

class DeckDurumu {
public:
    static const size_t SON_LIMIT = 30;
    explicit DeckDurumu(const fs::path& yol);
    void anlik(std::vector<std::string>& fav, std::map<std::string, std::vector<std::string>>& sira, std::vector<std::string>& son);
    void favori_ayarla(const std::string& kimlik, bool deger);
    void favori_sirala(const std::vector<std::string>& idler);
    void sira_ayarla(const std::string& klasor, const std::vector<std::string>& dosyalar);
    void son_ekle(const std::string& kimlik);
    void klasor_adi_degisti(const std::string& eski, const std::string& yeni);
private:
    void yukle();
    void kaydet();
    fs::path yol;
    std::recursive_mutex m;
    std::vector<std::string> favoriler, son;
    std::map<std::string, std::vector<std::string>> sira;
};
