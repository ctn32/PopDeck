@echo off
cd /d "%~dp0"
echo Kutuphaneler kuruluyor (keyboard, Pillow, qrcode)
py -m pip install -r requirements.txt
echo.
echo Kurulum bitti. Simdi baslat.bat dosyasina cift tiklayin.
pause
