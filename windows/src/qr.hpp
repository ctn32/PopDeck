// QR kod uretici (Surum 1-10, byte modu, L/M hata duzeltme) - tasinabilir
#pragma once
#include "util.hpp"
struct QrCode { int size = 0; std::vector<std::vector<bool>> m; };
bool qr_encode(const std::string& data, bool medium, QrCode& out);
// testler icin
std::vector<u8> qr_rs_remainder(const std::vector<u8>& data, int ecc_len);
