#include "hotkeys.hpp"

KeyHook* g_keyhook = nullptr;

bool KeyHook::start() {
    if (th_.joinable()) return true;
    g_keyhook = this;
    ready_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    th_ = std::thread([this] { run(); });
    WaitForSingleObject(ready_, 3000);
    CloseHandle(ready_); ready_ = nullptr;
    return hook_ != nullptr;
}

void KeyHook::stop() {
    if (!th_.joinable()) return;
    if (tid_) PostThreadMessageW(tid_, WM_QUIT, 0, 0);
    th_.join();
    tid_ = 0;
}

void KeyHook::run() {
    tid_ = GetCurrentThreadId();
    hook_ = SetWindowsHookExW(WH_KEYBOARD_LL, &KeyHook::proc, GetModuleHandleW(nullptr), 0);
    if (!hook_) log_line("Klavye kancasi kurulamadi: " + std::to_string(GetLastError()));
    SetEvent(ready_);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    if (hook_) UnhookWindowsHookEx(hook_);
    hook_ = nullptr;
}

void KeyHook::set_bindings(const std::vector<Binding>& b) {
    std::lock_guard<std::mutex> lk(m_);
    b_ = b;
}
void KeyHook::begin_capture() {
    std::lock_guard<std::mutex> lk(m_);
    capturing_ = true; cap_pressed_.clear(); cap_all_.clear();
}
void KeyHook::end_capture() {
    std::lock_guard<std::mutex> lk(m_);
    capturing_ = false; cap_pressed_.clear(); cap_all_.clear();
}

LRESULT CALLBACK KeyHook::proc(int code, WPARAM w, LPARAM l) {
    if (g_keyhook) return g_keyhook->handle(code, w, l);
    return CallNextHookEx(nullptr, code, w, l);
}

static bool held(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

LRESULT KeyHook::handle(int code, WPARAM w, LPARAM l) {
    if (code != HC_ACTION) return CallNextHookEx(nullptr, code, w, l);
    const KBDLLHOOKSTRUCT* k = (const KBDLLHOOKSTRUCT*)l;
    const bool down = (w == WM_KEYDOWN || w == WM_SYSKEYDOWN);
    const bool up = (w == WM_KEYUP || w == WM_SYSKEYUP);
    int vk = (int)k->vkCode;
    if (vk < 0 || vk > 255 || (!down && !up)) return CallNextHookEx(nullptr, code, w, l);

    std::string captured;
    bool capture_done = false;
    Cmd fire;
    bool do_fire = false;
    {
        std::lock_guard<std::mutex> lk(m_);
        if (capturing_) {
            int n = normalize_vk(vk);
            if (down) {
                if (std::find(cap_pressed_.begin(), cap_pressed_.end(), n) == cap_pressed_.end()) cap_pressed_.push_back(n);
                if (std::find(cap_all_.begin(), cap_all_.end(), n) == cap_all_.end()) cap_all_.push_back(n);
            } else {
                cap_pressed_.erase(std::remove(cap_pressed_.begin(), cap_pressed_.end(), n), cap_pressed_.end());
                if (cap_pressed_.empty() && !cap_all_.empty()) {
                    captured = combo_name(cap_all_);
                    capturing_ = false; cap_all_.clear();
                    capture_done = true;
                }
            }
        } else if (down) {
            bool repeat = down_[vk];
            down_[vk] = true;
            if (!repeat && enabled) {
                int n = normalize_vk(vk);
                int cur = 0;
                if (held(VK_CONTROL)) cur |= MOD_C;
                if (held(VK_MENU)) cur |= MOD_A;
                if (held(VK_SHIFT)) cur |= MOD_S;
                if (held(VK_LWIN) || held(VK_RWIN)) cur |= MOD_W;
                cur &= ~mod_bit(n);                                 // basilan tus modifier ise kendi bitini sayma
                for (auto& b : b_) {
                    if (b.hk.ok && b.hk.vk == n && b.hk.mods == cur) { fire = b.cmd; do_fire = true; break; }
                }
            }
        }
        if (up) down_[vk] = false;
    }
    if (capture_done && on_capture) on_capture(captured);
    if (do_fire && on_cmd) on_cmd(fire);
    return CallNextHookEx(nullptr, code, w, l);
}
