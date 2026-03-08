// =============================================================================
// RatCom v1.5.1 — Main Entry Point
// C1-C7: Radio, Keyboard, Display, Reticulum, Nodes, WiFi, LXMF
// =============================================================================

#include <Arduino.h>
#include <M5Unified.h>
#include <M5Cardputer.h>
#include <SPI.h>

#include "config/BoardConfig.h"
#include "config/Config.h"
#include "radio/SX1262.h"
#include "input/Keyboard.h"
#include "input/HotkeyManager.h"
#include "ui/UIManager.h"
#include "ui/screens/BootScreen.h"
#include "ui/screens/HomeScreen.h"
#include "storage/FlashStore.h"
#include "storage/SDStore.h"
#include "storage/MessageStore.h"
#include "reticulum/ReticulumManager.h"
#include "reticulum/AnnounceManager.h"
#include "reticulum/LXMFManager.h"
#include "transport/WiFiInterface.h"
#include "transport/TCPClientInterface.h"
#include "config/UserConfig.h"
#include "ui/screens/NodesScreen.h"
#include "ui/screens/MessagesScreen.h"
#include "ui/screens/MessageView.h"
#include "ui/screens/SettingsScreen.h"
#include "ui/screens/NameInputScreen.h"
#include "ui/screens/HelpOverlay.h"
#include "power/PowerManager.h"
#include "audio/AudioNotify.h"
#include "transport/BLEStub.h"
#include <Preferences.h>
#include <list>
#include <esp_system.h>
#include <freertos/task.h>

// --- Hardware ---
SPIClass loraSPI(HSPI);
SX1262 radio(&loraSPI,
    LORA_CS, LORA_SCK, LORA_MOSI, LORA_MISO,
    LORA_RST, LORA_IRQ, LORA_BUSY, LORA_RXEN,
    LORA_HAS_TCXO, LORA_DIO2_AS_RF_SWITCH);

// --- Subsystems ---
Keyboard keyboard;
HotkeyManager hotkeys;
UIManager ui;
FlashStore flash;
SDStore sdStore;
MessageStore messageStore;
ReticulumManager rns;
AnnounceManager* announceManager = nullptr;
RNS::HAnnounceHandler announceHandler;
LXMFManager lxmf;
WiFiInterface* wifiImpl = nullptr;
RNS::Interface wifiIface({RNS::Type::NONE});
std::vector<TCPClientInterface*> tcpClients;
std::list<RNS::Interface> tcpIfaces;  // Must persist — Transport stores references (list: no realloc)
UserConfig userConfig;
PowerManager power;
AudioNotify audio;
BLEStub ble;

// --- Screens ---
BootScreen bootScreen;
HomeScreen homeScreen;
NodesScreen nodesScreen;
MessagesScreen messagesScreen;
MessageView messageView;
NameInputScreen nameInputScreen;
SettingsScreen settingsScreen;
HelpOverlay helpOverlay;

// Tab-screen mapping
Screen* tabScreens[4] = {nullptr, nullptr, nullptr, nullptr};

// --- State ---
bool radioOnline = false;
bool bootComplete = false;
bool bootLoopRecovery = false;
bool wifiSTAStarted = false;
bool wifiSTAConnected = false;
bool tcpClientsCreated = false;

// --- Timing state (millis-based throttling) ---
unsigned long lastRNS = 0;
unsigned long lastRender = 0;
unsigned long lastAutoAnnounce = 0;
unsigned long lastHeartbeat = 0;
unsigned long lastStatusUpdate = 0;
unsigned long lastContactSave = 0;
unsigned long loopCycleStart = 0;
unsigned long maxLoopTime = 0;

// --- Intervals ---
constexpr unsigned long RNS_INTERVAL_MS = 5;          // 200 Hz (matches Ratdeck)
constexpr unsigned long RENDER_INTERVAL_MS = 50;       // 20 FPS
constexpr unsigned long STATUS_UPDATE_MS = 1000;       // 1 Hz status bar
constexpr unsigned long CONTACT_SAVE_MS = 30000;       // 30s contact batch save
constexpr unsigned long ANNOUNCE_INTERVAL_MS = 120000; // 2 minutes
constexpr unsigned long HEARTBEAT_INTERVAL_MS = 5000;

