// ble.h

#ifndef BLE_H
#define BLE_H

// =============================
// Header Files (Your Toolbox)
// =============================

    /* --- General --- */
    #include "esp_log.h"                    // Serial console output
    #include "esp_err.h"                    // ESP Error codes

    // =============================
    // BLE-Specific Includes (Minimal for Prototypes)
    // =============================

        // -----------------------------
        // Controller Layer — Hardware Initialization
        // -----------------------------
        #include "nvs_flash.h"
        #include "esp_bt.h"            // Core Bluetooth definitions (esp_bt_mode_t, esp_bt_controller_enable)
        #include "esp_bt_main.h"       // Controller lifecycle: init/enable/deinit/disabling APIs
        #include "esp_bt_defs.h"       // Shared Bluetooth types & enums (esp_bt_controller_status_t, esp_bt_mode_t)
        #include "esp_bt_device.h"     // For local device name, address configuration, and identity handling

        // -----------------------------
        // Stack Layer — Bluedroid (Software Host)
        // -----------------------------
        #include "esp_gatt_common_api.h"   // GATT common structures & definitions (used by esp_ble_gatts_* APIs)
        #include "esp_gap_ble_api.h"       // GAP callbacks, advertising params, and BLE connection event handling
        #include "esp_gatts_api.h"         // GATT server callbacks, characteristic creation, and app registration


// =============================
// Application Log Tag
// =============================
#define BLE_TAG "BLE_APP"


// ==============================
// GATT Service and Characteristic Definition: BLE UUIDs
// ==============================
extern const uint16_t SERVICE_UUID;              // "Eye Blink count" service
extern const uint16_t CHAR_UUID_BLINK_COUNT;     // Service characteristic 1
extern const uint16_t CHAR_UUID_ATTENTION_LEVEL; // Service characteristic 2


// =============================
// Global Handles (BLE-Specific; for cross-module use)
// =============================
extern uint16_t service_handle;  // Service handle
extern esp_gatt_srvc_id_t service_id;  // Service ID
extern esp_gatt_if_t gatts_if_global;  // Global GATT interface
extern uint8_t service_uuid[2];  // Service UUID array (little-endian 0x180A)
extern uint16_t conn_id;  // Active connection handle
extern uint16_t blink_handle;    // Blink char attr handle
extern uint16_t attention_handle; // Attention char attr handle


// =============================
// Event Handler Prototypes
// =============================
// GAP event handler (registered callback)
void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
// GATT event handler (registered callback)
void gatts_event_handler(esp_gatts_cb_event_t event,
                         esp_gatt_if_t gatts_if,
                         esp_ble_gatts_cb_param_t *param);


// =============================
// BLE Initialization
// =============================
/**
 * @brief Initializes the entire BLE stack, GATT server, and starts advertising.
 *
 * This function encapsulates all the setup steps for BLE, including:
 * 1. Initializing the BT Controller and Bluedroid Stack.
 * 2. Registering GATT and GAP event handlers.
 * 3. Creating the custom GATT service and its characteristics.
 * 4. Configuring and starting BLE advertising.
 */
void init_ble(void);    // (call from app_main)


// BLE notifications task (create via xTaskCreate)
void ble_notifications(void *arg);

#endif // BLE_H