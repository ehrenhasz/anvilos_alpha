 

#include "py/runtime.h"
#include "py/mperrno.h"
#include "py/mphal.h"

#if MICROPY_PY_BLUETOOTH && MICROPY_BLUETOOTH_BTSTACK

#include "lib/btstack/src/btstack.h"

#include "extmod/mpbthci.h"
#include "extmod/btstack/btstack_hci_uart.h"

#include "mpbthciport.h"
#include "mpbtstackport.h"

#define HCI_TRACE (0)
#define COL_OFF "\033[0m"
#define COL_GREEN "\033[0;32m"
#define COL_BLUE "\033[0;34m"





static bool send_done;
static void (*send_handler)(void);


static uint8_t *recv_buf;
static size_t recv_len;
static size_t recv_idx;
static void (*recv_handler)(void);
static bool init_success = false;

static int btstack_uart_init(const btstack_uart_config_t *uart_config) {
    (void)uart_config;

    send_done = false;
    recv_len = 0;
    recv_idx = 0;
    recv_handler = NULL;
    send_handler = NULL;

    
    if (mp_bluetooth_hci_uart_init(MICROPY_HW_BLE_UART_ID, MICROPY_HW_BLE_UART_BAUDRATE)) {
        init_success = false;
        return -1;
    }
    if (mp_bluetooth_hci_controller_init()) {
        init_success = false;
        return -1;
    }

    init_success = true;
    return 0;
}

static int btstack_uart_open(void) {
    return init_success ? 0 : 1;
}

static int btstack_uart_close(void) {
    mp_bluetooth_hci_controller_deinit();
    mp_bluetooth_hci_uart_deinit();
    return 0;
}

static void btstack_uart_set_block_received(void (*block_handler)(void)) {
    recv_handler = block_handler;
}

static void btstack_uart_set_block_sent(void (*block_handler)(void)) {
    send_handler = block_handler;
}

static int btstack_uart_set_baudrate(uint32_t baudrate) {
    mp_bluetooth_hci_uart_set_baudrate(baudrate);
    return 0;
}

static int btstack_uart_set_parity(int parity) {
    (void)parity;
    return 0;
}

static int btstack_uart_set_flowcontrol(int flowcontrol) {
    (void)flowcontrol;
    return 0;
}

static void btstack_uart_receive_block(uint8_t *buf, uint16_t len) {
    recv_buf = buf;
    recv_len = len;
}

static void btstack_uart_send_block(const uint8_t *buf, uint16_t len) {
    #if HCI_TRACE
    printf(COL_GREEN "< [% 8d] %02x", (int)mp_hal_ticks_ms(), buf[0]);
    for (size_t i = 1; i < len; ++i) {
        printf(":%02x", buf[i]);
    }
    printf(COL_OFF "\n");
    #endif

    mp_bluetooth_hci_uart_write(buf, len);
    send_done = true;

    
    
    mp_bluetooth_hci_poll_now();
}

static int btstack_uart_get_supported_sleep_modes(void) {
    return 0;
}

static void btstack_uart_set_sleep(btstack_uart_sleep_mode_t sleep_mode) {
    (void)sleep_mode;
    
}

static void btstack_uart_set_wakeup_handler(void (*wakeup_handler)(void)) {
    (void)wakeup_handler;
    
}

const btstack_uart_block_t mp_bluetooth_btstack_hci_uart_block = {
    &btstack_uart_init,
    &btstack_uart_open,
    &btstack_uart_close,
    &btstack_uart_set_block_received,
    &btstack_uart_set_block_sent,
    &btstack_uart_set_baudrate,
    &btstack_uart_set_parity,
    &btstack_uart_set_flowcontrol,
    &btstack_uart_receive_block,
    &btstack_uart_send_block,
    &btstack_uart_get_supported_sleep_modes,
    &btstack_uart_set_sleep,
    &btstack_uart_set_wakeup_handler,

    
    NULL, 
    NULL, 
    NULL, 
    NULL, 
};

void mp_bluetooth_btstack_hci_uart_process(void) {
    bool host_wake = mp_bluetooth_hci_controller_woken();

    if (send_done) {
        
        send_done = false;
        if (send_handler) {
            send_handler();
        }
    }

    
    
    int chr;
    while (recv_idx < recv_len && (chr = mp_bluetooth_hci_uart_readchar()) >= 0) {
        recv_buf[recv_idx++] = chr;
        if (recv_idx == recv_len) {
            #if HCI_TRACE
            printf(COL_BLUE "> [% 8d] %02x", (int)mp_hal_ticks_ms(), recv_buf[0]);
            for (size_t i = 1; i < recv_len; ++i) {
                printf(":%02x", recv_buf[i]);
            }
            printf(COL_OFF "\n");
            #endif
            recv_idx = 0;
            recv_len = 0;
            if (recv_handler) {
                recv_handler();
            }
        }
    }

    if (host_wake) {
        mp_bluetooth_hci_controller_sleep_maybe();
    }
}

#endif 
