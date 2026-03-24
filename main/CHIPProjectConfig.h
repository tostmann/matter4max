#pragma once

// Use the default test setup PIN of esp-matter: 20202021
// Change the discriminator slightly to force Home Assistant to ignore its BLE cache!
#define CHIP_DEVICE_CONFIG_USE_TEST_SETUP_DISCRIMINATOR 3841

// Minimal or no overrides to avoid Certification Declaration (CD) mismatches:
#define CHIP_DEVICE_CONFIG_DEVICE_PRODUCT_NAME "MAX-Bridge"
