# Zephyr-Based Environmental Compliance Monitoring System

A **Zephyr RTOS–based embedded IoT system** for **air quality (MQ-135) and flame detection**, implemented on an **STM32 microcontroller** with **UART-based ESP8266 connectivity** and a **server-side data logging pipeline**.

This project focuses on **firmware architecture, RTOS design, peripheral integration, and data transport**, rather than high-level IoT theory.

---

## Technical Objective

Design and implement a **real-time embedded monitoring node** that:
- Samples analog and digital environmental sensors
- Runs concurrent tasks under **Zephyr RTOS**
- Transmits structured telemetry over UART to a Wi-Fi module
- Pushes data to a backend for logging and visualization

The system is designed as a **resource-constrained IoT edge node**, emphasizing deterministic behavior and modular firmware structure.

---

## System Architecture (Implementation View)

```
┌──────────────┐
│ MQ-135       │  ADC
└─────┬────────┘
      │
┌─────▼────────┐        UART        ┌──────────────┐
│ Flame Sensor │ GPIO ───────────▶ │ ESP8266      │
└─────┬────────┘                    │ (AT FW)     │
      │                             └─────┬────────┘
┌─────▼────────┐                           │ HTTP
│ STM32 +      │                           ▼
│ Zephyr RTOS  │                    ┌──────────────┐
│              │                    │ Web Server   │
│ Threads + WD │                    │ (PHP/MySQL) │
└──────────────┘                    └──────────────┘
```

---

## Hardware Platform

- **MCU:** STM32 Nucleo-F103RB
- **RTOS:** Zephyr 4.2.x
- **Sensors:**
  - MQ-135 (analog air quality sensor)
  - IR-based flame sensor (digital)
- **Connectivity:** ESP8266 (UART, AT command firmware)

---

## Zephyr RTOS Firmware Design

### Device Tree Configuration

Sensors and peripherals are defined using **Zephyr Device Tree**, enabling:
- Hardware abstraction
- Board portability
- Clean driver binding

Examples:
- ADC channel binding for MQ-135
- GPIO input configuration for flame sensor
- UART binding for ESP8266 communication

---

### Thread Model

The firmware is structured using **multiple Zephyr threads**:

| Thread | Responsibility |
|------|---------------|
| `sensor_thread` | Periodic ADC + GPIO sampling |
| `comm_thread` | UART framing and ESP8266 communication |
| `watchdog_thread` | System liveness monitoring |

Threads run concurrently with defined priorities to avoid blocking behavior.

---

### Sensor Acquisition

- **MQ-135**
  - Sampled via Zephyr ADC API
  - Raw ADC values converted to relative air-quality readings

- **Flame Sensor**
  - Polled via GPIO input
  - Binary detection for flame presence

Sampling is performed at fixed intervals using `k_sleep()`.

---

## UART ↔ ESP8266 Integration

### Communication Model

- STM32 acts as **UART master**
- ESP8266 runs **AT command firmware**

UART responsibilities:
- Send AT commands
- Receive status and responses
- Stream HTTP payloads

---

### AT Command Flow

```
AT+CWJAP="SSID","PASSWORD"
AT+CIPSTART="TCP","<server_ip>",80
AT+CIPSEND=<len>
POST /insert.php HTTP/1.1
...
```

Zephyr UART APIs ensure:
- Non-blocking serial I/O
- Predictable communication timing

---

## Data Framing & Transport

Sensor data is formatted as key-value payloads before transmission:

```
sensor=air_quality&value=XXX
sensor=flame&value=0|1
```

Payloads are sent using **HTTP POST** requests handled by the ESP8266.

---

## Backend Pipeline

### Server Stack

- Apache (XAMPP)
- PHP backend
- MySQL database

### Data Flow

1. ESP8266 sends HTTP POST request
2. PHP script validates payload
3. Data inserted into SQL database
4. Dashboard queries latest records

Each record includes:
- Sensor name
- Value
- Source node
- Timestamp

---

## Reliability Mechanisms

- **Zephyr Watchdog Timer**
  - Detects stalled threads
  - Forces system reset on failure

- **Thread Separation**
  - Prevents communication failures from blocking sensor sampling

- **UART Error Handling**
  - Basic retry logic for AT command failures


