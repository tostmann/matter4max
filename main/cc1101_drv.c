#include "radio_hal.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "CC1101_DRV";

// Hardware SPI Pin Definitions
#define CC1101_PIN_MISO    GPIO_NUM_17
#define CC1101_PIN_MOSI    GPIO_NUM_16
#define CC1101_PIN_SCK     GPIO_NUM_18
#define CC1101_PIN_CS      GPIO_NUM_23
#define CC1101_PIN_GDO0    GPIO_NUM_2
#define CC1101_PIN_GDO2    GPIO_NUM_1

// SPI Host Device
#define CC1101_SPI_HOST    SPI2_HOST

static spi_device_handle_t spi_handle;
static radio_isr_callback_t rx_isr_cb = NULL;
static void *rx_isr_arg = NULL;

// Basic SPI read/write macro
static uint8_t cc1101_spi_transfer(uint8_t data) {
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA;
    t.tx_data[0] = data;
    esp_err_t ret = spi_device_transmit(spi_handle, &t);
    if (ret != ESP_OK) return 0;
    return t.rx_data[0];
}

static void cc1101_write_reg(uint8_t addr, uint8_t value) {
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 16;
    t.flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA;
    t.tx_data[0] = addr; // No read bit for write
    t.tx_data[1] = value;
    spi_device_transmit(spi_handle, &t);
}

static void cc1101_write_burst(uint8_t addr, const uint8_t *buffer, uint8_t len) {
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = (len + 1) * 8;
    
    uint8_t *tx_buf = (uint8_t *)heap_caps_malloc(len + 1, MALLOC_CAP_DMA);
    tx_buf[0] = addr | 0x40; // Burst bit
    memcpy(&tx_buf[1], buffer, len);
    
    t.tx_buffer = tx_buf;
    spi_device_transmit(spi_handle, &t);
    free(tx_buf);
}

static uint8_t cc1101_read_reg(uint8_t addr) {
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 16;
    t.flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA;
    t.tx_data[0] = addr | 0x80; // Set read bit
    t.tx_data[1] = 0x00;
    spi_device_transmit(spi_handle, &t);
    return t.rx_data[1];
}

static void cc1101_strobe(uint8_t cmd) {
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA;
    t.tx_data[0] = cmd;
    spi_device_transmit(spi_handle, &t);
}

// ISR handler
static void IRAM_ATTR cc1101_isr_handler(void *arg) {
    // IOCFG2=0x07 goes HIGH when CRC OK, and goes LOW when FIFO is read.
    // If we trigger on ANYEDGE, we might get two interrupts per packet.
    // Let's just forward the interrupt and let the polling logic read RXBYTES.
    if (gpio_get_level(CC1101_PIN_GDO2) == 1) { // Only notify on rising edge (packet ready)
        if (rx_isr_cb) {
            rx_isr_cb(rx_isr_arg);
        }
    }
}

// HAL Interface Implementation
static bool hal_init(void) {
    esp_err_t ret;

    // 1. Init SPI bus
    spi_bus_config_t buscfg = {
        .miso_io_num = CC1101_PIN_MISO,
        .mosi_io_num = CC1101_PIN_MOSI,
        .sclk_io_num = CC1101_PIN_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 128
    };

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 5 * 1000 * 1000, // 5 MHz
        .mode = 0,                         // SPI mode 0
        .spics_io_num = CC1101_PIN_CS,
        .queue_size = 7,
    };

    ret = spi_bus_initialize(CC1101_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus");
        return false;
    }

    ret = spi_bus_add_device(CC1101_SPI_HOST, &devcfg, &spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device");
        return false;
    }

    // 2. Init GPIOs for interrupts (GDO0/GDO2)
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_POSEDGE, // Rising edge (0x07 asserts high on CRC OK)
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << CC1101_PIN_GDO0) | (1ULL << CC1101_PIN_GDO2),
        .pull_up_en = 0,
        .pull_down_en = 0
    };
    gpio_config(&io_conf);
    
    gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    // We will use GDO2 (Pin 1) for the packet interrupt
    gpio_isr_handler_add(CC1101_PIN_GDO2, cc1101_isr_handler, NULL);

    // CC1101 Register Reset
    cc1101_strobe(0x30); // SRES (Software reset)
    vTaskDelay(pdMS_TO_TICKS(10));

    // eQ-3 MAX! settings initialization (10kbaud, 868.3MHz, Moritz mode from culfw)
    const uint8_t max_moritz_config[][2] = {
        {0x00, 0x07}, // IOCFG2
        {0x02, 0x46}, // IOCFG0
        {0x04, 0xC6}, // SYNC1
        {0x05, 0x26}, // SYNC0
        {0x0B, 0x06}, // FSCTRL1
        {0x10, 0xC8}, // MDMCFG4
        {0x11, 0x93}, // MDMCFG3
        {0x12, 0x03}, // MDMCFG2
        {0x13, 0x22}, // MDMCFG1
        {0x15, 0x34}, // DEVIATN
        {0x17, 0x3F}, // MCSM1
        {0x18, 0x28}, // MCSM0
        {0x19, 0x16}, // FOCCFG
        {0x1B, 0x43}, // AGCTRL2
        {0x21, 0x56}, // FREND1
        {0x25, 0x00}, // FSCAL1
        {0x26, 0x11}, // FSCAL0
        {0x0D, 0x21}, // FREQ2
        {0x0E, 0x65}, // FREQ1
        {0x0F, 0x6A}, // FREQ0
        {0x07, 0x0C}, // PKTCTRL1
        {0x16, 0x07}, // MCSM2
        {0x20, 0xF8}, // WORCTRL
        {0x1E, 0x87}, // WOREVT1
        {0x1F, 0x6B}, // WOREVT0
        {0x29, 0x59}, // FSTEST
        {0x2C, 0x81}, // TEST2
        {0x2D, 0x35}  // TEST1
    };

    for (int i = 0; i < sizeof(max_moritz_config)/2; i++) {
        cc1101_write_reg(max_moritz_config[i][0], max_moritz_config[i][1]);
    }

    cc1101_strobe(0x33); // SCAL

    // Write PA Table for +10dBm max power (2-FSK only uses first index)
    cc1101_write_reg(0x3E, 0xC3); // PATABLE index 0 = 0xC3 (+10dBm from culfw)
    
    // Enable RX
    cc1101_strobe(0x34); // SRX


    ESP_LOGI(TAG, "CC1101 initialized via SPI with MAX! (Moritz) parameters");
    return true;
}

