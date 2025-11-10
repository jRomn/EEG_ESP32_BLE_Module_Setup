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

-> | Note : At this point, the structure is only a blueprint—no HW is yet configured.

2. **Initialize Controller**:

```c
ret = esp_bt_controller_init(&bt_cfg);
```

- Allocates driver structures and programs low-level registers.
- Prepares BLE mode, radio remains off.


3. **Enable BLE Mode**:

```c
ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
```

- Powers on the BLE radio.
- Activates internal clocks and timing sources.
- Controller ready for radio-level communication



## Step 2: BLE Stack Initialization (Software Layer)

1. **Initialize Bluedroid Stack**:

```c
ret = esp_bluedroid_init();
```

- Allocates memory for GAP, GATT, L2CAP, SMP layers.
- Prepares event queues and callback structures.

2. **Enable Bluedroid Stack**:

```c
ret = esp_bluedroid_enable();
```

- Links the software stack to the hardware controller via HCI.
- Starts internal tasks for real-time BLE event handling.


## Step 3: GATT Server Registration (Application Layer)
Source File:
1. **Register Event Handlers:**:

```c
esp_ble_gap_register_callback(gap_event_handler);
esp_ble_gatts_register_callback(gatts_event_handler);
```

- GAP: Advertising, pairing, discovery.
- GATT: Service creation, read/write requests, notifications.

2. **Register Application Instance**:

```c
ret = esp_ble_gatts_app_register(0);
```

- Assigns a unique Application ID (0).
- Prepares internal resources for the GATT application.


5. **Start Advertising**:

```c
esp_ble_gap_start_advertising(&adv_params);
```

- ESP32 becomes a fully active BLE peripheral.
- Capable of broadcasting, accepting connections, and exchanging data.



## Step 4: BLE Notifications (Live Data Exchange)












 









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
        └────────────────────────┘
```

----------------------------------------------------------------------------------------------------


## Folder Structure

For “modular implementation” here is the expected general folder structure: 
	
	eeg/          — Root project directory
	├── .vscode/            — VS Code configs (for debugging bliss)
	├── components/
	│   └── adc/            — Our ADC module (reusable, testable)
	│       ├── include/
	│       │   └── adc.h   — Declarations, configs, globals
	│       ├── adc.c       — Implementations (init, future tasks/filters)
	│       ├── CMakeLists.txt — Builds the component
	│       └── test/       — Unit tests (mock ADC for filter validation)
	│           ├── CMakeLists.txt
	│           └── test_adc.c
	├── main/
	│   ├── main.c          — App entry (init everything, create tasks)
	│   └── CMakeLists.txt  — Main component build
	├── test/               — Full-system tests
	│   ├── CMakeLists.txt
	│   ├── sdkconfig.defaults
	│   └── main/
	│       ├── CMakeLists.txt
	│       └── test_main.c
	├── CMakeLists.txt      — Top-level project (boilerplate magic)
	├── pytest_unittest.py  — Test runner (optional)
	└── README.md           — This doc (or expand it!)
	

> | Note :  We recommend to follow this structure since it would be easy to add unit tests in the future. 

The most important parts right now are the main.c, adc.h and adc.c.
