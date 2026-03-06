# Ratputer

Standalone [Reticulum](https://reticulum.network/) transport node + [LXMF](https://github.com/markqvist/LXMF) encrypted messenger, built for the [M5Stack Cardputer Adv](https://docs.m5stack.com/en/core/M5Cardputer%20Adv) with Cap LoRa-1262 radio module.

Not an RNode. Not a gateway. A fully self-contained mesh node with a keyboard, screen, LoRa radio, and WiFi — no host computer required.

## What This Does

Ratputer turns a $60 pocket computer into a Reticulum mesh node that can:

- **Send and receive encrypted messages** over LoRa (LXMF protocol, Ed25519 signatures)
- **Discover other nodes** automatically via Reticulum announces
- **Bridge LoRa ↔ WiFi** so desktop Reticulum instances can reach the mesh
- **Connect to remote Reticulum nodes** over TCP (e.g., `rns.beleth.net`)
- **Store messages and contacts** on flash and SD card with automatic backup
- **Configure everything on-device** — no config files, no host tools

The device runs [microReticulum](https://github.com/attermann/microReticulum) (a C++ port of the Reticulum stack) directly on the ESP32-S3, with a register-level SX1262 LoRa driver.

## Features

| Category | Details |
|----------|---------|
| **Networking** | Reticulum transport node, path discovery, announce propagation, auto-announce every 5 min |
| **Messaging** | LXMF encrypted messages, Ed25519 signatures, delivery tracking, per-conversation storage |
| **LoRa Radio** | SX1262 at 915 MHz, configurable SF (5–12), BW (7.8–500 kHz), CR (4/5–4/8), TX power (2–22 dBm) |
| **WiFi** | AP mode (TCP server on :4242) or STA mode (TCP client to remote nodes) — not concurrent |
| **Storage** | Dual-backend: LittleFS (1.875 MB on flash) + FAT32 microSD, atomic writes, identity backup |
| **UI** | 240×135 TFT, signal green on black, 4 tabs (Home/Msgs/Nodes/Setup), boot animation |
| **Input** | Full QWERTY keyboard (TCA8418 I2C), 8 Ctrl+key hotkeys, tab cycling |
| **Audio** | ES8311 codec, notification sounds for messages, announces, errors, boot chime |
| **Power** | Screen dim → off → wake on keypress, configurable timeouts, battery % in status bar |
| **Reliability** | Boot loop recovery (NVS counter, forces WiFi OFF after 3 failures), serial WIPE command |
| **Diagnostics** | Ctrl+D full dump, Ctrl+T radio test packet, Ctrl+R 5-second RSSI monitor |
| **Planned** | BLE Sideband (v1.1 stub), GNSS (pins defined), OTA updates (partition reserved) |

## Hardware

| Component | Part | Notes |
|-----------|------|-------|
| **Board** | M5Stack Cardputer Adv | ESP32-S3, 8MB flash, 240×135 ST7789V2 TFT, TCA8418 keyboard, 1750mAh battery |
| **Radio** | Cap LoRa-1262 | Semtech SX1262, 915 MHz ISM, TCXO 3.0V, DIO2 RF switch, HSPI bus |
| **Storage** | microSD card | Optional but recommended. FAT32, any size. Auto-formatted on first boot |
| **USB** | USB-C | USB-Serial/JTAG (not native CDC). Port: `/dev/cu.usbmodem*` (macOS), `/dev/ttyACM*` (Linux) |

See [docs/PINMAP.md](docs/PINMAP.md) for the full GPIO pin map.

## Prerequisites

| Requirement | Version | Install |
|-------------|---------|---------|
| **Python** | 3.12+ | [python.org](https://www.python.org/downloads/) or your package manager |
| **PlatformIO Core** | 6.x | `pip install platformio` |
| **Git** | any | Your package manager |
| **USB driver** | — | None needed on macOS/Linux (ESP32-S3 USB-Serial/JTAG is built-in) |

PlatformIO automatically downloads the ESP32-S3 toolchain, Arduino framework, and all library dependencies on first build.

## Build & Flash

```bash
# Clone
git clone https://github.com/defidude/Ratputer.git
cd Ratputer

# Build (first build takes ~2 min to download toolchain + deps)
python3 -m platformio run -e ratputer_915

# Flash (plug in Cardputer Adv via USB-C)
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

## First Boot

1. Plug in or power on the Cardputer Adv
2. Boot animation with progress bar (~3 seconds)
3. SX1262 radio initializes at 915 MHz
4. SD card checked, `/ratputer/` directories auto-created
5. Reticulum identity generated (Ed25519 keypair, persisted to flash + SD)
6. WiFi AP starts: `ratputer-XXXX` (password: `ratspeak`)
7. Initial announce broadcast to the mesh
8. Home screen: identity hash, transport status, radio info, uptime

## UI Layout

```
┌──────────────────────────────────┐
│ [87%] [Ratspeak.org] [LoRa]     │  Status bar: battery, transport mode, radio
├──────────────────────────────────┤
│                                  │
│         CONTENT AREA             │  Screens: Home, Messages, Nodes, Settings
│                                  │
├──────────────────────────────────┤
│ [Home] [Msgs] [Nodes] [Setup]   │  Tab bar with unread badges
└──────────────────────────────────┘
```

**Theme**: Signal green (#00FF41) on black. 240×135 pixels. Double-buffered M5Canvas sprite rendering at 20 FPS.

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

## WiFi & Networking

Three WiFi modes, configured in Settings → WiFi:

### OFF Mode

No WiFi. Saves power and ~20KB heap. LoRa-only operation.

### AP Mode (default)

Creates a WiFi hotspot named `ratputer-XXXX` (password: `ratspeak`). Runs a TCP server on port 4242 with HDLC framing (0x7E delimiters, 0x7D byte stuffing).

**Bridge to desktop Reticulum**: Connect your laptop to the `ratputer-XXXX` network, then add to your Reticulum config (`~/.reticulum/config`):

```ini
[[ratputer]]
  type = TCPClientInterface
  target_host = 192.168.4.1
  target_port = 4242
```

Now your desktop `rnsd` can reach the LoRa mesh through Ratputer.

### STA Mode

Connects to an existing WiFi network. Establishes outbound TCP connections to configured Reticulum endpoints.

**Setup**:
1. Ctrl+S → WiFi → Mode → **STA**
2. Enter your WiFi SSID and password
3. Add TCP endpoints: e.g., `rns.beleth.net` port `4242`, auto-connect enabled
4. Save — Ratputer connects to your WiFi, then opens TCP links to the configured hosts

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

### On-Air Format

Every LoRa packet has a 1-byte header prepended (RNode-compatible):
- Upper nibble: random sequence number
- Lower nibble: flags (0x01 = split, reserved for future multi-frame packets)

This means Ratputer packets are structurally compatible with RNodes on the same frequency and modulation settings.

## SD Card

Optional microSD card (FAT32, any size). Provides backup storage and extended capacity beyond the 1.875 MB LittleFS partition.

### Directory Structure (auto-created on first boot)

```
/ratputer/
├── config/
│   └── user.json           Runtime settings (radio, WiFi, display, audio)
├── messages/
│   └── <peer_hex>/         Per-conversation message history (JSON)
├── contacts/               Discovered Reticulum nodes
└── identity/
    └── identity.key        Ed25519 keypair backup
```

### Dual-Backend Storage

All writes go to **both** flash (LittleFS) and SD (FAT32). Reads prefer SD (larger capacity), fall back to flash. Both backends use atomic writes (`.tmp` → verify → `.bak` → rename) to prevent corruption on power loss.

### Factory Reset (SD)

Send `WIPE` over serial within 500ms of boot to recursively delete `/ratputer/*` and recreate clean directories.

## Settings

All configurable from the device via Ctrl+S (no config files needed):

| Category | Settings |
|----------|----------|
| **Radio** | Frequency, spreading factor, bandwidth, coding rate, TX power |
| **WiFi** | Mode (OFF/AP/STA), AP SSID + password, STA SSID + password, TCP endpoints |
| **Display** | Brightness (0–255), dim timeout (seconds), off timeout (seconds) |
| **Audio** | Enable/disable, volume (0–100) |
| **System** | About (version, identity, uptime), factory reset |

Settings are stored as JSON in both flash (`/config/user.json`) and SD (`/ratputer/config/user.json`).

## Serial Monitor & Debugging

```bash
python3 -m platformio device monitor -b 115200
```

The firmware logs extensively to serial at 115200 baud. Every subsystem prints tagged messages: `[BOOT]`, `[RADIO]`, `[WIFI]`, `[LXMF]`, `[LORA_IF]`, `[SD]`, etc.

### Boot Output (normal)

```
=================================
  Ratputer v1.0.0
  M5Stack Cardputer Adv
=================================
[RADIO] SX1262 online at 915 MHz
[SD] Send 'WIPE' now to wipe SD card (500ms window)...
[BOOT] Identity: a1b2c3d4e5f6...
[BOOT] Radio configured: 915000000 Hz, SF7, BW500000, CR4/5, 10 dBm
[WIFI] AP started: ratputer-A1B2 (192.168.4.1:4242)
[BLE] Disabled (stub — v1.1)
[BOOT] Initial announce sent
[BOOT] Ratputer ready
```

### Diagnostic Dump (Ctrl+D)

Prints identity, transport state, path/link counts, full radio parameters, SX1262 register dump (sync word, IQ polarity, LNA, OCP, TX clamp), device errors, RSSI, free heap, flash usage, and uptime.

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

### Build Toolchain

| Component | Version | Notes |
|-----------|---------|-------|
| PlatformIO | espressif32@6.7.0 | ESP-IDF + Arduino framework |
| Board | esp32-s3-devkitc-1 | Generic ESP32-S3, 8MB flash |
| Arduino Core | ESP32 Arduino 2.x | C++ exceptions enabled (`-fexceptions`) |

### Build Flags

| Flag | Purpose |
|------|---------|
| `-DRATPUTER=1` | Main feature guard |
| `-DARDUINO_USB_CDC_ON_BOOT=1` | USB CDC serial at boot |
| `-DARDUINO_USB_MODE=1` | USB-Serial/JTAG mode (not native CDC — required for macOS) |
| `-DRNS_USE_FS` | microReticulum filesystem persistence |
| `-DRNS_PERSIST_PATHS` | Persist transport paths to LittleFS |
| `-DMSGPACK_USE_BOOST=OFF` | No Boost dependency in MsgPack |
| `-fexceptions` | C++ exceptions (required by microReticulum) |

## Flash Memory Layout

8MB flash, partitioned for OTA support:

| Partition | Offset | Size | Purpose |
|-----------|--------|------|---------|
| nvs | 0x9000 | 20 KB | Boot counter, WiFi credentials |
| otadata | 0xE000 | 8 KB | OTA boot selection |
| app0 | 0x10000 | 3 MB | Active firmware |
| app1 | 0x310000 | 3 MB | OTA update slot (reserved, not yet implemented) |
| littlefs | 0x610000 | 1.875 MB | Identity, config, messages, transport paths |
| coredump | 0x7F0000 | 64 KB | ESP-IDF crash dump |

## Project Structure

```
Ratputer/
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

## Current Status

**v1.0.0** — Working on hardware. Boots stable, LoRa TX/RX confirmed, TCP transport to `rns.beleth.net` operational.

| Subsystem | Status |
|-----------|--------|
| LoRa radio | Working — TX/RX verified, RSSI/SNR confirmed with Heltec V3 |
| WiFi AP | Working — TCP server, HDLC framing, bridges to desktop rnsd |
| WiFi STA + TCP | Working — connects to remote Reticulum nodes |
| LXMF messaging | Working — send/receive/store with Ed25519 signatures |
| Node discovery | Working — automatic announce processing |
| SD card storage | Working — dual-backend with atomic writes |
| Settings | Working — full on-device configuration |
| Boot loop recovery | Working — NVS counter, WiFi forced OFF after 3 failures |
| BLE | Stub only — disabled, saves ~50KB heap (v1.1: Sideband protocol) |
| GNSS | Pins defined only — no code path (v1.1) |
| OTA updates | Partition reserved — not implemented |

## License

GPL-3.0
