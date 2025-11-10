# EEG Headband Firmware Setup — BLE Module Subsystem 

*Development Environment*

This project was developed and tested using:

- IDE: Visual Studio Code

- C/C++ IntelliSense & Linting: clangd

- SDK / Framework: ESP-IDF (Espressif IoT Development Framework)

- Target Platform: ESP32

> | Note: Clangd integration ensures accurate code completion, error highlighting, and faster navigation while working with ESP-IDF projects in VS Code.


## Overview: 

This repository contains the setup for the **WiFi / BLE Module Subsystem** on the ESP32, designed for wireless communication of EEG sensor data. 

While WiFi supports high-throughput communication ( useful for firmware updates or bulk data transfers ), **Bluetooth Low Energy ( BLE )** is optimized for low-power, short-range, event-based communication—perfect for streaming lightweight sensor data such as EEG signal statistics or device telemetry

- **WiFi**: High-throughput communication, suitable for firmware updates and bulk data transfer.
- **Bluetooth Low Energy (BLE)**: Low-power, short-range, event-driven communication optimized for streaming lightweight EEG signal metrics such as blink count and attention level.

This project focuses on the **BLE portion** of the communication stack, turning the ESP32 into an active BLE peripheral capable of advertising, connecting, and exchanging live sensor data with external devices.

---

## Subsystem Architecture

The BLE setup follows a **three-layer structure**:

| Layer | Description |
|-------|-------------|
| Controller (Hardware) | Initializes the Bluetooth radio on the ESP32 and prepares it for BLE mode. |
| Stack (Software - Bluedroid) | Manages GAP & GATT protocols, event queues, and HCI communication with the controller. |
| Application (GATT Server) | Defines services, characteristics, notifications, and application-specific behavior. |


**Setup Flow**: 

> | Controller Initialization → Controller Enable → Stack Initialization → Stack Enable → Application (GATT Server, Advertising, Notifications)



---

## Step 1: BLE Controller Initialization (Hardware Layer)

**Source File**: [`ble.c`]  
**Task Launch** : N/A

At this stage, we are working at the lowest level of the Bluetooth system — the controller layer. This corresponds to the physical radio hardware inside the ESP32. 

The process is analogous to powering up and configuring a dedicated Bluetooth radio chip, except this one is integrated into the ESP32 SoC.

1. **Define the Controller Blueprint**:

The first step is to define a configuration structure ( esp_bt_controller_config_t ) that “describes how the Bluetooth controller should operate”. 

```c
esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
```

The BT_CONTROLLER_INIT_CONFIG_DEFAULT() macro populates this structure with recommended defaults :

- BLE-only or dual-mode, default memory allocation, hardware timing.
- At this stage, it’s only a blueprint, no hardware is configured yet.

> | Note : At this point, the structure is only a blueprint—no HW is yet configured.

2. **Initialize Controller**:

This command takes the configuration blueprint and translates it into a fully initialized controller object : 

```c
ret = esp_bt_controller_init(&bt_cfg);
```

- Allocates driver structures and programs low-level registers.
- Prepares BLE mode, radio remains off.

> | Note :  At this point we have taken our blueprint and turned it into an operational Controller driver object, but the radio itself remains off.

3. **Enable BLE Mode**:

```c
ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
```

This powers on the BLE radio and transitions it from an idle configuration to an operational state :

- Activates internal clocks and timing sources.
- Makes the controller fully ready for data transmission.

> | Note :  From this point on the controller ( hardware layer ) is alive and capable of radio-level communication. 



## Step 2: BLE Stack Initialization (Software Layer)

**Source File**: [`ble.c`]  
**Task Launch** : N/A

The **Bluedroid Stack** is the software brain that runs above the hardware controller.
It manages the higher-level Bluetooth protocols such as **GAP (Generic Access Profile)** and **GATT ( Generic Attribute Profile )**, which define how the ESP32 identifies itself, establishes connections, and exchanges data with external BLE devices such as smartphones or computers. 

In this phase, we are initializing the host stack ( **Bluedroid** ) that interacts with the controller through the HCI ( Host Controller Interface ). 

1. **Initialize Bluedroid Stack**:

Set up in memory the definition of the software environment that will manage the BLE controller : 

```c
ret = esp_bluedroid_init();
```

- Allocates memory for GAP, GATT, L2CAP, SMP layers.
- Prepares event queues and callback structures.

>  At this point, the “firmware ( OS ) ” of the Bluetooth brain exists structured in memory, but remains idle—it has not yet begun processing events.

2. **Enable Bluedroid Stack**:

This brings the software stack to life — launching its internal tasks and linking it to the controller hardware through the ( HCI ) .

