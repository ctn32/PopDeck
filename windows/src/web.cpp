// Telefon kumandasi: yetki, listeler, tetikleme, yukleme, rotalar
#include "core.hpp"
#include "lang.hpp"
#include "image.hpp"
#include "embedded.hpp"
#include <set>
#include <cstring>

static std::string json_body(const Json& j) { return j.dump(); }
static HttpResponse jr(int code, const Json& j) { HttpResponse r; r.code = code; r.body = json_body(j); return r; }
static Json hata_json(const std::string& m) { Json j = Json::object(); j.set("ok", Json::boolean(false)); j.set("hata", Json::str(m)); return j; }
static bool const_eq(const std::string& a, const std::string& b) {
    unsigned d = (unsigned)(a.size() ^ b.size());
    for (size_t i = 0; i < a.size() && i < b.size(); i++) d |= (unsigned)((u8)a[i] ^ (u8)b[i]);
    return d == 0;
}

bool Cekirdek::web_yetki(const std::string& ip, const std::string& verilen, std::string& mesaj, int& kod) {
    Genel g = ayar.kopya();
    std::string pin = g.web_pin_yok ? "" : g.web_pin;
    if (pin.empty()) { kod = 200; return true; }
    double simdi = now_s();
    std::lock_guard<std::mutex> lk(wm);
    auto& e = web_hatalar[ip];
    if (simdi < e.second) { mesaj = L("Çok fazla yanlış deneme. 1 dakika bekle."); kod = 429; return false; }
    if (const_eq(verilen, pin)) { web_hatalar.erase(ip); kod = 200; return true; }
    e.first++;
    e.second = 0;
    if (e.first >= 5) { e.first = 0; e.second = simdi + 60; }
    mesaj = L("PIN yanlış"); kod = 401;
    return false;
}

Json Cekirdek::web_klasor_listesi() {
    Json arr = Json::array();
    for (auto& t : ayar.tuslar_kopya()) {
        std::vector<std::string> g, s;
        klasor_dosyalari(t.first, g, s);
        Json o = Json::object();
        o.set("ad", Json::str(t.first));
        o.set("tus", Json::str(t.second.empty() ? "" : [&] { std::string u = t.second; for (auto& c : u) if (c >= 'a' && c <= 'z') c = (char)(c - 32); return u; }()));
        o.set("gorsel", Json::integer((long long)g.size()));
        o.set("ses", Json::integer((long long)s.size()));
        arr.push(o);
    }
    return arr;
}

std::vector<Cekirdek::Oge> Cekirdek::klasor_ogeleri(const std::string& ad) {
    std::vector<Oge> r;
    if (!ayar.klasor_var(ad)) return r;
    std::vector<std::string> g, s;
    klasor_dosyalari(ad, g, s);
    std::set<std::string> tabanlar;
    for (auto& x : g) tabanlar.insert(lower_u8(strip_ext(x)));
    for (auto& x : g) r.push_back({base_name(x), x, "gorsel"});
    for (auto& x : s) if (!tabanlar.count(lower_u8(strip_ext(x)))) r.push_back({base_name(x), x, "ses"});
    return r;
}

static std::string upper_ascii(std::string u) { for (auto& c : u) if (c >= 'a' && c <= 'z') c = (char)(c - 32); return u; }

