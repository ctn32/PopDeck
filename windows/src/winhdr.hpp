// Windows ortak basliklari (MSVC ve MinGW-w64 ile derlenir)
#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <commdlg.h>
#include <algorithm>
using std::min;     // GDI+ basliklari min/max'i adsiz kullanir (NOMINMAX ile birlikte gerekli)
using std::max;
#include <objidl.h>
#include <gdiplus.h>
#include "util.hpp"

inline std::wstring W(const std::string& s) { return utf8_to_wide(s); }
inline std::string U(const std::wstring& s) { return wide_to_utf8(s); }
