#include "wsys.hpp"
#include "keys.hpp"
#include "files.hpp"
#include <psapi.h>
#include <cstring>

SesOnbellegi SES;

std::string exe_path() {
    wchar_t b[MAX_PATH * 2];
    DWORD n = GetModuleFileNameW(nullptr, b, MAX_PATH * 2);
    return U(std::wstring(b, n));
}
std::string exe_dir() { return dir_name(exe_path()); }

std::string sistem_dili() {
    switch (GetUserDefaultUILanguage() & 0xFF) {
        case 0x1F: return "tr"; case 0x09: return "en"; case 0x07: return "de";
        case 0x0A: return "es"; case 0x0C: return "fr"; case 0x19: return "ru";
        default: return "en";
    }
}

static HANDLE g_mutex = nullptr;
bool tek_kopya_mi(const std::string& mesaj) {
    g_mutex = CreateMutexW(nullptr, FALSE, L"Local\\PopDeckTekKopya");
    if (g_mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, W(mesaj).c_str(), L"PopDeck", MB_OK | MB_ICONINFORMATION);
        return false;
    }
    return true;
}

double islem_bellegi_mb() {
    PROCESS_MEMORY_COUNTERS pmc;
    memset(&pmc, 0, sizeof pmc);
    pmc.cb = sizeof pmc;
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof pmc)) return -1;
    return pmc.WorkingSetSize / 1048576.0;
}
void bellek_kirp() { EmptyWorkingSet(GetCurrentProcess()); }

void panoya_kopyala(HWND h, const std::string& metin) {
    if (!OpenClipboard(h)) return;
    EmptyClipboard();
    std::wstring w = W(metin);
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, (w.size() + 1) * sizeof(wchar_t));
    if (g) {
        void* p = GlobalLock(g);
        memcpy(p, w.c_str(), (w.size() + 1) * sizeof(wchar_t));
        GlobalUnlock(g);
        if (!SetClipboardData(CF_UNICODETEXT, g)) GlobalFree(g);
    }
    CloseClipboard();
}

bool klasor_ac(const std::string& yol) {
    return (INT_PTR)ShellExecuteW(nullptr, L"open", W(yol).c_str(), nullptr, nullptr, SW_SHOWNORMAL) > 32;
}

std::vector<std::string> dosya_sec(HWND h, const std::string& baslik, const std::string& filtre_ad, const std::string& filtre_uz) {
    std::vector<wchar_t> buf(65536, 0);
    std::wstring filtre;
    filtre += W(filtre_ad); filtre.push_back(L'\0'); filtre += W(filtre_uz); filtre.push_back(L'\0');
    filtre += L"*.*"; filtre.push_back(L'\0'); filtre += L"*.*"; filtre.push_back(L'\0');
    OPENFILENAMEW o; memset(&o, 0, sizeof o);
    o.lStructSize = sizeof o; o.hwndOwner = h;
    o.lpstrFilter = filtre.c_str(); o.lpstrFile = buf.data(); o.nMaxFile = (DWORD)buf.size();
    std::wstring bs = W(baslik); o.lpstrTitle = bs.c_str();
    o.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    std::vector<std::string> r;
    if (!GetOpenFileNameW(&o)) return r;
    std::wstring first = buf.data();
    const wchar_t* p = buf.data() + first.size() + 1;
    if (*p == 0) { r.push_back(U(first)); return r; }          // tek dosya: tam yol
    while (*p) { std::wstring n = p; r.push_back(U(first + L"\\" + n)); p += n.size() + 1; }
    return r;
}

void klavye_yedeklerini_kur() {
    g_vk_name_fallback = [](int vk) -> std::string {
        UINT sc = MapVirtualKeyW((UINT)vk, MAPVK_VK_TO_VSC);
        if (!sc) return "";
        wchar_t n[64] = {0};
        LONG lp = (LONG)(sc << 16);
        if (vk == VK_LEFT || vk == VK_RIGHT || vk == VK_UP || vk == VK_DOWN) lp |= (1 << 24);
        int k = GetKeyNameTextW(lp, n, 64);
        if (k <= 0) return "";
        return lower_u8(U(std::wstring(n, (size_t)k)));
    };
    g_name_vk_fallback = [](const std::string& name) -> int {
        std::wstring w = W(name);
        if (w.size() != 1) return 0;
        SHORT s = VkKeyScanW(w[0]);
        if (s == -1) return 0;
        return s & 0xFF;
    };
}

// ---------------- SES (MCI) ----------------
static MCIERROR mci(const std::string& komut) { return mciSendStringW(W(komut).c_str(), nullptr, 0, nullptr); }

