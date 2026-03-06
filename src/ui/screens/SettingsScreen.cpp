#include "SettingsScreen.h"
#include "ui/Theme.h"
#include "config/Config.h"
#include <algorithm>

void SettingsScreen::onEnter() {
    _subMenu = MENU_MAIN;
    _editing = false;
    _editField = -1;
    buildMainMenu();
}

void SettingsScreen::buildMainMenu() {
    _list.clear();
    _list.addItem("> Radio Settings");
    _list.addItem("> WiFi Settings");
    _list.addItem("> SD Card");
    _list.addItem("> Display");
    _list.addItem("> Audio");
    _list.addItem("> About");
    _list.addItem("! Factory Reset");
}

static const char* detectPresetName(const UserSettings& s) {
    if (s.loraSF == 9 && s.loraBW == 250000 && s.loraCR == 5 && s.loraTxPower == 14)
        return "Balanced";
    if (s.loraSF == 12 && s.loraBW == 125000 && s.loraCR == 8 && s.loraTxPower == 17)
        return "Long Range";
    if (s.loraSF == 7 && s.loraBW == 500000 && s.loraCR == 5 && s.loraTxPower == 10)
        return "Fast";
    return "Custom";
}

void SettingsScreen::buildRadioMenu() {
    _list.clear();
    if (!_config) return;
    auto& s = _config->settings();
    char buf[40];

    // Active preset indicator
    snprintf(buf, sizeof(buf), "Active: %s", detectPresetName(s));
    _list.addItem(buf);

    // Presets (items 1-3)
    _list.addItem("[Balanced: SF9/250k]");
    _list.addItem("[Long Range: SF12/125k]");
    _list.addItem("[Fast: SF7/500k]");

    // Editable fields (items 4-8)
    snprintf(buf, sizeof(buf), "Frequency: %lu Hz", (unsigned long)s.loraFrequency);
    _list.addItem(buf);
    snprintf(buf, sizeof(buf), "SF: %d", s.loraSF);
    _list.addItem(buf);
    snprintf(buf, sizeof(buf), "BW: %lu Hz", (unsigned long)s.loraBW);
    _list.addItem(buf);
    snprintf(buf, sizeof(buf), "CR: %d", s.loraCR);
    _list.addItem(buf);
    snprintf(buf, sizeof(buf), "TX Power: %d dBm", s.loraTxPower);
    _list.addItem(buf);

    _list.addItem("< Back");
}

void SettingsScreen::buildWiFiMenu() {
    _list.clear();
    if (!_config) return;
    auto& s = _config->settings();

    // Item 0: Mode selector
    const char* modeNames[] = {"OFF", "AP", "STA"};
    char modeBuf[24];
    snprintf(modeBuf, sizeof(modeBuf), "Mode: %s", modeNames[s.wifiMode]);
    _list.addItem(modeBuf);

    if (s.wifiMode == RAT_WIFI_AP) {
        // Items 1-2: AP fields
        String apSSID = s.wifiAPSSID.isEmpty() ? "(auto)" : s.wifiAPSSID;
        _list.addItem(("AP SSID: " + std::string(apSSID.c_str())));
        _list.addItem(("AP Pass: " + std::string(s.wifiAPPassword.c_str())));
    } else if (s.wifiMode == RAT_WIFI_STA) {
        // Item 1: Connection status
        if (WiFi.status() == WL_CONNECTED) {
            char statusBuf[48];
            snprintf(statusBuf, sizeof(statusBuf), "Connected: %s", WiFi.SSID().c_str());
            _list.addItem(statusBuf);
            _list.addItem("[Disconnect]");          // Item 2
        } else if (!s.wifiSTASSID.isEmpty()) {
            char statusBuf[48];
            snprintf(statusBuf, sizeof(statusBuf), "Saved: %s (offline)", s.wifiSTASSID.c_str());
            _list.addItem(statusBuf);
            _list.addItem("[Connect]");             // Item 2
        } else {
            _list.addItem("No network configured");
            _list.addItem("");                      // Item 2 placeholder
        }
        _list.addItem("[Scan Networks]");           // Item 3
        _list.addItem("> TCP Connections");          // Item 4
    }
    // OFF mode: only mode selector + back

    _list.addItem("< Back");
}

