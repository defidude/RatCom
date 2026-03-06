#pragma once

// =============================================================================
// RatCom — Compile-Time Configuration
// =============================================================================

#define RATPUTER_VERSION_MAJOR  1
#define RATPUTER_VERSION_MINOR  2
#define RATPUTER_VERSION_PATCH  2
#define RATPUTER_VERSION_STRING "1.2.2"

// --- Feature Flags ---
#define HAS_DISPLAY     true
#define HAS_KEYBOARD    true
#define HAS_LORA        true
#define HAS_WIFI        true
#define HAS_BLE         true
#define HAS_SD          true
#define HAS_AUDIO       true

// --- WiFi Defaults ---
#define WIFI_AP_PORT        4242
#define WIFI_AP_PASSWORD    "ratspeak"

// --- Storage Paths ---
#define PATH_IDENTITY       "/identity/identity.key"
#define PATH_IDENTITY_BAK   "/identity/identity.key.bak"
#define PATH_PATHS          "/transport/paths.msgpack"
#define PATH_USER_CONFIG    "/config/user.json"
#define PATH_CONTACTS       "/contacts/"
#define PATH_MESSAGES       "/messages/"

// --- SD Card Paths ---
#define SD_PATH_CONFIG_DIR   "/ratcom/config"
#define SD_PATH_USER_CONFIG  "/ratcom/config/user.json"
#define SD_PATH_MESSAGES     "/ratcom/messages/"
#define SD_PATH_CONTACTS     "/ratcom/contacts/"
#define SD_PATH_IDENTITY     "/ratcom/identity/identity.key"

// --- TCP Client ---
#define MAX_TCP_CONNECTIONS         4
#define TCP_DEFAULT_PORT            4242
#define TCP_RECONNECT_INTERVAL_MS   10000
#define TCP_CONNECT_TIMEOUT_MS      5000

// --- Limits ---
#define RATPUTER_MAX_NODES           50
#define RATPUTER_MAX_MESSAGES_PER_CONV 100
#define FLASH_MSG_CACHE_LIMIT        20   // Keep only 20 most recent per conv in flash
#define RATPUTER_MAX_OUTQUEUE        20   // Cap LXMF outgoing queue
#define PATH_PERSIST_INTERVAL_MS 60000

// --- Power Management ---
#define SCREEN_DIM_TIMEOUT_MS   30000
#define SCREEN_OFF_TIMEOUT_MS   60000
#define SCREEN_DIM_BRIGHTNESS   64     // 25% of 255

// --- Serial Debug ---
#define SERIAL_BAUD  115200
