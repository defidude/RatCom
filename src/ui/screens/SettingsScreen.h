#pragma once

#include "ui/Screen.h"
#include "ui/widgets/ScrollList.h"
#include "ui/widgets/TextInput.h"
#include "config/UserConfig.h"
#include "storage/FlashStore.h"
#include "storage/SDStore.h"
#include "radio/SX1262.h"
#include <WiFi.h>
#include "audio/AudioNotify.h"
#include "power/PowerManager.h"
#include "transport/WiFiInterface.h"
#include "transport/TCPClientInterface.h"
#include "reticulum/ReticulumManager.h"
#include <vector>

class SettingsScreen : public Screen {
public:
    void render(M5Canvas& canvas) override;
    bool handleKey(const KeyEvent& event) override;
    const char* title() const override { return "Settings"; }
    void onEnter() override;

    void setUserConfig(UserConfig* cfg) { _config = cfg; }
    void setFlashStore(FlashStore* flash) { _flash = flash; }
    void setSDStore(SDStore* sd) { _sdStore = sd; }
    void setRadio(SX1262* radio) { _radio = radio; }
    void setAudio(AudioNotify* audio) { _audio = audio; }
    void setPower(PowerManager* power) { _power = power; }
    void setWiFi(WiFiInterface* wifi) { _wifi = wifi; }
    void setTCPClients(std::vector<TCPClientInterface*>* clients) { _tcpClients = clients; }
    void setRNS(ReticulumManager* rns) { _rns = rns; }
    void setIdentityHash(const String& hash) { _identityHash = hash; }

    // Callback for back navigation
    using BackCallback = std::function<void()>;
    void setBackCallback(BackCallback cb) { _backCb = cb; }

private:
    enum SubMenu { MENU_MAIN, MENU_RADIO, MENU_WIFI, MENU_TCP, MENU_SDCARD,
                   MENU_DISPLAY, MENU_AUDIO, MENU_ABOUT, MENU_WIFI_SCAN };

    void buildMainMenu();
    void buildRadioMenu();
    void buildWiFiMenu();
    void buildTCPMenu();
    void buildSDCardMenu();
    void sdCardFormat();
    void buildDisplayMenu();
    void buildAudioMenu();
    void renderAbout(M5Canvas& canvas);

    // WiFi scanner
    void startWiFiScan();
    void buildScanResultsMenu();
    void selectNetwork(int index);
    void disconnectWiFi();
    void connectWiFi();

    void addTCPConnection(const std::string& hostPort);
    void toggleTCPConnection(int index);
    void removeTCPConnection(int index);

    void startEditing(int field, const std::string& currentValue);
    void commitEdit(const std::string& value);
    std::string getCurrentValue(SubMenu menu, int field);
    void applyAndSave();
    void applyRadioPreset(int preset);  // 0=Balanced, 1=Long Range, 2=Fast
    void factoryReset();
    void showToast(const char* msg, unsigned long durationMs = 1500);

    UserConfig* _config = nullptr;
    FlashStore* _flash = nullptr;
    SDStore* _sdStore = nullptr;
    SX1262* _radio = nullptr;
    AudioNotify* _audio = nullptr;
    PowerManager* _power = nullptr;
    WiFiInterface* _wifi = nullptr;
    std::vector<TCPClientInterface*>* _tcpClients = nullptr;
    ReticulumManager* _rns = nullptr;
    String _identityHash;

    SubMenu _subMenu = MENU_MAIN;
    ScrollList _list;
    bool _editing = false;
    TextInput _editInput;
    int _editField = -1;
    BackCallback _backCb;

    // WiFi scan state
    struct WiFiNetwork { String ssid; int32_t rssi; uint8_t encType; };
    std::vector<WiFiNetwork> _scanResults;

    // Toast overlay
    unsigned long _toastUntil = 0;
    const char* _toastMessage = nullptr;
};