void SettingsScreen::buildTCPMenu() {
    _list.clear();
    if (!_config) return;
    auto& s = _config->settings();

    _list.addItem("+ Add Connection");

    for (size_t i = 0; i < s.tcpConnections.size(); i++) {
        auto& ep = s.tcpConnections[i];
        char buf[64];
        snprintf(buf, sizeof(buf), "%s:%d [%s]",
                 ep.host.c_str(), ep.port,
                 ep.autoConnect ? "ON" : "OFF");
        _list.addItem(buf);
    }

    _list.addItem("< Back");
}

void SettingsScreen::addTCPConnection(const std::string& hostPort) {
    if (!_config) return;
    auto& s = _config->settings();
    if (s.tcpConnections.size() >= MAX_TCP_CONNECTIONS) {
        showToast("Max 4 connections");
        return;
    }

    TCPEndpoint ep;
    // Parse "host:port" or just "host"
    size_t colon = hostPort.rfind(':');
    if (colon != std::string::npos && colon > 0) {
        ep.host = hostPort.substr(0, colon).c_str();
        ep.port = (uint16_t)atoi(hostPort.substr(colon + 1).c_str());
        if (ep.port == 0) ep.port = TCP_DEFAULT_PORT;
    } else {
        ep.host = hostPort.c_str();
    }

    if (ep.host.isEmpty()) return;

    s.tcpConnections.push_back(ep);
    applyAndSave();
    showToast("Reboot to connect");
    buildTCPMenu();
}

void SettingsScreen::toggleTCPConnection(int index) {
    if (!_config) return;
    auto& s = _config->settings();
    if (index < 0 || index >= (int)s.tcpConnections.size()) return;

    s.tcpConnections[index].autoConnect = !s.tcpConnections[index].autoConnect;
    applyAndSave();
    buildTCPMenu();
}

void SettingsScreen::removeTCPConnection(int index) {
    if (!_config) return;
    auto& s = _config->settings();
    if (index < 0 || index >= (int)s.tcpConnections.size()) return;

    s.tcpConnections.erase(s.tcpConnections.begin() + index);
    applyAndSave();
    showToast("Removed");
    buildTCPMenu();
}

// =============================================================================
// SD Card submenu
// =============================================================================

void SettingsScreen::buildSDCardMenu() {
    _list.clear();

    if (_sdStore && _sdStore->isReady()) {
        _list.addItem("Status: INSERTED");

        char buf[32];
        uint64_t total = _sdStore->totalBytes();
        uint64_t used = _sdStore->usedBytes();
        uint64_t free = total > used ? total - used : 0;

        snprintf(buf, sizeof(buf), "Total: %llu MB", total / (1024 * 1024));
        _list.addItem(buf);
        snprintf(buf, sizeof(buf), "Used: %llu MB", used / (1024 * 1024));
        _list.addItem(buf);
        snprintf(buf, sizeof(buf), "Free: %llu MB", free / (1024 * 1024));
        _list.addItem(buf);

        _list.addItem("[Initialize for RatCom]");
        _list.addItem("[Wipe All Data]");
    } else {
        _list.addItem("Status: NOT INSERTED");
    }

    _list.addItem("< Back");
}

void SettingsScreen::sdCardFormat() {
    if (!_sdStore || !_sdStore->isReady()) {
        showToast("No SD card");
        return;
    }

    if (_sdStore->formatForRatcom()) {
        showToast("SD initialized!");
    } else {
        showToast("Init failed");
    }
    buildSDCardMenu();
}

// =============================================================================
// WiFi Scanner
// =============================================================================

void SettingsScreen::startWiFiScan() {
    _list.clear();
    _list.addItem("Scanning...");

    Serial.println("[WIFI] Starting network scan...");

    // Disconnect from current network to free the radio for scanning
    WiFi.disconnect(false);  // disconnect but don't erase credentials
    delay(100);

    // Ensure WiFi is on in STA mode for scanning
    if (WiFi.getMode() == WIFI_OFF) {
        WiFi.mode(WIFI_STA);
    }

    int n = WiFi.scanNetworks(false, false);
    _scanResults.clear();

    if (n > 0) {
        for (int i = 0; i < n; i++) {
            WiFiNetwork net;
            net.ssid = WiFi.SSID(i);
            net.rssi = WiFi.RSSI(i);
            net.encType = WiFi.encryptionType(i);
            if (!net.ssid.isEmpty()) {
                _scanResults.push_back(net);
            }
        }
        // Sort by signal strength (strongest first)
        std::sort(_scanResults.begin(), _scanResults.end(),
            [](const WiFiNetwork& a, const WiFiNetwork& b) {
                return a.rssi > b.rssi;
            });
    }

    WiFi.scanDelete();
    Serial.printf("[WIFI] Found %d networks\n", (int)_scanResults.size());

    _subMenu = MENU_WIFI_SCAN;
    buildScanResultsMenu();
}

