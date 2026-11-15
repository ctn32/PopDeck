#include "util.hpp"
#include <cctype>
#include <cwctype>
#include <fstream>
#include <ctime>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

fs::path g_log_path;

std::string wide_to_utf8(const std::wstring& w) {
    std::string o;
    for (size_t i = 0; i < w.size(); i++) {
        u32 c = (u32)w[i];
        if (sizeof(wchar_t) == 2 && c >= 0xD800 && c < 0xDC00 && i + 1 < w.size()) {
            u32 d = (u32)w[i + 1];
            if (d >= 0xDC00 && d < 0xE000) { c = 0x10000 + ((c - 0xD800) << 10) + (d - 0xDC00); i++; }
        }
        if (c < 0x80) o += (char)c;
        else if (c < 0x800) { o += (char)(0xC0 | (c >> 6)); o += (char)(0x80 | (c & 63)); }
        else if (c < 0x10000) { o += (char)(0xE0 | (c >> 12)); o += (char)(0x80 | ((c >> 6) & 63)); o += (char)(0x80 | (c & 63)); }
        else { o += (char)(0xF0 | (c >> 18)); o += (char)(0x80 | ((c >> 12) & 63)); o += (char)(0x80 | ((c >> 6) & 63)); o += (char)(0x80 | (c & 63)); }
    }
    return o;
}

std::wstring utf8_to_wide(const std::string& s) {
    std::wstring w;
    size_t i = 0, n = s.size();
    while (i < n) {
        u32 c = (u8)s[i];
        int extra = 0;
        if (c < 0x80) extra = 0;
        else if ((c >> 5) == 6) { c &= 31; extra = 1; }
        else if ((c >> 4) == 14) { c &= 15; extra = 2; }
        else if ((c >> 3) == 30) { c &= 7; extra = 3; }
        else { c = 0xFFFD; }
        i++;
        for (int k = 0; k < extra && i < n; k++, i++) c = (c << 6) | ((u8)s[i] & 63);
        if (sizeof(wchar_t) == 2 && c >= 0x10000) {
            c -= 0x10000;
            w += (wchar_t)(0xD800 + (c >> 10));
            w += (wchar_t)(0xDC00 + (c & 0x3FF));
        } else w += (wchar_t)c;
    }
    return w;
}

fs::path path_u8(const std::string& s) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(s));
#else
    return fs::path(s);
#endif
}

std::string path_str(const fs::path& p) {
#ifdef _WIN32
    return wide_to_utf8(p.wstring());
#else
    return p.string();
#endif
}

std::string ascii_lower(std::string s) {
    for (auto& c : s) if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
    return s;
}

std::string lower_u8(const std::string& s) {
    std::wstring w = utf8_to_wide(s);
#ifdef _WIN32
    if (!w.empty()) CharLowerBuffW(&w[0], (DWORD)w.size());
#else
    for (auto& c : w) c = (wchar_t)std::towlower((wint_t)c);
#endif
    return wide_to_utf8(w);
}

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (unsigned char)s[a] <= ' ') a++;
    while (b > a && (unsigned char)s[b - 1] <= ' ') b--;
    return s.substr(a, b - a);
}
bool starts_with(const std::string& s, const std::string& p) { return s.size() >= p.size() && s.compare(0, p.size(), p) == 0; }
bool ends_with(const std::string& s, const std::string& p) { return s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0; }

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> r;
    size_t a = 0;
    for (;;) {
        size_t b = s.find(sep, a);
        if (b == std::string::npos) { r.push_back(s.substr(a)); break; }
        r.push_back(s.substr(a, b - a));
        a = b + 1;
    }
    return r;
}
std::string join(const std::vector<std::string>& v, const std::string& sep) {
    std::string o;
    for (size_t i = 0; i < v.size(); i++) { if (i) o += sep; o += v[i]; }
    return o;
}
std::string replace_all(std::string s, const std::string& a, const std::string& b) {
    if (a.empty()) return s;
    size_t p = 0;
    while ((p = s.find(a, p)) != std::string::npos) { s.replace(p, a.size(), b); p += b.size(); }
    return s;
}
std::string S(long long v) { return std::to_string(v); }

static int hexv(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
std::string url_decode(const std::string& s, bool plus_is_space) {
    std::string o;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size() + 0 && hexv(s[i + 1]) >= 0 && hexv(s[i + 2]) >= 0) {
            o += (char)(hexv(s[i + 1]) * 16 + hexv(s[i + 2])); i += 2;
        } else if (s[i] == '+' && plus_is_space) o += ' ';
        else o += s[i];
    }
    return o;
}
std::string url_encode(const std::string& s) {
    static const char* hx = "0123456789ABCDEF";
    std::string o;
    for (unsigned char c : s) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') o += (char)c;
        else { o += '%'; o += hx[c >> 4]; o += hx[c & 15]; }
    }
    return o;
}
std::string base64(const std::vector<u8>& d) {
    static const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string o;
    size_t i = 0;
    for (; i + 2 < d.size(); i += 3) {
        u32 v = (d[i] << 16) | (d[i + 1] << 8) | d[i + 2];
        o += t[v >> 18]; o += t[(v >> 12) & 63]; o += t[(v >> 6) & 63]; o += t[v & 63];
    }
    if (i + 1 == d.size()) { u32 v = d[i] << 16; o += t[v >> 18]; o += t[(v >> 12) & 63]; o += "=="; }
    else if (i + 2 == d.size()) { u32 v = (d[i] << 16) | (d[i + 1] << 8); o += t[v >> 18]; o += t[(v >> 12) & 63]; o += t[(v >> 6) & 63]; o += '='; }
    return o;
}

bool read_file(const fs::path& p, std::string& out) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    std::streamoff n = f.tellg();
    if (n < 0) return false;
    f.seekg(0);
    out.resize((size_t)n);
    if (n) f.read(&out[0], n);
    return (bool)f || f.eof();
}

bool write_file_atomic(const fs::path& p, const std::string& data) {
    fs::path tmp = p;
    tmp += ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f.write(data.data(), (std::streamsize)data.size());
        if (!f) return false;
    }
    std::error_code ec;
    fs::rename(tmp, p, ec);              // Windows'ta hedef varsa da degistirir (MoveFileEx)
    if (ec) {
        fs::remove(p, ec);
        std::error_code ec2;
        fs::rename(tmp, p, ec2);
        if (ec2) { fs::remove(tmp, ec2); return false; }
    }
    return true;
}

double now_s() {
    return std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
}
double mono_ms() {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
std::mt19937_64& rng() {
    static std::mt19937_64 g((u64)std::random_device{}() ^ ((u64)std::chrono::steady_clock::now().time_since_epoch().count() << 1));
    return g;
}
void log_line(const std::string& s) {
    if (g_log_path.empty()) return;
    static std::mutex m;
    std::lock_guard<std::mutex> lk(m);
    std::ofstream f(g_log_path, std::ios::app);
    if (!f) return;
    time_t t = time(nullptr);
    char buf[32];
    strftime(buf, sizeof buf, "%H:%M:%S ", localtime(&t));
    f << buf << s << "\n";
}
