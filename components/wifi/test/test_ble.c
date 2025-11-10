// =============================
// Header Files (Your Toolbox)
// =============================

// #include "unity.h"
// #include "esp_bt.h"
// #include "esp_gatts_api.h"
// #include "esp_gap_ble_api.h"
    #include "nvs_flash.h"

    /* --- BLE --- */

    #include "ble.h"                // Our header

// =============================
// Global Test Mock Event Capture Utilities for Reset Test State
// =============================
static bool reg_evt = false;
static bool create_evt = false;
static bool start_evt = false;
static bool event_sequence_ok = false;

// =============================
// Test Helper Function: Rest test state and register mock callbacks
// =============================
static void ble_test_event_log_reset(void) {
    reg_evt = create_evt = start_evt = event_sequence_ok = false;
}

// static void gatts_test_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
//     static int last_event = -1;
//     switch (event) {
//         case ESP_GATTS_REG_EVT:   reg_evt = true; break;
//         case ESP_GATTS_CREATE_EVT:create_evt = true; break;
//         case ESP_GATTS_START_EVT: start_evt = true; break;
//         default: break;
//     }

//     // Validate event order
//     if (last_event == ESP_GATTS_REG_EVT && event == ESP_GATTS_CREATE_EVT)
//         event_sequence_ok = true;
//     if (last_event == ESP_GATTS_CREATE_EVT && event == ESP_GATTS_START_EVT)
//         event_sequence_ok = true;

//     last_event = event;
// }

// =============================
// Test Helper Function: Helper function wrapping BLE stack initialization
// =============================
static esp_err_t ble_core_init(void) {

    esp_err_t ret;

    // ============================================================
    // STEP 1: Initialize the Bluetooth Controller (Hardware Layer)
    // ============================================================
    // The controller is the low-level firmware that talks directly to the radio hardware (PHY).
    // It handles timing, packet transmission, frequency hopping, etc.
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));

    // Enable the BLE controller mode specifically (no Classic BT).
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));


    // ============================================================
    // STEP 2: Initialize & Enable the Bluedroid Host Stack (Software Layer)
    // ============================================================
    // The Bluedroid host stack manages the high-level BLE protocols:
    // GAP (advertising, scanning), GATT (services, characteristics), ATT, SMP, etc.
    ret = esp_bluedroid_init();
    if (ret != ESP_OK) return ret;

    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) return ret;


    // ============================================================
    // STEP 3: Register Event Handlers (Callbacks)
    // ============================================================
    // The GATT Server (gatts) and GAP layers use callbacks to communicate asynchronously.
    // Here, we register our own custom event handler for testing.
    // “When something happens (like connection, read/write, etc.), call *this function*.”
    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(gatts_test_event_handler));

    // GAP is optional for this low-level test (no advertising yet), so we pass NULL.
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(NULL));



    // ============================================================
    // STEP 6: Register a Dummy GATT Application
    // ============================================================
    // The BLE stack identifies each GATT service application by an ID (app_id).
    // Here we register a dummy one (0x55) just to confirm that initialization is functional.
    return esp_ble_gatts_app_register(0x55);


}

void test_ble_core_initialization(void) {

    esp_err_t ret;

    /* --- Case 1: Baseline Initialization --- */ 

        // --- Arrange —
        ble_test_event_log_reset();

        // --- Act ---
        ret = ble_core_init();

        // --- Assert ---
        TEST_ASSERT_EQUAL(ESP_OK, ret);
        TEST_ASSERT_TRUE(reg_evt);
        TEST_ASSERT_TRUE(create_evt);
        TEST_ASSERT_TRUE(start_evt);
        TEST_ASSERT_TRUE(event_sequence_ok);

    /* --- --- Case 2: Double Initialization Attempt --- */
    ret = ble_core_init();
    TEST_ASSERT_TRUE((ret == ESP_OK) || (ret == ESP_ERR_INVALID_STATE));

    // --- Case 3: Initialization Without NVS ---
    nvs_flash_deinit();
    ble_test_event_log_reset();
    ret = esp_bluedroid_init();
    TEST_ASSERT_TRUE((ret != ESP_OK));

    // --- Case 4: Reinitialization After Deinit ---
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    ble_test_event_log_reset();
    ret = ble_core_init();

    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_TRUE(event_sequence_ok);
}
