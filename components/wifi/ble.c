// =============================
// Header Files (Your Toolbox)
// =============================

    /* --- General --- */
    // #include "freertos/FreeRTOS.h"
    // #include "freertos/task.h"
    // #include "esp_log.h"
    // #include "esp_err.h"


    /* --- BLE --- */

    // #include "esp_bt.h"
    // #include "esp_bt_main.h"
    // #include "esp_gap_ble_api.h"
    // #include "esp_gatts_api.h"
    // #include "esp_bt_defs.h"
    // #include "esp_bt_device.h"
    // #include "esp_gatt_common_api.h"

    #include "ble.h"                // Our header
    #include "adc.h"                // For shared adc_buffer/buffer_index access


// ==============================
// GATT Service and Characteristic Definition (Defined Here)
// ==============================
const uint16_t SERVICE_UUID              = 0x180A;  // "Eye Blink count" service
const uint16_t CHAR_UUID_BLINK_COUNT     = 0x2A56;  // Service characteristic 1
const uint16_t CHAR_UUID_ATTENTION_LEVEL = 0x2A57;  // Service characteristic 2

// =============================
// Module-Private Global Handles
// =============================
uint16_t service_handle = 0;
esp_gatt_srvc_id_t service_id;  // Definition for extern in ble.h
esp_gatt_if_t gatts_if_global = 0;
uint8_t service_uuid[2] = {0x0A, 0x18}; // Little-endian for 0x180A
uint16_t conn_id = 0xFFFF;
uint16_t blink_handle = 0;      // Blink char attr handle (set in ADD_CHAR_EVT)
uint16_t attention_handle = 0;  // Attention char attr handle (set in ADD_CHAR_EVT)


// Global adv params for restart on disconnect
esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x40,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};


// ==============================
// GAP (Generic Access Profile) Handler for Notifications
// ==============================
void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param){
    switch (event)
    {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            // No start here—handled in GATT START_EVT for async safety (avoids double-start)
            break;


        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS)
                ESP_LOGI(BLE_TAG, "Advertising started successfully.");
            else
                ESP_LOGE(BLE_TAG, "Failed to start advertising.");
            break;

        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            ESP_LOGI(BLE_TAG, "Advertising stopped.");
            break;

        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
            ESP_LOGI(BLE_TAG, "Connection parameters updated");
            break;

        default:
            ESP_LOGI(BLE_TAG, "Unhandled GAP event: %d", event);
            break;
    }
}


// ==============================
// GATT (Generic Attribute Profile) Handler
// ==============================
static int add_char_idx = 0;  // Temp: Track which char (0=blink, 1=attention)