Json Cekirdek::web_gorsel_listesi() {
    std::vector<std::string> fav, son;
    std::map<std::string, std::vector<std::string>> sira;
    deck.anlik(fav, sira, son);
    Json klasorler = Json::array();
    std::set<std::string> mevcut;
    for (auto& t : ayar.tuslar_kopya()) {
        struct O { std::string d, t; long long v; };
        std::vector<O> ogeler;
        for (auto& o : klasor_ogeleri(t.first)) {
            std::error_code ec;
            auto ft = fs::last_write_time(path_u8(o.yol), ec);
            if (ec) continue;
            long long surum = (long long)std::chrono::duration_cast<std::chrono::seconds>(ft.time_since_epoch()).count();
            std::string e = ext_of(o.dosya);
            ogeler.push_back({o.dosya, e.empty() ? "" : e.substr(1), surum});
            mevcut.insert(t.first + "/" + o.dosya);
        }
        std::map<std::string, size_t> sk;
        { auto it = sira.find(t.first); if (it != sira.end()) for (size_t i = 0; i < it->second.size(); i++) sk[it->second[i]] = i; }
        std::sort(ogeler.begin(), ogeler.end(), [&](const O& a, const O& b) {
            size_t ia = sk.count(a.d) ? sk[a.d] : (size_t)1e9, ib = sk.count(b.d) ? sk[b.d] : (size_t)1e9;
            if (ia != ib) return ia < ib;
            return lower_u8(a.d) < lower_u8(b.d);
        });
        Json ja = Json::array();
        for (auto& o : ogeler) {
            Json x = Json::object();
            x.set("d", Json::str(o.d)); x.set("t", Json::str(o.t)); x.set("v", Json::integer(o.v));
            ja.push(x);
        }
        Json k = Json::object();
        k.set("ad", Json::str(t.first));
        k.set("tus", Json::str(upper_ascii(t.second)));
        k.set("ogeler", ja);
        klasorler.push(k);
    }
    Json f = Json::array(), s = Json::array();
    for (auto& i : fav) if (mevcut.count(i)) f.push(Json::str(i));
    for (auto& i : son) if (mevcut.count(i)) s.push(Json::str(i));
    Json r = Json::object();
    r.set("klasorler", klasorler); r.set("favoriler", f); r.set("son", s);
    return r;
}

bool Cekirdek::web_oge(const std::string& ad, const std::string& dosya, std::string& yol, std::string& tur) {
    for (auto& o : klasor_ogeleri(ad)) if (o.dosya == dosya) { yol = o.yol; tur = o.tur; return true; }
    return false;
}

bool Cekirdek::web_deck_islem(const Json& veri, std::string& hata) {
    std::string islem = veri.str_or("islem", "");
    if (islem == "favori") {
        std::string kimlik = veri.str_or("id", "");
        bool deger = veri.bool_or("deger", false);
        if (deger) {
            size_t p = kimlik.find('/');
            std::string k = p == std::string::npos ? kimlik : kimlik.substr(0, p), d = p == std::string::npos ? "" : kimlik.substr(p + 1);
            std::string y, t;
            if (!web_oge(k, d, y, t)) { hata = L("Öğe bulunamadı"); return false; }
        }
        deck.favori_ayarla(kimlik, deger);
        return true;
    }
    if (islem == "favori_sira") {
        const Json* l = veri.get("idler");
        if (!l || !l->is_arr()) { hata = L("Geçersiz liste"); return false; }
        std::vector<std::string> v;
        for (auto& x : l->a) v.push_back(x.is_str() ? x.s : "");
        deck.favori_sirala(v);
        return true;
    }
    if (islem == "sira") {
        std::string ad = veri.str_or("klasor", "");
        const Json* l = veri.get("dosyalar");
        if (!ayar.klasor_var(ad) || !l || !l->is_arr()) { hata = L("Geçersiz istek"); return false; }
        std::set<std::string> var;
        for (auto& o : klasor_ogeleri(ad)) var.insert(o.dosya);
        std::vector<std::string> v;
        for (auto& x : l->a) if (x.is_str() && var.count(x.s)) v.push_back(x.s);
        deck.sira_ayarla(ad, v);
        return true;
    }
    hata = L("Bilinmeyen işlem");
    return false;
}

