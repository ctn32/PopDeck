#include "deck.hpp"
#include "json.hpp"
#include <set>

DeckDurumu::DeckDurumu(const fs::path& y) : yol(y) { yukle(); }

static std::vector<std::string> str_list(const Json* j) {
    std::vector<std::string> r;
    if (j && j->is_arr()) for (auto& x : j->a) if (x.is_str()) r.push_back(x.s);
    return r;
}
static Json to_arr(const std::vector<std::string>& v) { Json a = Json::array(); for (auto& s : v) a.push(Json::str(s)); return a; }

void DeckDurumu::yukle() {
    std::string txt; Json j;
    if (!read_file(yol, txt) || !Json::parse(txt, j) || !j.is_obj()) return;
    favoriler = str_list(j.get("favoriler"));
    if (const Json* s = j.get("sira")) if (s->is_obj())
        for (auto& kv : s->o) if (kv.second.is_arr()) { std::vector<std::string> v; for (auto& d : kv.second.a) v.push_back(d.is_str() ? d.s : ""); sira[kv.first] = v; }
    son = str_list(j.get("son"));
    if (son.size() > SON_LIMIT) son.resize(SON_LIMIT);
}
void DeckDurumu::kaydet() {
    Json root = Json::object(), s = Json::object();
    for (auto& kv : sira) s.set(kv.first, to_arr(kv.second));
    root.set("favoriler", to_arr(favoriler));
    root.set("sira", s);
    root.set("son", to_arr(son));
    if (!write_file_atomic(yol, root.dump(1))) log_line("Deck kaydedilemedi");
}
void DeckDurumu::anlik(std::vector<std::string>& f, std::map<std::string, std::vector<std::string>>& s, std::vector<std::string>& so) {
    std::lock_guard<std::recursive_mutex> l(m);
    f = favoriler; s = sira; so = son;
}
void DeckDurumu::favori_ayarla(const std::string& k, bool deger) {
    std::lock_guard<std::recursive_mutex> l(m);
    auto it = std::find(favoriler.begin(), favoriler.end(), k);
    if (deger && it == favoriler.end()) favoriler.push_back(k);
    else if (!deger && it != favoriler.end()) favoriler.erase(it);
    kaydet();
}
void DeckDurumu::favori_sirala(const std::vector<std::string>& idler) {
    std::lock_guard<std::recursive_mutex> l(m);
    std::vector<std::string> yeni;
    std::set<std::string> gor;
    for (auto& i : idler) if (!gor.count(i) && std::find(favoriler.begin(), favoriler.end(), i) != favoriler.end()) { yeni.push_back(i); gor.insert(i); }
    for (auto& i : favoriler) if (!gor.count(i)) yeni.push_back(i);
    favoriler = yeni;
    kaydet();
}
void DeckDurumu::sira_ayarla(const std::string& klasor, const std::vector<std::string>& dosyalar) {
    std::lock_guard<std::recursive_mutex> l(m);
    std::vector<std::string> y; std::set<std::string> g;
    for (auto& d : dosyalar) if (!g.count(d)) { y.push_back(d); g.insert(d); }
    sira[klasor] = y;
    kaydet();
}
void DeckDurumu::son_ekle(const std::string& k) {
    std::lock_guard<std::recursive_mutex> l(m);
    auto it = std::find(son.begin(), son.end(), k);
    if (it != son.end()) son.erase(it);
    son.insert(son.begin(), k);
    if (son.size() > SON_LIMIT) son.resize(SON_LIMIT);
    kaydet();
}
void DeckDurumu::klasor_adi_degisti(const std::string& eski, const std::string& yeni) {
    std::lock_guard<std::recursive_mutex> l(m);
    auto degis = [&](const std::string& k) { return starts_with(k, eski + "/") ? yeni + k.substr(eski.size()) : k; };
    for (auto& k : favoriler) k = degis(k);
    for (auto& k : son) k = degis(k);
    auto it = sira.find(eski);
    if (it != sira.end()) { auto v = it->second; sira.erase(it); sira[yeni] = v; }
    kaydet();
}
