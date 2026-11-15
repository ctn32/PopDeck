#include "lang.hpp"
#include "json.hpp"
#include "embedded.hpp"
#include <set>

typedef std::map<std::string, std::string> Dict;
static std::map<std::string, Dict> g_tr;
static std::map<std::string, std::string> g_names;
static std::string g_cur = "tr";
static std::mutex g_lm;
std::vector<std::string> g_lang_codes, g_lang_names;

static void ingest(const std::string& code, const std::string& text) {
    Json j;
    if (!Json::parse(text, j) || !j.is_obj()) return;
    for (auto& kv : j.o) {
        if (kv.first == "_ad") { if (kv.second.is_str()) g_names[code] = kv.second.s; continue; }
        if (kv.second.is_str() && !kv.second.s.empty()) g_tr[code][kv.first] = kv.second.s;
    }
    if (!g_names.count(code)) g_names[code] = code;
}

void lang_load(const fs::path& exe_dir) {
    g_tr.clear(); g_names.clear();
    g_names["tr"] = "T\xC3\xBCrk\xC3\xA7" "e";
    size_t n = 0;
    const Embedded* e = embedded_list(n);
    for (size_t i = 0; i < n; i++) {                       // once gomulu
        std::string name = e[i].name;
        if (starts_with(name, "diller/") && ends_with(name, ".json"))
            ingest(name.substr(7, name.size() - 12), std::string((const char*)e[i].data, e[i].size));
    }
    std::error_code ec;                                    // sonra exe yanindaki (ustune yazar)
    fs::path d = exe_dir / "diller";
    if (fs::is_directory(d, ec)) {
        for (auto& ent : fs::directory_iterator(d, ec)) {
            std::string f = path_str(ent.path().filename());
            if (!ends_with(f, ".json") || starts_with(f, "_")) continue;
            std::string txt;
            if (read_file(ent.path(), txt)) ingest(f.substr(0, f.size() - 5), txt);
        }
    }
    static const char* sira[] = {"en", "de", "es", "fr", "ru"};
    g_lang_codes = {"tr"};
    std::set<std::string> used = {"tr"};
    for (auto s : sira) if (g_tr.count(s)) { g_lang_codes.push_back(s); used.insert(s); }
    for (auto& kv : g_tr) if (!used.count(kv.first)) g_lang_codes.push_back(kv.first);   // map = alfabetik
    g_lang_names.clear();
    for (auto& c : g_lang_codes) g_lang_names.push_back(g_names.count(c) ? g_names[c] : c);
}

void lang_set(const std::string& code) {
    std::lock_guard<std::mutex> lk(g_lm);
    g_cur = std::find(g_lang_codes.begin(), g_lang_codes.end(), code) != g_lang_codes.end() ? code : "tr";
}
std::string lang_cur() { std::lock_guard<std::mutex> lk(g_lm); return g_cur; }
int lang_index(const std::string& code) {
    for (size_t i = 0; i < g_lang_codes.size(); i++) if (g_lang_codes[i] == code) return (int)i;
    return 0;
}

std::string L(const std::string& key, const std::vector<std::string>& args) {
    std::string s = key;
    const std::string cur = lang_cur();
    if (cur != "tr") {
        auto it = g_tr.find(cur);
        if (it != g_tr.end()) { auto f = it->second.find(key); if (f != it->second.end() && !f->second.empty()) s = f->second; }
    }
    if (args.empty()) return s;
    std::string o;                                         // {0} {1} ... yer tutucular
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '{' && i + 2 < s.size() + 0 && isdigit((unsigned char)s[i + 1]) && s[i + 2] == '}') {
            size_t k = (size_t)(s[i + 1] - '0');
            if (k < args.size()) o += args[k];
            i += 2;
        } else o += s[i];
    }
    return o;
}

std::string lang_dict_json() {
    Json j = Json::object();
    const std::string cur = lang_cur();
    if (cur != "tr") {
        auto it = g_tr.find(cur);
        if (it != g_tr.end()) for (auto& kv : it->second) j.o.push_back({kv.first, Json::str(kv.second)});
    }
    return j.dump();
}
