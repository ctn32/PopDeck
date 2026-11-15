@echo off
cd /d "%~dp0"
rem Telefon kumandasi icin Windows guvenlik duvarinda portu TUM AG TURLERI icin acar.
rem Bu dosyaya SAG TIK -> "Yonetici olarak calistir" de.
set PORT=8080
if exist "main\ayarlar.json" (
  for /f "usebackq delims=" %%p in (`powershell -NoProfile -Command "try{(Get-Content -Raw -Encoding UTF8 'main\ayarlar.json' | ConvertFrom-Json).genel.web_port}catch{8080}"`) do set PORT=%%p
)
echo Port: %PORT%
netsh advfirewall firewall delete rule name="PopDeck %PORT%" >nul 2>&1
netsh advfirewall firewall add rule name="PopDeck %PORT%" dir=in action=allow protocol=TCP localport=%PORT% profile=any
echo.
echo Tamam. Telefondan sayfayi yenile.
pause
