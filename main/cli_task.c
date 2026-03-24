#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_console.h"
#include "esp_log.h"
#include "cli_task.h"
#include "max_nvs.h"

static const char *TAG = "CLI";

static int do_ping_cmd(int argc, char **argv) {
    ESP_LOGI(TAG, "PONG!");
    return 0;
}

static int do_max_cmd(int argc, char **argv)
{
    if (argc < 2) {
        ESP_LOGE(TAG, "Usage: max <baseid|add|list|clear>");
        return 1;
    }

    const char *subcmd = argv[1];

    if (strcmp(subcmd, "baseid") == 0) {
        if (argc < 3) {
            ESP_LOGE(TAG, "Usage: max baseid <hex_id>");
            return 1;
        }
        uint32_t base_id = (uint32_t)strtoul(argv[2], NULL, 16);
        esp_err_t err = max_nvs_set_base_id(base_id);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Base ID set to 0x%06lX", base_id);
        } else {
            ESP_LOGE(TAG, "Error setting Base ID: %d", err);
        }
    } 
    else if (strcmp(subcmd, "add") == 0) {
        if (argc < 4) {
            ESP_LOGE(TAG, "Usage: max add <hex_addr> <int_type>");
            return 1;
        }
        uint32_t addr = (uint32_t)strtoul(argv[2], NULL, 16);
        uint8_t type = (uint8_t)atoi(argv[3]);
        esp_err_t err = max_nvs_add_device(addr, type);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Device added: Addr=0x%06lX, Type=%d", addr, type);
        } else {
            ESP_LOGE(TAG, "Error adding device: %d", err);
        }
    } 
    else if (strcmp(subcmd, "list") == 0) {
        uint32_t base_id = 0;
        max_nvs_get_base_id(&base_id);
        ESP_LOGI(TAG, "--- MAX Bridge Info ---");
        ESP_LOGI(TAG, "Base ID: 0x%06lX", base_id);

        max_device_t devices[50]; 
        size_t count = 50;
        esp_err_t err = max_nvs_get_devices(devices, &count);
        
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Device count: %d", (int)count);
            for (size_t i = 0; i < count; i++) {
                ESP_LOGI(TAG, "  [%d] Addr: 0x%06lX, Type: %d, Endpoint: %d", (int)i, (unsigned long)devices[i].address, devices[i].type, devices[i].endpoint_id);
            }
        } else {
            ESP_LOGE(TAG, "Error reading devices: %d", err);
        }
    } 
    else if (strcmp(subcmd, "clear") == 0) {
        esp_err_t err = max_nvs_clear_devices();
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Device list cleared.");
        } else {
            ESP_LOGE(TAG, "Error clearing devices: %d", err);
        }
    } 
    else {
        ESP_LOGE(TAG, "Unknown subcommand: %s", subcmd);
        return 1;
    }

    return 0;
}

void cli_task_init(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "max-bridge> ";
    repl_config.max_history_len = 10;

#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));
#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    esp_console_dev_usb_serial_jtag_config_t usbjtag_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&usbjtag_config, &repl_config, &repl));
#else
#error "No console configuration specified"
#endif

    const esp_console_cmd_t max_cmd = {
        .command = "max",
        .help = "MAX! Bridge commands (baseid, add, list, clear)",
        .hint = NULL,
        .func = &do_max_cmd,
        .argtable = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&max_cmd));

    const esp_console_cmd_t ping_cmd = {
        .command = "ping",
        .help = "Ping the bridge",
        .hint = NULL,
        .func = &do_ping_cmd,
        .argtable = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&ping_cmd));

    ESP_LOGI(TAG, "CLI initialized.");
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