static void hal_deinit(void) {
    spi_bus_remove_device(spi_handle);
    spi_bus_free(CC1101_SPI_HOST);
    gpio_isr_handler_remove(CC1101_PIN_GDO2);
}

static bool hal_transmit(const uint8_t *data, uint8_t len) {
    if (len > MAX_PACKET_LEN) return false;
    
    // Strobe Idle first
    cc1101_strobe(0x36); // SIDLE
    cc1101_strobe(0x3B); // SFTX (Flush TX FIFO)
    
    // WAKE-ON-RADIO: MAX! Thermostats need a ~1 second preamble to wake up!
    // If we strobe STX while TX FIFO is empty, CC1101 will continuously transmit preamble.
    cc1101_strobe(0x35); // STX (Start Transmitting Preamble)
    
    // Wait 1 second to wake up sleeping thermostats
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Write burst to FIFO (Addr 0x3F) - CC1101 will immediately append Sync Word and Data
    cc1101_write_burst(0x3F, data, len);
    
    // Wait for TX to finish by polling MARCSTATE
    int timeout = 100;
    while(timeout > 0) {
        uint8_t state = cc1101_read_reg(0x35 | 0xC0) & 0x1F;
        if (state == 0x01 || state == 0x0D || state == 0x11 || state == 0x16) { // IDLE, RX, RXFIFO Overflow, or TXFIFO Underflow
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
        timeout--;
    }
    return true;
}

static bool hal_receive(uint8_t *data, uint8_t *len) {
    // Basic RX logic: read RXBYTES, read FIFO
    uint8_t marcstate = cc1101_read_reg(0x35 | 0xC0);
    uint8_t rxbytes = cc1101_read_reg(0x3B | 0xC0); // RXBYTES status reg
    
    // Removed verbose polling log
    
    if (marcstate == 0x11) { // RX FIFO Overflow
        ESP_LOGW(TAG, "RX FIFO Overflow! Flushing...");
        cc1101_strobe(0x3A); // SFRX
        cc1101_strobe(0x34); // SRX
        return false;
    }
    
    if (rxbytes & 0x7F) {
        uint8_t bytes_to_read = rxbytes & 0x7F;
        if (bytes_to_read > MAX_PACKET_LEN) bytes_to_read = MAX_PACKET_LEN;
        
        for(uint8_t i = 0; i < bytes_to_read; i++) {
             data[i] = cc1101_read_reg(0x3F | 0x80); // Read FIFO
        }
        *len = bytes_to_read;
        return true;
    }
    return false;
}

static void hal_set_rx_mode(void) {
    cc1101_strobe(0x34); // SRX
}

static void hal_set_idle_mode(void) {
    cc1101_strobe(0x36); // SIDLE
}

static void hal_register_rx_callback(radio_isr_callback_t cb, void *arg) {
    rx_isr_cb = cb;
    rx_isr_arg = arg;
}

static void hal_set_tx_power(int8_t dbm) {
    // TODO: Implement PA table logic
}

static int8_t hal_get_rssi(void) {
    // Read RSSI register and convert
    uint8_t rssi_dec = cc1101_read_reg(0x34 | 0xC0);
    // Rough estimation
    if (rssi_dec >= 128) {
        return (int8_t)((rssi_dec - 256) / 2 - 74);
    } else {
        return (int8_t)((rssi_dec) / 2 - 74);
    }
}

// Export the struct
const radio_hal_t cc1101_hal = {
    .init = hal_init,
    .deinit = hal_deinit,
    .transmit = hal_transmit,
    .receive = hal_receive,
    .set_rx_mode = hal_set_rx_mode,
    .set_idle_mode = hal_set_idle_mode,
    .register_rx_callback = hal_register_rx_callback,
    .set_tx_power = hal_set_tx_power,
    .get_rssi = hal_get_rssi
};
