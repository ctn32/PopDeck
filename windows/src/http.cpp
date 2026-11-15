#include "http.hpp"
#include <cstring>
#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <winsock2.h>
  #include <ws2tcpip.h>
  typedef SOCKET sock_t;
  #define CLOSESOCK closesocket
  #define BAD_SOCK INVALID_SOCKET
#else
  #include <sys/socket.h>
  #include <sys/time.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  typedef int sock_t;
  #define CLOSESOCK close
  #define BAD_SOCK (-1)
#endif

std::map<std::string, std::string> parse_query(const std::string& qs) {
    std::map<std::string, std::string> m;
    for (auto& part : split(qs, '&')) {
        if (part.empty()) continue;
        size_t e = part.find('=');
        std::string k = url_decode(e == std::string::npos ? part : part.substr(0, e), true);
        std::string v = e == std::string::npos ? "" : url_decode(part.substr(e + 1), true);
        if (!m.count(k)) m[k] = v;
    }
    return m;
}
std::map<std::string, std::string> HttpRequest::q() const { return parse_query(query); }

static const char* reason(int c) {
    switch (c) {
        case 200: return "OK"; case 204: return "No Content"; case 206: return "Partial Content"; case 416: return "Range Not Satisfiable"; case 400: return "Bad Request"; case 401: return "Unauthorized";
        case 404: return "Not Found"; case 429: return "Too Many Requests"; case 500: return "Internal Server Error";
        case 501: return "Not Implemented"; default: return "OK";
    }
}

static void set_rcv_timeout(sock_t s, int sec) {
#ifdef _WIN32
    DWORD ms = (DWORD)sec * 1000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&ms, sizeof ms);
#else
    timeval tv; tv.tv_sec = sec; tv.tv_usec = 0;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
#endif
}

static bool send_all(sock_t s, const char* p, size_t n) {
    while (n > 0) {
        int k = (int)send(s, p, (int)std::min<size_t>(n, 1 << 20), 0);
        if (k <= 0) return false;
        p += k; n -= (size_t)k;
    }
    return true;
}

bool HttpServer::start(int port, HttpHandler handler) {
    stop();
#ifdef _WIN32
    static bool wsa = false;
    if (!wsa) { WSADATA d; if (WSAStartup(MAKEWORD(2, 2), &d) != 0) return false; wsa = true; }
#endif
    sock_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == BAD_SOCK) return false;
#ifdef _WIN32
    int one = 1;
    setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&one, sizeof one);   // ayni portu iki program almasin
#endif
    sockaddr_in a; memset(&a, 0, sizeof a);
    a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_ANY); a.sin_port = htons((unsigned short)port);
    if (bind(s, (sockaddr*)&a, sizeof a) != 0 || listen(s, 64) != 0) { CLOSESOCK(s); return false; }
    handler_ = handler;
    listen_ = (long long)s;
    stopping_ = false;
    th_ = std::thread([this] { accept_loop(); });
    return true;
}