bool Cekirdek::web_yukle(std::string klasor, const std::string& dosya, bool yeni, const std::function<size_t(u8*, size_t)>& read,
                         long long uzunluk, std::string& sonuc, std::string& klasor_out) {
    klasor_out = klasor;
    if (!ayar.kopya().web_yukleme) { sonuc = L("Telefondan dosya yükleme kapalı"); return false; }
    if (uzunluk <= 0 || uzunluk > (long long)YUKLEME_LIMIT_MB * 1048576LL) { sonuc = L("Dosya çok büyük (en fazla {0} MB)", {S(YUKLEME_LIMIT_MB)}); return false; }
    klasor = trim(klasor);
    std::string esles;
    for (auto& k : klasor_listesi()) if (lower_u8(k) == lower_u8(klasor)) { esles = k; break; }
    if (!esles.empty()) klasor = esles;
    else {
        if (!yeni) { sonuc = L("Klasör bulunamadı"); klasor_out = klasor; return false; }
        if (klasor.empty() || name_has_invalid(klasor) || klasor[0] == '.' || klasor[0] == '_' || klasor.back() == '.') {
            sonuc = L("Geçersiz ad"); klasor_out = klasor; return false;
        }
        std::error_code ec;
        if (!fs::create_directories(g_main_dir / path_u8(klasor), ec) || ec) { sonuc = L("Yükleme başarısız"); klasor_out = klasor; return false; }
    }
    klasor_out = klasor;
    bool ok = yukleme_kaydet(g_main_dir / path_u8(klasor), dosya, read, uzunluk, sonuc);
    if (ok && post) post(Cmd{"yuklendi", klasor, sonuc, false});
    return ok;
}

// ---- Telefonda oynatma yardimcilari
static long long gif_sure_ms(const std::string& d) {                 // GIF kare gecikmelerinin toplami
    const u8* p = (const u8*)d.data(); size_t n = d.size();
    if (n < 13 || memcmp(p, "GIF", 3) != 0) return 0;
    size_t i = 13;
    if (p[10] & 0x80) i += 3 * ((size_t)1 << ((p[10] & 7) + 1));
    long long tot = 0; int delay = 0;
    while (i < n) {
        u8 b = p[i++];
        if (b == 0x3B) break;
        if (b == 0x21) {
            if (i >= n) break;
            u8 lab = p[i++];
            if (lab == 0xF9 && i + 4 <= n && p[i] >= 4) delay = p[i + 2] | (p[i + 3] << 8);
            while (i < n) { u8 sz = p[i++]; if (!sz) break; i += sz; }
        } else if (b == 0x2C) {
            if (i + 9 > n) break;
            u8 fl = p[i + 8]; i += 9;
            if (fl & 0x80) i += 3 * ((size_t)1 << ((fl & 7) + 1));
            i++;
            while (i < n) { u8 sz = p[i++]; if (!sz) break; i += sz; }
            tot += (delay <= 1 ? 10 : delay) * 10LL; delay = 0;
        } else break;
    }
    return tot;
}

static std::string mime_of(const std::string& ext) {
    if (ext == ".gif") return "image/gif";
    if (ext == ".png") return "image/png";
    if (ext == ".webp") return "image/webp";
    if (ext == ".jpg" || ext == ".jpeg" || ext == ".jfif") return "image/jpeg";
    if (ext == ".bmp") return "image/bmp";
    if (ext == ".ico") return "image/x-icon";
    if (ext == ".tif" || ext == ".tiff") return "image/tiff";
    if (ext == ".avif") return "image/avif";
    if (ext == ".mp3") return "audio/mpeg";
    if (ext == ".wav") return "audio/wav";
    return "application/octet-stream";
}

bool Cekirdek::web_dosya_yolu(const std::string& ad, const std::string& dosya, std::string& yol) {
    if (!ayar.klasor_var(ad) || dosya.empty()) return false;
    std::vector<std::string> g, s;
    klasor_dosyalari(ad, g, s);
    for (auto* v : {&g, &s}) for (auto& x : *v) if (base_name(x) == dosya) { yol = x; return true; }
    return false;
}

