// =============================
// Header Files (Your Toolbox)
// =============================

    /* --- General --- */
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
    #include "esp_log.h"
    #include "esp_err.h"

    /* --- ADC --- */
    // #include "esp_adc/adc_oneshot.h"    // For ADC HW interation
    // #include "esp_adc/adc_cali.h"       // For voltage calibration
    #include "adc.h"
    #include <math.h>  // For Goertzel (sin/cos)


// =============================
// ADC Globals Definition (Here, for Module Ownership)
// =============================
adc_oneshot_unit_handle_t adc_handle = NULL;  // ADC driver handle
adc_cali_handle_t adc_cali_handle = NULL;     // ADC Calibration handle
int16_t adc_buffer[BUFFER_SIZE];  // Circular buffer for ADC samples
volatile size_t buffer_index = 0;   // producer (adc_sampling) writes then increments
volatile uint32_t blink_count = 0;
volatile uint8_t attention_level = 0;

// Mutex to protect shared buffer access
SemaphoreHandle_t adc_mutex = NULL;

// =============================
// IIR Bandpass Globals (2nd-order Butterworth, 0.5-30Hz @100Hz sample)
// =============================
float bp_a[3] = {1.0f, -0.2162f, 0.2174f};   // Denominator
float bp_b[3] = {0.3913f, 0.0f, -0.3913f};   // Numerator
float bp_x[2] = {0};  // Input history
float bp_y[2] = {0};  // Output history


// =============================
// ADC Unit Initialization + Channel Configuration + Calibration
// =============================
// Function to initialize the ADC unit, configure the channel, and set up calibration.
// Returns a handle to the initialized ADC unit.
adc_oneshot_unit_handle_t init_adc(void){

    esp_err_t ret;

    // ==============================
    // 1. ADC Unit Configuration
    // ==============================

    // STEP 1A : Create a Handle (Done - Alrady defined globally)
    // A handle is like a "pointer" or reference to a software object that represents
    // the ADC hardware inside ESP-IDF. This handle will be used for all future ADC calls.
    // (At this point, adc_handle is just a NULL pointer.)
    // static adc_oneshot_unit_handle_t adc_handle;    // Reference to ADC driver

    // STEP 1B : Define the ADC Unit configuration structure
    // This structure describes global settings for the ADC peripheral.
    // - unit_id: Which ADC hardware block (ADC1 or ADC2)
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT,   // Use ADC1 block
    };
    // Nothing is initialized yet. This is just the **desired configuration**.

    // STEP 1C : Initialize ADC Unit using the ESP-IDF API
    // Function: adc_oneshot_new_unit(init_config, &adc_handle)
    // What it does:
    // 1. Allocates memory for the ADC driver object
    // 2. Programs ADC hardware registers according to init_config
    // 3. Updates adc_handle to point to this driver object
    ret = adc_oneshot_new_unit(&init_config, &adc_handle);
    if (ret == ESP_OK) {
        ESP_LOGI(ADC_TAG, "ADC Unit initialized successfully!");
    } else {
        ESP_LOGE(ADC_TAG, "Failed to initialize ADC unit! Error code: %d", ret);
        return NULL; // Stop if initialization failed
    }
    // Now adc_handle points to a fully initialized ADC driver object
    // but the ADC channel/pin and input scaling are not set yet.

    // ==============================
    // 2. ADC Channel Configuration
    // ==============================

    // STEP 2B : Define the Channel Configuration structure
    // This is a separate configuration structure that describes how to read from a specific ADC channel (pin).
    // - bitwidth: Resolution of conversion (default 12-bit)
    // - attenuation: How much input voltage the ADC can measure (~3.3V for DB_11)
    adc_oneshot_chan_cfg_t chan_config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,  // Default 12-bit resolution
        .atten = ADC_ATTEN_DB_12           // ~3.3V full-scale voltage range
    };

    // STEP 2C: [ BLANK]
    ret = adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &chan_config);
    if (ret == ESP_OK) {
        ESP_LOGI(ADC_TAG, "ADC channel configured successfully!");
    } else {
        ESP_LOGE(ADC_TAG, "Failed to configure ADC channel! Error code: %d", ret);
        return NULL;
    }

    // ==============================
    // 3. ADC Calibration Initialization
    // ==============================

    // Calibration is optional but recommended for accurate voltage readings.
    // On ESP32, raw ADC values may vary due to temperature, voltage supply, and manufacturing.
    // The calibration API converts raw readings to mV.

    // Step 3A: Define the Calibration configuration structure
    // This structure describes how the calibration should be performed.
    // - unit_id: Which ADC unit (must match the one used before)
    // - atten: Must match the attenuation used in channel config
    // - bitwidth: Must match the bitwidth used in channel config
    adc_cali_line_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT,               // Same ADC unit as before
        .atten = ADC_ATTEN_DB_12,          // Same attenuation as channel config
        .bitwidth = ADC_BITWIDTH_DEFAULT   // Same bitwidth as channel config
    };

    // Step 3B: Initialize Calibration using ESP-IDF API
    // Function: adc_cali_create_scheme_curve_fitting(&cali_cfg, &handle)
    // What it does:
    // - Allocates memory for calibration object
    // - Prepares math to convert raw ADC → voltage (mV)
    if (adc_cali_create_scheme_line_fitting(&cali_cfg, &adc_cali_handle) == ESP_OK) {
        ESP_LOGI(ADC_TAG, "ADC calibration ready.");
    } else {
        ESP_LOGW(ADC_TAG, "ADC calibration not available. Using raw ADC values.");
        adc_cali_handle = NULL;          // Use raw values if calibration fails
    }

    // --- End of setup ---
    ESP_LOGI(ADC_TAG, "ADC is now initialized and ready for sampling.");

    // Return the ADC driver handle in case the caller wants it
    return adc_handle;

}


