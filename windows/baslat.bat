@echo off
cd /d "%~dp0"
if not exist PopDeck.exe (
  echo PopDeck.exe yok. Once derle_mingw.bat ya da derle_msvc.bat calistir.
  echo (Python surumu icin python_surum\baslat.bat)
  pause
  exit /b 1
)
start "" PopDeck.exe