bool Cekirdek::web_telefon(std::string ad, const std::string& dosya, Json& out, std::string& hata) {
    std::string yol, tur = "gorsel";
    if (!dosya.empty()) {
        if (!web_oge(ad, dosya, yol, tur)) { hata = L("Dosya bulunamadı"); return false; }
    } else if (ad == "__rastgele__") {
        std::vector<std::string> tum;
        for (auto& t : ayar.tuslar_kopya()) { std::vector<std::string> g, s; klasor_dosyalari(t.first, g, s); tum.insert(tum.end(), g.begin(), g.end()); }
        if (tum.empty()) { hata = L("Hiçbir klasörde görsel yok"); return false; }
        yol = RASTGELE.sec("__hepsi__", tum);
        ad = base_name(dir_name(yol));
    } else if (!ayar.klasor_var(ad)) { hata = L("Klasör bulunamadı"); return false; }
    else {
        std::vector<std::string> g, s;
        klasor_dosyalari(ad, g, s);
        if (g.empty()) { hata = L("'{0}' klasörü boş", {ad}); return false; }
        yol = RASTGELE.sec(ad, g);
    }
    Genel g = ayar.kopya();
    out = Json::object();
    out.set("ok", Json::boolean(true));
    out.set("klasor", Json::str(ad));
    out.set("d", Json::str(base_name(yol)));
    out.set("tur", Json::str(tur));
    if (tur == "gorsel") {
        std::vector<std::string> gl, sl;
        klasor_dosyalari(ad, gl, sl);
        std::string ses = ses_bul(yol, gl, sl);
        out.set("ses", Json::str(ses.empty() ? "" : base_name(ses)));
        long long ms = 0;
        if (ext_of(yol) == ".gif") {
            std::error_code ec;
            auto sz = fs::file_size(path_u8(yol), ec);
            std::string data;
            if (!ec && sz < 120u * 1024 * 1024 && read_file(path_u8(yol), data)) ms = gif_sure_ms(data);
        }
        out.set("sure_ms", Json::integer(ms));
    }
    out.set("boyut", Json::integer((long long)(g.boyut_orani * 100 + 0.5)));
    out.set("boz", Json::boolean(g.orani_boz));
    out.set("fade", Json::number(g.fade_in));
    out.set("sure", Json::number(g.sure));
    out.set("sonuna", Json::boolean(g.gif_sonuna_kadar));
    out.set("ses_aktif", Json::boolean(g.ses_aktif));
    out.set("seviye", Json::integer(g.ses_seviyesi));
    out.set("gecikme", Json::integer(g.ses_gecikme));
    out.set("kes", Json::boolean(g.ses_gif_ile_kesil));
    deck.son_ekle(ad + "/" + base_name(yol));
    return true;
}

bool Cekirdek::web_tetik(std::string ad, const std::string& dosya, std::string& sonuc) {
    double simdi = now_s();
    std::string yol;
    bool var_dosya = !dosya.empty();
    if (var_dosya) {
        std::string tur;
        if (!web_oge(ad, dosya, yol, tur)) { sonuc = L("Dosya bulunamadı"); return false; }
        if (tur == "ses") {                                          // soundboard: sadece ses
            { std::lock_guard<std::mutex> lk(wm); if (simdi - web_son_ses < 0.2) { sonuc = L("Biraz yavaş :)"); return false; } web_son_ses = simdi; }
            if (post) post(Cmd{"web_ses", ad, yol, false});
            sonuc = dosya;
            return true;
        }
    }
    { std::lock_guard<std::mutex> lk(wm); if (simdi - web_son < 0.4) { sonuc = L("Biraz yavaş :)"); return false; } }
    bool kes = var_dosya;
    if (mesgul && !(kes && oturum_var)) { sonuc = L("Şu an bir GIF oynuyor"); return false; }
    if (var_dosya) sonuc = dosya;
    else if (ad == "__rastgele__") {
        std::vector<std::string> tum;
        for (auto& t : ayar.tuslar_kopya()) { std::vector<std::string> g, s; klasor_dosyalari(t.first, g, s); tum.insert(tum.end(), g.begin(), g.end()); }
        if (tum.empty()) { sonuc = L("Hiçbir klasörde görsel yok"); return false; }
        yol = RASTGELE.sec("__hepsi__", tum);
        ad = base_name(dir_name(yol));
        sonuc = ad;
    } else if (!ayar.klasor_var(ad)) { sonuc = L("Klasör bulunamadı"); return false; }
    else {
        std::vector<std::string> g, s;
        klasor_dosyalari(ad, g, s);
        if (g.empty()) { sonuc = L("'{0}' klasörü boş", {ad}); return false; }
        sonuc = ad;
    }
    { std::lock_guard<std::mutex> lk(wm); web_son = simdi; }
    if (post) post(Cmd{"web_tetik", ad, yol, kes});
    return true;
}

