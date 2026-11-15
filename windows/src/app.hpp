// Ana pencere + uygulama mantigi (Python'daki Uygulama sinifinin C++ karsiligi)
#pragma once
#include "winhdr.hpp"
#include "ui.hpp"
#include "core.hpp"
#include "playback.hpp"
#include "hotkeys.hpp"
#include "winsys.hpp"
#include "lang.hpp"
#include "keys.hpp"
#include "uipost.hpp"
#include "netinfo.hpp"
#include "qr.hpp"
#include <set>

#define WM_PD_RUN (WM_APP + 1)

struct UiVars {
    double sure = 3, boyut = 100, fade = 0.4, seviye = 100, gecikme = 0, onbellek = 400, maxb = 2, fps = 30, bos = 10;
    bool boz = false, tikla = true, ses = true, kes = true, dinle = true, gizli = false, web = true, sonuna = true,
         fwoto = true, yukle = true, onyukle = true;
};

class MainWindow : public ui::Surface {
public:
    MainWindow();
    ~MainWindow();
    bool create();
    LRESULT other(UINT m, WPARAM w, LPARAM l) override;
    void on_relayout() override;
    void wheel_default(float x, float y, int d) override;
    void paint_bg(Gdiplus::Graphics& g, float w, float h) override;

    void komut_isle(const Cmd& c);
    void tamamen_kapat();

    Cekirdek core;
    std::unique_ptr<Oynatici> oynatici;
    KeyHook hook;
    HttpServer web;

private:
    struct CardL { ui::Card* card; std::vector<ui::Widget*> rows; std::vector<float> hs; int sayfa; int kolon; };

    // kurulum / arayuz
    void ilk_kurulum();
    void vars_yukle();
    void build_ui();
    void layout();
    void sayfa_goster(int i);
    void place_edits();
    HWND make_edit(bool number);
    int dp(float v) const { return (int)(v * scale + 0.5f); }
    ui::ToggleRow* toggle_row(int page, const std::string& label, bool* v);
    ui::SliderRow* slider_row(int page, const std::string& label, double* v, double a, double b, double step, const std::string& unit);
    ui::EditRow* edit_row(int page, const std::string& label, const std::string& init, bool number, float w);
    ui::KeyRow* key_row(int page, const std::string& label, const std::string& anahtar);
    ui::Button* button(int page, const std::string& text, ui::Button::Style st, std::function<void()> cb, bool clipped);
    ui::Label* label(int page, const std::string& text, float px, bool bold, const ui::Color& col, bool clipped, bool wrap = false);
    CardL& card(int page, int kolon, const std::string& baslik);
    void durum(const std::string& s);
    void dpi_guncelle(float yeni);
    void font_olustur();

    // ayarlar
    void ayar_degisti();
    void ayarlari_kaydet();
    void dinleme_guncelle();
    void ozel_tus_yazilari();
    // klasorler
    void listeyi_yenile(const std::string& sec = "");
    std::string secili(bool sor = true);
    void yeni_klasor();
    void yeniden_adlandir();
    void klasor_sil();
    void klasor_ac_();
    void dosya_ekle();
    void test_et();
    void tus_ata();
    void tus_kaldir();
    void ozel_tus_ata(const std::string& anahtar);
    void tus_yakala(const std::string& mesaj, std::function<void(const std::string&)> sonuc);
    void yakalama_bitir(const std::string& tus);
    std::string tus_cakisma(const std::string& tus, const std::string& haric_tur, const std::string& haric_ad);
    void tuslari_kaydet();
    bool ad_dogrula(std::string& ad, const std::string& eski);
    bool metin_sor(const std::string& baslik, const std::string& mesaj, const std::string& ilk, std::string& sonuc);
    // onbellek
    void tum_dosyalar(std::vector<std::string>& gorseller, std::vector<std::string>& sesler);
    void onyukle_baslat();
    void ses_isit_adimi();
    void onbellek_etiketi();
    void onbellegi_temizle();
    void otomatik_yenile();
    // web
    void web_yeniden_baslat();
    void web_bilgi_guncelle();
    void fw_etiketi();
    void fw_durum_guncelle();
    void fw_kontrol_et();
    void guvenlik_duvari_ac();
    void fw_dogrula(int deneme, int port);
    void qr_goster(const std::string& metin);
    // pencere
    void goster();
    void gizle();
    void pencere_kapatilirken();
    void dil_secildi(int idx);
    void arayuzu_yeniden_kur();
    int msg(const std::string& baslik, const std::string& metin, UINT flags);

    UiVars v_;
    bool ui_hazir_ = false, ui_kurulu_ = false, kapaniyor_ = false, yakalama_ = false, bellek_bosaltildi_ = false,
         ses_isitiliyor_ = false, fw_soruluyor_ = false, kayit_bekliyor_ = false;
    int fw_var_ = -1, sayfa_ = 0;
    std::vector<std::string> ses_kuyrugu_;
    std::string secili_url_, son_qr_ = "~";
    float scroll_[3] = {0, 0, 0};
    float content_h_[3] = {0, 0, 0};
    HFONT edit_font_ = nullptr;
    HBRUSH edit_brush_ = nullptr;
    Gdiplus::Bitmap* logo_ = nullptr;

    ui::TabBar* tabs_ = nullptr;
    ui::Label *lbl_title_ = nullptr, *lbl_sub_ = nullptr, *lbl_dinle_ = nullptr, *lbl_dil_ = nullptr, *lbl_durum_ = nullptr,
              *lbl_info_ = nullptr, *lbl_onb_ = nullptr, *lbl_fw_ = nullptr, *lbl_adres_ = nullptr, *lbl_note_tel_ = nullptr,
              *lbl_qr_cap_ = nullptr, *lbl_dinle_yazi_ = nullptr, *lbl_ses_note_ = nullptr;
    ui::DropDown *dd_dil_ = nullptr, *dd_url_ = nullptr;
    ui::FolderList* list_ = nullptr;
    ui::QrView* qrv_ = nullptr;
    ui::Toggle* tg_dinle_ = nullptr;
    ui::Button *btn_gizle_ = nullptr, *btn_kapat_ = nullptr, *btn_kopyala_ = nullptr, *btn_fw_ = nullptr;
    std::vector<ui::Button*> klasor_btn_;
    std::vector<CardL> cards_;
    std::vector<ui::EditRow*> edits_;
    ui::EditRow *ed_sure_ = nullptr, *ed_fade_ = nullptr, *ed_port_ = nullptr, *ed_pin_ = nullptr;
    ui::KeyRow *kr_arayuz_ = nullptr, *kr_cikis_ = nullptr, *kr_kapat_ = nullptr;
    ui::ToggleRow *tr_web_ = nullptr, *tr_fwoto_ = nullptr, *tr_yukle_ = nullptr;
    std::vector<ui::Widget*> page_widgets_[3];

    HWND cap_hwnd_ = nullptr;
    std::function<void(const std::string&)> cap_fn_;
    friend LRESULT CALLBACK cap_proc(HWND, UINT, WPARAM, LPARAM);
};

extern MainWindow* g_app;
extern HWND g_main_hwnd;