// Power-aware RNS interval
unsigned long rnsInterval = RNS_INTERVAL_MS;

// =============================================================================
// Hotkey callbacks
// =============================================================================

void onHotkeyHelp() {
    Serial.println("[HOTKEY] Help overlay");
    helpOverlay.toggle();
    ui.setOverlay(helpOverlay.isVisible() ? &helpOverlay : nullptr);
}
void onHotkeyMessages() {
    Serial.println("[HOTKEY] Jump to Messages");
    ui.tabBar().setActiveTab(TabBar::TAB_MSGS);
    ui.setScreen(&messagesScreen);
}
void onHotkeyNewMsg() {
    Serial.println("[HOTKEY] New message");
    ui.tabBar().setActiveTab(TabBar::TAB_MSGS);
    ui.setScreen(&messagesScreen);
}
void onHotkeySettings() {
    Serial.println("[HOTKEY] Jump to Settings");
    ui.tabBar().setActiveTab(TabBar::TAB_SETUP);
    ui.setScreen(&settingsScreen);
}
static void announceWithName();
void onHotkeyAnnounce() {
    Serial.println("[HOTKEY] Force announce");
    announceWithName();
}
void onHotkeyDiag() {
    Serial.println("=== DIAGNOSTIC DUMP ===");
    Serial.printf("Identity: %s\n", rns.identityHash().c_str());
    Serial.printf("Transport: %s\n", rns.isTransportActive() ? "ACTIVE" : "OFFLINE");
    Serial.printf("Paths: %d  Links: %d\n", (int)rns.pathCount(), (int)rns.linkCount());
    Serial.printf("Radio: %s\n", radioOnline ? "ONLINE" : "OFFLINE");
    if (radioOnline) {
        Serial.printf("Freq: %lu Hz  SF: %d  BW: %lu  CR: 4/%d  TXP: %d dBm\n",
                      (unsigned long)radio.getFrequency(),
                      radio.getSpreadingFactor(),
                      (unsigned long)radio.getSignalBandwidth(),
                      radio.getCodingRate4(),
                      radio.getTxPower());
        Serial.printf("Preamble: %ld symbols\n", radio.getPreambleLength());
        Serial.printf("SyncWord regs: 0x%02X%02X\n",
            radio.readRegister(REG_SYNC_WORD_MSB_6X),
            radio.readRegister(REG_SYNC_WORD_LSB_6X));
        uint16_t devErr = radio.getDeviceErrors();
        uint8_t status = radio.getStatus();
        Serial.printf("DevErrors: 0x%04X  Status: 0x%02X (mode=%d cmd=%d)\n",
            devErr, status, (status >> 4) & 0x07, (status >> 1) & 0x07);
        if (devErr & 0x40) Serial.println("  *** PLL LOCK FAILED ***");
        Serial.printf("Current RSSI: %d dBm\n", radio.currentRssi());

        Serial.println("--- SX1262 Register Dump ---");
        Serial.printf("  0x0740 (SyncWordMSB): 0x%02X\n", radio.readRegister(0x0740));
        Serial.printf("  0x0741 (SyncWordLSB): 0x%02X\n", radio.readRegister(0x0741));
        Serial.printf("  0x0889 (IQ polarity): 0x%02X\n", radio.readRegister(0x0889));
        Serial.printf("  0x0736 (IQ config):   0x%02X\n", radio.readRegister(0x0736));
        Serial.printf("  0x08AC (LNA):         0x%02X\n", radio.readRegister(0x08AC));
        Serial.printf("  0x08E7 (OCP):         0x%02X\n", radio.readRegister(0x08E7));
        Serial.printf("  0x0902 (TX clamp):    0x%02X\n", radio.readRegister(0x0902));
        Serial.println("----------------------------");
    }
    Serial.printf("Free heap: %lu bytes\n", (unsigned long)ESP.getFreeHeap());
    Serial.printf("Flash: %lu/%lu bytes\n",
                  (unsigned long)LittleFS.usedBytes(),
                  (unsigned long)LittleFS.totalBytes());
    Serial.printf("WriteQ pending: %d\n", messageStore.writeQueue().drainCount());
    Serial.printf("Uptime: %lu s\n", millis() / 1000);
    Serial.println("=======================");
}

