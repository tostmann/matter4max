# Integrating MAX! Devices into Home Assistant via Matter Bridge

This guide explains how to pair and manage your existing eQ-3 MAX! heating system devices (thermostats, window contacts, etc.) within Home Assistant using the custom ESP32-C6 Matter Bridge.

## Step 1: Flashing the Bridge

If you haven't flashed your ESP32-C6 yet, you can use the pre-compiled factory binary found in the `binaries/` folder of this repository.

Run the following command (adjust the COM port `ttyACM0` or `/dev/serial/by-id/...` to your system):
```bash
esptool.py --chip esp32c6 -b 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_freq 80m --flash_size 4MB 0x0 binaries/max_matter_bridge_factory.bin
```

## Step 2: Commissioning the Bridge (Bluetooth LE)

The firmware uses the standard Matter BLE (Bluetooth Low Energy) commissioning process.

1. Power on the ESP32-C6 Bridge. It will automatically start broadcasting via Bluetooth.
2. Open the **Home Assistant Companion App** on your smartphone.
3. Navigate to **Settings** -> **Devices & Services**.
4. Tap **Add Integration** (bottom right) -> **Add Matter device**.
5. Choose **"Setup without QR Code"** and enter the standard test pairing code:
   **`34970112332`**
6. In the Bluetooth device list that appears, look for a device named **`MAX-XXXX`** (where XXXX are the last characters of your bridge's MAC address, e.g., `MAX-9E5C`). Select it.
7. Home Assistant will connect via Bluetooth and securely transmit your Wi-Fi credentials to the Bridge.
8. Once added, assign the "MAX! Bridge XXXX" to a room.

## Step 3: Auto-Discovery of MAX! Devices

Instead of manually typing 24-bit RF addresses, the bridge features a **Virtual Auto-Discovery Switch**.

1. Look at the newly added Bridge device in Home Assistant. You will notice it exposes a **Switch** entity (often named `MAX-DISCOVERY`).
2. **Enable Pairing Mode:** Toggle this switch to **ON**. The bridge is now in "Sniffing/Pairing" mode.
3. **Wake up your MAX! Devices:** Go to your physical MAX! Thermostats or Window Contacts and force them to transmit a packet:
   * *Thermostat:* Turn the dial to change the temperature.
   * *Window Contact:* Open or close the window.
   * *Wall Thermostat / Push Button:* Press a button.
4. **Automatic Registration:** The bridge instantly intercepts the packet, extracts the unique address (e.g., `0x01CAC5`), saves it to its internal memory (NVS), and dynamically creates a new Matter endpoint.
5. A new Thermostat or Contact Sensor will automatically appear in Home Assistant, grouped under your Bridge!

> **CRITICAL:** Once you have paired all your local MAX! devices, **turn the Auto-Discovery switch OFF**. Leaving it on may cause the bridge to accidentally sniff and register your neighbor's MAX! devices.

## Step 4: Matter Thermostat Modes & Schedulers

According to the official Matter Specification, the "Auto" mode is strictly for HVAC systems that can *both* heat and cool. Since MAX! thermostats are heating-only, the Matter standard mandates that the "Auto" mode is hidden. 

The bridge maps the modes as follows:
- **Heat:** Maps to MAX! "Manual" mode or "Auto" mode.
- **Off:** Maps to MAX! "Off" mode (typically 4.5°C).

**Recommendation:** It is highly recommended to control schedules entirely through Home Assistant (using Automations or Scheduler integrations) rather than relying on the physical MAX! device's internal weekly schedules.

## Step 5: Duty Cycle Considerations (1% Rule)

Because the bridge must adhere to the 1% Duty Cycle (ETSI 868MHz regulation), you must be careful with aggressive automations.
- **The Limit:** The bridge can physically send roughly **30 to 40 commands per hour**. (Receiving states from window contacts is unlimited).
- **Best Practices:** Do not create automations that change the temperature every few minutes. If you have 10 thermostats, avoid changing the temperature on all of them at the exact same millisecond; stagger the commands by a few seconds if possible.
- If you exceed the duty cycle, the bridge will log a warning and silently drop transmission commands until the internal "token bucket" has refilled.
