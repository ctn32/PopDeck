#include "files.hpp"

std::vector<std::string> g_image_exts = {".gif", ".png", ".webp", ".jpg", ".jpeg", ".jfif", ".bmp", ".tif", ".tiff", ".ico", ".avif", ".heic", ".heif"};
const char* const AUDIO_EXTS[] = {".mp3", ".wav", nullptr};
AkilliRastgele RASTGELE;

bool is_image_name(const std::string& n) {
    for (auto& e : g_image_exts) if (ends_with(n, e)) return true;
    return false;
}
bool is_audio_name(const std::string& n) {
    for (int i = 0; AUDIO_EXTS[i]; i++) if (ends_with(n, AUDIO_EXTS[i])) return true;
    return false;
}
static size_t last_sep(const std::string& p) {
    size_t a = p.rfind('/'), b = p.rfind('\\');
    if (a == std::string::npos) return b;
    if (b == std::string::npos) return a;
    return a > b ? a : b;
}
std::string base_name(const std::string& p) { size_t s = last_sep(p); return s == std::string::npos ? p : p.substr(s + 1); }
std::string dir_name(const std::string& p) { size_t s = last_sep(p); return s == std::string::npos ? "" : p.substr(0, s); }
std::string strip_ext(const std::string& p) {
    size_t s = last_sep(p);
    size_t start = s == std::string::npos ? 0 : s + 1;
    size_t d = p.rfind('.');
    if (d == std::string::npos || d <= start) return p;
    return p.substr(0, d);
}
std::string ext_of(const std::string& p) {
    size_t s = last_sep(p);
    size_t start = s == std::string::npos ? 0 : s + 1;
    size_t d = p.rfind('.');
    if (d == std::string::npos || d <= start) return "";
    return lower_u8(p.substr(d));
}
std::string join_path(const std::string& a, const std::string& b) { return path_str(path_u8(a) / path_u8(b)); }

std::vector<std::string> klasor_listesi() {
    std::vector<std::string> r;
    std::error_code ec;
    for (fs::directory_iterator it(g_main_dir, ec), end; !ec && it != end; it.increment(ec)) {
        std::error_code e2;
        if (!it->is_directory(e2)) continue;
        std::string n = path_str(it->path().filename());
        if (n.empty() || n[0] == '.' || n[0] == '_') continue;
        r.push_back(n);
    }
    std::sort(r.begin(), r.end());
    return r;
}

void klasor_dosyalari(const std::string& ad, std::vector<std::string>& gorseller, std::vector<std::string>& sesler) {
    gorseller.clear(); sesler.clear();
    fs::path dir = g_main_dir / path_u8(ad);
    std::vector<std::string> adlar;
    std::error_code ec;
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        std::error_code e2;
        if (!it->is_regular_file(e2)) continue;
        adlar.push_back(path_str(it->path().filename()));
    }
    std::sort(adlar.begin(), adlar.end());
    for (auto& n : adlar) {
        std::string k = lower_u8(n);
        std::string tam = path_str(dir / path_u8(n));
        if (is_image_name(k)) gorseller.push_back(tam);
        else if (is_audio_name(k)) sesler.push_back(tam);
    }
}

std::string AkilliRastgele::sec(const std::string& anahtar, const std::vector<std::string>& liste) {
    if (liste.empty()) return "";
    std::lock_guard<std::mutex> lk(m);
    if (liste.size() == 1) { son[anahtar] = liste[0]; return liste[0]; }
    std::set<std::string> kume_yeni(liste.begin(), liste.end());
    if (kume[anahtar] != kume_yeni || torba[anahtar].empty()) {
        std::vector<std::string> t = liste;
        std::shuffle(t.begin(), t.end(), rng());
        auto it = son.find(anahtar);
        if (it != son.end() && t.back() == it->second) {            // pop sondan alir: ilk cikan, son cikanla ayni olmasin
            size_t j = (size_t)(rng()() % (t.size() - 1));
            std::swap(t.back(), t[j]);
        }
        kume[anahtar] = kume_yeni;
        torba[anahtar] = t;
    }
    std::string s = torba[anahtar].back();
    torba[anahtar].pop_back();
    son[anahtar] = s;
    return s;
}
void AkilliRastgele::bildir(const std::string& anahtar, const std::string& oge) {
    std::lock_guard<std::mutex> lk(m);
    son[anahtar] = oge;
}

std::string ses_bul(const std::string& gorsel, const std::vector<std::string>& gorseller, const std::vector<std::string>& sesler) {
    std::string taban = lower_u8(strip_ext(gorsel));
    for (auto& s : sesler) if (lower_u8(strip_ext(s)) == taban) return s;
    std::set<std::string> gt;
    for (auto& g : gorseller) gt.insert(lower_u8(strip_ext(g)));
    std::vector<std::string> serbest;
    for (auto& s : sesler) if (!gt.count(lower_u8(strip_ext(s)))) serbest.push_back(s);
    return serbest.empty() ? "" : RASTGELE.sec("ses|" + dir_name(gorsel), serbest);
}
