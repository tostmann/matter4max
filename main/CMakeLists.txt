idf_component_register(
                    add_definitions(-DCHIP_DEVICE_CONFIG_DEVICE_VENDOR_NAME="busware.de")
SRCS "main.c" "cc1101_drv.c" "matter_bridge.cpp" "max_nvs.c" "cli_task.c"
                    INCLUDE_DIRS "."
                    REQUIRES nvs_flash freertos driver spi_flash esp_timer console)
