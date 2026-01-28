

#ifndef MICROPY_INCLUDED_STM32_NIMBLE_NIMBLE_NIMBLE_NPL_OS_H
#define MICROPY_INCLUDED_STM32_NIMBLE_NIMBLE_NIMBLE_NPL_OS_H



#include <stdint.h>
#include <limits.h>




#if __WORDSIZE == 64
#define BLE_NPL_OS_ALIGNMENT 8
#else
#define BLE_NPL_OS_ALIGNMENT 4
#endif
#define BLE_NPL_TIME_FOREVER (0xffffffff)




#define OS_CFG_ALIGN_4 (4)
#define OS_CFG_ALIGN_8 (8)
#if (ULONG_MAX == 0xffffffffffffffff)
#define OS_CFG_ALIGNMENT (OS_CFG_ALIGN_8)
#else
#define OS_CFG_ALIGNMENT (OS_CFG_ALIGN_4)
#endif

typedef uint32_t ble_npl_time_t;
typedef int32_t ble_npl_stime_t;

struct ble_npl_event {
    ble_npl_event_fn *fn;
    void *arg;
    bool pending;
    struct ble_npl_event *prev;
    struct ble_npl_event *next;
};

struct ble_npl_eventq {
    struct ble_npl_event *head;
    struct ble_npl_eventq *nextq;
};

struct ble_npl_callout {
    bool active;
    uint32_t ticks;
    struct ble_npl_eventq *evq;
    struct ble_npl_event ev;
    struct ble_npl_callout *nextc;
};

struct ble_npl_mutex {
    volatile uint8_t locked;
};

struct ble_npl_sem {
    volatile uint16_t count;
};



void mp_bluetooth_nimble_os_eventq_run_all(void);
void mp_bluetooth_nimble_os_callout_process(void);



void mp_bluetooth_nimble_hci_uart_wfi(void);

#endif 
