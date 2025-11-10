// =============================
// Header Files (Your Toolbox)
// =============================

    /* --- General --- */
    #include "esp_log.h"                    // Serial console output
    #include "esp_err.h"                    // ESP Error codes  
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"

    /* --- ADC --- */
    #include "adc.h"

    /* --- BLE --- */
    #include "ble.h"

// =============================
// Main Application Entry Point
// =============================
void app_main(void)
{   

    // --- Start logging ---
    // ESP-IDF functon to print to serial console
    ESP_LOGI(ADC_TAG, "Starting ADC Initialization and Calibration...");

    // --- Initialize ADC ---
    // ESP-IDF function to Configures ADC Unit 1, Channel 6 (GPIO34) and sets up calibration
    // Returns a handle that uniquely identifies this ADC configuration.
    adc_handle = init_adc();
    if (!adc_handle) {
        ESP_LOGE(ADC_TAG, "ADC initialization failed. Exiting.");
        return;
    }

    // --- Create Mutex for ADC Buffer ---
    adc_mutex = xSemaphoreCreateMutex();
    if (adc_mutex == NULL) {
        ESP_LOGE(ADC_TAG, "Failed to create ADC mutex!");
        return;
    }

    // --- Initialize BLE ---
    // ESP-IDF function to configure the built-in Bluetooth controller, enables the Bluedroid stack,
    // and registers the GATT server. 
    // Note:
    // Unlike the ADC (which maps to a specific hardware channel/pin and provides a task-level handle),
    // the Bluetooth controller is a single shared hardware block within the SoC.
    // Once initialized, it operates system-wide — all tasks access BLE functions
    // through this global controller and its protocol stack (the BLE stack).
    init_ble();
    ESP_LOGI(BLE_TAG, "BLE initialized successfully!");

    BaseType_t task_status;

    // --- Task for ADC Sampling ---
    task_status = xTaskCreate(adc_sampling, "ADC Sampling", 2048, NULL, 5, NULL);
    if (task_status == pdPASS) {
        ESP_LOGI(ADC_TAG, "ADC Sampling task created successfully!");
    } else {
        ESP_LOGE(ADC_TAG, "Failed to create ADC sampling task!");
    }

    // --- Task for ADC Filtering ---
    task_status = xTaskCreate(adc_filtering, "ADC Filtering", 2048, NULL, 4, NULL);
    if (task_status == pdPASS) {
        ESP_LOGI(ADC_TAG, "ADC Filtering task created successfully!");
    } else {
        ESP_LOGE(ADC_TAG, "Failed to create ADC task!");
    }

    // --- Task for BLE Advertising & Notifications ---
    task_status = xTaskCreate(ble_notifications, "BLE Notifications", 4096, NULL, 3, NULL);
    if (task_status != pdPASS){
        ESP_LOGE(BLE_TAG, "Failed to create BLE task!");
    }

}







