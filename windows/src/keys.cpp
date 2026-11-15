#include "keys.hpp"
#include <map>

std::function<std::string(int)> g_vk_name_fallback;
std::function<int(const std::string&)> g_name_vk_fallback;

namespace {
struct Tbl {
    std::map<std::string, int> by_name;
    std::map<int, std::string> by_vk;
    void add(const std::string& n, int vk, bool canon = true) { by_name[n] = vk; if (canon && !by_vk.count(vk)) by_vk[vk] = n; }
    Tbl() {
        add("esc", 0x1B); add("tab", 0x09); add("caps lock", 0x14); add("space", 0x20); add("enter", 0x0D); add("backspace", 0x08);
        add("up", 0x26); add("down", 0x28); add("left", 0x25); add("right", 0x27);
        add("page up", 0x21); add("page down", 0x22); add("home", 0x24); add("end", 0x23); add("insert", 0x2D); add("delete", 0x2E);
        add("print screen", 0x2C); add("scroll lock", 0x91); add("pause", 0x13); add("num lock", 0x90);
        add("shift", 0x10); add("ctrl", 0x11); add("alt", 0x12); add("windows", 0x5B);
        add("menu", 0x5D, false);
        for (int i = 1; i <= 24; i++) add("f" + std::to_string(i), 0x6F + i);
        for (int i = 0; i < 10; i++) { add(std::string(1, (char)('0' + i)), 0x30 + i); add("num " + std::to_string(i), 0x60 + i); }
        for (char c = 'a'; c <= 'z'; c++) add(std::string(1, c), 0x41 + (c - 'a'));
        add("num *", 0x6A); add("num +", 0x6B); add("num -", 0x6D); add("num .", 0x6E); add("num /", 0x6F);
        add(",", 0xBC); add(".", 0xBE); add("/", 0xBF); add(";", 0xBA); add("'", 0xDE); add("[", 0xDB); add("]", 0xDD);
        add("\\", 0xDC); add("-", 0xBD); add("=", 0xBB); add("`", 0xC0);
        // takma adlar
        add("control", 0x11, false); add("left ctrl", 0x11, false); add("right ctrl", 0x11, false);
        add("left shift", 0x10, false); add("right shift", 0x10, false);
        add("left alt", 0x12, false); add("right alt", 0x12, false); add("alt gr", 0x12, false);
        add("win", 0x5B, false); add("left windows", 0x5B, false); add("right windows", 0x5B, false); add("cmd", 0x5B, false);
        add("escape", 0x1B, false); add("return", 0x0D, false); add("del", 0x2E, false); add("ins", 0x2D, false);
        add("pgup", 0x21, false); add("pgdn", 0x22, false); add("pageup", 0x21, false); add("pagedown", 0x22, false);
        add("capslock", 0x14, false); add("prtsc", 0x2C, false); add("printscreen", 0x2C, false);
    }
};
Tbl& tbl() { static Tbl t; return t; }
}

int vk_from_name(const std::string& name) {
    std::string n = ascii_lower(trim(name));
    auto it = tbl().by_name.find(n);
    if (it != tbl().by_name.end()) return it->second;
    if (g_name_vk_fallback) return g_name_vk_fallback(n);
    return 0;
}

std::string vk_name(int vk) {
    vk = normalize_vk(vk);
    auto it = tbl().by_vk.find(vk);
    if (it != tbl().by_vk.end()) return it->second;
    if (g_vk_name_fallback) return g_vk_name_fallback(vk);
    return "";
}

int normalize_vk(int vk) {
    if (vk == 0xA0 || vk == 0xA1) return 0x10;
    if (vk == 0xA2 || vk == 0xA3) return 0x11;
    if (vk == 0xA4 || vk == 0xA5) return 0x12;
    if (vk == 0x5C) return 0x5B;
    return vk;
}
int mod_bit(int n) { return n == 0x11 ? MOD_C : n == 0x12 ? MOD_A : n == 0x10 ? MOD_S : n == 0x5B ? MOD_W : 0; }

HotKey parse_hotkey(const std::string& s) {
    HotKey h;
    std::string t = ascii_lower(trim(s));
    if (t.empty()) return h;
    std::vector<std::string> parts;
    size_t a = 0;
    for (size_t i = 0; i <= t.size(); i++) {                // "+" tusunun kendisi: "ctrl++"
        if (i == t.size() || (t[i] == '+' && i > a)) { parts.push_back(trim(t.substr(a, i - a))); a = i + 1; }
        else if (t[i] == '+' && i == a && i + 1 == t.size()) { parts.push_back("+"); a = i + 1; break; }
    }
    if (parts.empty()) return h;
    int mods = 0;
    for (size_t i = 0; i + 1 < parts.size(); i++) {
        int vk = vk_from_name(parts[i]);
        int b = mod_bit(normalize_vk(vk));
        if (!b) return h;
        mods |= b;
    }
    int vk = vk_from_name(parts.back());
    if (!vk) return h;
    h.mods = mods; h.vk = normalize_vk(vk); h.ok = true;
    return h;
}

std::string combo_name(const std::vector<int>& vks) {
    int mods = 0;
    std::vector<int> others;
    for (int v : vks) {
        int n = normalize_vk(v), b = mod_bit(n);
        if (b) mods |= b;
        else if (std::find(others.begin(), others.end(), n) == others.end()) others.push_back(n);
    }
    std::vector<std::string> p;
    if (mods & MOD_C) p.push_back("ctrl");
    if (mods & MOD_A) p.push_back("alt");
    if (mods & MOD_S) p.push_back("shift");
    if (mods & MOD_W) p.push_back("windows");
    for (int n : others) { std::string nm = vk_name(n); if (!nm.empty()) p.push_back(nm); }
    return join(p, "+");
}
