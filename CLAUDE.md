# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an STM32F103C8T6-based agricultural monitoring system using LoRa for inter-node communication and NB-IoT for cloud data transmission. The system collects environmental data from multiple sensors (temperature, humidity, light, CO2, soil moisture, pH) and transmits it via a two-tier architecture:

- **Sender nodes**: Collect sensor data and broadcast via LoRa
- **Receiver/Gateway**: Receives LoRa data, adds timestamps via NB-IoT, and forwards to cloud via MQTT

## Build and Flash

The project uses Embedded IDE (EIDE) with Keil AC5 toolchain. Build commands:

- **Build**: Use EIDE extension in VSCode or the `eide.yml` configuration
- **Flash**: Use OpenOCD (ST-Link) with target `stm32f1x`, interface `stlink`, base address `0x08000000`
- **Clean**: Run `清理.cmd` (clean.cmd) for build artifacts cleanup

No standard test commands exist - hardware testing required on STM32 board.

## Code Architecture

### Device Selection via Compile Flags

Edit `main.c` to select device type:

```c
#define DEVICE_SENDER      // Lora发送端
//#define DEVICE_RECEIVER   // Lora接收端/NB-Iot发送端
```

### Directory Structure

```
LoRa-NB-IoT/
├── Application/       # Application layer (main logic)
│   ├── src/          # main.c, acquisition.c, transmission.c, display.c
│   └── inc/          # Application headers
├── Components/       # Sensor and peripheral drivers
│   ├── sht30/       # Temperature/humidity (I2C)
│   ├── bh1750/      # Light sensor (I2C)
│   ├── sh393/       # Soil moisture (ADC)
│   ├── jw01/        # CO2 sensor (UART)
│   ├── ph4052/      # pH sensor (ADC)
│   ├── oled/        # Display (I2C/SPI)
│   ├── key/         # Button input
│   ├── led/         # LED indicators
│   └── buzzer/      # Audio feedback
├── Middleware/       # Communication modules
│   ├── lora/        # LoRa radio (ATK-LORA-01 via AT commands)
│   └── nbiot/       # NB-IoT (MQTT over TCP)
└── Driver/           # Hardware abstraction
    ├── BSP/         # Board support (delay, I2C, SPI, UART, ADC, sys)
    └── std_periph_driver/  # STM32F10x standard peripheral library
```

### Key Data Flow

1. **Acquisition Layer** (`acquisition.c`):
   - Polls all sensors via `acquisition_poll()`
   - Caches data in `sensor_data_t` struct
   - Provides read-only access via `acquisition_read()`

2. **Transmission Layer** (`transmission.c`):
   - LoRa send/receive via `transmission_lora_send()` and `transmission_receive()`
   - NB-IoT MQTT publish via `transmission_nbiot_send()`
   - JSON serialization for cloud format
   - `get_comm_status()` provides module status (throttled to 10s interval)

3. **Display Layer** (`display.c`):
   - OLED UI updates via `display_sensor_data()`

### Communication Protocol

JSON format for all transmissions:
```json
{"temp":25.5,"air_humi":60.2,"soil_humi":45.3,"light":500.0,"ph":6.8,"co2":400,"time":1234567890}
```

- LoRa: Uses ATK-LORA-01 module in transparent transmission mode
- NB-IoT: GA7 module with MQTT broker at 82.157.129.239:1883, topic `farm/sensor/collect`

### Hardware Constraints

- LoRa aux pin: PA8
- LoRa MD0 (config mode): PB15
- NB-IoT uses separate UART with frame-based RX parsing
- Main loop uses `__WFI()` for power savings

## Code Style

Follow `注释风格规范.md`:

- **Doxygen comments** for all functions: `@brief`, `@param`, `@retval`
- **Chinese language** for comments
- **File headers** with @file, @brief, @author, @date, @version
- **4-space indentation**
- Use .clang-format for automatic formatting

## Error Handling

Common error codes:
- `ACQ_OK`, `ACQ_ERROR`, `ACQ_NO_DATA` (acquisition)
- `TRANS_OK`, `TRANS_BUSY`, `TRANS_ERROR`, `TRUNCATED` (transmission)
- `SHT30_EOK`, `BH1750_EOK`, `JW01_EOK`, `SH393_EOK`, `PH4052_EOK` (sensors)

Modules use OLED display + buzzer feedback for initialization status.