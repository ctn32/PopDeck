// Global tus dinleme (WH_KEYBOARD_LL; tuslar oyuna da gider - bastirilmaz)
#pragma once
#include "winhdr.hpp"
#include "keys.hpp"
#include "core.hpp"
#include <thread>
#include <atomic>

class KeyHook {
public:
    struct Binding { HotKey hk; Cmd cmd; };
    std::function<void(const Cmd&)> on_cmd;                        // hook thread'inden cagrilir (PostMessage yapin)
    std::function<void(const std::string&)> on_capture;            // yakalama bitti (hook thread'i); "" = iptal

    bool start();
    void stop();
    void set_bindings(const std::vector<Binding>& b);
    void begin_capture();                                          // bir kombinasyon yakalar
    void end_capture();
    std::atomic<bool> enabled{true};
private:
    static LRESULT CALLBACK proc(int code, WPARAM w, LPARAM l);
    LRESULT handle(int code, WPARAM w, LPARAM l);
    void run();
    std::thread th_;
    DWORD tid_ = 0;
    HHOOK hook_ = nullptr;
    std::mutex m_;
    std::vector<Binding> b_;
    bool down_[256] = {false};
    bool capturing_ = false;
    std::vector<int> cap_pressed_, cap_all_;
    HANDLE ready_ = nullptr;
    friend struct HookAccess;
};
extern KeyHook* g_keyhook;
