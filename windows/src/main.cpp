// PopDeck - giris noktasi
#include "app.hpp"
#include <objbase.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    std::string exe = exe_dir();
    g_main_dir = path_u8(exe) / "main";
    g_log_path = path_u8(exe) / "popdeck_log.txt";
    lang_load(path_u8(exe));
    lang_set(sistem_dili());
    if (!tek_kopya_mi(L("PopDeck zaten çalışıyor.\n\nArayüz gizliyse CTRL+SHIFT+F11 (ya da ayarladığın tuş) ile açabilirsin."))) return 0;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ui::init_gdiplus();
    klavye_yedeklerini_kur();
    g_codec = win_codec_create();
    int code = 0;
    {
        MainWindow app;
        if (!app.create()) {
            MessageBoxW(nullptr, L"PopDeck penceresi olusturulamadi.", L"PopDeck", MB_OK | MB_ICONERROR);
            code = 1;
        } else {
            MSG m;
            while (GetMessageW(&m, nullptr, 0, 0) > 0) { TranslateMessage(&m); DispatchMessageW(&m); }
            code = (int)m.wParam;
        }
        g_app = nullptr;
    }
    ui::shutdown_gdiplus();
    CoUninitialize();
    return code;
}
