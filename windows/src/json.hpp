// Kucuk JSON okuyucu/yazici (sirayi korur, UTF-8)
#pragma once
#include <string>
#include <vector>
#include <utility>
#include <cmath>
#include <cstdio>
#include <cstdlib>

struct Json {
    enum Type { Null, Bool, Num, Str, Arr, Obj } t = Null;
    bool b = false;
    double n = 0;
    bool is_int = false;
    std::string s;
    std::vector<Json> a;
    std::vector<std::pair<std::string, Json>> o;

    Json() {}
    static Json null() { return Json(); }
    static Json boolean(bool v) { Json j; j.t = Bool; j.b = v; return j; }
    static Json number(double v) { Json j; j.t = Num; j.n = v; j.is_int = false; return j; }
    static Json integer(long long v) { Json j; j.t = Num; j.n = (double)v; j.is_int = true; return j; }
    static Json str(const std::string& v) { Json j; j.t = Str; j.s = v; return j; }
    static Json array() { Json j; j.t = Arr; return j; }
    static Json object() { Json j; j.t = Obj; return j; }

    bool is_obj() const { return t == Obj; }
    bool is_arr() const { return t == Arr; }
    bool is_str() const { return t == Str; }
    bool is_num() const { return t == Num; }
    bool is_bool() const { return t == Bool; }

    const Json* get(const std::string& k) const {
        if (t != Obj) return nullptr;
        for (auto& kv : o) if (kv.first == k) return &kv.second;
        return nullptr;
    }
    Json& set(const std::string& k, const Json& v) {
        for (auto& kv : o) if (kv.first == k) { kv.second = v; return kv.second; }
        o.push_back({k, v});
        return o.back().second;
    }
    void push(const Json& v) { a.push_back(v); }
    std::string str_or(const std::string& k, const std::string& d) const { auto p = get(k); return (p && p->t == Str) ? p->s : d; }
    double num_or(const std::string& k, double d) const { auto p = get(k); return (p && p->t == Num) ? p->n : d; }
    bool bool_or(const std::string& k, bool d) const { auto p = get(k); return (p && p->t == Bool) ? p->b : d; }

