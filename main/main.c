#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "radio_hal.h"
#include "wifi-conf.h"
#include "matter_bridge.h"
#include "max_nvs.h"

static const char *TAG = "MAX_MATTER_MAIN";

// FreeRTOS Queues
static QueueHandle_t rf_tx_queue;
static QueueHandle_t rf_rx_queue;

// Data structures for Inter-Task Communication
typedef struct {
    uint8_t length;
    uint8_t payload[MAX_PACKET_LEN];
} rf_packet_t;

// NVS Settings Keys
#define NVS_NAMESPACE "max_config"
#define NVS_KEY_BASE_ID "base_id"

static uint32_t current_base_id = 0x000000; // 24-bit

// --- NVS & Migration logic ---
static void load_nvs_settings(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS: %s", esp_err_to_name(err));
        return;
    }

    err = nvs_get_u32(nvs_handle, NVS_KEY_BASE_ID, &current_base_id);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        // Migration trick: Not found, inject a default or provide a CLI to set it later
        ESP_LOGI(TAG, "Base ID not found in NVS. Setting default (0x123456).");
        current_base_id = 0x123456; // Example default
        nvs_set_u32(nvs_handle, NVS_KEY_BASE_ID, current_base_id);
        nvs_commit(nvs_handle);
    } else if (err == ESP_OK) {
        ESP_LOGI(TAG, "Loaded Base ID from NVS: 0x%06X", (unsigned int)current_base_id);
    }

    nvs_close(nvs_handle);
}

