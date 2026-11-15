// Tus adlari <-> sanal tus kodlari (Windows VK degerleri sayi olarak; platformdan bagimsiz)
#pragma once
#include "util.hpp"

enum { MOD_C = 1, MOD_A = 2, MOD_S = 4, MOD_W = 8 };
struct HotKey { int mods = 0; int vk = 0; bool ok = false; };

HotKey parse_hotkey(const std::string& s);           // "ctrl+shift+f11"
int vk_from_name(const std::string& name);           // 0 = bilinmiyor
std::string vk_name(int vk);                         // "" = bilinmiyor (platform yedegi: g_vk_name_fallback)
int normalize_vk(int vk);                            // sol/sag modifier -> 0x10/0x11/0x12/0x5B
int mod_bit(int nvk);                                // normalize edilmis modifier -> bit (degilse 0)
std::string combo_name(const std::vector<int>& vks); // basilan tuslar -> "ctrl+shift+a"
extern std::function<std::string(int)> g_vk_name_fallback;
extern std::function<int(const std::string&)> g_name_vk_fallback;
