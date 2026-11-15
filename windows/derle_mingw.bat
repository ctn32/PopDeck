@echo off
cd /d "%~dp0"
echo ============================================
echo   PopDeck - C++ derleme (MinGW-w64 / MSYS2)
echo ============================================
where cmake >nul 2>&1 || goto yok
where g++ >nul 2>&1 || goto yok
if not exist build mkdir build
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE="-O2 -DNDEBUG"
if errorlevel 1 goto hata
cmake --build build -j 4
if errorlevel 1 goto hata
copy /y "build\PopDeck.exe" "PopDeck.exe" >nul
echo.
echo TAMAM!  PopDeck.exe bu klasorde hazir. Cift tikla.
pause
exit /b 0
:yok
echo cmake ve g++ (MinGW-w64) PATH'te bulunamadi.
echo MSYS2 kur:  https://www.msys2.org   sonra MSYS2 MINGW64 kabugunda:
echo   pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-make
echo ve  C:\msys64\mingw64\bin  klasorunu PATH'e ekle.
pause
exit /b 1
:hata
echo.
echo HATA: Derleme basarisiz. Yukaridaki ilk hata satirlarini gonder, duzelteyim.
pause
exit /b 1
