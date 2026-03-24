# Using Matter-to-MAX! with Home Assistant

This guide explains how to integrate your MAX! devices into Home Assistant via the Matter-to-MAX! Bridge.

## Pairing with Home Assistant
1. Ensure the **Matter Server** add-on is installed in your Home Assistant instance.
2. Open the Home Assistant Mobile App.
3. Go to **Settings > Devices & Services**.
4. Click **Add Integration > Add Matter Device**.
5. Choose **"Setup without QR Code"** and enter the static Setup PIN manually:
   - **Setup PIN:** `86801101`
   - During setup, the device creates a BLE Hotspot. Home Assistant will discover the Bridge and automatically send your Wi-Fi credentials to it! No more hardcoded Wi-Fi passwords in the C-Code!
6. Once paired, the bridge will automatically expose all paired MAX! devices as individual entities in Home Assistant.

## Thermostat Modes (Heat vs. Auto)
According to the official Matter Specification, the "Auto" mode strictly implies a system that can switch between Heating and Cooling (HVAC systems). Since eQ-3 MAX! thermostats are heating-only devices, the Matter standard mandates that the "Auto" mode is hidden. The bridge maps modes as follows:

- **Heat:** Maps to MAX! "Manual" mode. This allows you to set a specific target temperature from Home Assistant.
- **Off:** Maps to MAX! "Off" mode (typically 4.5°C).
- *(Internal Auto)*: If the physical thermostat runs its internal weekly schedule, the bridge transparently reports this state to Home Assistant as "Heat" to avoid UI flickering and spec violations.

### Important: Schedules
MAX! thermostats have internal hardware schedules. However, **Matter does not natively support managing these internal schedules as of today.** 
- It is highly recommended to keep the thermostats in **"Heat"** mode permanently.
- Use **Home Assistant Automations** or the **Scheduler Component** (Blueprint/HACS) to change the temperature based on time, weather, or occupancy. Home Assistant becomes the master scheduler.

## Duty Cycle Considerations for Automations
Because the bridge must adhere to the 1% Duty Cycle (ETSI 868MHz regulation), you must be careful with aggressive automations.

- **The Limit:** The bridge can handle roughly **30 to 40 commands per hour**. Receiving states (window open/close) is unlimited and does not affect the duty cycle.
- **Best Practices:**
    - Do not create automations that change the temperature every few minutes.
    - If you have 20 thermostats, avoid changing the temperature on all of them exactly simultaneously; stagger the commands by a few seconds or minutes if possible.
- **Symptoms of Exhaustion:** If you exceed the duty cycle, the bridge will silently drop transmission commands for a period, and your thermostats will not respond to changes until the "token bucket" has refilled. Wait 15-30 minutes if this happens.