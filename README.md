# PopDeck

Hotkey- and phone-triggered **transparent GIF/PNG overlays** with **sound** — a meme/soundboard deck for gaming and streaming.
Press a key (or tap your phone) and a random GIF pops up full-screen on your PC, or on your phone, or both.

| Folder | What it is |
|---|---|
| [`windows/`](windows) | **Main app**, written in C++ (Win32 + GDI+): global hotkeys, click-through transparent overlay, audio, phone-remote web server, QR code |
| [`android/`](android) | Android app (WebView wrapper): opens the PC's remote page like a native app and plays the GIFs/sounds on the phone |

## Features
- **One hotkey per folder.** Press it and a random image from that folder shows up. "Smart random" never repeats the same item back-to-back.
- **Automatic sound pairing:** put `funny.mp3` next to `funny.gif` and they play together.
- **Fill the whole screen:** with "stretch to fill" enabled, GIFs cover the screen at any resolution.
- **Transparent, click-through overlay** that works over borderless/windowed games, with fade-in, size, FPS limit and a memory-friendly frame cache.
- **Phone remote** (same Wi-Fi, no app needed): Random / Deck / Upload tabs, PIN protection, QR code, favorites, drag-and-drop ordering, soundboard, upload from phone or browser.
- **Display target switch:** play on the **PC**, on the **phone**, or on **both** with the exact same image.
- **6 languages:** Turkish, English, German, Spanish, French, Russian (`windows/lang/*.json`; add a language by dropping in a new JSON file).
- Plain `main/` folder layout: every sub-folder is a category; settings live in `main/settings.json`.

## Build the Windows app
Requires **MinGW-w64 (MSYS2)** or **Visual Studio 2019/2022** with the "Desktop development with C++" workload.

```bat
cd windows
build_mingw.bat     :: build with MinGW-w64
build_msvc.bat      :: build with Visual Studio (MSVC)
```

`PopDeck.exe` is produced next to the scripts. Keep the `main/` and `lang/` folders beside the exe.

> Keep the project path ASCII-only (no characters like ç, ğ, ş): MinGW's make tool breaks on them.
> No external dependencies: it uses the Windows built-ins (GDI+, WIC, MCI, Winsock).

Detailed usage guide (Turkish): [`windows/USER_GUIDE_TR.txt`](windows/USER_GUIDE_TR.txt)

## Build the Android app
Open the `android/` folder in Android Studio, then **Build > Build Bundle(s) / APK(s) > Build APK(s)**.
Enter the address shown in PopDeck's "Phone Remote" tab (e.g. `192.168.1.25:8080`) and your PIN. See [`android/README.txt`](android/README.txt).

## How the phone remote works
PopDeck runs a tiny HTTP server on your PC. Your phone opens its page over the local network, picks a GIF, and either triggers it on the PC
or downloads the same file and plays it locally with its sound. The server is LAN-only and PIN-protected.
If the phone can't connect, allow the port in Windows Firewall (the app offers to do it, or run `windows/open_firewall.bat` as administrator).

## Tests (Linux, optional)
The portable core (GIF decoder, QR encoder, JSON, web API) builds and runs on Linux:
## Personal Note
This app is vibe-coded because i needed something like streamdeck and no other app was working for me or met my needs as a free deck-app for my streaming sessions so i build my own i hope you guys will use it too.
```sh
cd windows
g++ -std=c++17 -O1 -pthread tests/test_web.cpp src/util.cpp src/gif.cpp src/scale.cpp src/keys.cpp src/image.cpp \
    src/lang.cpp src/files.cpp src/settings.cpp src/deck.cpp src/embedded_data.cpp src/http.cpp src/upload.cpp src/web.cpp -lz
```
`tests/test_web.cpp` expects sample GIFs at `/tmp/t1.gif`, `/tmp/t2.gif`, `/tmp/t3.gif`.

## Translations
```sh
python windows/tools/translate_tool.py check    # check missing/unused strings
python windows/tools/translate_tool.py template     # create lang/_missing.json to fill in
python windows/tools/translate_tool.py merge       # merge filled translations
python windows/tools/gen_embedded.py            # regenerate embedded data after editing web.html / lang/*.json
```

## Notes
- Games running in *exclusive fullscreen* hide overlays (a Windows limitation); use windowed or borderless mode.
- Anti-cheat systems may block global keyboard hooks in some games.
- The phone playback happens inside the browser/app page; it cannot draw over other Android apps.
