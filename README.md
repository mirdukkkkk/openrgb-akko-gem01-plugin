# OpenRGB plugin for the AKKO Gem 01

An [OpenRGB](https://openrgb.org) 1.0 plugin that adds the **AKKO Gem 01** keyboard
(model `103-Gem1B-005`, USB `342d:e4e3`) as a device. It works with the **stock
VIA firmware**: no flashing is needed.

## What works

The stock firmware exposes lighting only through VIA's RGB Matrix channel. So
the plugin can control everything VIA can:

| OpenRGB mode | What it does |
|---|---|
| **Direct** | The whole keyboard follows one OpenRGB color. Use it for profiles, effect plugins and the SDK. |
| **Static**, **Breathing**, **Cycle …**, **Splash …** and others: 46 firmware effects | Each one has brightness, speed and color where the effect supports them. |
| **Save To Device** | Writes the current settings to the keyboard's EEPROM. Until then, changes are lost on replug. |

**Per-key colors are not possible with the stock firmware.** The firmware has no
command for setting individual LEDs. Per-key control would need custom QMK
firmware with the OpenRGB protocol, which this project does not cover yet.

Only a wired USB connection is supported. In 2.4G and Bluetooth modes the raw HID
interface is not available.

## Install

### 1. udev rule: device access without root

```sh
sudo install -m644 udev/60-akko-gem01.rules /etc/udev/rules.d/
sudo udevadm control --reload
sudo udevadm trigger --subsystem-match=hidraw --subsystem-match=usb
```

Without it the plugin cannot open the keyboard unless OpenRGB runs as root:
OpenRGB's own udev rules do not cover AKKO devices.

### 2. Build

Requirements: OpenRGB **1.0** (plugin API v5), Qt 6, hidapi, pkg-config, a C++17 compiler.

```sh
git clone --recursive https://github.com/mirdukkkkk/openrgb-akko-gem01-plugin.git
cd openrgb-akko-gem01-plugin
mkdir build && cd build
qmake6 ../AkkoGem01Plugin.pro
make -j$(nproc)
make install        # -> ~/.config/OpenRGB/plugins/
```

On Arch: `sudo pacman -S --needed base-devel qt6-base hidapi`.

### 3. Restart OpenRGB

Fully quit OpenRGB, including the tray icon, then start it again. The plugin
appears in *Settings → Plugins*. A status page appears in
*Information → AKKO Gem 01*, and the keyboard appears in the device list.

## CLI tool

`tools/akko_via_cli.cpp` uses the same protocol code and is handy for testing:

```sh
g++ -std=c++17 -Isrc tools/akko_via_cli.cpp src/ViaKeyboard.cpp $(pkg-config --cflags --libs hidapi-hidraw) -o akko_via_cli
./akko_via_cli get
./akko_via_cli effect 1          # Solid Color
./akko_via_cli color 170 255     # hue / saturation (0-255)
./akko_via_cli brightness 128
./akko_via_cli save
```

## Protocol notes

* Raw HID interface: usage page `0xFF60`, usage `0x61`, 32-byte reports, VIA protocol 12.
* `07 03 <id> <data>` / `08 03 <id>`: set or get RGB Matrix value; `09 03`: save.
  The value IDs are 1 = brightness, 2 = effect, 3 = speed, 4 = color (hue, saturation).
* The effect numbering comes from AKKO's VIA definition (`gem_01.json` from
  <https://file.akkogear.com/gem_01.zip>).

## License

GPL-2.0-or-later, the same license as OpenRGB.
