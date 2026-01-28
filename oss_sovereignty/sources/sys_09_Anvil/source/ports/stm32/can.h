
#ifndef MICROPY_INCLUDED_STM32_CAN_H
#define MICROPY_INCLUDED_STM32_CAN_H

#include "py/obj.h"

#if MICROPY_HW_ENABLE_CAN

#define PYB_CAN_1 (1)
#define PYB_CAN_2 (2)
#define PYB_CAN_3 (3)

#define MASK16 (0)
#define LIST16 (1)
#define MASK32 (2)
#define LIST32 (3)

#if MICROPY_HW_ENABLE_FDCAN
#define CAN_TypeDef                 FDCAN_GlobalTypeDef
#define CAN_HandleTypeDef           FDCAN_HandleTypeDef
#define CanTxMsgTypeDef             FDCAN_TxHeaderTypeDef
#define CanRxMsgTypeDef             FDCAN_RxHeaderTypeDef
#endif

enum {
    CAN_STATE_STOPPED,
    CAN_STATE_ERROR_ACTIVE,
    CAN_STATE_ERROR_WARNING,
    CAN_STATE_ERROR_PASSIVE,
    CAN_STATE_BUS_OFF,
};

typedef enum _rx_state_t {
    RX_STATE_FIFO_EMPTY = 0,
    RX_STATE_MESSAGE_PENDING,
    RX_STATE_FIFO_FULL,
    RX_STATE_FIFO_OVERFLOW,
} rx_state_t;

typedef struct _pyb_can_obj_t {
    mp_obj_base_t base;
    mp_obj_t rxcallback0;
    mp_obj_t rxcallback1;
    mp_uint_t can_id : 8;
    bool is_enabled : 1;
    byte rx_state0;
    byte rx_state1;
    uint16_t num_error_warning;
    uint16_t num_error_passive;
    uint16_t num_bus_off;
    CAN_HandleTypeDef can;
} pyb_can_obj_t;

extern const mp_obj_type_t pyb_can_type;

void can_init0(void);
void can_deinit_all(void);
bool can_init(pyb_can_obj_t *can_obj, uint32_t mode, uint32_t prescaler, uint32_t sjw, uint32_t bs1, uint32_t bs2, bool auto_restart);
void can_deinit(pyb_can_obj_t *self);

void can_clearfilter(pyb_can_obj_t *self, uint32_t f, uint8_t bank);
int can_receive(CAN_HandleTypeDef *can, int fifo, CanRxMsgTypeDef *msg, uint8_t *data, uint32_t timeout_ms);
HAL_StatusTypeDef CAN_Transmit(CAN_HandleTypeDef *hcan, uint32_t Timeout);
void pyb_can_handle_callback(pyb_can_obj_t *self, uint fifo_id, mp_obj_t callback, mp_obj_t irq_reason);

#endif 

#endif 