// Ctrl+R: Continuous RSSI sampling for 5 seconds
volatile bool rssiMonitorActive = false;

void onHotkeyRssiMonitor() {
    if (!radioOnline) {
        Serial.println("[RSSI] Radio offline");
        return;
    }
    Serial.println("[RSSI] Sampling RSSI for 5 seconds — transmit from another device now...");
    rssiMonitorActive = true;
    int minRssi = 0, maxRssi = -200;
    unsigned long start = millis();
    int samples = 0;
    while (millis() - start < 5000) {
        int rssi = radio.currentRssi();
        if (rssi < minRssi) minRssi = rssi;
        if (rssi > maxRssi) maxRssi = rssi;
        samples++;
        Serial.printf("[RSSI] %d dBm\n", rssi);
        delay(100);
    }
    rssiMonitorActive = false;
    Serial.printf("[RSSI] Done: %d samples, min=%d max=%d dBm\n", samples, minRssi, maxRssi);
    Serial.printf("[RSSI] If max stayed near %d dBm, RX front-end may not be receiving RF\n", minRssi);
}

void onHotkeyRadioTest() {
    Serial.println("[TEST] Sending raw radio test packet...");
    uint8_t header = 0xA0;
    const char* testPayload = "RATPUTER_TEST_1234567890";
    size_t totalLen = 1 + strlen(testPayload);

    radio.beginPacket();
    radio.write(header);
    radio.write((const uint8_t*)testPayload, strlen(testPayload));
    bool ok = radio.endPacket();

    Serial.printf("[TEST] TX %s (%d bytes)\n", ok ? "OK" : "FAILED", (int)totalLen);

    uint8_t verify[32] = {0};
    radio.readBuffer(verify, totalLen);
    Serial.printf("[TEST] FIFO verify: ");
    for (size_t i = 0; i < totalLen; i++) Serial.printf("%02X ", verify[i]);
    Serial.println();

    radio.receive();
}

// =============================================================================
// Announce with display name
// =============================================================================

static RNS::Bytes encodeAnnounceName(const String& name) {
    if (name.isEmpty()) return {};
    size_t len = name.length();
    if (len > 31) len = 31;
    uint8_t buf[2 + 31];
    buf[0] = 0x91;                     // msgpack fixarray(1)
    buf[1] = 0xA0 | (uint8_t)len;     // msgpack fixstr(len)
    memcpy(buf + 2, name.c_str(), len);
    return RNS::Bytes(buf, 2 + len);
}

static void announceWithName() {
    RNS::Bytes appData = encodeAnnounceName(userConfig.settings().displayName);
    rns.announce(appData);
    ui.statusBar().flashAnnounce();
}

// =============================================================================
// Setup
// =============================================================================

