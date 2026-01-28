
#ifndef MICROPY_INCLUDED_NRF_TUSB_CONFIG_H
#define MICROPY_INCLUDED_NRF_TUSB_CONFIG_H



#define CFG_TUSB_MCU                OPT_MCU_NRF5X
#define CFG_TUSB_RHPORT0_MODE       OPT_MODE_DEVICE

#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN          TU_ATTR_ALIGNED(4)



#define CFG_TUD_ENDOINT0_SIZE       (64)
#define CFG_TUD_CDC                 (1)
#define CFG_TUD_CDC_RX_BUFSIZE      (64)
#define CFG_TUD_CDC_TX_BUFSIZE      (64)

#endif 
