# Matter-to-MAX! Bridge (cul32-c6)

The **Matter-to-MAX! Bridge** is an ESP-IDF based firmware that bridges legacy eQ-3 MAX! radiator thermostats, window contacts, and eco buttons to the Matter ecosystem over Wi-Fi. It allows you to control your existing 868MHz heating hardware using modern smart home platforms like Apple Home, Google Home, and Home Assistant without requiring the original MAX! Cube or FHEM.

## Features
- **Native Matter Integration:** Directly discoverable as a Matter bridge via Bluetooth LE. No additional plugins or "hacks" required.
- **High Capacity:** Supports up to 61 MAX! peripheral devices per bridge.
- **Auto-Discovery:** Easily clone your existing MAX! devices via a virtual switch in your Smart Home app.
- **Duty Cycle Management:** Implements a strict ETSI-compliant 1% Duty Cycle limit using a Token-Bucket algorithm to ensure regulatory compliance and prevent 868MHz band spam.
- **Hardware Optimized:** Built for the ESP32-C6, utilizing its modern architecture for Wi-Fi connectivity and SPI communication with the CC1101.

## Hardware Requirements
- **Microcontroller:** ESP32-C6 (e.g., cul32-c6).
- **Radio Module:** TI CC1101 (868MHz MATCHED).
- **SPI Pinout:** GDO2=GPIO1, GDO0=GPIO2, CS=GPIO23, SCK=GPIO18, MISO=GPIO17, MOSI=GPIO16.

## Installation & Commissioning

### 1. Flashing the Firmware
You don't need to compile the firmware yourself. A pre-compiled factory binary is available in the `binaries/` folder.

```bash
esptool.py --chip esp32c6 -b 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_freq 80m --flash_size 4MB 0x0 binaries/max_matter_bridge_factory.bin
```

### 2. Pairing via Bluetooth LE
The firmware uses the standard Matter SDK test credentials for commissioning.

1. Power on the ESP32-C6.
2. Open your Smart Home App (e.g., Home Assistant, Apple Home, Google Home).
3. Add a new Matter device without a QR code and enter the standard Setup PIN:
   - **Setup PIN:** `34970112332`
4. The device will appear in your Bluetooth list as `MAX-XXXX` (where XXXX is derived from its MAC address).
5. The App will securely transmit your Wi-Fi credentials to the Bridge over the air.

*For detailed Home Assistant instructions, please refer to the `HASS_HowTo.md` file.*

## Duty Cycle Explanation
The 868MHz frequency band is subject to the ETSI EN 300 220 regulation, which limits transmission time to 1% (36 seconds per hour). 
- This bridge implements a **Token-Bucket** mechanism.
- Every transmission consumes "airtime tokens" (especially the 1-second Wake-On-Radio preamble).
- If the bucket is empty, further commands are blocked until the bucket refills.
- This prevents the bridge from being blocked by regulatory authorities and ensures fair frequency usage.
