// Herhangi bir thread'den arayuz thread'inde calisacak is kuyruga atar
#pragma once
#include <functional>
void ui_post(std::function<void()> fn);
void ui_after(int ms, std::function<void()> fn);       // arayuz thread'inden cagrilmali
