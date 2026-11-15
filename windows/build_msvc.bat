@echo off
cd /d "%~dp0"
echo ============================================
echo   PopDeck - C++ derleme (Visual Studio / MSVC)
echo ============================================
echo "x64 Native Tools Command Prompt for VS" icinde calistirin (ya da VS yuklu ise otomatik denenir).
where cl >nul 2>&1
if errorlevel 1 (
  for %%V in (2022 2019) do for %%E in (Community Professional Enterprise BuildTools) do (
    if exist "%ProgramFiles%\Microsoft Visual Studio\%%V\%%E\VC\Auxiliary\Build\vcvars64.bat" (
      call "%ProgramFiles%\Microsoft Visual Studio\%%V\%%E\VC\Auxiliary\Build\vcvars64.bat" & goto basla
    )
    if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\%%V\%%E\VC\Auxiliary\Build\vcvars64.bat" (
      call "%ProgramFiles(x86)%\Microsoft Visual Studio\%%V\%%E\VC\Auxiliary\Build\vcvars64.bat" & goto basla
    )
  )
  echo Visual Studio bulunamadi. "Desktop development with C++" kurun.
  pause
  exit /b 1
)
:basla
if not exist build mkdir build
cmake -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 goto hata
cmake --build build
if errorlevel 1 goto hata
copy /y "build\PopDeck.exe" "PopDeck.exe" >nul
echo.
echo TAMAM!  PopDeck.exe bu klasorde hazir.
pause
exit /b 0
:hata
echo.
echo HATA: Derleme basarisiz. Yukaridaki ilk hata satirlarini gonder, duzelteyim.
pause
exit /b 1