```c
ret = esp_bluedroid_enable();
```

Basically, this step turns the static “firmware image” in RAM into a running system.

- Links the Bluedroid software with the hardware controller through Host Controller Interface ( HCI )..
- Begins real-time event handling between layers

> | From this point on, the controller ( hardware layer ) and software ( Bluedroid stack ) are fully synchronized and ready to support BLE operations ( GATT services, advertising, and connections ). 

## Step 3: GATT Server Registration (Application Layer)

**Source File**: [`ble.c`]  
**Task Launch** : N/A

Once both the Controller ( hardware ) and the Bluedroid Stack ( software ) are initialized and enabled, the BLE subsystem is alive but functionally blank — it has no personality yet : it doesn’t advertise, expose data, or respond to clients.

Here you will define those properties. You will define what your ESP32 is in the BLE world — i.e., the services it offers, the data it exposes, and how it reacts to connected clients.

Source File:
1. **Register Event Handlers:**:

Before defining any services or characteristics, we must tell the Bluedroid Stack ( software layer ) which are the functions for the application layer to handle Bluetooth events. 

This is done by registering callback functions : 

```c
esp_ble_gap_register_callback(gap_event_handler);
esp_ble_gatts_register_callback(gatts_event_handler);
```

- GAP ( Generic Access Profile ) handles connection-level events — advertising, discovery, pairing, and connection state changes.
- GATT ( Generic Attribute Profile ) handles data-level events — service creation, characteristic read/write requests, notifications, and indications.

> | Note : Those two callback functions should be defined already in your ble.c source file. See Appendix C for more details. 

2. **Register Application Instance**:

This allows Bluedroid to route events and service operations to the correct application context.

```c
ret = esp_ble_gatts_app_register(0);
```

This command assigns your firmware an Application ID ( 0 in this case) and creates an internal record within Bluedroid that represents your GATT application.

Under the hood, Bluedroid allocates internal resources to represent your application instance, setting up data structures to handle future service creation and client interactions.

> | Note : At this point, the GATT application exists as an internal representation within Bluedroid — registered but not yet fully configured. 

> Through the two callback functions defined earlier, Bluedroid begins an asynchronous setup sequence: it triggers events that prompt your firmware to define services, add characteristics, and configure advertising parameters.

> This callback mechanism forms the communication bridge between the Bluedroid Stack (software layer) and your application logic (firmware layer), ensuring that all GATT-related structures and behaviors are established dynamically during initialization. 


3. **Start Advertising**:

The advertising activation step is handled within the gatts_event_handler() under the ESP_GATTS_START_EVT case. 

```c
esp_ble_gap_start_advertising(&adv_params);
```

At this point, the internal asynchronous chain of GATT setup events has completed — meaning the service is created, characteristics are added, and the GATT database is finalized.

Once this call executes, the ESP32 transitions into a fully active BLE peripheral state, capable of :

- Broadcast identification packets ( device name, UUIDs ).
- Accept incoming connections ( e.g., phone, tablets  ).
- Exchange data through the GATT operations defined earlier — such as characteristic reads, writes, and notifications. 


## Step 4: BLE Notifications (Live Data Exchange)

**Source File**: [`ble.c`](components/ble/ble.c)  
**Task Launch** (from `main.c`):

The **BLE notification phase** represents the **live data exchange** between your ESP32 ( acting as the **GATT server** ) and a connected Central device ( Client ).

Once the GATT service has been started and advertising is active, you can begin transmitting dynamic data updates — such as sensor readings ( blink_count ) or computed metrics ( attention_level ) — through notifications or indications.

In this implementation, the notification logic resides in a dedicated **FreeRTOS task**, launched from main.c :

```c
// --- Task for BLE Advertising & Notifications ---
task_status = xTaskCreate(ble_notifications, "BLE Notifications", 4096, NULL, 3, NULL);
if (task_status != pdPASS){
    ESP_LOGE(BLE_TAG, "Failed to create BLE task!");
}
```

For this to work you will need to implement the following sequence :

1. **Connection & Handle Validation**

Validate that a valid BLE connection exists ( conn_id != 0xFFFF ) and the characteristic handles for Blink Count and Attention Level are available. 

```c
while (1) {


    // Only if Connected + handles ready
    if (conn_id != 0xFFFF && blink_handle && attention_handle) {         
    }
    vTaskDelay(pdMS_TO_TICKS(250));  // 1s delay


}
```


2. **Sent Blink Count Notification / Attention Level Notification**

Once the Blink Count or Attention Level changes, transmit the updated values using  esp_ble_gatts_send_indicate( ). 

Each characteristic is sent independently, only when its value differs from the previous one.

