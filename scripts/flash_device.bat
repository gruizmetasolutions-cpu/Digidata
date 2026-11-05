@echo off
cd /d "%~dp0.."
title DigiVice Firmware Flasher (Cyber-Face)
echo =====================================================================
echo   ⚡ DigiVice Firmware Flasher -- Cyber-Face 2.5D (ESP32 CYD)
echo   Ruta activa: %CD%
echo =====================================================================
echo.

:: Verificar si esptool esta disponible
python -c "import esptool" >nul 2>&1
if %errorlevel% neq 0 (
    echo [*] Instalando esptool en tu sistema...
    python -m pip install --upgrade esptool
)

echo [*] Buscando archivo binario del firmware...
set BIN_PATH=digivice_cyberface.bin
if not exist "%BIN_PATH%" (
    set BIN_PATH=firmware\cyd_resolve_pet\.pio\build\esp32-2432S028R\firmware.bin
)

if not exist "%BIN_PATH%" (
    echo [!] Error: No se encontro el archivo firmware.bin.
    echo     Ejecuta primero la compilacion o descarga el archivo digivice_cyberface.bin.
    pause
    exit /b 1
)

echo [*] Firmware detectado: %BIN_PATH%
echo [*] Conecta tu DigiVice por USB y asegurate de que no haya otro programa usando el puerto COM.
echo.
pause

echo [*] Flasheando firmware a 921600 baudios en offset 0x10000...
python -m esptool --chip esp32 --baud 921600 write_flash 0x10000 "%BIN_PATH%"
if %errorlevel% neq 0 (
    echo [*] Reintentando especificamente en puerto COM7...
    python -m esptool --chip esp32 --port COM7 --baud 921600 write_flash 0x10000 "%BIN_PATH%"
)

if %errorlevel% equ 0 (
    echo.
    echo =====================================================================
    echo   [EXITO] ¡Tu DigiVice ha sido flasheado y actualizado con exito!
    echo =====================================================================
) else (
    echo.
    echo =====================================================================
    echo   [AVISO] Si fallo la deteccion del puerto COM:
    echo   1. Manten presionado el boton 'BOOT' en la parte trasera del ESP32.
    echo   2. Vuelve a ejecutar este archivo mientras lo presionas.
    echo =====================================================================
)

echo.
pause
