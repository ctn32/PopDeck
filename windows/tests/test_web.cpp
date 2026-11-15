// Linux'ta web kumandasi uctan uca testi (sahte kodek ile)
#include "../src/core.hpp"
#include "../src/image.hpp"
#include "../src/lang.hpp"
#include "../src/gif.hpp"
#include <iostream>
#include <fstream>
#include <thread>
#include <csignal>
#include <zlib.h>

struct FakeCodec : Codec {
    bool probe(const std::string& p, CodecInfo& i) override {
        std::string d; if (!read_file(path_u8(p), d)) return false;
        if (gif_check_header((const u8*)d.data(), d.size(), &i.w, &i.h)) { i.format = "GIF"; return true; }
        if (d.size() > 24 && d.compare(1, 3, "PNG") == 0) { i.format = "PNG"; i.w = ((u8)d[16] << 24) | ((u8)d[17] << 16) | ((u8)d[18] << 8) | (u8)d[19]; i.h = ((u8)d[20] << 24) | ((u8)d[21] << 16) | ((u8)d[22] << 8) | (u8)d[23]; return true; }
        return false;
    }
    bool decode(const std::string&, Rgba&, int&) override { return false; }
    bool encode_png(const Rgba& im, std::vector<u8>& out) override {      // zlib ile gercek PNG
        auto be = [&](std::vector<u8>& v, u32 x) { v.push_back(x >> 24); v.push_back(x >> 16); v.push_back(x >> 8); v.push_back(x); };
        auto chunk = [&](const char* t, const std::vector<u8>& data) {
            be(out, (u32)data.size()); std::vector<u8> b(t, t + 4); b.insert(b.end(), data.begin(), data.end());
            out.insert(out.end(), b.begin(), b.end()); be(out, (u32)crc32(0, b.data(), (uInt)b.size()));
        };
        out = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
        std::vector<u8> ih; be(ih, im.w); be(ih, im.h); ih.push_back(8); ih.push_back(6); ih.push_back(0); ih.push_back(0); ih.push_back(0);
        chunk("IHDR", ih);
        std::vector<u8> raw; for (int y = 0; y < im.h; y++) { raw.push_back(0); raw.insert(raw.end(), im.px.begin() + (size_t)y * im.w * 4, im.px.begin() + (size_t)(y + 1) * im.w * 4); }
        uLongf n = compressBound((uLong)raw.size()); std::vector<u8> z(n); compress(z.data(), &n, raw.data(), (uLong)raw.size()); z.resize(n);
        chunk("IDAT", z); chunk("IEND", {});
        return true;
    }
    bool encode_jpeg(const Rgba&, int, std::vector<u8>&) override { return false; }
};

int main(int argc, char** argv) {
    int port = argc > 1 ? atoi(argv[1]) : 18080;
    fs::path root = "/tmp/pdtest";
    std::error_code ec; fs::remove_all(root, ec);
    g_main_dir = root / "main";
    fs::create_directories(g_main_dir / "Komik"); fs::create_directories(g_main_dir / "Ses"); fs::create_directories(g_main_dir / "Bos");
    fs::copy_file("/tmp/t1.gif", g_main_dir / "Komik" / "a.gif"); fs::copy_file("/tmp/t3.gif", g_main_dir / "Komik" / "b.gif");
    fs::copy_file("/tmp/t2.gif", g_main_dir / "Komik" / "b.gif.bak.gif");
    { std::ofstream f(g_main_dir / "Komik" / "a.mp3"); f << "ID3xxxxxxxxxxxx"; }
    { std::ofstream f(g_main_dir / "Ses" / "alkis.wav"); f << "RIFFxxxxWAVEfmt "; }
    fs::copy_file("/tmp/t3.gif", g_main_dir / "Ses" / "x.gif");
    FakeCodec fc; g_codec = &fc;
    lang_load(root); lang_set(argc > 2 ? argv[2] : "tr");
    g_log_path = root / "log.txt";
    Cekirdek c(root / "main" / "deck.json");
    c.ayar.yukle(); c.ayar.klasor_esitle(); c.ayar.tus_koy("Komik", "f9");
    { std::lock_guard<std::recursive_mutex> l(c.ayar.m); c.ayar.genel.web_pin_yok = false; c.ayar.genel.web_pin = "1234"; c.ayar.genel.web_port = port; }
    c.ayar.kaydet();
    c.post = [](const Cmd& m) { std::cout << "CMD " << m.tur << " " << m.a << " " << m.b << " kes=" << m.kes << std::endl; };
    HttpServer srv;
    if (!srv.start(port, [&](HttpRequest& r) { return c.web_isle(r); })) { std::cerr << "port acilamadi\n"; return 2; }
    std::cout << "READY" << std::endl;
    for (int i = 0; i < 600; i++) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    srv.stop();
    return 0;
}
