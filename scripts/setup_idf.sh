#!/usr/bin/env bash
# One-time ESP-IDF v5.5 bootstrap for Linux and macOS.
#
#   ./scripts/setup_idf.sh
#
# Installs ESP-IDF into ~/esp/esp-idf-v5.5 (or $IDF_PATH if set) and the
# Xtensa toolchain for the ESP32. Needs ~2 GB disk and a decent connection.
# Windows: use the official installer instead — see README "Prerequisites".
set -euo pipefail

IDF_VERSION="v5.5.4"
IDF_DIR="${IDF_PATH:-$HOME/esp/esp-idf-v5.5}"
OS="$(uname -s)"

say() { printf '\n==> %s\n' "$*"; }

if [[ -f "$IDF_DIR/export.sh" ]]; then
    say "ESP-IDF already present at $IDF_DIR — nothing to do."
    echo "    use it with:  source $IDF_DIR/export.sh"
    exit 0
fi

# ---- prerequisites ----------------------------------------------------------
missing=()
for tool in git python3 cmake; do
    command -v "$tool" >/dev/null 2>&1 || missing+=("$tool")
done
if [[ ${#missing[@]} -gt 0 ]]; then
    say "missing prerequisites: ${missing[*]}"
    if [[ "$OS" == "Darwin" ]]; then
        echo "    install with Homebrew:  brew install ${missing[*]}"
        echo "    (no Homebrew? https://brew.sh)"
    else
        echo "    Debian/Ubuntu:  sudo apt-get install git wget flex bison gperf python3 python3-pip python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0"
        echo "    Fedora:         sudo dnf install git wget flex bison gperf python3 python3-pip python3-setuptools cmake ninja-build ccache dfu-util libusbx"
    fi
    exit 1
fi

# ---- clone + install --------------------------------------------------------
say "cloning ESP-IDF $IDF_VERSION into $IDF_DIR (~1.5 GB, this takes a while)"
mkdir -p "$(dirname "$IDF_DIR")"
git clone --branch "$IDF_VERSION" --depth 1 --recursive --shallow-submodules \
    https://github.com/espressif/esp-idf.git "$IDF_DIR"

say "installing the ESP32 toolchain"
"$IDF_DIR/install.sh" esp32

# ---- serial port access -----------------------------------------------------
if [[ "$OS" == "Linux" ]]; then
    if ! id -nG "$USER" | grep -qw dialout; then
        say "adding $USER to the 'dialout' group (serial port access)"
        sudo usermod -aG dialout "$USER" || true
        echo "    log out and back in (or run 'newgrp dialout') before flashing."
    fi
elif [[ "$OS" == "Darwin" ]]; then
    say "macOS note: the board's CH340 USB chip is supported natively on"
    echo "    macOS 10.14+. If no /dev/cu.usbserial*/cu.wchusbserial* port"
    echo "    appears when plugged in, install the WCH CH34x driver."
fi

say "done. Build and flash with:"
echo "    ./scripts/flash.sh"