void SettingsScreen::buildScanResultsMenu() {
    _list.clear();

    if (_scanResults.empty()) {
        _list.addItem("No networks found");
    } else {
        for (auto& net : _scanResults) {
            char buf[48];
            const char* lock = (net.encType == WIFI_AUTH_OPEN) ? "" : "*";
            snprintf(buf, sizeof(buf), "%s%s (%d dBm)",
                     lock, net.ssid.c_str(), net.rssi);
            _list.addItem(buf);
        }
    }

    _list.addItem("[Rescan]");
    _list.addItem("< Back");
}

void SettingsScreen::disconnectWiFi() {
    WiFi.disconnect(false);
    showToast("Disconnected");
    buildWiFiMenu();
}

void SettingsScreen::connectWiFi() {
    auto& s = _config->settings();
    if (s.wifiSTASSID.isEmpty()) {
        showToast("No SSID set");
        return;
    }
    WiFi.disconnect(false);
    delay(100);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(s.wifiSTASSID.c_str(), s.wifiSTAPassword.c_str());
    showToast("Connecting...");
    buildWiFiMenu();
}

void SettingsScreen::selectNetwork(int index) {
    if (index < 0 || index >= (int)_scanResults.size()) return;
    if (!_config) return;

    auto& s = _config->settings();
    s.wifiSTASSID = _scanResults[index].ssid;
    Serial.printf("[WIFI] Selected: %s\n", s.wifiSTASSID.c_str());

    // Open password editor
    _subMenu = MENU_WIFI;
    // field 1 = STA password in WiFi STA mode
    startEditing(1, s.wifiSTAPassword.c_str());
}

// =============================================================================
// Display / Audio menus
// =============================================================================

void SettingsScreen::buildDisplayMenu() {
    _list.clear();
    if (!_config) return;
    auto& s = _config->settings();
    char buf[40];

    snprintf(buf, sizeof(buf), "Brightness: %d", s.brightness);
    _list.addItem(buf);
    snprintf(buf, sizeof(buf), "Dim timeout: %ds", s.screenDimTimeout);
    _list.addItem(buf);
    snprintf(buf, sizeof(buf), "Off timeout: %ds", s.screenOffTimeout);
    _list.addItem(buf);

    String name = s.displayName.isEmpty() ? "(none)" : s.displayName;
    _list.addItem(("Name: " + std::string(name.c_str())));
    _list.addItem("< Back");
}

void SettingsScreen::buildAudioMenu() {
    _list.clear();
    if (!_config) return;
    auto& s = _config->settings();
    char buf[40];

    _list.addItem(s.audioEnabled ? "Audio: ON" : "Audio: OFF");
    snprintf(buf, sizeof(buf), "Volume: %d%%", s.audioVolume);
    _list.addItem(buf);
    _list.addItem("< Back");
}

// Start editing a field — show TextInput with current value
void SettingsScreen::startEditing(int field, const std::string& currentValue) {
    _editField = field;
    _editing = true;
    _editInput.clear();
    _editInput.setText(currentValue);
    _editInput.setActive(true);
    _editInput.setMaxLength(64);
    _editInput.setSubmitCallback([this](const std::string& value) {
        commitEdit(value);
    });
}

