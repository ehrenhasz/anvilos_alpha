
#ifndef MICROPY_INCLUDED_STM32_MBOOT_DFU_H
#define MICROPY_INCLUDED_STM32_MBOOT_DFU_H

#include <stdbool.h>
#include <stdint.h>



#define DFU_XFER_SIZE (2048)


#define MBOOT_ERROR_STR_OVERWRITE_BOOTLOADER_IDX 0x10
#define MBOOT_ERROR_STR_OVERWRITE_BOOTLOADER "Can't overwrite mboot"

#define MBOOT_ERROR_STR_INVALID_ADDRESS_IDX 0x11
#define MBOOT_ERROR_STR_INVALID_ADDRESS "Address out of range"

#if MBOOT_ENABLE_PACKING
#define MBOOT_ERROR_STR_INVALID_SIG_IDX 0x12
#define MBOOT_ERROR_STR_INVALID_SIG "Invalid signature in file"

#define MBOOT_ERROR_STR_INVALID_READ_IDX 0x13
#define MBOOT_ERROR_STR_INVALID_READ "Read support disabled on encrypted bootloader"
#endif


enum {
    DFU_DETACH = 0,
    DFU_DNLOAD = 1,
    DFU_UPLOAD = 2,
    DFU_GETSTATUS = 3,
    DFU_CLRSTATUS = 4,
    DFU_GETSTATE = 5,
    DFU_ABORT = 6,
};


typedef enum {
    DFU_STATE_IDLE = 2,
    DFU_STATE_BUSY = 4,
    DFU_STATE_DNLOAD_IDLE = 5,
    DFU_STATE_MANIFEST = 7,
    DFU_STATE_UPLOAD_IDLE = 9,
    DFU_STATE_ERROR = 0xa,
} dfu_state_t;

typedef enum {
    DFU_CMD_NONE = 0,
    DFU_CMD_EXIT = 1,
    DFU_CMD_UPLOAD = 7,
    DFU_CMD_DNLOAD = 8,
} dfu_cmd_t;

enum {
    DFU_CMD_DNLOAD_SET_ADDRESS = 0x21,
    DFU_CMD_DNLOAD_ERASE = 0x41,
    DFU_CMD_DNLOAD_READ_UNPROTECT = 0x92,
};


typedef enum {
    DFU_STATUS_OK = 0x00,  
    DFU_STATUS_ERROR_TARGET = 0x01,  
    DFU_STATUS_ERROR_FILE = 0x02,  
    DFU_STATUS_ERROR_WRITE = 0x03,  
    DFU_STATUS_ERROR_ERASE = 0x04,  
    DFU_STATUS_ERROR_CHECK_ERASED = 0x05,  
    DFU_STATUS_ERROR_PROG = 0x06,  
    DFU_STATUS_ERROR_VERIFY = 0x07,  
    DFU_STATUS_ERROR_ADDRESS = 0x08,  
    DFU_STATUS_ERROR_NOTDONE = 0x09,  
    DFU_STATUS_ERROR_FIRMWARE = 0x0A,  
    DFU_STATUS_ERROR_VENDOR = 0x0B,  
    DFU_STATUS_ERROR_USBR = 0x0C,  
    DFU_STATUS_ERROR_POR = 0x0D,  
    DFU_STATUS_ERROR_UNKNOWN = 0x0E,  
    DFU_STATUS_ERROR_STALLEDPKT = 0x0F,  
} dfu_status_t;

typedef struct _dfu_state_t {
    dfu_state_t state;
    dfu_cmd_t cmd;
    dfu_status_t status;
    uint8_t error;
    bool leave_dfu;
    uint16_t wBlockNum;
    uint16_t wLength;
    uint32_t addr;
    uint8_t buf[DFU_XFER_SIZE] __attribute__((aligned(4)));
} dfu_context_t;

extern dfu_context_t dfu_context;

#endif 