void gatts_event_handler(esp_gatts_cb_event_t event,
                         esp_gatt_if_t gatts_if,
                         esp_ble_gatts_cb_param_t *param)
{
    
    gatts_if_global = gatts_if;

    switch (event)
    {

        // ------------------------------------------
        // 1. Register Event
        // ------------------------------------------
        case ESP_GATTS_REG_EVT:
            ESP_LOGI(BLE_TAG, "[GATT EVENT] GATT server registered.");

            // Define our custom service
            service_id.is_primary = true;
            service_id.id.inst_id = 0x00;
            service_id.id.uuid.len = ESP_UUID_LEN_16;
            service_id.id.uuid.uuid.uuid16 = SERVICE_UUID;

            // Create the service with up to 4 handles (for demo)
            esp_ble_gatts_create_service(gatts_if, &service_id, 8);
            break;

        // ------------------------------------------
        // 2. Service Created
        // ------------------------------------------
        case ESP_GATTS_CREATE_EVT:
            ESP_LOGI(BLE_TAG, "[GATT EVENT] Service created.");
            service_handle = param->create.service_handle;
            add_char_idx = 0;  // Reset tracker

            // Add first characteristic (Blink Count) - NOTIFY enabled
            {
                esp_bt_uuid_t char_uuid;
                char_uuid.len = ESP_UUID_LEN_16;
                char_uuid.uuid.uuid16 = CHAR_UUID_BLINK_COUNT;

                esp_gatt_char_prop_t property = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
                esp_err_t ret = esp_ble_gatts_add_char(service_handle, &char_uuid,
                                                       ESP_GATT_PERM_READ, property, NULL, NULL);
                if (ret != ESP_OK)
                    ESP_LOGE(BLE_TAG, "Failed to add char 0");
            }
            break;

        // ------------------------------------------
        // 3. Characteristic Added
        // ------------------------------------------
        case ESP_GATTS_ADD_CHAR_EVT:
            if (param->add_char.status == ESP_GATT_OK)
            {
                if (add_char_idx == 0)
                {
                    blink_handle = param->add_char.attr_handle;
                    ESP_LOGI(BLE_TAG, "Blink char handle: 0x%04x", blink_handle);

                    // Add second characteristic (Attention Level)
                    esp_bt_uuid_t char_uuid = {
                        .len = ESP_UUID_LEN_16,
                        .uuid.uuid16 = CHAR_UUID_ATTENTION_LEVEL
                    };
                    esp_gatt_char_prop_t property = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE;
                    esp_err_t ret = esp_ble_gatts_add_char(service_handle, &char_uuid,
                                                           ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                                           property, NULL, NULL);
                    if (ret != ESP_OK)
                        ESP_LOGE(BLE_TAG, "Failed to add char 1");
                }
                else
                {
                    attention_handle = param->add_char.attr_handle;
                    ESP_LOGI(BLE_TAG, "Attention char handle: 0x%04x", attention_handle);

                    // All characteristics added → start service
                    esp_ble_gatts_start_service(service_handle);
                }
                add_char_idx++;
            }
            else
            {
                ESP_LOGE(BLE_TAG, "Failed to add char %d", add_char_idx);
            }
            break;

        // ------------------------------------------
        // 4. Service Started
        // ------------------------------------------
        case ESP_GATTS_START_EVT:
            if (param->start.status == ESP_GATT_OK) {
                ESP_LOGI(BLE_TAG, "Service started. Now starting advertising.");
                // Define advertising data and params here for safety
                esp_ble_adv_data_t adv_data = {
                    .set_scan_rsp = false,
                    .include_name = true,
                    .include_txpower = true,
                    .min_interval = 0x20,
                    .max_interval = 0x40,
                    .appearance = 0x00,
                    .manufacturer_len = 0,
                    .p_manufacturer_data = NULL,
                    .service_data_len = 0,
                    .p_service_data = NULL,
                    .service_uuid_len = sizeof(service_uuid),
                    .p_service_uuid = service_uuid,
                    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
                };
                esp_ble_gap_config_adv_data(&adv_data);
                esp_ble_gap_start_advertising(&adv_params); // Global (removed local def)
            } else {
                ESP_LOGE(BLE_TAG, "Failed to start service.");
            }
            break;


        case ESP_GATTS_CONNECT_EVT:
            conn_id = param->connect.conn_id;
            ESP_LOGI(BLE_TAG, "Connected! Conn ID: %d", conn_id);
            break;
          
            
        case ESP_GATTS_DISCONNECT_EVT:
            conn_id = 0xFFFF;
            ESP_LOGI(BLE_TAG, "Disconnected.");
            // Restart adv with global params
            esp_ble_gap_start_advertising(&adv_params);  // Restart adv (add adv_params global if needed)
            break;

        default:
            ESP_LOGI(BLE_TAG, "[GATT EVENT] Event %d", event);
            break;
    }
}



// =============================
// BLE Initialization: Controller + Stack (software) + GATT Service (app) 
// =============================
//
// Controller Initialization → Controller Enable → Stack Init → Stack Enable → Application (GATT, Advertising, Notifications)
//
// Function to initializes the Bluetooth Controller (hardware) and the BLE Stack (software).
// Prepares the ESP32 to act as a BLE peripheral with GATT services.