// Apply edited value to settings
void SettingsScreen::commitEdit(const std::string& value) {
    if (!_config) return;
    auto& s = _config->settings();

    if (_subMenu == MENU_RADIO) {
        long v = atol(value.c_str());
        switch (_editField) {
            case 0:  // Frequency: 150MHz-960MHz
                if (v >= 150000000L && v <= 960000000L) s.loraFrequency = (uint32_t)v;
                break;
            case 1:  // SF: 5-12
                if (v >= 5 && v <= 12) s.loraSF = (uint8_t)v;
                break;
            case 2:  // BW: 7800-500000
                if (v >= 7800 && v <= 500000) s.loraBW = (uint32_t)v;
                break;
            case 3:  // CR: 5-8
                if (v >= 5 && v <= 8) s.loraCR = (uint8_t)v;
                break;
            case 4:  // TX Power: -9 to 22
                if (v >= -9 && v <= 22) s.loraTxPower = (int8_t)v;
                break;
        }
        applyAndSave();
        buildRadioMenu();
    } else if (_subMenu == MENU_TCP) {
        // field 99 = add new TCP connection
        if (_editField == 99 && !value.empty()) {
            addTCPConnection(value);
        }
        buildTCPMenu();
    } else if (_subMenu == MENU_WIFI) {
        // Fields: 0=SSID, 1=Password (AP or STA depending on mode)
        if (_config->settings().wifiMode == RAT_WIFI_AP) {
            switch (_editField) {
                case 0: s.wifiAPSSID = value.c_str(); break;
                case 1: s.wifiAPPassword = value.c_str(); break;
            }
        } else if (_config->settings().wifiMode == RAT_WIFI_STA) {
            switch (_editField) {
                case 0: s.wifiSTASSID = value.c_str(); break;
                case 1: s.wifiSTAPassword = value.c_str(); break;
            }
        }
        applyAndSave();
        if (_config->settings().wifiMode == RAT_WIFI_STA && _editField == 1) {
            connectWiFi();  // Live reconnect with new credentials
        } else {
            showToast("Saved!");
        }
        buildWiFiMenu();
    } else if (_subMenu == MENU_DISPLAY) {
        switch (_editField) {
            case 0: {
                int v = atoi(value.c_str());
                if (v < 1) v = 1;
                if (v > 255) v = 255;
                s.brightness = (uint8_t)v;
                if (_power) _power->setBrightness(s.brightness);
                break;
            }
            case 1: s.screenDimTimeout = (uint16_t)atoi(value.c_str()); break;
            case 2: s.screenOffTimeout = (uint16_t)atoi(value.c_str()); break;
            case 3: s.displayName = value.c_str(); break;
        }
        applyAndSave();
        buildDisplayMenu();
    } else if (_subMenu == MENU_AUDIO) {
        if (_editField == 1) {
            int v = atoi(value.c_str());
            if (v < 0) v = 0;
            if (v > 100) v = 100;
            s.audioVolume = (uint8_t)v;
            if (_audio) _audio->setVolume(s.audioVolume);
        }
        applyAndSave();
        buildAudioMenu();
    }

    _editing = false;
    _editField = -1;
}

// Get current value of a field as string for editing
std::string SettingsScreen::getCurrentValue(SubMenu menu, int field) {
    if (!_config) return "";
    auto& s = _config->settings();
    char buf[32];

    if (menu == MENU_RADIO) {
        switch (field) {
            case 0: snprintf(buf, sizeof(buf), "%lu", (unsigned long)s.loraFrequency); return buf;
            case 1: snprintf(buf, sizeof(buf), "%d", s.loraSF); return buf;
            case 2: snprintf(buf, sizeof(buf), "%lu", (unsigned long)s.loraBW); return buf;
            case 3: snprintf(buf, sizeof(buf), "%d", s.loraCR); return buf;
            case 4: snprintf(buf, sizeof(buf), "%d", s.loraTxPower); return buf;
        }
    } else if (menu == MENU_WIFI) {
        if (s.wifiMode == RAT_WIFI_AP) {
            switch (field) {
                case 0: return s.wifiAPSSID.c_str();
                case 1: return s.wifiAPPassword.c_str();
            }
        } else if (s.wifiMode == RAT_WIFI_STA) {
            switch (field) {
                case 0: return s.wifiSTASSID.c_str();
                case 1: return s.wifiSTAPassword.c_str();
            }
        }
    } else if (menu == MENU_DISPLAY) {
        switch (field) {
            case 0: snprintf(buf, sizeof(buf), "%d", s.brightness); return buf;
            case 1: snprintf(buf, sizeof(buf), "%d", s.screenDimTimeout); return buf;
            case 2: snprintf(buf, sizeof(buf), "%d", s.screenOffTimeout); return buf;
            case 3: return s.displayName.c_str();
        }
    } else if (menu == MENU_AUDIO) {
        if (field == 1) { snprintf(buf, sizeof(buf), "%d", s.audioVolume); return buf; }
    }
    return "";
}

