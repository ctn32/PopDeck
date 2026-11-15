@echo off
cd /d "%~dp0"
echo ============================================
echo   PopDeck - EXE olusturuluyor
echo ============================================
echo.
echo 1/2 Gerekli paketler kuruluyor (internet gerekir)...
py -m pip install --upgrade pyinstaller keyboard Pillow qrcode
if errorlevel 1 goto hata
echo.
echo 2/2 EXE derleniyor (1-3 dakika surebilir)...
py -m PyInstaller --noconfirm --clean --onefile --noconsole --name PopDeck --icon popdeck.ico --add-data "popdeck.ico;." --add-data "diller;diller" --hidden-import diller --hidden-import pillow_heif --collect-submodules qrcode popdeck.py
if errorlevel 1 goto hata
copy /y "dist\PopDeck.exe" "PopDeck.exe" >nul
echo.
echo ============================================
echo   TAMAM!  PopDeck.exe bu klasorde hazir.
echo   Cift tikla, calisir. "main" klasoru exe'nin
echo   yaninda otomatik olusur / kullanilir.
echo   (build, dist ve .spec dosyalarini silebilirsin)
echo ============================================
pause
exit /b 0
:hata
echo.
echo HATA olustu. Python kurulu mu? ("py" komutu calisiyor mu?)
echo Internet baglantini kontrol edip tekrar dene.
pause
exit /b 1