std::string SesOnbellegi::ac(const std::string& yol) {
    auto it = acik.find(yol);
    if (it != acik.end()) {
        sira.remove(yol); sira.push_back(yol);
        return it->second;
    }
    std::string alias = "ps" + std::to_string(sayac++);
    if (mci("open \"" + yol + "\" alias " + alias) != 0) { log_line("Ses acilamadi: " + yol); return ""; }
    acik[yol] = alias; sira.push_back(yol);
    while (acik.size() > ADET) {
        std::string eski = sira.front(); sira.pop_front();
        std::string ea = acik[eski]; acik.erase(eski);
        mci("stop " + ea); mci("close " + ea);
    }
    return alias;
}
bool SesOnbellegi::isit(const std::string& yol) { return !ac(yol).empty(); }
std::string SesOnbellegi::hazirla(const std::string& yol, int seviye) {
    std::string a = ac(yol);
    if (!a.empty()) {
        mci("stop " + a);
        mci("setaudio " + a + " volume to " + std::to_string(std::max(0, std::min(100, seviye)) * 10));
    }
    return a;
}
void SesOnbellegi::oynat(const std::string& a) { if (!a.empty()) mci("play " + a + " from 0"); }
void SesOnbellegi::durdur(const std::string& a) { if (!a.empty()) mci("stop " + a); }
void SesOnbellegi::soundboard(const std::string& yol, int seviye) {
    durdur(sb);
    std::string a = hazirla(yol, seviye);
    oynat(a);
    sb = a;
}
void SesOnbellegi::hepsini_kapat() {
    for (auto& kv : acik) { mci("stop " + kv.second); mci("close " + kv.second); }
    acik.clear(); sira.clear(); sb.clear();
}

// ---------------- GUVENLIK DUVARI ----------------
static int run_hidden(const std::wstring& cmdline, DWORD timeout_ms = 15000) {
    STARTUPINFOW si; memset(&si, 0, sizeof si); si.cb = sizeof si;
    si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi; memset(&pi, 0, sizeof pi);
    std::wstring c = cmdline;
    if (!CreateProcessW(nullptr, &c[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) return -1;
    DWORD w = WaitForSingleObject(pi.hProcess, timeout_ms);
    DWORD code = (DWORD)-1;
    if (w == WAIT_OBJECT_0) GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return w == WAIT_OBJECT_0 ? (int)code : -1;
}

static std::string kural_adi(int port) { return "PopDeck " + std::to_string(port); }

int fw_kural_var(int port) {
    int rc = run_hidden(L"netsh advfirewall firewall show rule name=\"" + W(kural_adi(port)) + L"\"", 10000);
    if (rc < 0) return -1;
    return rc == 0 ? 1 : 0;
}

static bool is_admin() {
    typedef BOOL(WINAPI * Fn)();
    HMODULE m = LoadLibraryW(L"shell32.dll");
    Fn f = m ? (Fn)GetProcAddress(m, "IsUserAnAdmin") : nullptr;
    return f ? f() != FALSE : false;
}

bool fw_izin_baslat(int port) {
    std::string ad = kural_adi(port), exe = exe_path();
    if (is_admin()) {
        run_hidden(L"netsh advfirewall firewall delete rule name=\"" + W(ad) + L"\"", 30000);
        run_hidden(L"netsh advfirewall firewall delete rule name=all dir=in program=\"" + W(exe) + L"\"", 30000);
        run_hidden(L"netsh advfirewall firewall add rule name=\"" + W(ad) + L"\" dir=in action=allow protocol=TCP localport=" + std::to_wstring(port) + L" profile=any", 30000);
        return true;
    }
    std::string kacis = replace_all(exe, "'", "''");
    std::string betik = "$p = '" + kacis + "'\n"
        "netsh advfirewall firewall delete rule \"name=" + ad + "\" | Out-Null\n"
        "netsh advfirewall firewall delete rule name=all dir=in \"program=$p\" | Out-Null\n"
        "netsh advfirewall firewall add rule \"name=" + ad + "\" dir=in action=allow protocol=TCP localport=" + std::to_string(port) + " profile=any | Out-Null\n";
    std::wstring wb = W(betik);
    std::vector<u8> bytes((u8*)wb.data(), (u8*)wb.data() + wb.size() * sizeof(wchar_t));   // UTF-16LE
    std::wstring args = L"-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -EncodedCommand " + W(base64(bytes));
    HINSTANCE r = ShellExecuteW(nullptr, L"runas", L"powershell.exe", args.c_str(), nullptr, SW_HIDE);
    return (INT_PTR)r > 32;
}