void SettingsScreen::render(M5Canvas& canvas) {
    if (_subMenu == MENU_ABOUT) {
        renderAbout(canvas);
        return;
    }

    int y0 = Theme::CONTENT_Y;

    // Header
    const char* headers[] = {"SETTINGS", "RADIO", "WIFI", "TCP CONNECTIONS",
                             "SD CARD", "DISPLAY", "AUDIO", "ABOUT", "WIFI SCAN"};
    canvas.setTextColor(Theme::SECONDARY);
    canvas.setTextSize(Theme::FONT_SIZE);
    canvas.drawString(headers[_subMenu], 4, y0 + 2);
    canvas.drawFastHLine(0, y0 + 10, Theme::CONTENT_W, Theme::BORDER);

    if (_editing) {
        // Show field name
        canvas.setTextColor(Theme::MUTED);
        std::string label = "Edit value:";
        canvas.drawString(label.c_str(), 4, y0 + 14);

        // Show text input
        _editInput.render(canvas, 0, y0 + 28, Theme::CONTENT_W);

        // Hint
        canvas.setTextColor(Theme::MUTED);
        canvas.drawString("Enter=save  Esc=cancel", 4, y0 + 44);
    } else {
        _list.render(canvas, 0, y0 + 12, Theme::CONTENT_W, Theme::CONTENT_H - 14);
    }

    // Toast overlay (drawn on top of everything)
    if (_toastMessage && millis() < _toastUntil) {
        int tw = strlen(_toastMessage) * Theme::CHAR_W + 12;
        int th = Theme::CHAR_H + 8;
        int tx = (Theme::CONTENT_W - tw) / 2;
        int ty = Theme::CONTENT_Y + Theme::CONTENT_H - th - 4;
        canvas.fillRoundRect(tx, ty, tw, th, 3, Theme::SELECTION_BG);
        canvas.drawRoundRect(tx, ty, tw, th, 3, Theme::PRIMARY);
        canvas.setTextColor(Theme::PRIMARY);
        canvas.setCursor(tx + 6, ty + 4);
        canvas.print(_toastMessage);
    } else {
        _toastMessage = nullptr;
    }
}

void SettingsScreen::renderAbout(M5Canvas& canvas) {
    int y0 = Theme::CONTENT_Y;
    canvas.setTextColor(Theme::SECONDARY);
    canvas.setTextSize(Theme::FONT_SIZE);
    canvas.drawString("ABOUT", 4, y0 + 2);
    canvas.drawFastHLine(0, y0 + 10, Theme::CONTENT_W, Theme::BORDER);

    int y = y0 + 14;
    canvas.setTextColor(Theme::PRIMARY);
    canvas.drawString("RatCom v" RATPUTER_VERSION_STRING, 4, y); y += 10;

    canvas.setTextColor(Theme::SECONDARY);
    canvas.drawString("M5Stack Cardputer Adv", 4, y); y += 10;
    canvas.drawString("Cap LoRa-1262 (SX1262)", 4, y); y += 10;

    canvas.setTextColor(Theme::MUTED);
    canvas.drawString("ratspeak.org", 4, y); y += 12;

    canvas.setTextColor(Theme::SECONDARY);
    String idLine = "ID: " + _identityHash;
    canvas.drawString(idLine.c_str(), 4, y); y += 10;

    char heap[32];
    snprintf(heap, sizeof(heap), "Heap: %lu bytes", (unsigned long)ESP.getFreeHeap());
    canvas.drawString(heap, 4, y); y += 10;

    char uptime[32];
    snprintf(uptime, sizeof(uptime), "Up: %lus", millis() / 1000);
    canvas.drawString(uptime, 4, y); y += 12;

    canvas.setTextColor(Theme::MUTED);
    canvas.drawString("[Esc: back]", 4, y);
}

