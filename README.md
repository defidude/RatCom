<div align="center">

# ratcom

**Encrypted mesh messenger for the M5Stack Cardputer**

[![Ratspeak Demo](https://img.youtube.com/vi/F6I6fkMPxgI/maxresdefault.jpg)](https://www.youtube.com/watch?v=F6I6fkMPxgI)

<sub>[▶ YouTube: Reticulum Standalone - T-Deck & Cardputer Adv](https://www.youtube.com/watch?v=F6I6fkMPxgI)</sub>

A $50 handheld that runs [Reticulum](https://reticulum.network/) — no phone, no internet, no infrastructure required.

</div>

---

RatCom turns an [M5Stack Cardputer Adv](https://docs.m5stack.com/en/core/M5Cardputer%20Adv) into a self-contained encrypted mesh node. It's not an RNode and it's not a gateway — it's a complete Reticulum instance with a keyboard and a screen that fits in your pocket.

You get end-to-end encrypted [LXMF](https://github.com/markqvist/LXMF) messaging over LoRa, WiFi TCP bridging to the wider Reticulum network, contact management, multiple swappable identities, and configurable radio — all without ever touching a config file.

## Get one

1. Buy an **M5Stack Cardputer Adv** (~$50 — [M5Stack](https://shop.m5stack.com/), [AliExpress](https://aliexpress.com), or Amazon)
2. Attach a **915 MHz LoRa antenna** (SMA, included with some kits)
3. Flash the firmware

### Flash it

The easiest way is the **[web flasher](https://ratspeak.org/download.html)** — plug in USB, click flash, done.

To build from source:

```bash
git clone https://github.com/ratspeak/ratcom
cd ratcom
pip install platformio
python3 -m platformio run -e ratputer_915 -t upload
```

First build takes a couple minutes while PlatformIO pulls the ESP32-S3 toolchain. After that it's fast.

> If upload fails at 921600 baud, use esptool directly at 460800. See [docs/BUILDING.md](docs/BUILDING.md) for details.

## Using it

On first boot, RatCom generates a Reticulum identity and drops you on the Home tab. Your LXMF address (a 32-character hex string) is what you share with people so they can reach you.

**Tabs:** Home, Messages, Nodes, Setup — navigate with `,` and `/` keys.

**Sending a message:** Select a node from the Nodes tab, press Enter, type, press Enter to send. Messages are encrypted end-to-end with Ed25519 signatures.

**Radio presets** (Settings → Radio):
- **Long Range** — SF12, 62.5 kHz, 22 dBm. Maximum distance, very slow.
- **Balanced** — SF9, 125 kHz, 17 dBm. Good default.
- **Fast** — SF7, 250 kHz, 14 dBm. Short range, quick transfers.

All radio parameters are individually tunable. Changes apply immediately, no reboot.

### WiFi bridging

This is the killer feature for desktop users. RatCom can bridge your laptop to the LoRa mesh:

1. Set WiFi to **AP mode** (creates `ratcom-XXXX`, password: `ratspeak`)
2. Connect your laptop to that network
3. Add to your Reticulum config:

```ini
[[ratcom]]
  type = TCPClientInterface
  target_host = 192.168.4.1
  target_port = 4242
```

Now your desktop Reticulum instance can reach the LoRa mesh through RatCom's radio.

Or use **STA mode** to connect RatCom to your existing WiFi and reach remote nodes like `rns.ratspeak.org:4242`.

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