// =============================
// Helper: Push New Sample into ADC Buffer
// =============================
void adc_push_sample(int16_t sample) {
    adc_buffer[buffer_index] = sample;
    buffer_index = (buffer_index + 1) % BUFFER_SIZE;
}


// =============================
// FreeRTOS Task: ADC Sampling
// =============================
void adc_sampling(void *arg){

    ESP_LOGI(ADC_TAG, "ADC sampling task started!");

    while (1) {

        int raw = 0;
        int voltage = 0; // Calibrated voltage in mV

        // --- 1. Read raw ADC value ---
        adc_oneshot_read(adc_handle, ADC_CHANNEL, &raw); // ESP-IDF API

        // --- 2. Convert raw to calibrated voltage (mV) ---
        if (adc_cali_handle) {
            adc_cali_raw_to_voltage(adc_cali_handle, raw, &voltage); // ESP-IDF API
        } else {
            // Fallback if calibration unavailable
            voltage = raw;
        }

        // --- 3. Store calibrated voltage in circular buffer ---
        // Note: 1 unit = 0.1 mV scaling for EEG µV interpretation (e.g., 200 threshold = 20µV actual)
        xSemaphoreTake(adc_mutex, portMAX_DELAY);
        adc_buffer[buffer_index] = (int16_t)(voltage * 10);
        buffer_index = (buffer_index + 1) % BUFFER_SIZE; // Wrap around
        xSemaphoreGive(adc_mutex);

        // --- 4. Optional: Print to serial ---
        size_t prev_idx = (buffer_index + BUFFER_SIZE - 1) % BUFFER_SIZE;
        ESP_LOGD(ADC_TAG, "Raw ADC: %d mV -> Buffer[%zu]=%d", voltage, prev_idx, adc_buffer[prev_idx-1]);

        // --- 5. Delay for next sample (100 ms) ---
        vTaskDelay(pdMS_TO_TICKS(ADC_SAMPLE_PERIOD_MS));

    }
}


