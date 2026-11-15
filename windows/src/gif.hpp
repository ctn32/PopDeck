// GIF cozucu (LZW, disposal, seffaflik) - tasinabilir
#pragma once
#include "util.hpp"

struct RawFrame { std::vector<u8> rgba; int delay_ms = 0; };      // tam tuval, duz (premultiplied degil) RGBA
struct RawImage { int w = 0, h = 0; std::vector<RawFrame> frames; };

// Kare kare akis: cb(tam_tuval_RGBA, w, h, gecikme_ms) false dondururse durur. Bellek dostu.
typedef std::function<bool(const u8*, int, int, int)> GifFrameCb;
bool gif_stream(const u8* data, size_t size, bool first_only, const GifFrameCb& cb);

// first_only: sadece ilk kare (onizleme icin). Basarisizsa false.
bool gif_decode(const u8* data, size_t size, RawImage& out, bool first_only = false, size_t max_bytes = (size_t)3 << 30);
bool gif_check_header(const u8* data, size_t size, int* w = nullptr, int* h = nullptr);
