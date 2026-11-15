#include "netinfo.hpp"
#include <cstring>
#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #define CLOSESOCK closesocket
  typedef SOCKET sock_t;
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <unistd.h>
  #define CLOSESOCK close
  typedef int sock_t;
#endif

static std::string ana_ip() {
#ifdef _WIN32
    WSADATA d; WSAStartup(MAKEWORD(2, 2), &d);
#endif
    sock_t s = socket(AF_INET, SOCK_DGRAM, 0);
    std::string r;
    if (s != (sock_t)-1) {
        sockaddr_in a; memset(&a, 0, sizeof a);
        a.sin_family = AF_INET; a.sin_port = htons(1);
        inet_pton(AF_INET, "10.255.255.255", &a.sin_addr);
        if (connect(s, (sockaddr*)&a, sizeof a) == 0) {           // gercekte paket gonderilmez: dogru ag karti secilir
            sockaddr_in me; socklen_t l = sizeof me;
            if (getsockname(s, (sockaddr*)&me, &l) == 0) { char b[64]; inet_ntop(AF_INET, &me.sin_addr, b, sizeof b); r = b; }
        }
        CLOSESOCK(s);
    }
    return r;
}

std::vector<std::string> yerel_ip_listesi() {
    std::string ana = ana_ip();
    std::vector<std::string> diger;
    char host[256] = "";
    if (gethostname(host, sizeof host) == 0) {
        addrinfo hints; memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET;
        addrinfo* res = nullptr;
        if (getaddrinfo(host, nullptr, &hints, &res) == 0) {
            for (addrinfo* p = res; p; p = p->ai_next) {
                char b[64];
                inet_ntop(AF_INET, &((sockaddr_in*)p->ai_addr)->sin_addr, b, sizeof b);
                diger.push_back(b);
            }
            freeaddrinfo(res);
        }
    }
    auto oncelik = [](const std::string& ip) {
        if (starts_with(ip, "192.168.")) return 0;
        if (starts_with(ip, "10.")) return 1;
        if (starts_with(ip, "172.")) { int b = atoi(ip.c_str() + 4); if (b >= 16 && b <= 31) return 2; }
        return 3;
    };
    std::stable_sort(diger.begin(), diger.end(), [&](const std::string& a, const std::string& b) { return oncelik(a) < oncelik(b); });
    std::vector<std::string> out, all;
    if (!ana.empty()) all.push_back(ana);
    all.insert(all.end(), diger.begin(), diger.end());
    for (auto& ip : all) {
        if (std::find(out.begin(), out.end(), ip) != out.end()) continue;
        if (starts_with(ip, "127.") || starts_with(ip, "169.254.") || starts_with(ip, "0.")) continue;
        out.push_back(ip);
    }
    if (out.empty()) out.push_back("127.0.0.1");
    return out;
}
