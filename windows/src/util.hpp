// PopDeck - ortak yardimcilar (tasinabilir: Windows'a bagimli degil)
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <mutex>
#include <filesystem>
#include <chrono>
#include <algorithm>
#include <random>

namespace fs = std::filesystem;
typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;

std::string wide_to_utf8(const std::wstring& w);
std::wstring utf8_to_wide(const std::string& s);
fs::path path_u8(const std::string& s);          // UTF-8 metinden yol
std::string path_str(const fs::path& p);          // yoldan UTF-8 metin
std::string lower_u8(const std::string& s);       // Unicode kucuk harf (Windows'ta tam, digerlerinde ASCII+)
std::string ascii_lower(std::string s);
std::string trim(const std::string& s);
bool starts_with(const std::string& s, const std::string& p);
bool ends_with(const std::string& s, const std::string& p);
std::vector<std::string> split(const std::string& s, char sep);
std::string join(const std::vector<std::string>& v, const std::string& sep);
std::string replace_all(std::string s, const std::string& a, const std::string& b);
std::string S(long long v);                       // sayi -> metin
std::string url_decode(const std::string& s, bool plus_is_space);
std::string url_encode(const std::string& s);
std::string base64(const std::vector<u8>& d);

bool read_file(const fs::path& p, std::string& out);
bool write_file_atomic(const fs::path& p, const std::string& data);   // gecici dosya + rename
double now_s();                                    // duvar saati (sn)
double mono_ms();                                  // monoton saat (ms)
std::mt19937_64& rng();
void log_line(const std::string& s);               // popdeck_log.txt'ye ekler (hata ayiklama)
extern fs::path g_log_path;
