// Dil sistemi: Turkce metin = anahtar. Ceviriler diller/*.json (gomulu + exe yanindaki klasor)
#pragma once
#include "util.hpp"
#include <map>

extern std::vector<std::string> g_lang_codes, g_lang_names;
void lang_load(const fs::path& exe_dir);
void lang_set(const std::string& code);
std::string lang_cur();
std::string L(const std::string& key, const std::vector<std::string>& args = {});
std::string lang_dict_json();                 // Secili dilin sozlugu (web sayfasina gomulur)
int lang_index(const std::string& code);
