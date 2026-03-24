#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MAX_DEV_TYPE_CUBE = 0x00,
    MAX_DEV_TYPE_THERMOSTAT = 0x01,
    MAX_DEV_TYPE_THERMOSTAT_PLUS = 0x02,
    MAX_DEV_TYPE_WALL_THERMOSTAT = 0x03,
    MAX_DEV_TYPE_SHUTTER_CONTACT = 0x04,
    MAX_DEV_TYPE_PUSH_BUTTON = 0x05,
    MAX_DEV_TYPE_VIRTUAL_SHUTTER = 0x06,
    MAX_DEV_TYPE_VIRTUAL_THERMOSTAT = 0x07,
    MAX_DEV_TYPE_PLUG_ADAPTER = 0x08
} max_device_type_t;

typedef struct {
    uint32_t address;      // 24-bit MAX! RF Address
    uint8_t type;          // max_device_type_t
    uint16_t endpoint_id;  // Matter Endpoint ID (Thermostat or Contact)
} max_device_t;

#define MAX_DEVICES_LIMIT 50

void matter_bridge_init(void);
void matter_bridge_start(void);
bool add_max_device_to_matter(max_device_t *device);

// Global state for auto-discovery
bool get_auto_discovery_state(void);

#ifdef __cplusplus
}
#endif