bool SettingsScreen::handleKey(const KeyEvent& event) {
    // ESC always goes back
    if (event.character == 27) {
        if (_editing) {
            _editing = false;
            _editField = -1;
            return true;
        }
        if (_subMenu == MENU_TCP) {
            _subMenu = MENU_WIFI;
            buildWiFiMenu();
            return true;
        }
        if (_subMenu == MENU_WIFI_SCAN) {
            _subMenu = MENU_WIFI;
            buildWiFiMenu();
            return true;
        }
        if (_subMenu != MENU_MAIN) {
            _subMenu = MENU_MAIN;
            buildMainMenu();
            return true;
        }
        return false;
    }

    if (_editing) {
        if (_editInput.handleKey(event)) return true;
        return false;
    }

    // Delete key in TCP submenu removes selected connection
    if (event.del && _subMenu == MENU_TCP) {
        int sel = _list.getSelectedIndex();
        int tcpIdx = sel - 1;  // item 0 is "Add", items 1..N are connections
        if (tcpIdx >= 0 && tcpIdx < (int)_config->settings().tcpConnections.size()) {
            removeTCPConnection(tcpIdx);
            return true;
        }
    }

    // Navigation: ; = up, . = down
    if (event.character == ';') {
        _list.scrollUp();
        return true;
    }
    if (event.character == '.') {
        _list.scrollDown();
        return true;
    }

    // Enter — select item
    if (event.enter) {
        int sel = _list.getSelectedIndex();

        if (_subMenu == MENU_MAIN) {
            switch (sel) {
                case 0: _subMenu = MENU_RADIO; buildRadioMenu(); break;
                case 1: _subMenu = MENU_WIFI; buildWiFiMenu(); break;
                case 2: _subMenu = MENU_SDCARD; buildSDCardMenu(); break;
                case 3: _subMenu = MENU_DISPLAY; buildDisplayMenu(); break;
                case 4: _subMenu = MENU_AUDIO; buildAudioMenu(); break;
                case 5: _subMenu = MENU_ABOUT; break;
                case 6: factoryReset(); break;
            }
            return true;
        }

        // WiFi scan results handling
        if (_subMenu == MENU_WIFI_SCAN) {
            int lastItem = _list.itemCount() - 1;
            int rescanItem = lastItem - 1;

            if (sel == lastItem) {
                // "< Back"
                _subMenu = MENU_WIFI;
                buildWiFiMenu();
            } else if (sel == rescanItem) {
                // "[Rescan]"
                startWiFiScan();
            } else if (sel < (int)_scanResults.size()) {
                selectNetwork(sel);
            }
            return true;
        }

        // "Back" is always last item
        if (sel == _list.itemCount() - 1) {
            if (_subMenu == MENU_TCP) {
                _subMenu = MENU_WIFI;
                buildWiFiMenu();
            } else {
                _subMenu = MENU_MAIN;
                buildMainMenu();
            }
            return true;
        }

        // Handle radio presets (items 1-3; item 0 is the "Active:" label)
        if (_subMenu == MENU_RADIO && sel >= 1 && sel <= 3) {
            applyRadioPreset(sel - 1);
            return true;
        }
        // Item 0 ("Active: ...") is non-interactive
        if (_subMenu == MENU_RADIO && sel == 0) {
            return true;
        }

        // Toggle audio on/off (item 0 in Audio menu)
        if (_subMenu == MENU_AUDIO && sel == 0) {
            auto& s = _config->settings();
            s.audioEnabled = !s.audioEnabled;
            if (_audio) _audio->setEnabled(s.audioEnabled);
            applyAndSave();
            buildAudioMenu();
            return true;
        }

        // Cycle WiFi mode (item 0 in WiFi menu)
        if (_subMenu == MENU_WIFI && sel == 0) {
            auto& s = _config->settings();
            s.wifiMode = (RatWiFiMode)(((int)s.wifiMode + 1) % 3);
            applyAndSave();
            showToast("Reboot to apply");
            buildWiFiMenu();
            return true;
        }

        // STA mode WiFi menu actions
        if (_subMenu == MENU_WIFI && _config->settings().wifiMode == RAT_WIFI_STA) {
            if (sel == 1) {
                // Status line (non-interactive)
                return true;
            }
            if (sel == 2) {
                // [Disconnect] or [Connect]
                if (WiFi.status() == WL_CONNECTED) {
                    disconnectWiFi();
                } else {
                    connectWiFi();
                }
                return true;
            }
            if (sel == 3) {
                // [Scan Networks]
                startWiFiScan();
                return true;
            }
            if (sel == 4) {
                // > TCP Connections
                _subMenu = MENU_TCP;
                buildTCPMenu();
                return true;
            }
        }

        // SD Card menu actions
        if (_subMenu == MENU_SDCARD) {
            if (_sdStore && _sdStore->isReady()) {
                // Item 4 = Initialize, Item 5 = Wipe All Data
                if (sel == 4) {
                    sdCardFormat();
                    return true;
                }
                if (sel == 5) {
                    if (_sdStore->wipeRatcom()) {
                        showToast("SD wiped!");
                    } else {
                        showToast("Wipe failed");
                    }
                    buildSDCardMenu();
                    return true;
                }
            }
            // Info items are non-interactive
            return true;
        }

        // TCP submenu actions
        if (_subMenu == MENU_TCP) {
            if (sel == 0) {
                // Add new connection — open text input
                startEditing(99, "");  // field 99 = TCP host:port
                return true;
            }
            int tcpIdx = sel - 1;
            if (tcpIdx >= 0 && tcpIdx < (int)_config->settings().tcpConnections.size()) {
                toggleTCPConnection(tcpIdx);
                return true;
            }
            return true;  // Back handled above
        }

        // Edit the selected field (offset by 4 for radio header+presets, 1 for WiFi mode)
        int fieldIdx = sel;
        if (_subMenu == MENU_RADIO) fieldIdx -= 4;
        if (_subMenu == MENU_WIFI) fieldIdx -= 1;
        std::string currentVal = getCurrentValue(_subMenu, fieldIdx);
        startEditing(fieldIdx, currentVal);
        return true;
    }

    return false;
}

