# RatCom

Standalone [Reticulum](https://reticulum.network/) + [LXMF](https://github.com/markqvist/LXMF) encrypted messenger, built for the [M5Stack Cardputer Adv](https://docs.m5stack.com/en/core/M5Cardputer%20Adv).

Not an RNode. Not a gateway. A fully self-contained key to Reticulum, with a keyboard, screen, and no host computer required.

## What RatCom Does

RatCom turns a small $50 Cardputer Adv into a Reticulum mesh node with encrypted LXMF messaging using Ed25519 signatures, support for unlimited swappable identities, and both WiFi/TCP and LoRa connectivity. It can connect remotely over WiFi to TCP peers such as `rns.ratspeak.org:4242`, store messages and contacts on flash and SD card with automatic backup, and lets you configure everything directly on-device with no config files or host tools.

## Features

| Category | Details |
|----------|---------|
| **Connectivity** | Internet-capable over TCP/WiFi and LoRa-capable over radio |
| **Messaging** | LXMF encrypted messages, Ed25519 signatures, delivery tracking, and per-conversation storage |
| **WiFi Modes** | AP mode or STA mode for TCP client connections to remote nodes; not concurrent |
| **Identity** | Manage and swap between unlimited identities |
| **Storage** | Store messages and contacts on flash and SD card with automatic backup |
| **Configuration** | Full on-device setup with no config files or host tools |
| **Planned** | BLE Sideband (v1.1 stub), GNSS (pins defined), OTA updates (partition reserved) |

## Prerequisites

| Requirement | Version | Install |
|-------------|---------|---------|
| **Python** | 3.12+ | [python.org](https://www.python.org/downloads/) or your package manager |
| **PlatformIO Core** | 6.x | `pip install platformio` |
| **Git** | any | Your package manager |
| **USB driver** | — | None needed on macOS/Linux (ESP32-S3 USB-Serial/JTAG is built-in) |

PlatformIO automatically downloads the ESP32-S3 toolchain, Arduino framework, and all library dependencies on first build.

## Build & Flash

The fastest way to flash RatCom is by visiting our [in-browser flasher](https://ratspeak.org/download.html), otherwise:

```bash
# Clone
git clone https://github.com/ratspeak/ratcom.git
cd RatCom

# Build (first build takes ~2 min to download toolchain + deps)
python3 -m platformio run -e ratputer_915

# Flash (plug in Cardputer Adv via USB-C while in download mode - holding G0 before plugging in)
python3 -m platformio run -e ratputer_915 -t upload --upload-port /dev/cu.usbmodem*
```

> If `pio` is not on your PATH after install, use `python3 -m platformio` everywhere.

### Alternative: esptool (more reliable)

PlatformIO's default 921600 baud sometimes fails over USB-Serial/JTAG. esptool at 460800 is more reliable:

```bash
python3 -m esptool --chip esp32s3 --port /dev/cu.usbmodem* --baud 460800 \
    --before default-reset --after hard-reset \
    write-flash -z 0x10000 .pio/build/ratputer_915/firmware.bin
```

See [docs/BUILDING.md](docs/BUILDING.md) for merged binaries, build flags, partition table, and CI/CD details.

## Keyboard & Hotkeys

### Hotkeys (Ctrl+key)

| Shortcut | Action |
|----------|--------|
| Ctrl+H | Toggle help overlay (shows all hotkeys on screen) |
| Ctrl+M | Jump to Messages tab |
| Ctrl+N | Compose new message |
| Ctrl+S | Jump to Settings tab |
| Ctrl+A | Force announce to network (immediate, doesn't wait for 5-min timer) |
| Ctrl+D | Dump full diagnostics to serial (identity, radio regs, heap, flash usage) |
| Ctrl+T | Send radio test packet (`0xA0` header + `RATPUTER_TEST_1234567890`, FIFO readback) |
| Ctrl+R | RSSI monitor — continuous sampling for 5 seconds (use while another device transmits) |

### Navigation

| Key | Action |
|-----|--------|
| `;` | Scroll up / previous item |
| `.` | Scroll down / next item |
| `,` | Previous tab (left) |
| `/` | Next tab (right) |
| Enter | Select / confirm / send message |
| Esc | Back / cancel |
| Backspace | Delete character in text input |
| Aa (double-tap) | Caps lock |

### AP Mode

Creates a WiFi hotspot named `ratcom-XXXX` (password: `ratspeak`). Runs a TCP server on port 4242 with HDLC framing (0x7E delimiters, 0x7D byte stuffing).

**Bridge to desktop Reticulum**: Connect your laptop to the `ratcom-XXXX` network, then add to your Reticulum config (`~/.reticulum/config`):

```ini
[[ratcom]]
  type = TCPClientInterface
  target_host = 192.168.4.1
  target_port = 4242
```

Now your desktop `rnsd` can reach the LoRa mesh through RatCom.

### STA Mode

Connects to an existing WiFi network. Establishes outbound TCP connections to configured Reticulum endpoints.

**Setup**:
1. Ctrl+S → WiFi → Mode → **STA**
2. Enter your WiFi SSID and password
3. Add TCP endpoints: e.g., `rns.ratspeak.net` port `4242`, auto-connect enabled
4. Save — RatCom connects to your WiFi, then opens TCP links to the configured hosts

TCP clients auto-reconnect every 10 seconds if the connection drops. Up to 4 simultaneous TCP connections supported.

## LoRa Radio

### Default Configuration

| Parameter | Default | Range |
|-----------|---------|-------|
| Frequency | 915 MHz | Hardware-dependent (SX1262 supports 150–960 MHz) |
| Spreading Factor | SF7 | SF5–SF12 (higher = longer range, slower) |
| Bandwidth | 500 kHz | 7.8 kHz – 500 kHz |
| Coding Rate | 4/5 | 4/5 – 4/8 (higher = more error correction) |
| TX Power | 10 dBm | 2–22 dBm |
| Preamble | 18 symbols | Configurable |
| Sync Word | 0x1424 | Reticulum standard |
| Max Packet | 255 bytes | SX1262 hardware limit (254 bytes payload + 1-byte header) |

All parameters configurable via Settings → Radio. Changes take effect immediately and persist across reboots.

## SD Card (*RECOMMENDED)

Optional microSD card (FAT32, 32GB max suggested). Provides backup storage and extended capacity beyond the 1.875 MB LittleFS partition.

### Directory Structure (auto-created on first boot)

```
/ratcom/
├── config/
│   └── user.json           Runtime settings (radio, WiFi, display, audio)
├── messages/
│   └── <peer_hex>/         Per-conversation message history (JSON)
├── contacts/               Discovered Reticulum nodes
└── identity/
    └── identity.key        Ed25519 keypair backup
```

## Serial Monitor & Debugging

```bash
python3 -m platformio device monitor -b 115200
```

The firmware logs extensively to serial at 115200 baud. Every subsystem prints tagged messages: `[BOOT]`, `[RADIO]`, `[WIFI]`, `[LXMF]`, `[LORA_IF]`, `[SD]`, etc.

## Dependencies

All automatically managed by PlatformIO — no manual installation needed:

| Library | Version | Purpose |
|---------|---------|---------|
| [microReticulum](https://github.com/attermann/microReticulum) | git HEAD | Reticulum protocol stack (C++ port) — Identity, Transport, Packet, Link |
| [Crypto](https://github.com/attermann/Crypto) | git HEAD | Ed25519, X25519, AES-128, SHA-256, HMAC |
| [ArduinoJson](https://github.com/bblanchon/ArduinoJson) | ^7.4.2 | JSON serialization for UserConfig and message storage |
| [M5Unified](https://github.com/m5stack/M5Unified) | latest | Hardware abstraction — display, battery, power, buttons |
| [M5GFX](https://github.com/m5stack/M5GFX) | latest | Display rendering, M5Canvas sprite double-buffering |
| [M5Cardputer](https://github.com/m5stack/M5Cardputer) | latest | TCA8418 keyboard driver, Cardputer-specific hardware |

## Project Structure

```
RatCom/
├── src/
│   ├── main.cpp                 Entry point: 24-step setup(), 20 FPS loop()
│   ├── config/                  BoardConfig.h (pins), Config.h (compile-time), UserConfig.* (runtime JSON)
│   ├── radio/                   SX1262.* (register-level driver), RadioConstants.h
│   ├── input/                   Keyboard.* (TCA8418), HotkeyManager.* (Ctrl+key dispatch)
│   ├── ui/                      UIManager, StatusBar, TabBar, Theme, Screen base class
│   │   ├── screens/             BootScreen, HomeScreen, MessagesScreen, MessageView, NodesScreen, SettingsScreen, HelpOverlay
│   │   ├── widgets/             ScrollList, TextInput, ProgressBar
│   │   └── assets/              BootLogo.h
│   ├── reticulum/               ReticulumManager, AnnounceManager, LXMFManager, LXMFMessage
│   ├── transport/               LoRaInterface, WiFiInterface, TCPClientInterface, BLEStub
│   ├── storage/                 FlashStore (LittleFS), SDStore (FAT32), MessageStore (dual)
│   ├── power/                   PowerManager (dim/off/wake)
│   └── audio/                   AudioNotify (boot, message, announce, error sounds)
├── docs/                        BUILDING, PINMAP, TROUBLESHOOTING, DEVELOPMENT, ARCHITECTURE, QUICKSTART, HOTKEYS
├── platformio.ini               Build configuration (single env: ratputer_915)
├── partitions_8MB_ota.csv       Flash partition table
└── .github/workflows/build.yml  CI: build on push, release on tag
```

## Documentation

| Document | Contents |
|----------|----------|
| [Quick Start](docs/QUICKSTART.md) | First build, first boot, navigation, WiFi setup, SD card |
| [Building](docs/BUILDING.md) | Build commands, flashing (PlatformIO + esptool), merged binaries, CI/CD, build flags |
| [Pin Map](docs/PINMAP.md) | Full GPIO assignments — LoRa SPI, SD card, keyboard I2C, GNSS UART, display, audio |
| [Hotkeys](docs/HOTKEYS.md) | Complete keyboard reference — hotkeys, navigation, text input, serial diagnostics |
| [Architecture](docs/ARCHITECTURE.md) | Layer diagram, directory tree, design decisions (radio, storage, WiFi, boot recovery) |
| [Development](docs/DEVELOPMENT.md) | How to add screens/hotkeys/settings/transports, init sequence, main loop, LXMF wire format |
| [Troubleshooting](docs/TROUBLESHOOTING.md) | Radio (TCXO, IRQ latch, PA ramp), build (USB mode, baud rate), boot loop, storage, WiFi |

## License

GPL-3.0