// =============================
// Simple Goertzel for Alpha Power (8-12 Hz; For Focus)
// =============================
uint8_t compute_alpha_score(const int16_t *window, size_t len){

    float coeff = 2.0f * cosf(2.0f * M_PI * 10.0f / SAMPLE_RATE_HZ);

    float q0 = 0, q1 = 0, q2 = 0;

    for (size_t i = 0; i < len; i++) {

        float s = (float)window[i];
        q0 = coeff * q1 - q2 + s;
        q2 = q1;
        q1 = q0;
        
    }

    // Power (no sqrt for speed)
    float magnitude = q1 * q1 + q2 * q2 - q1 * q2 * coeff;

    // Normalize to 0–100 (tune scale empirically)
    return (uint8_t)fminf(100.0f, magnitude * 0.00001f);
}


// =============================
// Event Detection (Blinks & Focus)
// =============================
void detect_events(int16_t filtered_current) {  // Changed: Param for filtered
    
    static int16_t prev_sample = 0;
    static uint8_t refractory = REFRACTORY_PERIOD_SAMPLES;  // simple debounce counter

    // Blink: Spike detection (derivative >200µV threshold)
    int16_t derivative = filtered_current - prev_sample;
    if (!refractory && abs(derivative) > 20) {  // µV threshold; adjust for your amp
        blink_count++;
        ESP_LOGI(ADC_TAG, "Blink detected! Count: %lu", blink_count);
        refractory = 20; // e.g., skip next 20 samples (~200 ms)
    }

    if (refractory) refractory--;

    prev_sample = filtered_current;
    
	// Focus: Every 50 samples (~0.5s @100Hz), compute alpha on window
	static size_t sample_counter = 0;   // Keeps track of elapsed samples
	sample_counter++;                   // Increment for each new sample processed

    // When 50 samples have accumulated (~0.5s @ 100Hz)
	if (sample_counter >= 50) {

        // Step 1: Compute alpha score using full ADC buffer
        attention_level = compute_alpha_score(adc_buffer, BUFFER_SIZE);  // Use full buffer as window

	  	// Step 2: Reset counter for the next window
        sample_counter = 0;
        
        // Step 3: Log or transmit the computed focus metric
	 	ESP_LOGI(ADC_TAG, "Attention level: %u", attention_level);

	}

}


// =============================
// IIR Bandpass Filter ( 0.5-30 Hz )
// =============================
int16_t apply_bandpass_iir(int16_t input) {
    
    float x = (float)input;
    
    float y = bp_b[0] * x + bp_b[1] * bp_x[0] + bp_b[2] * bp_x[1] -
              bp_a[1] * bp_y[0] - bp_a[2] * bp_y[1];
    
    bp_x[1] = bp_x[0]; bp_x[0] = x;
    bp_y[1] = bp_y[0]; bp_y[0] = y;
    
    return (int16_t)y;
}


// =============================
// FreeRTOS Task: Filtering + Detection 
// =============================
void adc_filtering(void *arg) {

    ESP_LOGI(ADC_TAG, "ADC filtering task started!");
    
    while (1) {

        // --- 1. Apply filter & detect on latest sample
        xSemaphoreTake(adc_mutex, portMAX_DELAY);
        size_t latest_idx = (buffer_index - 1 + BUFFER_SIZE) % BUFFER_SIZE;
        int16_t current_sample = adc_buffer[latest_idx];
        xSemaphoreGive(adc_mutex);

        // --- 2. Apply the digital IIR bandpass filter
        int16_t filtered = apply_bandpass_iir(current_sample);  // Compute once

        // --- 3. Detect events (blinks, attention) using filtered data
        detect_events(filtered);  // Pass to avoid double filter

        // --- 4. Optional: Print to serial ---
        // ESP_LOGI(ADC_TAG, "Filtered: %d µV, Blinks: %lu, Attention: %u", filtered, blink_count, attention_level);
        
        // --- 5. Delay for next sample (100 ms) ---
        vTaskDelay(pdMS_TO_TICKS(ADC_SAMPLE_PERIOD_MS));

    }
}


// =============================
// Test Helper Fn: Internal State Reset
// =============================
void reset_filter_state(void) {
    bp_x[0] = bp_x[1] = 0.0f;
    bp_y[0] = bp_y[1] = 0.0f;
}

void reset_adc_state(void) {
    memset(adc_buffer, 0, sizeof(adc_buffer));
    buffer_index = 0;
    blink_count = 0;
    attention_level = 0;
    reset_filter_state();
}




