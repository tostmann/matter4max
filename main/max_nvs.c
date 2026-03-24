#include "max_nvs.h"
#include <nvs.h>
#include <nvs_flash.h>
#include <esp_log.h>
#include <string.h>
#include <stdio.h>

static const char *TAG = "MAX_NVS";
static const char *NVS_NAMESPACE = "max_config";
static const char *KEY_BASE_ID = "base_id";
static const char *KEY_DEV_COUNT = "dev_count";
static const char *KEY_DEV_PREFIX = "dev_";

static max_device_t g_device_cache[MAX_DEVICES_LIMIT];
static size_t g_device_cache_count = 0;
static bool g_device_cache_valid = false;

static void invalidate_cache() {
    g_device_cache_valid = false;
}

static void reload_cache_if_needed() {
    if (!g_device_cache_valid) {
        g_device_cache_count = MAX_DEVICES_LIMIT;
        if (max_nvs_get_devices(g_device_cache, &g_device_cache_count) == ESP_OK) {
            g_device_cache_valid = true;
        }
    }
}

max_device_t* max_nvs_get_device(uint32_t address) {
    reload_cache_if_needed();
    for (size_t i = 0; i < g_device_cache_count; i++) {
        if (g_device_cache[i].address == address) {
            return &g_device_cache[i];
        }
    }
    return NULL;
}

max_device_t* max_nvs_get_device_by_ep(uint16_t endpoint_id) {
    if (endpoint_id == 0) return NULL;
    reload_cache_if_needed();
    for (size_t i = 0; i < g_device_cache_count; i++) {
        if (g_device_cache[i].endpoint_id == endpoint_id) {
            return &g_device_cache[i];
        }
    }
    return NULL;
}

esp_err_t max_nvs_init(void) {
    return ESP_OK;
}

esp_err_t max_nvs_set_base_id(uint32_t base_id) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    err = nvs_set_u32(handle, KEY_BASE_ID, base_id);
    if (err == ESP_OK) {
        nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

esp_err_t max_nvs_get_base_id(uint32_t *base_id) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return err;
    err = nvs_get_u32(handle, KEY_BASE_ID, base_id);
    nvs_close(handle);
    return err;
}

bool max_device_exists(uint32_t address) {
    max_device_t devs[MAX_DEVICES_LIMIT];
    size_t count = MAX_DEVICES_LIMIT;
    if (max_nvs_get_devices(devs, &count) == ESP_OK) {
        for (size_t i = 0; i < count; i++) {
            if (devs[i].address == address) return true;
        }
    }
    return false;
}

esp_err_t max_nvs_add_device(uint32_t address, uint8_t type) {
    if (max_device_exists(address)) return ESP_ERR_INVALID_STATE; // Already exists

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    
    uint32_t count = 0;
    nvs_get_u32(handle, KEY_DEV_COUNT, &count);
    
    if (count >= MAX_DEVICES_LIMIT) {
        nvs_close(handle);
        return ESP_ERR_NO_MEM;
    }
    
    char key[16];
    snprintf(key, sizeof(key), "%s%lu", KEY_DEV_PREFIX, (unsigned long)count);
    
    max_device_t dev = { .address = address, .type = type, .endpoint_id = 0 };
    err = nvs_set_blob(handle, key, &dev, sizeof(max_device_t));
    if (err == ESP_OK) {
        count++;
        nvs_set_u32(handle, KEY_DEV_COUNT, count);
        nvs_commit(handle);
        invalidate_cache();
    }
    nvs_close(handle);
    return err;
}

esp_err_t max_nvs_update_device(max_device_t *device) {
    if (!device) return ESP_ERR_INVALID_ARG;
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    
    uint32_t count = 0;
    nvs_get_u32(handle, KEY_DEV_COUNT, &count);
    
    for (uint32_t i = 0; i < count; i++) {
        char key[16];
        snprintf(key, sizeof(key), "%s%lu", KEY_DEV_PREFIX, (unsigned long)i);
        max_device_t tmp;
        size_t len = sizeof(tmp);
        if (nvs_get_blob(handle, key, &tmp, &len) == ESP_OK && tmp.address == device->address) {
            err = nvs_set_blob(handle, key, device, sizeof(max_device_t));
            if (err == ESP_OK) {
                nvs_commit(handle);
                invalidate_cache();
            }
            break;
        }
    }
    nvs_close(handle);
    return err;
}

esp_err_t max_nvs_get_devices(max_device_t *devices, size_t *count) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        *count = 0;
        return err;
    }
    
    uint32_t stored_count = 0;
    nvs_get_u32(handle, KEY_DEV_COUNT, &stored_count);
    
    size_t actual_count = 0;
    for (uint32_t i = 0; i < stored_count && i < *count; i++) {
        char key[16];
        snprintf(key, sizeof(key), "%s%lu", KEY_DEV_PREFIX, (unsigned long)i);
        size_t len = sizeof(max_device_t);
        if (nvs_get_blob(handle, key, &devices[actual_count], &len) == ESP_OK) {
            actual_count++;
        }
    }
    *count = actual_count;
    nvs_close(handle);
    return ESP_OK;
}

esp_err_t max_nvs_clear_devices(void) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    
    uint32_t count = 0;
    if (nvs_get_u32(handle, KEY_DEV_COUNT, &count) == ESP_OK) {
        for (uint32_t i = 0; i < count; i++) {
            char key[16];
            snprintf(key, sizeof(key), "%s%lu", KEY_DEV_PREFIX, (unsigned long)i);
            nvs_erase_key(handle, key);
        }
    }
    nvs_set_u32(handle, KEY_DEV_COUNT, 0);
    nvs_commit(handle);
    nvs_close(handle);
    return ESP_OK;
}
