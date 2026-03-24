#pragma once

// Use the default test setup PIN of esp-matter: 20202021
// And default Discriminator: 3840
#define CHIP_DEVICE_CONFIG_USE_TEST_SETUP_DISCRIMINATOR 3840

// Minimal or no overrides to avoid Certification Declaration (CD) mismatches:
#define CHIP_DEVICE_CONFIG_DEVICE_PRODUCT_NAME "MAX-Bridge"