    static bool parse(const std::string& src, Json& out) {
        size_t i = 0;
        if (!parse_value(src, i, out, 0)) return false;
        skip(src, i);
        return i == src.size();
    }
    std::string dump(int indent = -1) const { std::string r; write(r, indent, 0); return r; }

private:
    static void skip(const std::string& s, size_t& i) {
        if (i == 0 && s.size() >= 3 && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) i = 3;
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) i++;
    }
    static void put_utf8(std::string& o, unsigned c) {
        if (c < 0x80) o += (char)c;
        else if (c < 0x800) { o += (char)(0xC0 | (c >> 6)); o += (char)(0x80 | (c & 63)); }
        else if (c < 0x10000) { o += (char)(0xE0 | (c >> 12)); o += (char)(0x80 | ((c >> 6) & 63)); o += (char)(0x80 | (c & 63)); }
        else { o += (char)(0xF0 | (c >> 18)); o += (char)(0x80 | ((c >> 12) & 63)); o += (char)(0x80 | ((c >> 6) & 63)); o += (char)(0x80 | (c & 63)); }
    }
    static bool hex4(const std::string& s, size_t i, unsigned& v) {
        if (i + 4 > s.size()) return false;
        v = 0;
        for (int k = 0; k < 4; k++) {
            char c = s[i + k]; v <<= 4;
            if (c >= '0' && c <= '9') v |= c - '0';
            else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
            else return false;
        }
        return true;
    }
    static bool parse_string(const std::string& s, size_t& i, std::string& out) {
        if (i >= s.size() || s[i] != '"') return false;
        i++;
        while (i < s.size()) {
            char c = s[i++];
            if (c == '"') return true;
            if (c == '\\') {
                if (i >= s.size()) return false;
                char e = s[i++];
                switch (e) {
                    case '"': out += '"'; break; case '\\': out += '\\'; break; case '/': out += '/'; break;
                    case 'b': out += '\b'; break; case 'f': out += '\f'; break; case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break; case 't': out += '\t'; break;
                    case 'u': {
                        unsigned v;
                        if (!hex4(s, i, v)) return false;
                        i += 4;
                        if (v >= 0xD800 && v < 0xDC00 && i + 6 <= s.size() && s[i] == '\\' && s[i + 1] == 'u') {
                            unsigned w;
                            if (hex4(s, i + 2, w) && w >= 0xDC00 && w < 0xE000) { v = 0x10000 + ((v - 0xD800) << 10) + (w - 0xDC00); i += 6; }
                        }
                        put_utf8(out, v);
                        break;
                    }
                    default: return false;
                }
            } else out += c;
        }
        return false;
    }
    static bool parse_value(const std::string& s, size_t& i, Json& out, int depth) {
        if (depth > 64) return false;
        skip(s, i);
        if (i >= s.size()) return false;
        char c = s[i];
        if (c == '{') {
            i++; out = object(); skip(s, i);
            if (i < s.size() && s[i] == '}') { i++; return true; }
            for (;;) {
                skip(s, i);
                std::string k;
                if (!parse_string(s, i, k)) return false;
                skip(s, i);
                if (i >= s.size() || s[i] != ':') return false;
                i++;
                Json v;
                if (!parse_value(s, i, v, depth + 1)) return false;
                out.set(k, v);
                skip(s, i);
                if (i < s.size() && s[i] == ',') { i++; continue; }
                if (i < s.size() && s[i] == '}') { i++; return true; }
                return false;
            }
        }
        if (c == '[') {
            i++; out = array(); skip(s, i);
            if (i < s.size() && s[i] == ']') { i++; return true; }
            for (;;) {
                Json v;
                if (!parse_value(s, i, v, depth + 1)) return false;
                out.a.push_back(v);
                skip(s, i);
                if (i < s.size() && s[i] == ',') { i++; continue; }
                if (i < s.size() && s[i] == ']') { i++; return true; }
                return false;
            }
        }
        if (c == '"') { out = Json(); out.t = Str; return parse_string(s, i, out.s); }
        if (s.compare(i, 4, "true") == 0) { out = boolean(true); i += 4; return true; }
        if (s.compare(i, 5, "false") == 0) { out = boolean(false); i += 5; return true; }
        if (s.compare(i, 4, "null") == 0) { out = Json(); i += 4; return true; }
        if (c == '-' || (c >= '0' && c <= '9')) {
            size_t st = i;
            bool isint = true;
            if (s[i] == '-') i++;
            while (i < s.size() && ((s[i] >= '0' && s[i] <= '9') || s[i] == '.' || s[i] == 'e' || s[i] == 'E' || s[i] == '+' || s[i] == '-')) {
                if (s[i] == '.' || s[i] == 'e' || s[i] == 'E') isint = false;
                i++;
            }
            std::string num = s.substr(st, i - st);
            char* end = nullptr;
            double d = strtod(num.c_str(), &end);
            if (!end || *end) return false;
            out = Json(); out.t = Num; out.n = d; out.is_int = isint;
            return true;
        }
        return false;
    }
    static void esc(std::string& r, const std::string& s) {
        r += '"';
        for (unsigned char c : s) {
            switch (c) {
                case '"': r += "\\\""; break; case '\\': r += "\\\\"; break;
                case '\n': r += "\\n"; break; case '\r': r += "\\r"; break; case '\t': r += "\\t"; break;
                case '\b': r += "\\b"; break; case '\f': r += "\\f"; break;
                default:
                    if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); r += b; }
                    else r += (char)c;
            }
        }
        r += '"';
    }
    void write(std::string& r, int ind, int lvl) const {
        auto nl = [&](int l) { if (ind >= 0) { r += '\n'; r.append((size_t)(ind * l), ' '); } };
        switch (t) {
            case Null: r += "null"; break;
            case Bool: r += b ? "true" : "false"; break;
            case Num: {
                char buf[40];
                if (is_int || (std::floor(n) == n && std::fabs(n) < 1e15)) {
                    if (is_int) snprintf(buf, sizeof buf, "%lld", (long long)n);
                    else snprintf(buf, sizeof buf, "%.1f", n);
                } else snprintf(buf, sizeof buf, "%.15g", n);
                r += buf; break;
            }
            case Str: esc(r, s); break;
            case Arr:
                if (a.empty()) { r += "[]"; break; }
                r += '[';
                for (size_t i = 0; i < a.size(); i++) { if (i) r += ','; nl(lvl + 1); a[i].write(r, ind, lvl + 1); }
                nl(lvl); r += ']'; break;
            case Obj:
                if (o.empty()) { r += "{}"; break; }
                r += '{';
                for (size_t i = 0; i < o.size(); i++) {
                    if (i) r += ',';
                    nl(lvl + 1); esc(r, o[i].first); r += ind >= 0 ? ": " : ":"; o[i].second.write(r, ind, lvl + 1);
                }
                nl(lvl); r += '}'; break;
        }
    }
};
