// Kucuk HTTP sunucusu (Winsock / BSD soket; Python http.server davranisi: baglanti kapanir)
#pragma once
#include "util.hpp"
#include <map>
#include <atomic>
#include <thread>

struct HttpRequest {
    std::string method, path, query, peer_ip;
    std::map<std::string, std::string> h;                       // basliklar (kucuk harf)
    long long content_length = 0;
    std::function<size_t(u8*, size_t)> body_read;               // govdeyi akis olarak okur (0 = bitti/hata)
    std::function<void(int)> set_timeout;                       // alma zaman asimi (sn)
    std::map<std::string, std::string> q() const;               // sorgu parametreleri (ilk deger)
};
struct HttpResponse {
    int code = 200;
    std::string type = "application/json; charset=utf-8";
    std::string cache = "no-store";
    std::string body;
    std::vector<std::pair<std::string, std::string>> extra;      // ek basliklar (Content-Range vb.)
};
typedef std::function<HttpResponse(HttpRequest&)> HttpHandler;

std::map<std::string, std::string> parse_query(const std::string& qs);

class HttpServer {
public:
    ~HttpServer() { stop(); }
    bool start(int port, HttpHandler handler);       // false: port kullanimda
    void stop();
    bool running() const { return listen_ != -1; }
private:
    void accept_loop();
    void serve(long long sock, const std::string& ip);
    long long listen_ = -1;
    std::thread th_;
    HttpHandler handler_;
    std::atomic<int> active_{0};
    std::atomic<bool> stopping_{false};
};
