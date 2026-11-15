// Linux'ta calisan birim testleri (tasinabilir cekirdek)
#include "../src/util.hpp"
#include "../src/json.hpp"
#include "../src/gif.hpp"
#include "../src/scale.hpp"
#include "../src/qr.hpp"
#include "../src/keys.hpp"
#include "../src/image.hpp"
#include "../src/lang.hpp"
#include "../src/files.hpp"
#include "../src/settings.hpp"
#include "../src/deck.hpp"
#include <cassert>
#include <iostream>
#include <cstring>
#include <fstream>
#define CHECK(c) do { if (!(c)) { std::cerr << "FAIL " << __LINE__ << ": " #c "\n"; fails++; } } while (0)
static int fails = 0;

int main(int argc, char** argv) {
    // ---- JSON
    { Json j; CHECK(Json::parse("{\"a\":1,\"b\":[true,null,\"\\u00fc\\ud83d\\ude00\"],\"c\":3.5}", j));
      CHECK(j.get("a")->n == 1); CHECK(j.get("b")->a[2].s == "\xC3\xBC\xF0\x9F\x98\x80"); CHECK(j.get("c")->n == 3.5);
      std::string d = j.dump(); Json k; CHECK(Json::parse(d, k)); CHECK(k.dump() == d);
      CHECK(!Json::parse("{\"a\":", j)); }
    // ---- util
    CHECK(wide_to_utf8(utf8_to_wide("T\xC3\xBCrk\xC3\xA7" "e \xF0\x9F\x98\x80")) == "T\xC3\xBCrk\xC3\xA7" "e \xF0\x9F\x98\x80");
    CHECK(url_decode("a%20b+c", true) == "a b c"); CHECK(url_decode("%C3%BC", false) == "\xC3\xBC");
    CHECK(base64({'M', 'a', 'n'}) == "TWFu"); CHECK(base64({'M', 'a'}) == "TWE=");
    CHECK(strip_ext("/a/b.c/d.GIF") == "/a/b.c/d"); CHECK(ext_of("x/y.PNG") == ".png"); CHECK(strip_ext("/a/.hid") == "/a/.hid");
    // ---- keys
    { HotKey h = parse_hotkey("ctrl+shift+f11"); CHECK(h.ok && h.mods == (MOD_C | MOD_S) && h.vk == 0x7A);
      h = parse_hotkey("f10"); CHECK(h.ok && h.mods == 0 && h.vk == 0x79);
      h = parse_hotkey("ctrl+g"); CHECK(h.ok && h.vk == 'G');
      h = parse_hotkey("ctrl+shift"); CHECK(h.ok && h.mods == MOD_C && h.vk == 0x10);
      CHECK(!parse_hotkey("").ok); CHECK(!parse_hotkey("ctrl+nokey").ok);
      CHECK(combo_name({0xA2, 0xA0, 0x7A}) == "ctrl+shift+f11"); CHECK(combo_name({0x47}) == "g"); CHECK(vk_name(0x27) == "right"); }
    // ---- QR: RS test vektoru (HELLO WORLD 1-M)
    { std::vector<u8> d = {32, 91, 11, 120, 209, 114, 220, 77, 67, 64, 236, 17, 236, 17, 236, 17};
      std::vector<u8> e = qr_rs_remainder(d, 10); std::vector<u8> exp = {196, 35, 39, 119, 235, 215, 231, 226, 93, 23};
      CHECK(e == exp); }
    // ---- QR dosyaya dondur (python ile dogrulanir)
    { const char* urls[] = {"http://192.168.1.25:8080", "http://192.168.1.25:8080/?pin=1234", "http://10.0.0.123:8090/?pin=abcdef123456",
                            "http://192.168.100.200:65535/?pin=%C3%BC%C3%BC%C3%BC%C3%BC%C3%BC%C3%BC"};
      int n = 0;
      for (bool med : {false, true}) for (auto u : urls) {
        QrCode q; CHECK(qr_encode(u, med, q));
        std::ofstream f(std::string("/tmp/qr_") + std::to_string(n++) + ".txt");
        f << (med ? "M" : "L") << "\n" << u << "\n" << q.size << "\n";
        for (auto& r : q.m) { for (bool b : r) f << (b ? '1' : '0'); f << "\n"; }
      } }
    // ---- olcekleme
    { std::vector<u8> s(4 * 4 * 4, 200), d(10 * 7 * 4, 0); scale_bilinear(s.data(), 4, 4, d.data(), 10, 7);
      bool ok = true; for (u8 v : d) if (v != 200) ok = false; CHECK(ok);
      std::vector<u8> big(2000 * 1500 * 4, 77), o2(1920 * 1080 * 4, 0); scale_bilinear(big.data(), 2000, 1500, o2.data(), 1920, 1080);
      ok = true; for (size_t i = 0; i < o2.size(); i += 4097) if (o2[i] != 77) ok = false; CHECK(ok);
      // gradyan: 2px -> 5px
      u8 two[8] = {0, 0, 0, 255, 255, 255, 255, 255}; u8 five[20]; scale_bilinear(two, 2, 1, five, 5, 1);
      CHECK(five[0] == 0 && five[16] == 255 && five[8] > five[4] && five[12] > five[8]); }
    // ---- display_size
    { int w, h; display_size(200, 150, 1920, 1080, 1.0, true, 2.0, w, h); CHECK(w == 1920 && h == 1080);
      display_size(200, 150, 3840, 2160, 1.0, true, 2.0, w, h); CHECK(w == 3840 && h == 2160);
      display_size(200, 150, 1920, 1080, 0.5, true, 2.0, w, h); CHECK(w == 960 && h == 540);
      display_size(200, 150, 1920, 1080, 1.0, false, 2.0, w, h); CHECK(w == 400 && h == 300);
      display_size(1000, 500, 1920, 1080, 1.0, false, 4.0, w, h); CHECK(w == 1920 && h == 960); }
    // ---- orientation
    { Rgba im; im.w = 2; im.h = 1; im.px = {1, 0, 0, 255, 2, 0, 0, 255}; apply_orientation(im, 6); CHECK(im.w == 1 && im.h == 2 && im.px[0] == 1 && im.px[4] == 2);
      Rgba i2; i2.w = 2; i2.h = 1; i2.px = {1, 0, 0, 255, 2, 0, 0, 255}; apply_orientation(i2, 8); CHECK(i2.px[0] == 2 && i2.px[4] == 1);
      Rgba i3; i3.w = 2; i3.h = 1; i3.px = {1, 0, 0, 255, 2, 0, 0, 255}; apply_orientation(i3, 2); CHECK(i3.px[0] == 2); }
    // ---- GIF
    if (argc > 1) {
        std::string d; CHECK(read_file(argv[1], d));
        RawImage ri; CHECK(gif_decode((const u8*)d.data(), d.size(), ri));
        std::cout << "gif " << ri.w << "x" << ri.h << " frames=" << ri.frames.size() << "\n";
        for (size_t i = 0; i < ri.frames.size(); i++) {
            std::ofstream f("/tmp/gif_out_" + std::to_string(i) + ".raw", std::ios::binary);
            f.write((const char*)ri.frames[i].rgba.data(), (std::streamsize)ri.frames[i].rgba.size());
            std::cout << "delay " << ri.frames[i].delay_ms << "\n";
        }
        std::shared_ptr<ImgData> img; std::string err;
        CHECK(load_image(argv[1], 30, img, &err)); if (img) std::cout << "load_image frames=" << img->frames.size() << " total=" << img->total_ms() << "\n";
        CHECK(load_image(argv[1], 60, img, &err)); if (img) std::cout << "load_image(60fps) frames=" << img->frames.size() << "\n";
    }
    std::cout << (fails ? "BASARISIZ" : "TUMU GECTI") << " (" << fails << ")\n";
    return fails ? 1 : 0;
}
