#!/usr/bin/env bash
# =====================================================================
#   ⚡ DigiVice Firmware Flasher -- Cyber-Face 2.5D (ESP32 CYD)
# =====================================================================
set -euo pipefail

echo "====================================================================="
echo "  ⚡ DigiVice Firmware Flasher -- Cyber-Face 2.5D (ESP32 CYD)"
echo "====================================================================="

if ! python3 -c "import esptool" >/dev/null 2>&1; then
    echo "[*] Instalando esptool..."
    python3 -m pip install --upgrade esptool
fi

BIN_PATH="digivice_cyberface.bin"
if [ ! -f "$BIN_PATH" ]; then
    BIN_PATH="firmware/cyd_resolve_pet/.pio/build/esp32-2432S028R/firmware.bin"
fi

if [ ! -f "$BIN_PATH" ]; then
    echo "[!] Error: No se encontro $BIN_PATH."
    exit 1
fi

echo "[*] Firmware detectado: $BIN_PATH"
echo "[*] Flasheando dispositivo ESP32..."

python3 -m esptool --chip esp32 --baud 921600 write_flash 0x10000 "$BIN_PATH"

echo "====================================================================="
echo "  [EXITO] ¡Tu DigiVice ha sido flasheado y actualizado con exito!"
echo "====================================================================="
