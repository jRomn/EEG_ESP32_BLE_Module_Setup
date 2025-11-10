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

1. **Define the Controller Blueprint**:

```c
ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
```

- BLE-only or dual-mode, default memory allocation, hardware timing.
- At this stage, it’s only a blueprint, no hardware is configured yet.

2. **Initialize Controller**:

```c
ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
```

- Allocates driver structures and programs low-level registers.
- Prepares BLE mode, radio remains off.


3. **Enable BLE Mode**:

```c
ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
```

- APowers on the BLE radio.
- Activates internal clocks and timing sources.
- Controller ready for radio-level communication



## Step 2: BLE Stack Initialization (Software Layer)

1. **Initialize Bluedroid Stack**:
2. **Enable Bluedroid Stack**:


## Step 3: GATT Server Registration (Application Layer)

1. **Register Event Handlers:**:
2. **Register Application Instance**:
3. **Start Advertising**:


## Step 4: BLE Notifications (Live Data Exchange)












 









----------------------------------------------------------------------------------------------------






## System Diagram — ADC Subsystem Data Flow

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
