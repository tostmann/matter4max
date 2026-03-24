#ifndef RADIO_HAL_H
#define RADIO_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Max length for a MAX! packet
#define MAX_PACKET_LEN 64

// Hardware callback type for interrupts (e.g., RX finished / Sync word received)
typedef void (*radio_isr_callback_t)(void *arg);

// Radio Hardware Abstraction Layer interface
typedef struct {
    // Basic lifecycle
    bool (*init)(void);
    void (*deinit)(void);

    // TX / RX operations
    bool (*transmit)(const uint8_t *data, uint8_t len);
    bool (*receive)(uint8_t *data, uint8_t *len);
    
    // State control
    void (*set_rx_mode)(void);
    void (*set_idle_mode)(void);
    
    // Callbacks
    void (*register_rx_callback)(radio_isr_callback_t cb, void *arg);

    // Power and link control (Future use/duty cycle)
    void (*set_tx_power)(int8_t dbm);
    int8_t (*get_rssi)(void);

} radio_hal_t;

// Externally available HAL instance for CC1101
extern const radio_hal_t cc1101_hal;

#endif // RADIO_HAL_H
