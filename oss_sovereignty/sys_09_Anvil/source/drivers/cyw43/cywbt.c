 

#include <stdio.h>
#include <string.h>

#include "py/runtime.h"
#include "py/mphal.h"
#include "extmod/mpbthci.h"

#if MICROPY_PY_NETWORK_CYW43

#include "lib/cyw43-driver/src/cyw43_config.h"
#include "lib/cyw43-driver/firmware/cyw43_btfw_4343A1.h"


extern uint8_t mp_bluetooth_hci_cmd_buf[4 + 256];

 


#ifdef CYW43_PIN_BT_CTS


#include "pin_static_af.h"
#include "uart.h"


extern machine_uart_obj_t mp_bluetooth_hci_uart_obj;

static void cywbt_wait_cts_low(void) {
    mp_hal_pin_config(CYW43_PIN_BT_CTS, MP_HAL_PIN_MODE_INPUT, MP_HAL_PIN_PULL_UP, 0);
    for (int i = 0; i < 200; ++i) {
        if (mp_hal_pin_read(CYW43_PIN_BT_CTS) == 0) {
            break;
        }
        mp_hal_delay_ms(1);
    }
    mp_hal_pin_config_alt(CYW43_PIN_BT_CTS, MP_HAL_PIN_MODE_ALT,
        MP_HAL_PIN_PULL_UP, AF_FN_UART, mp_bluetooth_hci_uart_obj.uart_id);
}
#endif

static int cywbt_hci_cmd_raw(size_t len, uint8_t *buf) {
    mp_bluetooth_hci_uart_write((void *)buf, len);
    for (int c, i = 0; i < 6; ++i) {
        while ((c = mp_bluetooth_hci_uart_readchar()) == -1) {
            mp_event_wait_indefinite();
        }
        buf[i] = c;
    }

    
    if (buf[0] != 0x04 || buf[1] != 0x0e) {
        printf("unknown response: %02x %02x %02x %02x\n", buf[0], buf[1], buf[2], buf[3]);
        return -1;
    }

     

    int sz = buf[2] - 3;
    for (int c, i = 0; i < sz; ++i) {
        while ((c = mp_bluetooth_hci_uart_readchar()) == -1) {
            mp_event_wait_indefinite();
        }
        buf[i] = c;
    }

    return 0;
}

static int cywbt_hci_cmd(int ogf, int ocf, size_t param_len, const uint8_t *param_buf) {
    uint8_t *buf = mp_bluetooth_hci_cmd_buf;
    buf[0] = 0x01;
    buf[1] = ocf;
    buf[2] = ogf << 2 | ocf >> 8;
    buf[3] = param_len;
    if (param_len) {
        memcpy(buf + 4, param_buf, param_len);
    }
    return cywbt_hci_cmd_raw(4 + param_len, buf);
}

static void put_le16(uint8_t *buf, uint16_t val) {
    buf[0] = val;
    buf[1] = val >> 8;
}

static void put_le32(uint8_t *buf, uint32_t val) {
    buf[0] = val;
    buf[1] = val >> 8;
    buf[2] = val >> 16;
    buf[3] = val >> 24;
}

static int cywbt_set_baudrate(uint32_t baudrate) {
    uint8_t buf[6];
    put_le16(buf, 0);
    put_le32(buf + 2, baudrate);
    return cywbt_hci_cmd(0x3f, 0x18, 6, buf);
}


static int cywbt_download_firmware(const uint8_t *firmware) {
    cywbt_hci_cmd(0x3f, 0x2e, 0, NULL);

    bool last_packet = false;
    while (!last_packet) {
        uint8_t *buf = mp_bluetooth_hci_cmd_buf;
        memcpy(buf + 1, firmware, 3);
        firmware += 3;
        last_packet = buf[1] == 0x4e;
        if (buf[2] != 0xfc) {
            printf("fail1 %02x\n", buf[2]);
            break;
        }
        uint8_t len = buf[3];

        memcpy(buf + 4, firmware, len);
        firmware += len;

        buf[0] = 1;
        cywbt_hci_cmd_raw(4 + len, buf);
        if (buf[0] != 0) {
            printf("fail3 %02x\n", buf[0]);
            break;
        }
    }

    
    #if MICROPY_HW_ENABLE_RF_SWITCH
    mp_hal_pin_config(CYW43_PIN_WL_GPIO_1, MP_HAL_PIN_MODE_INPUT, MP_HAL_PIN_PULL_UP, 0);
    #endif
    mp_hal_delay_ms(10); 
    #ifdef CYW43_PIN_BT_CTS
    cywbt_wait_cts_low();
    #endif
    #if MICROPY_HW_ENABLE_RF_SWITCH
    
    mp_hal_pin_config(CYW43_PIN_WL_GPIO_1, MP_HAL_PIN_MODE_INPUT, MP_HAL_PIN_PULL_DOWN, 0);
    #endif

    mp_bluetooth_hci_uart_set_baudrate(115200);
    cywbt_set_baudrate(MICROPY_HW_BLE_UART_BAUDRATE_SECONDARY);
    mp_bluetooth_hci_uart_set_baudrate(MICROPY_HW_BLE_UART_BAUDRATE_SECONDARY);

    return 0;
}