// ISR Callback triggered by Radio HAL
static void radio_rx_isr(void *arg) {
    // Send a minimal notification to RF task to read the payload
    // Not directly reading SPI in ISR
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint8_t dummy = 1; 
    xQueueSendFromISR(rf_rx_queue, &dummy, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

// --- Tasks ---

static void rf_task(void *pvParameters) {
    ESP_LOGI(TAG, "RF Task started");

    // Initialize Radio HAL (CC1101)
    if (!cc1101_hal.init()) {
        ESP_LOGE(TAG, "Radio init failed! Suspending RF Task.");
        vTaskSuspend(NULL);
    }

    cc1101_hal.register_rx_callback(radio_rx_isr, NULL);
    cc1101_hal.set_rx_mode();

    rf_packet_t tx_pkt;
    uint8_t dummy_rx_signal;

    uint32_t duty_cycle_budget_ms = 36000;
    TickType_t last_refill_tick = xTaskGetTickCount();

    while (1) {
        TickType_t current_tick = xTaskGetTickCount();
        uint32_t elapsed_ms = pdTICKS_TO_MS(current_tick - last_refill_tick);
        if (elapsed_ms >= 100) { // Every 100ms
            duty_cycle_budget_ms += (elapsed_ms / 100); // 1% of 100ms = 1ms
            if (duty_cycle_budget_ms > 36000) {
                duty_cycle_budget_ms = 36000;
            }
            last_refill_tick = current_tick;
        }

        // Check for incoming packets (ISR signaled via GDO2)
        if (xQueueReceive(rf_rx_queue, &dummy_rx_signal, pdMS_TO_TICKS(100)) == pdTRUE) {
            rf_packet_t rx_pkt;
            // The ISR only triggers if CRC is OK and packet is fully in FIFO
            if (cc1101_hal.receive(rx_pkt.payload, &rx_pkt.length)) {
                if (rx_pkt.length > 0) {
                    // MAX! Protocol Validation & Parsing
                    if (rx_pkt.length >= 12) { // Minimum MAX! packet: Len(1) + Payload(10) + RSSI(1) + LQI(1)
                        uint8_t payload_len = rx_pkt.payload[0];
                        if (payload_len >= 10 && payload_len + 3 <= rx_pkt.length) { // Len byte doesn't include itself, but does include the 10 bytes header
                            uint8_t crc_lqi = rx_pkt.payload[payload_len + 2];
                            bool crc_ok = (crc_lqi & 0x80) != 0;
                            
                            if (crc_ok) {
                                char hex_str[256] = {0};
                                for(int i = 0; i < rx_pkt.length && i < sizeof(rx_pkt.payload); i++) {
                                    sprintf(&hex_str[i*2], "%02X", rx_pkt.payload[i]);
                                }
                                
                                uint8_t msg_cnt = rx_pkt.payload[1];
                                uint8_t flags   = rx_pkt.payload[2];
                                uint8_t cmd     = rx_pkt.payload[3];
                                uint32_t src_id = (rx_pkt.payload[4] << 16) | (rx_pkt.payload[5] << 8) | rx_pkt.payload[6];
                                uint32_t dst_id = (rx_pkt.payload[7] << 16) | (rx_pkt.payload[8] << 8) | rx_pkt.payload[9];
                                uint8_t group_id= rx_pkt.payload[10];
                                
                                ESP_LOGI(TAG, "MAX! RX [CRC OK] -> Cmd: 0x%02X, Src: %06X, Dst: %06X | Hex: %s", 
                                         cmd, (unsigned int)src_id, (unsigned int)dst_id, hex_str);
                                         
                                // Auto-Discovery / Auto-Provisioning
                                if (get_auto_discovery_state()) {
                                    uint8_t dev_type = MAX_DEV_TYPE_THERMOSTAT; // Default fallback type for new devices
                                    if (cmd == 0x60 || cmd == 0x42 || cmd == 0x70 || cmd == 0x40 || cmd == 0x44 || cmd == 0x50 || cmd == 0x03) dev_type = MAX_DEV_TYPE_THERMOSTAT;
                                    else if (cmd == 0x30) dev_type = MAX_DEV_TYPE_SHUTTER_CONTACT;
                                    // We are dropping PUSH_BUTTON for cmd=0x40, as 0x40 is often SetTemperature from a Thermostat!
                                    // So we just assume THERMOSTAT for almost everything unknown to guarantee it gets created.
                                    
                                    // Avoid registering broadcast or dummy IDs
                                    if (src_id != 0x000000 && src_id != 0xFFFFFF) {
                                        esp_err_t add_err = max_nvs_add_device(src_id, dev_type);
                                        if (add_err == ESP_OK) {
                                            ESP_LOGI(TAG, "Auto-Discovery: New device 0x%06X added to NVS (Type %d)!", (unsigned int)src_id, dev_type);
                                            max_device_t new_dev = { .address = src_id, .type = dev_type, .endpoint_id = 0 };
                                            if (add_max_device_to_matter(&new_dev)) {
                                                max_nvs_update_device(&new_dev);
                                                ESP_LOGI(TAG, "Auto-Discovery: Matter Endpoint %d created for 0x%06X", new_dev.endpoint_id, (unsigned int)src_id);
                                            }
                                        }
                                    }
                                }

                                extern void update_matter_thermostat(uint32_t address, float actual_temp, float setpoint_temp, uint8_t max_mode);
                                extern void update_matter_contact(uint32_t address, bool is_open);

                                if (cmd == 0x60 && payload_len >= 13) {
                                    // 0x60: ThermostatState (HeatingThermostat)
                                    uint8_t bits2 = rx_pkt.payload[11];
                                    uint8_t desired = rx_pkt.payload[13] & 0x3F;
                                    float target_temp = desired / 2.0f;
                                    float actual_temp = -999.0f; // No fallback! Most heating thermostats don't send actual temperature.
                                    
                                    uint8_t mode = bits2 & 0x03;
                                    if (mode != 2 && payload_len >= 15) {
                                        uint16_t measured = ((rx_pkt.payload[14] & 0x01) << 8) | rx_pkt.payload[15];
                                        if (measured != 0) {
                                            actual_temp = measured / 10.0f;
                                        }
                                    }
                                    update_matter_thermostat(src_id, actual_temp, target_temp, mode);
                                }
                                else if (cmd == 0x42 && payload_len >= 12) {
                                    // 0x42: WallThermostatControl (WallThermostat)
                                    uint8_t desired_raw = rx_pkt.payload[11];
                                    uint8_t temp_raw = rx_pkt.payload[12];
                                    
                                    uint8_t mode = (desired_raw & 0xC0) >> 6;
                                    float target_temp = (desired_raw & 0x3F) / 2.0f;
                                    float actual_temp = temp_raw / 10.0f; // No MSB in 0x42
                                    update_matter_thermostat(src_id, actual_temp, target_temp, mode);
                                }
                                else if (cmd == 0x70 && payload_len >= 15) {
                                    // 0x70: WallThermostatState
                                    uint8_t desired_raw = rx_pkt.payload[13];
                                    uint8_t temp_raw = rx_pkt.payload[15];
                                    
                                    uint8_t mode = (desired_raw & 0xC0) >> 6;
                                    float target_temp = (desired_raw & 0x3F) / 2.0f;
                                    float actual_temp = (((desired_raw & 0x80) << 1) + temp_raw) / 10.0f;
                                    update_matter_thermostat(src_id, actual_temp, target_temp, mode);
                                }
                                else if (cmd == 0x30 && payload_len >= 11) {
                                    // 0x30: ShutterContactState (Window Sensor)
                                    uint8_t bits2 = rx_pkt.payload[11];
                                    bool is_open = (bits2 & 0x02) != 0;
                                    update_matter_contact(src_id, is_open);
                                }
                                else if (cmd == 0x40 && payload_len >= 11) {
                                    // 0x40: SetTemperature (from WallThermostat, Cube or PushButton/Eco-Taster)
                                    // Payload byte is at index 11
                                    uint8_t bits = rx_pkt.payload[11];
                                    uint8_t mode = (bits & 0xC0) >> 6;
                                    float target_temp = (bits & 0x3F) / 2.0f;
                                    
                                    // We don't have the actual temp in this packet, so we pass -100.0f
                                    // to indicate it hasn't changed / shouldn't be updated.
                                    update_matter_thermostat(src_id, -100.0f, target_temp, mode);
                                }
                            }
                        }
                    }
                }
                cc1101_hal.set_rx_mode(); // Re-enter RX
            }
        } else {
            // Periodically check if MARCSTATE is stuck in RX FIFO Overflow (0x11)
            // If the chip was jammed by a malformed infinite packet, our HAL auto-flushes it.
            uint8_t dummy_len;
            uint8_t dummy_buf[MAX_PACKET_LEN];
            cc1101_hal.receive(dummy_buf, &dummy_len); 
        }

        // Check for outgoing packets (Matter task signaled)
        if (xQueueReceive(rf_tx_queue, &tx_pkt, 0) == pdTRUE) {
            if (duty_cycle_budget_ms >= 1100) {
                duty_cycle_budget_ms -= 1100;
                ESP_LOGI(TAG, "RF TX: %d bytes (Budget remaining: %lu ms)", tx_pkt.length, duty_cycle_budget_ms);
                cc1101_hal.transmit(tx_pkt.payload, tx_pkt.length);
            } else {
                ESP_LOGW(TAG, "TX Dropped: 1%% Duty Cycle Limit reached! (Budget: %lu ms)", duty_cycle_budget_ms);
            }
            cc1101_hal.set_rx_mode(); // Always ensure we are back in RX mode
        }
    }
}

uint8_t g_tx_msg_count = 0;

void send_max_set_temperature(uint32_t target_address, float setpoint_temp, uint8_t max_mode) {
    rf_packet_t tx_pkt;
    tx_pkt.length = 12; // 1 byte len + 11 bytes payload
    tx_pkt.payload[0] = 11; // Length of following bytes
    tx_pkt.payload[1] = g_tx_msg_count++; // Msg count
    tx_pkt.payload[2] = 0x00; // Flags (0x00 for unicast/no group)
    tx_pkt.payload[3] = 0x40; // Cmd: SetTemperature
    
    uint32_t base_id = 0;
    max_nvs_get_base_id(&base_id);
    tx_pkt.payload[4] = (base_id >> 16) & 0xFF;
    tx_pkt.payload[5] = (base_id >> 8) & 0xFF;
    tx_pkt.payload[6] = base_id & 0xFF;
    
    tx_pkt.payload[7] = (target_address >> 16) & 0xFF;
    tx_pkt.payload[8] = (target_address >> 8) & 0xFF;
    tx_pkt.payload[9] = target_address & 0xFF;
    
    tx_pkt.payload[10] = 0x00; // Group ID
    
    // temp_val is bits 0-5 (*2). max_mode has mode in bits 6-7 (e.g. 0x00, 0x40, 0x80, 0xC0)
    uint8_t temp_val = (uint8_t)(setpoint_temp * 2.0f);
    if (temp_val > 63) temp_val = 63; // Max 31.5C (6 bits max)
    tx_pkt.payload[11] = temp_val | max_mode; 
    
    xQueueSend(rf_tx_queue, &tx_pkt, 0);
    ESP_LOGI(TAG, "Queued SetTemperature: 0x%06X -> %.1fC (Mode: 0x%02X)", (unsigned int)target_address, setpoint_temp, max_mode);
}

static void matter_bridge_task(void *pvParameters) {
    ESP_LOGI(TAG, "Matter Bridge Task started");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void cli_task_init(void);
#include "max_nvs.h"
void app_main(void) {
    ESP_LOGI(TAG, "Starting MAX! to Matter Bridge...");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    load_nvs_settings();

    // Init CLI
    max_nvs_init();
    cli_task_init();

    // Init Matter Bridge
    matter_bridge_init();

    // Load devices from NVS and add to Matter
    size_t count = 50;
    max_device_t devices[50];
    if (max_nvs_get_devices(devices, &count) == ESP_OK) {
        ESP_LOGI(TAG, "Loaded %d devices from NVS", (int)count);
        for (size_t i = 0; i < count; i++) {
            if (add_max_device_to_matter(&devices[i])) {
                // Device successfully added
                max_nvs_update_device(&devices[i]);
                ESP_LOGI(TAG, "Successfully added device 0x%06lX to Matter (EP %d)", (unsigned long)devices[i].address, devices[i].endpoint_id);
            } else {
                ESP_LOGE(TAG, "Failed to add device 0x%06lX to Matter", (unsigned long)devices[i].address);
            }
        }
    }

    // Start Matter stack now that all endpoints are created
    matter_bridge_start();

    // Create Queues
    rf_tx_queue = xQueueCreate(10, sizeof(rf_packet_t));
    rf_rx_queue = xQueueCreate(10, sizeof(uint8_t)); // Just a trigger queue

    // Create Tasks
    xTaskCreate(rf_task, "rf_task", 4096, NULL, 10, NULL);
    xTaskCreate(matter_bridge_task, "matter_task", 4096, NULL, 5, NULL);
}