void setup() {
    // Initialize M5Cardputer (includes M5Unified + keyboard)
    auto cfg = M5.config();
    cfg.serial_baudrate = SERIAL_BAUD;
    M5Cardputer.begin(cfg, true);

    Serial.println();
    Serial.println("=================================");
    Serial.printf("  RatCom v%s\n", RATPUTER_VERSION_STRING);
    Serial.println("  M5Stack Cardputer Adv");
    Serial.println("=================================");

    esp_reset_reason_t reason = esp_reset_reason();
    const char* reasonStr = "UNKNOWN";
    switch (reason) {
        case ESP_RST_POWERON:   reasonStr = "POWER_ON"; break;
        case ESP_RST_SW:        reasonStr = "SOFTWARE"; break;
        case ESP_RST_PANIC:     reasonStr = "PANIC (crash!)"; break;
        case ESP_RST_INT_WDT:   reasonStr = "INT_WDT (interrupt watchdog!)"; break;
        case ESP_RST_TASK_WDT:  reasonStr = "TASK_WDT (task watchdog!)"; break;
        case ESP_RST_WDT:       reasonStr = "WDT (other watchdog!)"; break;
        case ESP_RST_BROWNOUT:  reasonStr = "BROWNOUT (low voltage!)"; break;
        case ESP_RST_DEEPSLEEP: reasonStr = "DEEP_SLEEP"; break;
        default: break;
    }
    Serial.printf("[BOOT] Reset reason: %s (%d)\n", reasonStr, (int)reason);
    Serial.printf("[BOOT] Free heap: %lu, min ever: %lu\n",
                  (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMinFreeHeap());

    // Initialize UI + boot screen
    ui.begin();
    ui.setBootMode(true);
    ui.setScreen(&bootScreen);
    bootScreen.setProgress(0.1f, "Display ready");
    ui.render();

    // Initialize keyboard
    keyboard.begin();
    bootScreen.setProgress(0.15f, "Keyboard ready");
    ui.render();

    // Register hotkeys
    hotkeys.registerHotkey('h', "Help", onHotkeyHelp);
    hotkeys.registerHotkey('m', "Messages", onHotkeyMessages);
    hotkeys.registerHotkey('n', "New Message", onHotkeyNewMsg);
    hotkeys.registerHotkey('s', "Settings", onHotkeySettings);
    hotkeys.registerHotkey('a', "Announce", onHotkeyAnnounce);
    hotkeys.registerHotkey('d', "Diagnostics", onHotkeyDiag);
    hotkeys.registerHotkey('t', "Radio Test", onHotkeyRadioTest);
    hotkeys.registerHotkey('r', "RSSI Monitor", onHotkeyRssiMonitor);
    hotkeys.setTabCycleCallback([](int dir) {
        ui.tabBar().cycleTab(dir);
        int tab = ui.tabBar().getActiveTab();
        if (tabScreens[tab]) {
            ui.setScreen(tabScreens[tab]);
        }
    });
    bootScreen.setProgress(0.2f, "Hotkeys registered");
    ui.render();

    // Initialize flash storage
    bootScreen.setProgress(0.25f, "Mounting flash...");
    ui.render();
    if (!flash.begin()) {
        Serial.println("[BOOT] Flash init failed!");
    }
    bootScreen.setProgress(0.3f, "Storage ready");
    ui.render();

    // Boot loop detection (NVS — separate from LittleFS)
    {
        Preferences prefs;
        if (prefs.begin("ratcom", false)) {
            int bc = prefs.getInt("bootc", 0);
            prefs.putInt("bootc", bc + 1);
            prefs.end();
            if (bc >= 3) {
                Serial.printf("[BOOT] Boot loop detected (%d consecutive failures)\n", bc);
                Serial.println("[BOOT] Falling back to WiFi OFF for safe boot");
                bootLoopRecovery = true;
            }
        }
    }

    // Initialize radio
    bootScreen.setProgress(0.4f, "Starting radio...");
    ui.render();
    if (radio.begin(LORA_DEFAULT_FREQ)) {
        radio.setSpreadingFactor(LORA_DEFAULT_SF);
        radio.setSignalBandwidth(LORA_DEFAULT_BW);
        radio.setCodingRate4(LORA_DEFAULT_CR);
        radio.setTxPower(LORA_DEFAULT_TX_POWER);
        radio.setPreambleLength(LORA_DEFAULT_PREAMBLE);
        radio.receive();
        radioOnline = true;
        ui.statusBar().setLoRaOnline(true);
        Serial.println("[RADIO] SX1262 online at 915 MHz");
        bootScreen.setProgress(0.6f, "Radio online");
    } else {
        Serial.println("[RADIO] SX1262 not detected!");
        bootScreen.setProgress(0.6f, "Radio: OFFLINE");
    }
    ui.render();

    // Initialize SD card (shares HSPI with radio — must init after radio)
    bootScreen.setProgress(0.65f, "Checking SD card...");
    ui.render();
    if (sdStore.begin(&loraSPI, SD_CS)) {
        sdStore.ensureDir("/ratcom");
        sdStore.ensureDir("/ratcom/messages");
        sdStore.ensureDir("/ratcom/contacts");
        sdStore.ensureDir("/ratcom/identity");

        Serial.println("[SD] Send 'WIPE' now to wipe SD card (500ms window)...");
        unsigned long wipeDeadline = millis() + 500;
        String cmd;
        while (millis() < wipeDeadline) {
            while (Serial.available()) cmd += (char)Serial.read();
            if (cmd.indexOf("WIPE") >= 0) {
                sdStore.wipeRatcom();
                Serial.println("[SD] SD card wiped and reinitialized");
                break;
            }
            delay(10);
        }

        bootScreen.setProgress(0.68f, "SD card ready");
    } else {
        bootScreen.setProgress(0.68f, "No SD card");
    }
    ui.render();

    // Initialize Reticulum
    bootScreen.setProgress(0.7f, "Starting Reticulum...");
    ui.render();
    rns.setSDStore(&sdStore);
    if (rns.begin(&radio, &flash)) {
        Serial.printf("[BOOT] Identity: %s\n", rns.identityHash().c_str());
        bootScreen.setProgress(0.9f, "Reticulum active");
    } else {
        Serial.println("[BOOT] Reticulum init failed!");
        bootScreen.setProgress(0.9f, "RNS: FAILED");
    }
    ui.render();

    // Initialize message store + LXMF
    bootScreen.setProgress(0.91f, "Starting messaging...");
    ui.render();
    messageStore.begin(&flash, &sdStore);
    lxmf.begin(&rns, &messageStore);
    lxmf.setMessageCallback([](const LXMFMessage& msg) {
        Serial.printf("[LXMF] New message from %s\n", msg.sourceHash.toHex().substr(0, 8).c_str());
        ui.tabBar().setUnreadCount(TabBar::TAB_MSGS, lxmf.unreadCount());
        ui.markContentDirty();
        ui.markTabDirty();
        messagesScreen.notifyNewMessage();
        messageView.notifyNewMessage(msg);
        audio.playMessage();
    });

    // Status callback — update UI when send completes (SENT/FAILED)
    lxmf.setStatusCallback([](const std::string& peerHex, double timestamp, LXMFStatus status) {
        messageView.notifyStatusChange(peerHex, timestamp, status);
        ui.markContentDirty();
    });

    // Unread counts load lazily on first MessagesScreen open (deferred from boot)

    // Register announce handler
    bootScreen.setProgress(0.93f, "Starting discovery...");
    ui.render();
    announceManager = new AnnounceManager();
    announceManager->setStorage(&sdStore, &flash);
    announceManager->setLocalDestHash(rns.destination().hash());
    announceManager->loadContacts();
    announceHandler = RNS::HAnnounceHandler(announceManager);
    RNS::Transport::register_announce_handler(announceHandler);

    // Load user config (SD primary, flash fallback)
    userConfig.load(sdStore, flash);

    // Seed default Ratspeak TCP hub if no connections configured
    if (userConfig.settings().tcpConnections.empty()) {
        TCPEndpoint ep;
        ep.host = "rns.ratspeak.org";
        ep.port = 4242;
        ep.autoConnect = true;
        userConfig.settings().tcpConnections.push_back(ep);
        Serial.println("[CONFIG] Default TCP hub: rns.ratspeak.org:4242");
    }

    // Boot loop recovery: force WiFi OFF to break crash cycle
    if (bootLoopRecovery) {
        userConfig.settings().wifiMode = RAT_WIFI_OFF;
        Serial.println("[BOOT] WiFi forced OFF (boot loop recovery)");
    }

    // Apply radio settings
    if (radioOnline) {
        auto& s = userConfig.settings();
        radio.setFrequency(s.loraFrequency);
        radio.setSpreadingFactor(s.loraSF);
        radio.setSignalBandwidth(s.loraBW);
        radio.setCodingRate4(s.loraCR);
        radio.setTxPower(s.loraTxPower);
        radio.receive();
        Serial.printf("[BOOT] Radio configured: %lu Hz, SF%d, BW%lu, CR4/%d, %d dBm\n",
                      (unsigned long)s.loraFrequency, s.loraSF,
                      (unsigned long)s.loraBW, s.loraCR, s.loraTxPower);
    }

    // Mode-based WiFi startup
    RatWiFiMode wifiMode = userConfig.settings().wifiMode;

    if (wifiMode == RAT_WIFI_AP) {
        bootScreen.setProgress(0.95f, "Starting WiFi AP...");
        ui.render();
        wifiImpl = new WiFiInterface("WiFi.AP");
        if (!userConfig.settings().wifiAPSSID.isEmpty()) {
            wifiImpl->setAPCredentials(
                userConfig.settings().wifiAPSSID.c_str(),
                userConfig.settings().wifiAPPassword.c_str());
        }
        wifiIface = wifiImpl;
        wifiIface.mode(RNS::Type::Interface::MODE_GATEWAY);
        RNS::Transport::register_interface(wifiIface);
        wifiImpl->start();

    } else if (wifiMode == RAT_WIFI_STA) {
        bootScreen.setProgress(0.95f, "WiFi STA starting...");
        ui.render();
        if (!userConfig.settings().wifiSTASSID.isEmpty()) {
            WiFi.mode(WIFI_STA);
            WiFi.setAutoReconnect(true);
            WiFi.begin(userConfig.settings().wifiSTASSID.c_str(),
                       userConfig.settings().wifiSTAPassword.c_str());
            wifiSTAStarted = true;
            Serial.printf("[WIFI] STA non-blocking begin (SSID: %s)\n",
                          userConfig.settings().wifiSTASSID.c_str());
        } else {
            Serial.println("[WIFI] STA mode but SSID empty — skipping");
        }
    } else {
        bootScreen.setProgress(0.95f, "WiFi disabled");
        ui.render();
        Serial.println("[WIFI] Disabled by config");
    }

    // BLE disabled
    Serial.println("[BLE] Disabled (stub — v1.1)");

    // Initialize power manager + audio
    power.begin();
    power.setDimTimeout(userConfig.settings().screenDimTimeout);
    power.setOffTimeout(userConfig.settings().screenOffTimeout);
    power.setBrightness(userConfig.settings().brightness);

    audio.setEnabled(userConfig.settings().audioEnabled);
    audio.setVolume(userConfig.settings().audioVolume);
    audio.begin();

    // Boot complete
    delay(200);
    bootScreen.setProgress(1.0f, "Ready");
    ui.render();
    audio.playBoot();
    delay(400);

    bootComplete = true;
    ui.statusBar().setTransportMode("Ratspeak.org");

    // Set up screens
    homeScreen.setReticulumManager(&rns);
    homeScreen.setRadio(&radio);
    homeScreen.setUserConfig(&userConfig);
    homeScreen.setAnnounceCallback([]() { announceWithName(); });
    nodesScreen.setAnnounceManager(announceManager);
    nodesScreen.setNodeSelectedCallback([](const std::string& peerHex) {
        messageView.setPeerHex(peerHex);
        ui.tabBar().setActiveTab(TabBar::TAB_MSGS);
        ui.setScreen(&messageView);
    });
    nodesScreen.setNodeSaveCallback([](const std::string& peerHex, bool save) {
        if (save) announceManager->saveNode(peerHex);
        else announceManager->unsaveNode(peerHex);
    });
    messagesScreen.setLXMFManager(&lxmf);
    messagesScreen.setAnnounceManager(announceManager);
    messagesScreen.setOpenCallback([](const std::string& peerHex) {
        messageView.setPeerHex(peerHex);
        ui.setScreen(&messageView);
    });
    messageView.setLXMFManager(&lxmf);
    messageView.setAnnounceManager(announceManager);
    messageView.setBackCallback([]() {
        ui.setScreen(&messagesScreen);
    });
    messageView.setUnreadUpdateCallback([]() {
        ui.tabBar().setUnreadCount(TabBar::TAB_MSGS, lxmf.unreadCount());
        ui.markTabDirty();
    });

    settingsScreen.setUserConfig(&userConfig);
    settingsScreen.setFlashStore(&flash);
    settingsScreen.setSDStore(&sdStore);
    settingsScreen.setRadio(&radio);
    settingsScreen.setAudio(&audio);
    settingsScreen.setPower(&power);
    settingsScreen.setWiFi(wifiImpl);
    settingsScreen.setTCPClients(&tcpClients);
    settingsScreen.setRNS(&rns);
    settingsScreen.setIdentityHash(rns.destinationHashStr());

    tabScreens[TabBar::TAB_HOME]  = &homeScreen;
    tabScreens[TabBar::TAB_MSGS]  = &messagesScreen;
    tabScreens[TabBar::TAB_NODES] = &nodesScreen;
    tabScreens[TabBar::TAB_SETUP] = &settingsScreen;

    // Name input flow or straight to home
    if (userConfig.settings().displayName.isEmpty()) {
        // Show name input screen (boot mode stays on for clean branded look)
        nameInputScreen.setDoneCallback([](const String& name) {
            if (!name.isEmpty()) {
                userConfig.settings().displayName = name;
                if (sdStore.isReady()) {
                    userConfig.save(sdStore, flash);
                } else {
                    userConfig.save(flash);
                }
            }
            ui.setBootMode(false);
            ui.setScreen(&homeScreen);
            ui.tabBar().setActiveTab(TabBar::TAB_HOME);
            announceWithName();
            lastAutoAnnounce = millis();
            Serial.println("[BOOT] Initial announce sent");
        });
        ui.setScreen(&nameInputScreen);
    } else {
        ui.setBootMode(false);
        ui.setScreen(&homeScreen);
        ui.tabBar().setActiveTab(TabBar::TAB_HOME);
        announceWithName();
        lastAutoAnnounce = millis();
        Serial.println("[BOOT] Initial announce sent");
    }

    // Clear boot loop counter — setup completed successfully
    {
        Preferences prefs;
        if (prefs.begin("ratcom", false)) {
            prefs.putInt("bootc", 0);
            prefs.end();
        }
    }

    // Initialize timing
    unsigned long now = millis();
    lastRNS = now;
    lastRender = now;
    lastStatusUpdate = now;
    lastContactSave = now;
    loopCycleStart = now;

    Serial.println("[BOOT] RatCom ready");
}

// =============================================================================
// Main Loop — Throttled, non-blocking
// =============================================================================

void loop() {
    unsigned long now = millis();
    M5Cardputer.update();

    // 1. Input (always, no throttle — responsiveness critical)
    keyboard.update();
    if (keyboard.hasEvent()) {
        const KeyEvent& evt = keyboard.getEvent();
        power.activity();

        if (helpOverlay.isVisible()) {
            helpOverlay.handleKey(evt);
            ui.setOverlay(helpOverlay.isVisible() ? &helpOverlay : nullptr);
        }
        else if (!hotkeys.process(evt)) {
            bool consumed = ui.handleKey(evt);

            if (!consumed && !evt.ctrl) {
                if (evt.character == ',') {
                    ui.tabBar().cycleTab(-1);
                    int tab = ui.tabBar().getActiveTab();
                    if (tabScreens[tab]) ui.setScreen(tabScreens[tab]);
                }
                if (evt.character == '/') {
                    ui.tabBar().cycleTab(1);
                    int tab = ui.tabBar().getActiveTab();
                    if (tabScreens[tab]) ui.setScreen(tabScreens[tab]);
                }
            }
        }
    }

    // 2. Reticulum + radio (throttled — 200Hz active, 20Hz screen off)
    if (now - lastRNS >= rnsInterval) {
        lastRNS = now;
        rns.loop();
    }

    // 3. LXMF outgoing queue (every loop — cheap if empty)
    lxmf.loop();

    // 4. Auto-announce (2 minutes)
    if (bootComplete && now - lastAutoAnnounce >= ANNOUNCE_INTERVAL_MS) {
        lastAutoAnnounce = now;
        rns.announce();
        ui.statusBar().flashAnnounce();
        Serial.println("[AUTO] Periodic announce");
    }

    // 5. WiFi STA non-blocking connection handler
    if (wifiSTAStarted) {
        bool connected = (WiFi.status() == WL_CONNECTED);
        if (connected && !wifiSTAConnected) {
            wifiSTAConnected = true;
            Serial.printf("[WIFI] STA connected: %s\n", WiFi.localIP().toString().c_str());

            {
                int8_t off = userConfig.settings().utcOffset;
                char tz[16];
                snprintf(tz, sizeof(tz), "UTC%d", -off);
                configTzTime(tz, "pool.ntp.org", "time.nist.gov");
                Serial.printf("[NTP] Time sync started (UTC%+d, TZ=%s)\n", off, tz);
            }

            if (!tcpClientsCreated) {
                tcpClientsCreated = true;
                for (auto& ep : userConfig.settings().tcpConnections) {
                    if (ep.autoConnect) {
                        char name[32];
                        snprintf(name, sizeof(name), "TCP.%s", ep.host.c_str());
                        auto* tcp = new TCPClientInterface(ep.host.c_str(), ep.port, name);
                        tcpIfaces.emplace_back(tcp);
                        tcpIfaces.back().mode(RNS::Type::Interface::MODE_GATEWAY);
                        RNS::Transport::register_interface(tcpIfaces.back());
                        tcp->start();
                        tcpClients.push_back(tcp);
                    }
                }
            }
        } else if (!connected && wifiSTAConnected) {
            wifiSTAConnected = false;
            Serial.println("[WIFI] STA disconnected (auto-reconnect active)");
        }
    }

    // 6. TCP transport (frame-limited per client)
    if (wifiImpl) wifiImpl->loop();
    for (auto* tcp : tcpClients) {
        tcp->loop();
    }

    // 7. Deferred contact save (every 30s)
    if (now - lastContactSave >= CONTACT_SAVE_MS) {
        lastContactSave = now;
        if (announceManager) announceManager->flushContacts();
    }

    // 8. Power management
    power.loop();

    // 9. Power-aware RNS throttle
    if (power.state() == PowerManager::SCREEN_OFF) {
        rnsInterval = 50;  // 20 Hz when screen off
    } else {
        rnsInterval = RNS_INTERVAL_MS;  // 200 Hz when active
    }

    // 10. Render (20 FPS, skip if screen off or dimmed-frozen)
    if (now - lastRender >= RENDER_INTERVAL_MS) {
        lastRender = now;
        if (power.isScreenOn() && power.state() != PowerManager::DIMMED) {
            // Status bar needs periodic refresh for battery + announce flash
            if (now - lastStatusUpdate >= STATUS_UPDATE_MS) {
                lastStatusUpdate = now;
                ui.markStatusDirty();
            }
            ui.render();
        }
    }

    // 11. Heartbeat (5s)
    {
        unsigned long cycleTime = now - loopCycleStart;
        if (cycleTime > maxLoopTime) maxLoopTime = cycleTime;

        if (now - lastHeartbeat >= HEARTBEAT_INTERVAL_MS) {
            lastHeartbeat = now;
            Serial.printf("[HEART] heap=%lu min=%lu stack=%lu loop=%lums nodes=%d paths=%d links=%d lxmfQ=%d writeQ=%d up=%lus\n",
                          (unsigned long)ESP.getFreeHeap(),
                          (unsigned long)ESP.getMinFreeHeap(),
                          (unsigned long)uxTaskGetStackHighWaterMark(NULL),
                          maxLoopTime,
                          announceManager ? announceManager->nodeCount() : 0,
                          (int)rns.pathCount(),
                          (int)rns.linkCount(),
                          lxmf.queuedCount(),
                          messageStore.writeQueue().drainCount(),
                          millis() / 1000);
            maxLoopTime = 0;
        }
    }
    loopCycleStart = now;

    yield();
}
