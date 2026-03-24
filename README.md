# Matter-to-MAX! Bridge (cul32-c6)

The **Matter-to-MAX! Bridge** is an ESP-IDF based firmware that bridges legacy eQ-3 MAX! radiator thermostats, window contacts, and eco buttons to the Matter ecosystem over Wi-Fi. It allows you to control your existing 868MHz heating hardware using modern smart home platforms like Apple Home, Google Home, and Home Assistant without requiring the original MAX! Cube or FHEM.

## Features
- **Native Matter Integration:** Directly discoverable as a Matter bridge. No additional plugins or "hacks" required.
- **High Capacity:** Supports up to 61 MAX! peripheral devices per bridge.
- **Device Mapping:** Each MAX! Thermostat is represented as an individual Matter Endpoint (Heating Only).
- **Duty Cycle Management:** Implements a strict ETSI-compliant 1% Duty Cycle limit using a Token-Bucket algorithm to ensure regulatory compliance and prevent 868MHz band spam.
- **Individual Commissioning:** Automatically generates a unique Matter commissioning code based on the ESP32's MAC address.
- **Hardware Optimized:** Built for the ESP32-C6, utilizing its modern architecture for Wi-Fi connectivity and SPI communication with the CC1101.

## Hardware Requirements
- **Microcontroller:** ESP32-C6 (e.g., cul32-c6).
- **Radio Module:** TI CC1101 (868MHz MATCHED).
- **SPI Pinout:** GDO2=GPIO1, GDO0=GPIO2, CS=GPIO23, SCK=GPIO18, MISO=GPIO17, MOSI=GPIO16.

## Building and Flashing
This project is based on the ESP-IDF framework and the `esp-matter` SDK.

1. **Set up ESP-IDF (v5.1+):**
   ```bash
   . /path/to/esp-idf/export.sh
   ```
2. **Build and Flash:**
   ```bash
   idf.py build
   idf.py -p /dev/ttyUSB0 flash monitor
   ```
3. **Commissioning:** The firmware includes a fixed, static setup code to make deployment as simple as possible. No manual provisioning scripts are needed!
   - **Setup PIN:** `86801101`
   - **Discriminator:** `2152`
   - During setup, the device creates a BLE Hotspot. Use your Smart Home App (like Apple Home or Google Home) to pass your Wi-Fi credentials to the Bridge over the air.

## Duty Cycle Explanation
The 868MHz frequency band is subject to the ETSI EN 300 220 regulation, which limits transmission time to 1% (36 seconds per hour). 
- This bridge implements a **Token-Bucket** mechanism.
- Every transmission consumes "airtime tokens" (especially the 1-second Wake-On-Radio preamble).
- If the bucket is empty, further commands are blocked until the bucket refills.
- This prevents the bridge from being blocked by regulatory authorities and ensures fair frequency usage.