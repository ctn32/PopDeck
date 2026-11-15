@echo off
cd /d "%~dp0"
if not exist PopDeck.exe (
  echo PopDeck.exe yok. Once build_mingw.bat ya da build_msvc.bat calistir.
  pause
  exit /b 1
)
start "" PopDeck.exe
