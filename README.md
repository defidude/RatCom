<div align="center">

# [Ratcom](https://ratspeak.org/)

**Standalone Reticulum for the M5Stack Cardputer**

</div>

Ratcom turns an [M5Stack Cardputer Adv](https://docs.m5stack.com/en/core/M5Cardputer%20Adv) into a full standalone [Reticulum](https://reticulum.network/) node. It's not just an RNode which requires another device — it's the complete setup.

End-to-end encrypted [LXMF](https://github.com/markqvist/LXMF) messaging over LoRa, TCP over WiFi for bridging to the wider Reticulum network, node discovery, identity management, and more.

<div align="center">

---
[![Ratspeak Demo](https://img.youtube.com/vi/F6I6fkMPxgI/maxresdefault.jpg)](https://www.youtube.com/watch?v=F6I6fkMPxgI)

<sub>[▶ YouTube: Reticulum Standalone - T-Deck & Cardputer Adv](https://www.youtube.com/watch?v=F6I6fkMPxgI)</sub>

---
</div>

## Installing

The easiest way is the **[web flasher](https://ratspeak.org/download.html)** — enable download mode (hold G0 while plugging it in), select the USB, click flash, done.

To build from source:

```bash
git clone https://github.com/ratspeak/ratcom
cd ratcom
pip install platformio
python3 -m platformio run -e ratputer_915 -t upload
```

> If upload fails at 921600 baud, use esptool directly at 460800 or lower. See [docs/BUILDING.md](docs/BUILDING.md) for details.

## Usage

On first boot, Ratcom generates a Reticulum identity and shows a name input screen. Your LXMF address (32-character hex string) is what you share with contacts.

**Tabs:** Home, Messages, Nodes, Setup — navigate with `,` and `/` (arrow) keys.

**Manually announce:** To send an announcement manually, press the trackball or enter on the home tab.

**Sending a message:** Select a node from the Nodes tab, press Enter, type, press Enter to send. Messages are encrypted end-to-end with Ed25519 signatures.
**Radio presets** (Setup → Radio):
- **Long Range** — SF12, 62.5 kHz, 22 dBm. Longest distance, slow.
- **Balanced** — SF9, 125 kHz, 17 dBm. Medium distance, medium.
- **Fast** — SF7, 250 kHz, 14 dBm. Shortest distance, fast.

All radio parameters are individually tunable. Changes apply immediately, no reboot. Please operate in accordance with local laws, as you are solely responsible for knowing which regulations and requirements apply to your jurisdiction.

### WiFi Bridging (Alpha)

Use **STA mode** to connect to existing WiFi and reach remote nodes like `rns.ratspeak.org:4242`.

To bridge LoRa with Reticulum on your computer:

1. Set WiFi to **AP mode** in Setup → Network (creates `ratcom-XXXX`, password: `ratspeak`)
2. Connect your computer to that network
3. Add to your Reticulum config:

```ini
[[ratdeck]]
  type = TCPClientInterface
  target_host = 192.168.4.1
  target_port = 4242
```

Note: WiFi bridging methods and interfaces will be revamped with Ratspeak's client release, therefore, it's unlikely AP mode works at all currently.

## Docs

The detailed stuff lives in [`docs/`](docs/):

- **[Quick Start](docs/QUICKSTART.md)** — first build, first boot, first message
- [Building](docs/BUILDING.md) — build flags, esptool, merged binaries, CI
- [Architecture](docs/ARCHITECTURE.md) — layer diagram, design decisions
- [Development](docs/DEVELOPMENT.md) — adding screens, transports, settings
- [Hotkeys](docs/HOTKEYS.md) — full keyboard reference
- [Pin Map](docs/PINMAP.md) — GPIO assignments
- [Troubleshooting](docs/TROUBLESHOOTING.md) — radio, build, boot, storage

## License

GPL-3.0
