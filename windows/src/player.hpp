// Seffaf, ustte duran, tiklanamaz gosterim penceresi (kendi thread'inde calisir)
#pragma once
#include "winhdr.hpp"
#include "image.hpp"
#include <thread>
#include <atomic>

struct PlayParams {
    std::shared_ptr<ImgData> img;
    int sw = 1920, sh = 1080;                 // ekran (piksel)
    double oran = 1.0, maxb = 2.0, fade_in = 0.4, sure_s = 3.0;
    bool boz = false, click_through = true, tek_tur = true;
    std::function<void()> on_first;           // ilk kare ekrana gelince (player thread'inden)
    std::function<void()> on_done;            // pencere kapaninca (player thread'inden)
};

class Player {
public:
    explicit Player(const PlayParams& p);
    ~Player();
    void stop();                              // pencereyi kapatmasini ister ve bekler
private:
    void run();
    PlayParams p_;
    std::thread th_;
    HANDLE ev_;
    std::atomic<bool> stop_{false};
};