int mp_bluetooth_hci_controller_init(void) {
    

    mp_hal_pin_output(CYW43_PIN_BT_REG_ON);
    mp_hal_pin_low(CYW43_PIN_BT_REG_ON);
    #ifdef CYW43_PIN_BT_HOST_WAKE
    mp_hal_pin_input(CYW43_PIN_BT_HOST_WAKE);
    #endif
    #ifdef CYW43_PIN_BT_DEV_WAKE
    mp_hal_pin_output(CYW43_PIN_BT_DEV_WAKE);
    mp_hal_pin_low(CYW43_PIN_BT_DEV_WAKE);
    #endif

    #if MICROPY_HW_ENABLE_RF_SWITCH
    
    mp_hal_pin_config(CYW43_PIN_WL_GPIO_4, MP_HAL_PIN_MODE_OUTPUT, MP_HAL_PIN_PULL_NONE, 0); 
    mp_hal_pin_high(CYW43_PIN_WL_GPIO_4); 
    #endif

    uint8_t buf[256];

    mp_hal_pin_low(CYW43_PIN_BT_REG_ON);
    mp_bluetooth_hci_uart_set_baudrate(115200);
    mp_hal_delay_ms(100);
    mp_hal_pin_high(CYW43_PIN_BT_REG_ON);
    #ifdef CYW43_PIN_BT_CTS
    cywbt_wait_cts_low();
    #else
    mp_hal_delay_ms(100);
    #endif

    
    cywbt_hci_cmd(0x03, 0x0003, 0, NULL);

    #ifdef MICROPY_HW_BLE_UART_BAUDRATE_DOWNLOAD_FIRMWARE
    
    cywbt_set_baudrate(MICROPY_HW_BLE_UART_BAUDRATE_DOWNLOAD_FIRMWARE);
    mp_bluetooth_hci_uart_set_baudrate(MICROPY_HW_BLE_UART_BAUDRATE_DOWNLOAD_FIRMWARE);
    #endif

    cywbt_download_firmware((const uint8_t *)&cyw43_btfw_4343A1[0]);

    
    cywbt_hci_cmd(0x03, 0x0003, 0, NULL);

    
    uint8_t bdaddr[6];
    mp_hal_get_mac(MP_HAL_MAC_BDADDR, bdaddr);
    buf[0] = bdaddr[5];
    buf[1] = bdaddr[4];
    buf[2] = bdaddr[3];
    buf[3] = bdaddr[2];
    buf[4] = bdaddr[1];
    buf[5] = bdaddr[0];
    cywbt_hci_cmd(0x3f, 0x0001, 6, buf);

    
    
    
    

    
    cywbt_hci_cmd(0x3f, 0x27, 12, (const uint8_t *)"\x01\x02\x02\x00\x00\x00\x01\x00\x00\x00\x00\x00");

    
    cywbt_hci_cmd(3, 109, 2, (const uint8_t *)"\x01\x00");

    #ifdef CYW43_PIN_BT_DEV_WAKE
    mp_hal_pin_high(CYW43_PIN_BT_DEV_WAKE); 
    #endif

    return 0;
}

int mp_bluetooth_hci_controller_deinit(void) {
    mp_hal_pin_low(CYW43_PIN_BT_REG_ON);

    return 0;
}

#ifdef CYW43_PIN_BT_DEV_WAKE
static uint32_t bt_sleep_ticks;
#endif

int mp_bluetooth_hci_controller_sleep_maybe(void) {
    #ifdef CYW43_PIN_BT_DEV_WAKE
    if (mp_hal_pin_read(CYW43_PIN_BT_DEV_WAKE) == 0) {
        if (mp_hal_ticks_ms() - bt_sleep_ticks > 500) {
            mp_hal_pin_high(CYW43_PIN_BT_DEV_WAKE); 
        }
    }
    #endif
    return 0;
}

bool mp_bluetooth_hci_controller_woken(void) {
    #ifdef CYW43_PIN_BT_HOST_WAKE
    bool host_wake = mp_hal_pin_read(CYW43_PIN_BT_HOST_WAKE);
     
    return host_wake;
    #else
    return true;
    #endif
}

int mp_bluetooth_hci_controller_wakeup(void) {
    #ifdef CYW43_PIN_BT_DEV_WAKE
    bt_sleep_ticks = mp_hal_ticks_ms();

    if (mp_hal_pin_read(CYW43_PIN_BT_DEV_WAKE) == 1) {
        mp_hal_pin_low(CYW43_PIN_BT_DEV_WAKE); 
        
        
        mp_hal_delay_us(5000); 
    }
    #endif

    return 0;
}

#endif