void SettingsScreen::showToast(const char* msg, unsigned long durationMs) {
    _toastMessage = msg;
    _toastUntil = millis() + durationMs;
}

void SettingsScreen::applyAndSave() {
    if (!_config || !_flash) return;

    // Save to both SD + flash when SD is available
    if (_sdStore && _sdStore->isReady()) {
        _config->save(*_sdStore, *_flash);
    } else {
        _config->save(*_flash);
    }

    auto& s = _config->settings();

    // Apply radio settings to hardware
    if (_radio) {
        _radio->setFrequency(s.loraFrequency);
        _radio->setSpreadingFactor(s.loraSF);
        _radio->setSignalBandwidth(s.loraBW);
        _radio->setCodingRate4(s.loraCR);
        _radio->setTxPower(s.loraTxPower);
        _radio->receive();  // Re-enter RX after reconfiguration
    }

    // Apply power settings
    if (_power) {
        _power->setDimTimeout(s.screenDimTimeout);
        _power->setOffTimeout(s.screenOffTimeout);
        _power->setBrightness(s.brightness);
    }

    // Apply audio settings
    if (_audio) {
        _audio->setEnabled(s.audioEnabled);
        _audio->setVolume(s.audioVolume);
    }

    Serial.println("[SETTINGS] Saved and applied");
    if (!_toastMessage) showToast("Saved!");
}

void SettingsScreen::applyRadioPreset(int preset) {
    if (!_config) return;
    auto& s = _config->settings();

    switch (preset) {
        case 0:  // Balanced — SF9, BW250k, CR5, TX14
            s.loraSF = 9;
            s.loraBW = 250000;
            s.loraCR = 5;
            s.loraTxPower = 14;
            break;
        case 1:  // Long Range — SF12, BW125k, CR8, TX17
            s.loraSF = 12;
            s.loraBW = 125000;
            s.loraCR = 8;
            s.loraTxPower = 17;
            break;
        case 2:  // Fast — SF7, BW500k, CR5, TX10
            s.loraSF = 7;
            s.loraBW = 500000;
            s.loraCR = 5;
            s.loraTxPower = 10;
            break;
    }

    applyAndSave();
    buildRadioMenu();
    if (_rns) {
        // Encode display name as msgpack app_data for announce
        const String& name = _config ? _config->settings().displayName : String();
        if (!name.isEmpty()) {
            size_t len = name.length();
            if (len > 31) len = 31;
            uint8_t buf[2 + 31];
            buf[0] = 0x91;
            buf[1] = 0xA0 | (uint8_t)len;
            memcpy(buf + 2, name.c_str(), len);
            _rns->announce(RNS::Bytes(buf, 2 + len));
        } else {
            _rns->announce();
        }
    }
    showToast("Preset applied + announced");
    Serial.printf("[SETTINGS] Radio preset %d applied\n", preset);
}

void SettingsScreen::factoryReset() {
    Serial.println("[SETTINGS] Factory reset!");
    if (_flash) {
        _flash->format();
        // Also wipe SD config if available
        if (_sdStore && _sdStore->isReady()) {
            _sdStore->remove(SD_PATH_USER_CONFIG);
            Serial.println("[SETTINGS] SD config removed");
        }
        ESP.restart();
    }
}
