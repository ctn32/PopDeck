// Bilinear olcekleme (premultiplied BGRA/RGBA 32-bit) + yardimcilar
#pragma once
#include "util.hpp"

// src: sw*sh*4 -> dst: dw*dh*4. Cok cekirdekli (buyuk hedeflerde). Her kanal ayri lerp'lenir.
void scale_bilinear(const u8* src, int sw, int sh, u8* dst, int dw, int dh);
void premultiply_swap_rb(const u8* rgba, u8* bgra_premul, size_t pixels);   // RGBA -> premultiplied BGRA
void unpremultiply_swap_rb(const u8* bgra_premul, u8* rgba, size_t pixels); // premultiplied BGRA -> RGBA