void HttpServer::stop() {
    if (listen_ == -1) return;
    stopping_ = true;
    sock_t s = (sock_t)listen_;
    listen_ = -1;
#ifndef _WIN32
    shutdown(s, SHUT_RDWR);
#endif
    CLOSESOCK(s);
    if (th_.joinable()) th_.join();
    for (int i = 0; i < 40 && active_ > 0; i++) std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

void HttpServer::accept_loop() {
    while (!stopping_) {
        sock_t ls = (sock_t)listen_;
        if (listen_ == -1) break;
        sockaddr_in ca; socklen_t cl = sizeof ca;
        sock_t c = accept(ls, (sockaddr*)&ca, &cl);
        if (c == BAD_SOCK) { if (stopping_) break; std::this_thread::sleep_for(std::chrono::milliseconds(20)); continue; }
        if (active_ > 200) { CLOSESOCK(c); continue; }
        char ip[64] = "";
        inet_ntop(AF_INET, &ca.sin_addr, ip, sizeof ip);
        active_++;
        std::string sip = ip;
        std::thread([this, c, sip] {
            try { serve((long long)c, sip); } catch (...) {}
            active_--;
        }).detach();
    }
}

void HttpServer::serve(long long sk, const std::string& ip) {
    sock_t s = (sock_t)sk;
    set_rcv_timeout(s, 30);
    std::string buf;
    char tmp[4096];
    size_t head_end = std::string::npos;
    while (head_end == std::string::npos) {
        int k = (int)recv(s, tmp, sizeof tmp, 0);
        if (k <= 0) { CLOSESOCK(s); return; }
        buf.append(tmp, (size_t)k);
        head_end = buf.find("\r\n\r\n");
        if (buf.size() > 65536 && head_end == std::string::npos) { CLOSESOCK(s); return; }
    }
    std::string head = buf.substr(0, head_end);
    std::string pre = buf.substr(head_end + 4);
    HttpRequest rq;
    rq.peer_ip = ip;
    std::vector<std::string> lines = split(head, '\n');
    for (auto& l : lines) if (!l.empty() && l.back() == '\r') l.pop_back();
    HttpResponse rs;
    bool ok = false;
    if (!lines.empty()) {
        std::vector<std::string> p = split(lines[0], ' ');
        if (p.size() >= 2) {
            rq.method = p[0];
            std::string target = p[1];
            size_t qm = target.find('?');
            rq.path = qm == std::string::npos ? target : target.substr(0, qm);
            rq.query = qm == std::string::npos ? "" : target.substr(qm + 1);
            rq.path = url_decode(rq.path, false);
            ok = true;
        }
        for (size_t i = 1; i < lines.size(); i++) {
            size_t c = lines[i].find(':');
            if (c == std::string::npos) continue;
            rq.h[ascii_lower(trim(lines[i].substr(0, c)))] = trim(lines[i].substr(c + 1));
        }
    }
    auto cl = rq.h.find("content-length");
    if (cl != rq.h.end()) { rq.content_length = atoll(cl->second.c_str()); if (rq.content_length < 0) rq.content_length = 0; }
    size_t pre_pos = 0;
    rq.body_read = [&](u8* dst, size_t n) -> size_t {
        if (pre_pos < pre.size()) {
            size_t k = std::min(n, pre.size() - pre_pos);
            memcpy(dst, pre.data() + pre_pos, k);
            pre_pos += k;
            return k;
        }
        int k = (int)recv(s, (char*)dst, (int)std::min<size_t>(n, 1 << 20), 0);
        return k > 0 ? (size_t)k : 0;
    };
    rq.set_timeout = [s](int sec) { set_rcv_timeout(s, sec); };
    if (!ok || (rq.method != "GET" && rq.method != "POST")) {
        rs.code = rq.method.empty() || !ok ? 400 : 501;
        rs.body = "{\"ok\":false}";
    } else {
        try { rs = handler_(rq); } catch (...) { rs.code = 500; rs.body = "{\"ok\":false}"; }
    }
    std::string out = "HTTP/1.1 " + std::to_string(rs.code) + " " + reason(rs.code) + "\r\n";
    out += "Content-Type: " + rs.type + "\r\n";
    out += "Content-Length: " + std::to_string(rs.body.size()) + "\r\n";
    out += "Cache-Control: " + rs.cache + "\r\n";
    for (auto& e : rs.extra) out += e.first + ": " + e.second + "\r\n";
    out += "Connection: close\r\n\r\n";
    send_all(s, out.data(), out.size());
    if (!rs.body.empty()) send_all(s, rs.body.data(), rs.body.size());
#ifdef _WIN32
    shutdown(s, SD_SEND);
#else
    shutdown(s, SHUT_WR);
#endif
    // karsi tarafin kalan verisini bosalt (RST olmasin)
    set_rcv_timeout(s, 1);
    for (int i = 0; i < 4; i++) { int k = (int)recv(s, tmp, sizeof tmp, 0); if (k <= 0) break; }
    CLOSESOCK(s);
}
