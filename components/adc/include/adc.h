#ifndef ADC_H
#define ADC_H

// =============================
// Header Files (Your Toolbox)
// =============================

    /* --- General --- */
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
    // #include "esp_log.h"
    // #include "esp_err.h"

    /* --- ADC --- */
    #include "esp_adc/adc_oneshot.h"    // For ADC HW interation
    #include "esp_adc/adc_cali.h"       // For voltage calibration

    /* --- [  ] --- */
    #include "freertos/semphr.h"

// =============================
// Application Log Tag
// =============================
   
    #define ADC_TAG "ADC"


// =============================
// Global Declarations (For cross-file access)
// =============================
extern adc_oneshot_unit_handle_t adc_handle;  // ADC driver handle
extern adc_cali_handle_t adc_cali_handle;     // ADC Calibration handle


// =============================
// ADC Configuration (Exposed for ADC.c)
// =============================
#define ADC_UNIT       ADC_UNIT_1
#define ADC_CHANNEL    ADC_CHANNEL_6   // GPIO34
#define BUFFER_SIZE    256             // Circular buffer length
#define ADC_SAMPLE_PERIOD_MS 10.0       // Sampling period (ms)
#define SAMPLE_RATE_HZ (1000 / ADC_SAMPLE_PERIOD_MS)  // Derived rate
#define REFRACTORY_PERIOD_SAMPLES 20  // 200 ms at 100 Hz


// =============================
// Circular Buffer & Index (Exposed for ADC.c)
extern int16_t adc_buffer[BUFFER_SIZE];
extern volatile size_t buffer_index; // producer increments after write


// =============================
// Processed metrics (shared with BLE)
// =============================
extern volatile uint32_t blink_count;
extern volatile uint8_t attention_level;


// =============================
// Synchronization Primitives
// =============================
extern SemaphoreHandle_t adc_mutex;


// =============================
// IIR Bandpass Globals (Exposed for ADC.c)
// =============================
extern float bp_a[3];
extern float bp_b[3];
extern float bp_x[2];   // Input History
extern float bp_y[2];   // IIR Output History (for test reset)


// =============================
// Helper: Push New Sample into ADC Buffer
// =============================
void adc_push_sample(int16_t sample);


// =============================
// Main Functions:
// =============================

    // =============================
    /* ADC Unit Initialization + Channel Configuration + Calibration */
    // ============================= 
    adc_oneshot_unit_handle_t init_adc(void);
    // Returns a handle to the initialized ADC unit.

    // =============================
    // FreeRTOS Task: ADC Sampling
    // =============================
    void adc_sampling(void *arg);


    // =============================
    // FreeRTOS Task: Filtering
    // =============================
    void adc_filtering(void *arg);
    int16_t apply_bandpass_iir(int16_t input);      // Bandpass filter
    void detect_events(int16_t filtered_current);   // Blink & alpha detection
    uint8_t compute_alpha_score(const int16_t* window, size_t len);  // Goertzel-based


#endif // ADC_H


// =============================
// Test Support API
// =============================
#ifdef UNIT_TEST

void reset_filter_state(void);
void reset_adc_state(void);

#endif
