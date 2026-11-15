#include "core.hpp"
#include "image.hpp"
#include "lang.hpp"
#include <thread>
#include <fstream>
#include <sstream>

const int YUKLEME_LIMIT_MB = 60;

bool name_has_invalid(const std::string& s) {
    for (char c : s) if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') return true;
    return false;
}
std::string name_sanitize(const std::string& s) {
    std::string o = s;
    for (auto& c : o) if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') c = '_';
    return o;
}
std::string utf8_truncate(const std::string& s, size_t max_chars) {
    size_t n = 0, i = 0;
    while (i < s.size() && n < max_chars) {
        unsigned char c = (unsigned char)s[i];
        size_t len = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : 4;
        i += len; n++;
    }
    return s.substr(0, std::min(i, s.size()));
}
std::vector<std::string> yukleme_uzantilari() {
    std::vector<std::string> v = g_image_exts;
    v.push_back(".mp3"); v.push_back(".wav");
    return v;
}

static fs::path benzersiz_yol(const fs::path& dir, const std::string& taban, const std::string& uz) {
    fs::path y = dir / path_u8(taban + uz);
    int i = 2;
    std::error_code ec;
    while (fs::exists(y, ec)) { y = dir / path_u8(taban + "_" + std::to_string(i) + uz); i++; }
    return y;
}
static bool write_bytes(const fs::path& p, const std::vector<u8>& d) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write((const char*)d.data(), (std::streamsize)d.size());
    return (bool)f;
}

bool yukleme_kaydet(const fs::path& klasor, const std::string& ad_in, const std::function<size_t(u8*, size_t)>& read,
                    long long uzunluk, std::string& sonuc) {
    std::string ad = ad_in;
    for (auto& c : ad) if (c == '\\') c = '/';
    ad = base_name(ad);
    ad = name_sanitize(ad);
    ad = trim(ad);
    while (!ad.empty() && ad[0] == '.') ad.erase(0, 1);
    ad = utf8_truncate(ad, 90);
    std::string taban = strip_ext(ad), uz = ext_of(ad);
    taban = trim(taban);
    if (taban.empty()) taban = "dosya";
    {
        bool izinli = false;
        for (auto& e : yukleme_uzantilari()) if (e == uz) izinli = true;
        if (!izinli) { sonuc = L("Desteklenmeyen dosya türü"); return false; }
    }
    std::ostringstream tid; tid << std::this_thread::get_id();
    fs::path gecici = klasor / path_u8(".yukleniyor_" + tid.str() + "_" + std::to_string((long long)(now_s() * 1000)) + ".tmp");
    struct Temizle { fs::path p; ~Temizle() { std::error_code ec; fs::remove(p, ec); } } temizle{gecici};
    try {
        {
            std::ofstream f(gecici, std::ios::binary | std::ios::trunc);
            if (!f) { sonuc = L("Yükleme başarısız"); return false; }
            std::vector<u8> buf(1 << 20);
            long long kalan = uzunluk;
            while (kalan > 0) {
                size_t k = read(buf.data(), (size_t)std::min<long long>(kalan, (long long)buf.size()));
                if (k == 0) { sonuc = L("Yükleme başarısız"); return false; }
                f.write((const char*)buf.data(), (std::streamsize)k);
                kalan -= (long long)k;
            }
            if (!f) { sonuc = L("Yükleme başarısız"); return false; }
        }
        std::error_code ec;
        if (uz == ".mp3" || uz == ".wav") {                          // ses: baslik kontrolu
            std::string bas;
            { std::ifstream f(gecici, std::ios::binary); bas.resize(12); f.read(&bas[0], 12); bas.resize((size_t)f.gcount()); }
            bool mp3 = bas.compare(0, 3, "ID3") == 0 || (bas.size() > 1 && (u8)bas[0] == 0xFF && ((u8)bas[1] & 0xE0) == 0xE0);
            bool wav = bas.size() >= 12 && bas.compare(0, 4, "RIFF") == 0 && bas.compare(8, 4, "WAVE") == 0;
            if (!((uz == ".mp3" && mp3) || (uz == ".wav" && wav))) { sonuc = L("Geçersiz ya da bozuk dosya"); return false; }
            fs::path hedef = benzersiz_yol(klasor, taban, uz);
            fs::rename(gecici, hedef, ec);
            if (ec) { sonuc = L("Yükleme başarısız"); return false; }
            sonuc = path_str(hedef.filename());
            return true;
        }
        CodecInfo info;                                               // gorsel: gercekten gorsel mi?
        std::string gp = path_str(gecici);
        if (!g_codec || !g_codec->probe(gp, info) || info.w <= 0 || info.h <= 0) { sonuc = L("Geçersiz ya da bozuk dosya"); return false; }
        if ((long long)info.w * info.h > 120000000LL) { sonuc = L("Geçersiz ya da bozuk dosya"); return false; }
        if (info.format == "GIF" || info.format == "PNG" || info.format == "WEBP") {
            const char* e = info.format == "GIF" ? ".gif" : info.format == "PNG" ? ".png" : ".webp";
            fs::path hedef = benzersiz_yol(klasor, taban, e);
            fs::rename(gecici, hedef, ec);
            if (ec) { sonuc = L("Yükleme başarısız"); return false; }
            sonuc = path_str(hedef.filename());
            return true;
        }
        if (info.format == "JPEG") {
            bool duzelt = (info.orient != 0 && info.orient != 1) || std::max(info.w, info.h) > 2048;
            fs::path hedef = benzersiz_yol(klasor, taban, ".jpg");
            if (duzelt) {
                Rgba im; int orient = 1;
                if (!g_codec->decode(gp, im, orient)) { sonuc = L("Geçersiz ya da bozuk dosya"); return false; }
                apply_orientation(im, orient);
                if (std::max(im.w, im.h) > 2048) {
                    double k = 2048.0 / std::max(im.w, im.h);
                    Rgba sm; resize_rgba(im, std::max(1, (int)(im.w * k + 0.5)), std::max(1, (int)(im.h * k + 0.5)), sm);
                    im = std::move(sm);
                }
                std::vector<u8> jpg;
                if (!g_codec->encode_jpeg(im, 92, jpg) || !write_bytes(hedef, jpg)) { sonuc = L("Yükleme başarısız"); return false; }
            } else {
                fs::rename(gecici, hedef, ec);
                if (ec) { sonuc = L("Yükleme başarısız"); return false; }
            }
            sonuc = path_str(hedef.filename());
            return true;
        }
        Rgba im; int orient = 1;                                      // BMP, TIFF, ICO, HEIC, AVIF...: PNG'ye cevir
        if (!g_codec->decode(gp, im, orient)) { sonuc = L("Geçersiz ya da bozuk dosya"); return false; }
        apply_orientation(im, orient);
        if (std::max(im.w, im.h) > 2048) {
            double k = 2048.0 / std::max(im.w, im.h);
            Rgba sm; resize_rgba(im, std::max(1, (int)(im.w * k + 0.5)), std::max(1, (int)(im.h * k + 0.5)), sm);
            im = std::move(sm);
        }
        std::vector<u8> png;
        fs::path hedef = benzersiz_yol(klasor, taban, ".png");
        if (!g_codec->encode_png(im, png) || !write_bytes(hedef, png)) { sonuc = L("Yükleme başarısız"); return false; }
        sonuc = path_str(hedef.filename());
        return true;
    } catch (const std::exception&) {
        sonuc = L("Yükleme başarısız");
        return false;
    }
}