bool Cekirdek::kucuk_resim(const std::string& yol, std::vector<u8>& out) {
    std::error_code ec;
    auto ft = fs::last_write_time(path_u8(yol), ec);
    if (ec) return false;
    std::string anahtar = yol + "|" + std::to_string((long long)ft.time_since_epoch().count());
    {
        std::lock_guard<std::mutex> lk(km);
        auto it = kucuk_idx.find(anahtar);
        if (it != kucuk_idx.end()) { kucuk_lru.splice(kucuk_lru.begin(), kucuk_lru, it->second); out = it->second->second; return true; }
    }
    if (!make_thumbnail_png(yol, 192, out)) return false;
    std::lock_guard<std::mutex> lk(km);
    if (!kucuk_idx.count(anahtar)) {
        kucuk_lru.push_front({anahtar, out});
        kucuk_idx[anahtar] = kucuk_lru.begin();
        while (kucuk_lru.size() > 150) { kucuk_idx.erase(kucuk_lru.back().first); kucuk_lru.pop_back(); }
    }
    return true;
}

std::string Cekirdek::web_sayfasi() {
    const Embedded* e = embedded_find("web.html");
    std::string html = e ? std::string((const char*)e->data, e->size) : "<html><body>web.html yok</body></html>";
    std::string uz = "[";
    bool ilk = true;
    for (auto& x : yukleme_uzantilari()) { if (!ilk) uz += ","; ilk = false; uz += "\"" + x.substr(1) + "\""; }
    uz += "]";
    html = replace_all(html, "/*DIL*/{}", lang_dict_json());
    html = replace_all(html, "/*UZ*/[]", uz);
    html = replace_all(html, "<html lang=\"tr\">", "<html lang=\"" + lang_cur() + "\">");
    return html;
}

