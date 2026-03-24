#pragma once
#include <stdint.h>
#include <stddef.h>
#include <esp_err.h>
#include "matter_bridge.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t max_nvs_init(void);
esp_err_t max_nvs_set_base_id(uint32_t base_id);
esp_err_t max_nvs_get_base_id(uint32_t *base_id);
esp_err_t max_nvs_add_device(uint32_t address, uint8_t type);
esp_err_t max_nvs_get_devices(max_device_t *devices, size_t *count);
esp_err_t max_nvs_clear_devices(void);
esp_err_t max_nvs_update_device(max_device_t *device);

max_device_t* max_nvs_get_device(uint32_t address);
max_device_t* max_nvs_get_device_by_ep(uint16_t endpoint_id);

#ifdef __cplusplus
}
#endif
