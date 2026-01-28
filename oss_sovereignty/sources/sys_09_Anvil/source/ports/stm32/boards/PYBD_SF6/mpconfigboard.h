


#include "boards/PYBD_SF2/mpconfigboard.h"

#undef MICROPY_HW_BOARD_NAME
#undef MICROPY_HW_MCU_NAME
#undef MICROPY_HW_CLK_PLLM
#undef MICROPY_HW_CLK_PLLN
#undef MICROPY_HW_CLK_PLLP
#undef MICROPY_HW_CLK_PLLQ
#undef MICROPY_HW_FLASH_LATENCY

#define MICROPY_HW_BOARD_NAME       "PYBD-SF6W"
#define MICROPY_HW_MCU_NAME         "STM32F767IIK"


#define MICROPY_HW_CLK_PLLM         (25)
#define MICROPY_HW_CLK_PLLN         (288)
#define MICROPY_HW_CLK_PLLP         (RCC_PLLP_DIV2)
#define MICROPY_HW_CLK_PLLQ         (6)
#define MICROPY_HW_FLASH_LATENCY    (FLASH_LATENCY_4)


#define MICROPY_HW_UART7_TX         (pyb_pin_W16)
#define MICROPY_HW_UART7_RX         (pyb_pin_W22B)


#define MICROPY_HW_CAN2_NAME        "Y"
#define MICROPY_HW_CAN2_TX          (pyb_pin_Y6)
#define MICROPY_HW_CAN2_RX          (pyb_pin_Y5)


#define MICROPY_HW_ETH_MDC          (pyb_pin_W24)
#define MICROPY_HW_ETH_MDIO         (pyb_pin_W15)
#define MICROPY_HW_ETH_RMII_REF_CLK (pyb_pin_W17)
#define MICROPY_HW_ETH_RMII_CRS_DV  (pyb_pin_W14)
#define MICROPY_HW_ETH_RMII_RXD0    (pyb_pin_W51)
#define MICROPY_HW_ETH_RMII_RXD1    (pyb_pin_W47)
#define MICROPY_HW_ETH_RMII_TX_EN   (pyb_pin_W8)
#define MICROPY_HW_ETH_RMII_TXD0    (pyb_pin_W45)
#define MICROPY_HW_ETH_RMII_TXD1    (pyb_pin_W49)
