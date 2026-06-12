#!/usr/bin/env bash
# Build + flash + monitor for the ESP32 Mini TV (Linux + macOS).
# Windows: use scripts\flash.ps1 from PowerShell instead.
#
#   ./scripts/flash.sh            # build, flash, monitor
#   ./scripts/flash.sh build      # build only
#   ./scripts/flash.sh monitor    # monitor only (no rebuild)
#
# Exit monitor with Ctrl+].
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IDF_DIR="${IDF_PATH:-$HOME/esp/esp-idf-v5.5}"
OS="$(uname -s)"

if ! command -v idf.py >/dev/null 2>&1; then
    if [[ ! -f "$IDF_DIR/export.sh" ]]; then
        echo "ESP-IDF not found at $IDF_DIR." >&2
        if [[ -t 0 ]]; then
            read -r -p "Install ESP-IDF v5.5 now via scripts/setup_idf.sh? (~2 GB download) [y/N] " ans
            if [[ "$ans" =~ ^[Yy]$ ]]; then
                "$PROJECT_DIR/scripts/setup_idf.sh"
            else
                echo "aborted — run ./scripts/setup_idf.sh when ready" >&2
                exit 1
            fi
        else
            echo "run ./scripts/setup_idf.sh first (or set IDF_PATH)" >&2
            exit 1
        fi
    fi
    # shellcheck disable=SC1091
    source "$IDF_DIR/export.sh" >/dev/null
fi

cd "$PROJECT_DIR"

MODE="${1:-all}"

if [[ "$MODE" == "build" ]]; then
    exec idf.py build
fi

# The board's CH340 USB-UART bridge:
#   Linux -> /dev/ttyUSB*      macOS -> /dev/cu.usbserial* or /dev/cu.wchusbserial*
if [[ "$OS" == "Darwin" ]]; then
    PORT="$(ls /dev/cu.usbserial* /dev/cu.wchusbserial* 2>/dev/null | head -1 || true)"
else
    PORT="$(ls /dev/ttyUSB* 2>/dev/null | head -1 || true)"
fi
if [[ -z "$PORT" ]]; then
    echo "error: no serial port found — is the Mini TV plugged in (data-capable USB cable)?" >&2
    if [[ "$OS" == "Linux" ]]; then
        echo "hint: permission issues? sudo usermod -aG dialout \$USER (then re-login)" >&2
    else
        echo "hint: macOS may need the WCH CH34x driver if no /dev/cu.* port appears" >&2
    fi
    exit 1
fi
echo "using port: $PORT"

if [[ "$MODE" == "monitor" ]]; then
    exec idf.py -p "$PORT" monitor
fi

exec idf.py -p "$PORT" build flash monitor
