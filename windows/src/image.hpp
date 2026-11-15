// Gorsel yukleme / hazirlama (tasinabilir). Windows'ta kodek olarak WIC kullanilir (win_codec.cpp).
#pragma once
#include "util.hpp"
#include <atomic>
#include <memory>

struct Rgba { int w = 0, h = 0; std::vector<u8> px; };                 // duz (premultiplied degil) RGBA

struct ImgFrame { std::vector<u8> bgra; int ms = 0; };                 // premultiplied BGRA, kaynak cozunurlukte
struct ImgData {
    int w = 0, h = 0;
    std::vector<ImgFrame> frames;
    size_t bytes() const { return (size_t)w * h * 4 * frames.size(); }
    int total_ms() const { int t = 0; for (auto& f : frames) t += f.ms; return t < 1 ? 1 : t; }
};

struct CodecInfo { std::string format; int w = 0, h = 0, orient = 1; };   // format: GIF PNG WEBP JPEG OTHER
struct Codec {
    virtual ~Codec() {}
    virtual bool probe(const std::string& path, CodecInfo& info) = 0;          // dosya gercekten gorsel mi? (dogrular)
    virtual bool decode(const std::string& path, Rgba& out, int& orient) = 0;  // ilk kare, yon uygulanmamis
    virtual bool encode_png(const Rgba& img, std::vector<u8>& out) = 0;
    virtual bool encode_jpeg(const Rgba& img, int quality, std::vector<u8>& out) = 0;
};
extern Codec* g_codec;

void apply_orientation(Rgba& img, int orient);                                  // EXIF 1..8
bool decode_first_frame(const std::string& path, Rgba& out);                    // GIF: kendi cozucu, digerleri: kodek (+yon)
bool load_image(const std::string& path, int fps_limit, std::shared_ptr<ImgData>& out,
                std::string* err = nullptr, const std::atomic<bool>* cancel = nullptr);
bool make_thumbnail_png(const std::string& path, int size, std::vector<u8>& out);
void resize_rgba(const Rgba& in, int nw, int nh, Rgba& out);                    // kenar rengi sizdirmadan
// Ekranda gosterilecek pencere boyutu. boz=true: oran*ekran'i TAM doldurur (max_buyutme uygulanmaz).
void display_size(int w, int h, int sw, int sh, double oran, bool boz, double max_buyutme, int& dw, int& dh);
