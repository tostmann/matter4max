# Integrating MAX! Devices into Home Assistant via Matter Bridge

This guide explains how to pair and manage your existing eQ-3 MAX! heating system devices (thermostats, window contacts, etc.) within Home Assistant using the custom ESP32-C6 Matter Bridge.

## Overview

The ESP32-C6 acts as a **Matter Bridge**. Instead of requiring you to manually type in the 24-bit RF addresses of your MAX! devices via a serial console, the bridge features a **Virtual Auto-Discovery Switch**. When activated via Home Assistant, the bridge listens to the 868MHz RF traffic, automatically captures ("sniffs") your MAX! devices, and dynamically clones them into your Home Assistant environment.

---

## Step 1: Commissioning the Bridge into Home Assistant

Before adding individual MAX! thermostats, you must pair the bridge itself with your Home Assistant instance.

1. Open the **Home Assistant Companion App** on your smartphone (Matter commissioning requires the mobile app).
2. Navigate to **Settings** -> **Devices & Services**.
3. Tap **Add Integration** (bottom right) -> **Add Matter device**.
4. When prompted, scan the Matter QR code or enter the pairing code.
   > **Note:** Since this is a development firmware based on the `esp-matter` SDK, the default manual pairing code is usually: **`34970112332`**
5. Home Assistant will connect to the bridge, provision Wi-Fi credentials (if not hardcoded), and register the device.
6. Once added, assign the bridge to a room. 

You will now see a new device in Home Assistant (e.g., "Espressif Matter Bridge").

---

## Step 2: Locating the Auto-Discovery Switch

If you look at the newly added Bridge device in Home Assistant, you will notice it exposes a **Switch** entity (often categorized under controls or lights, depending on HA's default interpretation of an `on_off_plugin_unit`).

This switch controls the **Auto-Discovery / Pairing Mode** of the bridge.
* **OFF (Default):** The bridge operates normally, routing data only for *known* devices. It ignores packets from unknown MAX! devices.
* **ON:** The bridge enters "Sniffing/Pairing" mode. Any valid MAX! packet intercepted on the 868MHz band will trigger the creation of a new device.

*Tip: Rename this entity in Home Assistant to "MAX! Pairing Mode" for clarity.*

---

## Step 3: Cloning/Pairing your MAX! Devices

To add your existing MAX! thermostats or window contacts to Home Assistant:

1. **Enable Pairing Mode:** In Home Assistant, toggle the "MAX! Pairing Mode" switch to **ON**.
2. **Wake up the MAX! Device:** Go to your physical MAX! Thermostat or Window Contact and force it to transmit an RF packet. 
   * *For a Thermostat:* Turn the dial to change the setpoint temperature by 0.5°C.
   * *For a Window Contact:* Open or close the window.
   * *For a Push Button:* Press a button.
3. **Automatic Registration:** The bridge will instantly intercept the packet, extract the unique 24-bit source address (e.g., `0x01CAC5`), and save it to its internal non-volatile storage (NVS).
4. **Dynamic Matter Endpoint:** Within milliseconds, the bridge dynamically creates a new Matter endpoint for this device and announces it to the Matter fabric.
5. **Success:** Look at your Devices list in Home Assistant. A new Thermostat or Contact Sensor will have appeared automatically, grouped under your Bridge device!

---

## Step 4: Securing the Network

**CRITICAL:** Once you have paired all your local MAX! devices, **turn the Auto-Discovery switch OFF**.

Because the MAX! protocol lacks modern encryption, leaving the pairing mode active indefinitely means the bridge might accidentally sniff and register your neighbor's MAX! devices if they are within RF range.

---

## Under the Hood & Troubleshooting

* **Persistence:** All discovered devices are saved to the ESP32's NVS flash memory. If the bridge loses power or restarts, it will read the known devices from memory and immediately recreate their Matter endpoints upon boot. You do not need to re-pair them.
* **Device Limits:** The bridge is currently configured to hold a maximum of 50 paired MAX! devices.
* **Device Types:** Currently, the auto-discovery attempts to classify the device based on the intercepted command. If a device appears as the wrong type in Home Assistant, it may require a manual RF address injection via the ESP's serial CLI.
* **No Devices Appearing?**
  * Ensure the ESP32-C6 is equipped with an 868MHz matched CC1101 transceiver.
  * Check the Home Assistant logs or connect a serial monitor to the ESP32 to verify if RF packets are being received (look for `MAX! RX [CRC OK]` in the console output).