```c
// Example: Notify Blink Count
esp_ble_gatts_send_indicate(gatts_if_global, conn_id, blink_handle, sizeof(blink_data), blink_data, false);


// Example: Notify Attention Level
esp_ble_gatts_send_indicate(gatts_if_global, conn_id, attention_handle, sizeof(attn_data), attn_data, false);
```

> | Note : Each payload is packed in little-endian format, matching BLE GATT conventions.

3. **Periodic Task Delay**

The notification task runs periodically to avoid flooding the BLE connection:

```c
vTaskDelay(pdMS_TO_TICKS(250)); // 250 ms delay between notifications
```

- This maintains a smooth and consistent update rate (~4 updates/sec).
- Ensures real-time streaming of blink count and attention metrics without overloading the BLE stack.
  
At this stage, the GATT service is active, the client is connected, and your BLE peripheral is sending periodic notifications representing live application data — formatted in the correct GATT-compliant little-endian layout.

This concludes the setup of the WiFi / BLE Module Subsystem, fully enabling the ESP32 as a functioning BLE peripheral that can broadcast, connect, and exchange live sensor data with external devices.


 









----------------------------------------------------------------------------------------------------






## System Diagram — ADC / BLE Subsystem Data Flow

```
        ┌────────────────────────┐
        │  Analog EEG Signal     │
        │ (0–3.3 V after AFE)    │
        └──────────┬─────────────┘
                   │
                   ▼
        ┌────────────────────────┐
        │ ADC Unit (ADC1)        │
        │ - adc_oneshot_read()   │
        │ - 12-bit @ 12 dB atten │
        └──────────┬─────────────┘
                   │ Raw Counts (0–4095)
                   ▼
        ┌────────────────────────┐
        │ ADC Calibration         │
        │ - Line Fitting Scheme   │
        │ - Converts to mV        │
        └──────────┬─────────────┘
                   │ Calibrated mV
                   ▼
        ┌────────────────────────┐
        │ FreeRTOS Acquisition   │
        │ Task (100 Hz)          │
        │ - Periodic sampling     │
        │ - Stores to buffer      │
        └──────────┬─────────────┘
                   │
                   ▼
        ┌────────────────────────┐
        │ Circular Buffer         │
        │ - Rolling data storage  │
        └──────────┬─────────────┘
                   │
                   ▼
        ┌────────────────────────┐
        │ Signal Processing       │
        │ Module (Filtering, FFT) │
        └──────────┬─────────────┘
                   │ Processed EEG metrics
                   ▼
        ┌────────────────────────┐
        │ BLE Notification Task   │
        │ - FreeRTOS Task         │
        │ - Sends Blink Count     │
        │   & Attention Level     │
        │   via GATT Notifications│
        └──────────┬─────────────┘
                   │ BLE Peripheral
                   ▼
        ┌────────────────────────┐
        │ External BLE Client     │
        │ (Phone, Tablet, PC)    │
        │ - Receives live EEG     │
        │   metrics               │
        └────────────────────────┘

```

----------------------------------------------------------------------------------------------------


## Folder Structure

For “modular implementation” here is the expected general folder structure: 
	
eeg/                      — Root project directory
├── .vscode/              — VS Code configs (debugging, linting, IntelliSense)
├── components/
│   ├── adc/              — ADC module (reusable, testable)
│   │   ├── include/
│   │   │   └── adc.h     — Declarations, configs, globals
│   │   ├── adc.c         — Implementations (init, tasks, filters)
│   │   ├── CMakeLists.txt— Component build
│   │   └── test/         — Unit tests (mock ADC for filter validation)
│   │       ├── CMakeLists.txt
│   │       └── test_adc.c
│   └── ble/              — BLE module (GATT server, notifications)
│       ├── include/
│       │   └── ble.h     — Declarations, configs, globals
│       ├── ble.c         — Implementations (GATT setup, notifications)
│       ├── CMakeLists.txt— Component build
│       └── test/         — Unit tests (mock BLE events / GATT)
│           ├── CMakeLists.txt
│           └── test_ble.c
├── main/
│   ├── main.c            — App entry (init everything, create tasks)
│   └── CMakeLists.txt    — Main component build
├── test/                 — Full-system tests
│   ├── CMakeLists.txt
│   ├── sdkconfig.defaults
│   └── main/
│       ├── CMakeLists.txt
│       └── test_main.c
├── CMakeLists.txt        — Top-level project (boilerplate magic)
├── pytest_unittest.py    — Test runner (optional)
└── README.md             — This documentation (expanded!)


	

> | Note :  We recommend to follow this structure since it would be easy to add unit tests in the future. 

The most important parts right now are the main.c, adc.h and adc.c.