HttpResponse Cekirdek::web_isle(HttpRequest& rq) {
    const std::string& yol = rq.path;
    auto yetkili = [&](HttpResponse& hata) -> bool {
        auto q = rq.q();
        std::string verilen;
        auto it = rq.h.find("x-pin");
        if (it != rq.h.end()) verilen = url_decode(it->second, false);
        if (verilen.empty() && q.count("pin")) verilen = q["pin"];
        std::string mesaj; int kod = 200;
        if (web_yetki(rq.peer_ip, verilen, mesaj, kod)) return true;
        hata = jr(kod, hata_json(mesaj));
        return false;
    };
    HttpResponse r;
    if (rq.method == "GET") {
        if (yol == "/" || yol == "/index.html") { r.type = "text/html; charset=utf-8"; r.body = web_sayfasi(); return r; }
        if (yol == "/api/klasorler") {
            if (!yetkili(r)) return r;
            Json j = Json::object(); j.set("ok", Json::boolean(true)); j.set("klasorler", web_klasor_listesi());
            return jr(200, j);
        }
        if (yol == "/api/gorseller") {
            if (!yetkili(r)) return r;
            Json j = web_gorsel_listesi(); j.set("ok", Json::boolean(true));
            return jr(200, j);
        }
        if (yol == "/api/kucuk") {
            if (!yetkili(r)) return r;
            auto q = rq.q();
            std::string y, t;
            if (!web_oge(q["k"], q["d"], y, t) || t != "gorsel") return jr(404, hata_json(L("Dosya bulunamadı")));
            std::vector<u8> png;
            if (!kucuk_resim(y, png)) return jr(500, hata_json(L("Önizleme üretilemedi")));
            r.type = "image/png"; r.cache = "private, max-age=86400";
            r.body.assign((const char*)png.data(), png.size());
            return r;
        }
        if (yol == "/api/dosya") {
            if (!yetkili(r)) return r;
            auto q = rq.q();
            std::string y;
            if (!web_dosya_yolu(q["k"], q["d"], y)) return jr(404, hata_json(L("Dosya bulunamadı")));
            std::error_code ec;
            auto sz = fs::file_size(path_u8(y), ec);
            std::string data;
            if (ec || sz > 120u * 1024 * 1024 || !read_file(path_u8(y), data)) return jr(404, hata_json(L("Dosya bulunamadı")));
            r.type = mime_of(ext_of(y)); r.cache = "private, max-age=3600";
            r.extra.push_back({"Accept-Ranges", "bytes"});
            auto it = rq.h.find("range");
            if (it != rq.h.end() && starts_with(it->second, "bytes=")) {
                std::string spec = it->second.substr(6);
                size_t dash = spec.find('-');
                long long total = (long long)data.size(), a = 0, b = total - 1;
                if (dash != std::string::npos) {
                    std::string s1 = spec.substr(0, dash), s2 = spec.substr(dash + 1);
                    if (s1.empty() && !s2.empty()) { a = std::max<long long>(0, total - atoll(s2.c_str())); }
                    else { a = atoll(s1.c_str()); if (!s2.empty()) b = std::min<long long>(total - 1, atoll(s2.c_str())); }
                }
                if (a >= total || a > b) {
                    r.code = 416; r.body.clear();
                    r.extra.push_back({"Content-Range", "bytes */" + std::to_string(total)});
                    return r;
                }
                r.code = 206;
                r.extra.push_back({"Content-Range", "bytes " + std::to_string(a) + "-" + std::to_string(b) + "/" + std::to_string(total)});
                r.body = data.substr((size_t)a, (size_t)(b - a + 1));
                return r;
            }
            r.body = std::move(data);
            return r;
        }
        if (yol == "/favicon.ico") { r.code = 204; r.type = "image/x-icon"; return r; }
        return jr(404, hata_json(L("Bulunamadı")));
    }
    if (yol != "/api/tetik" && yol != "/api/deck" && yol != "/api/durdur" && yol != "/api/yukle" && yol != "/api/telefon") return jr(404, hata_json(L("Bulunamadı")));
    if (!yetkili(r)) return r;
    if (yol == "/api/yukle") {
        auto q = rq.q();
        if (rq.set_timeout) rq.set_timeout(120);
        std::string sonuc, klasor;
        bool ok = web_yukle(q["k"], q["d"], q["yeni"] == "1", rq.body_read, rq.content_length, sonuc, klasor);
        Json j = Json::object();
        j.set("ok", Json::boolean(ok));
        if (ok) { j.set("dosya", Json::str(sonuc)); j.set("klasor", Json::str(klasor)); } else j.set("hata", Json::str(sonuc));
        return jr(200, j);
    }
    Json veri;
    {
        long long n = std::min<long long>(rq.content_length, 65536);
        std::string body((size_t)n, '\0');
        size_t got = 0;
        while (got < (size_t)n) { size_t k = rq.body_read((u8*)&body[got], (size_t)n - got); if (!k) break; got += k; }
        body.resize(got);
        if (body.empty()) body = "{}";
        if (!Json::parse(body, veri) || !veri.is_obj()) return jr(400, hata_json(L("Geçersiz istek")));
    }
    if (yol == "/api/durdur") { if (post) post(Cmd{"durdur", "", "", false}); Json j = Json::object(); j.set("ok", Json::boolean(true)); return jr(200, j); }
    if (yol == "/api/deck") {
        std::string h;
        bool ok = web_deck_islem(veri, h);
        Json j = Json::object(); j.set("ok", Json::boolean(ok)); j.set("hata", Json::str(h));
        return jr(200, j);
    }
    if (yol == "/api/telefon") {
        Json j; std::string h;
        if (!web_telefon(veri.str_or("klasor", ""), veri.str_or("dosya", ""), j, h)) { Json e = Json::object(); e.set("ok", Json::boolean(false)); e.set("hata", Json::str(h)); return jr(200, e); }
        return jr(200, j);
    }
    std::string sonuc;
    std::string dosya = veri.str_or("dosya", "");
    bool ok = web_tetik(veri.str_or("klasor", ""), dosya, sonuc);
    Json j = Json::object();
    j.set("ok", Json::boolean(ok));
    if (ok) j.set("klasor", Json::str(sonuc)); else j.set("hata", Json::str(sonuc));
    return jr(200, j);
}