// This is a global setup: once initialized, all tasks can use BLE APIs.
void init_ble(void){
    
    esp_err_t ret;

    // =============================
    // 0. NVS Flash Init (Required for BLE)
    // =============================

    // Without this step, Bluedroid will log "NVS not initialized" errors and run unconfigured.
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(BLE_TAG, "NVS Flash initialized successfully.");


    // ==============================
    // 1. BLE Controller Initialization (Hardware Layer)
    // ==============================

    // STEP 1A: Allocate and configure the controller memory and settings
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    // Note: At this point, this is just the blueprint or plan for how the hardware should be set up.

    // STEP 1B: Initialize in the Heap the controller hardware specification based on the previous blueprint.
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(BLE_TAG, "Bluetooth controller initialization failed! Error code: %d", ret);
        return;
    }
    ESP_LOGI(BLE_TAG, "Bluetooth controller initialized successfully.");
    // Note: At this point, the hardware is fully assembled but not yet powered on.

    // STEP 1C: Enable BLE mode (start the controller hardware) 
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret != ESP_OK) {
        ESP_LOGE(BLE_TAG, "Failed to enable BLE controller! Error code: %d", ret);
        return;
    }
    ESP_LOGI(BLE_TAG, "Bluetooth controller BLE mode enabled.");


    // ==============================
    // 2. BLE Stack Initialization (Software Layer)
    // ==============================

    // STEP 2A: Initialize the Bluedroid Stack (Load Bluedroid stack structures into memory)
    ret = esp_bluedroid_init();
    if (ret != ESP_OK) {
        ESP_LOGE(BLE_TAG, "Bluedroid stack initialization failed! Error code: %d", ret);
        return;
    }
    ESP_LOGI(BLE_TAG, "Bluedroid stack initialized successfully.");
    // Note: At this point, the Bluetooth software logic exists in memory,
    // but they are *idle*. The stack is not yet running or communicating with the controller. 


    // STEP 2B: Enable the Bluedroid stack (Activate software execution) and link it to controller (HCI)
    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
        ESP_LOGE(BLE_TAG, "Failed to enable Bluedroid stack! Error code: %d", ret);
        return;
    }
    ESP_LOGI(BLE_TAG, "Bluedroid stack enabled.");


    // ==============================
    // 3. GATT Server Registration (Application Layer)
    // ==============================

    // STEP 3A: Register event handlers ( the GATT server callback functions)
    esp_ble_gap_register_callback(gap_event_handler);
    esp_ble_gatts_register_callback(gatts_event_handler);

    // STEP 3B: Register the GATT application (ID 0)
    ret = esp_ble_gatts_app_register(0);
    if (ret != ESP_OK) {
        ESP_LOGE(BLE_TAG, "Failed to register GATT application! Error code: %d", ret);
        return;
    }
    ESP_LOGI(BLE_TAG, "GATT server registered successfully.");

    // Note:
    // GATT setup now proceeds asynchronously:
    //  1. REG_EVT → create service
    //  2. CREATE_EVT → add chars
    //  3. ADD_CHAR_EVT → start service
    //  4. START_EVT → start advertising 
    // The reason why of the two Handler functions defined above.

}

// =============================
// FreeRTOS Task: BLE Notifications (Stub - Expand for actual notify logic)
// =============================
void ble_notifications(void *arg){

    static uint32_t last_blink = 0;
    static uint8_t last_attention = 0;
    uint8_t blink_data[4];  // uint32_t little-endian
    uint8_t attn_data[1];   // uint8_t

    // TODO: Implement logic to read filtered data (e.g., blink count) and send GATT notifications
    // Example: esp_ble_gatts_send_indicate(gatts_if_global, ...);
    while (1) {

        if (conn_id != 0xFFFF && blink_handle && attention_handle) {  // Only if Connected + handles ready
            
            // Check blink change
            if (blink_count > last_blink) {
                // Pack little-endian
                blink_data[0] = (uint8_t)(blink_count & 0xFF);
                blink_data[1] = (uint8_t)((blink_count >> 8) & 0xFF);
                blink_data[2] = (uint8_t)((blink_count >> 16) & 0xFF);
                blink_data[3] = (uint8_t)((blink_count >> 24) & 0xFF);
                
                esp_ble_gatts_send_indicate(gatts_if_global, conn_id, CHAR_UUID_BLINK_COUNT /* handle from add_char evt */, 
                                            sizeof(blink_data), blink_data, false);
                last_blink = blink_count;
                ESP_LOGI(BLE_TAG, "Notified blink: %lu", blink_count);


            }
            
            // Check attention change (every update, as it's frequent)
            if (attention_level != last_attention) {
                attn_data[0] = attention_level;
                esp_ble_gatts_send_indicate(gatts_if_global, conn_id, CHAR_UUID_ATTENTION_LEVEL /* handle */,
                                            sizeof(attn_data), attn_data, false);
                last_attention = attention_level;
                ESP_LOGI(BLE_TAG, "Notified attention: %u", attention_level);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(250));  // 1s delay

    }
}





