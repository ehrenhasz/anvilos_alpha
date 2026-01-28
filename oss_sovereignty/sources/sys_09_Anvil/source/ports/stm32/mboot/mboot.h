
#ifndef MICROPY_INCLUDED_STM32_MBOOT_MBOOT_H
#define MICROPY_INCLUDED_STM32_MBOOT_MBOOT_H

#include "py/mpconfig.h"
#include "py/mphal.h"


#define SECTION_NOZERO_BSS __attribute__((section(".nozero_bss")))

#define ELEM_DATA_SIZE (1024)
#define ELEM_DATA_START (&_estack[0])
#define ELEM_DATA_MAX (&_estack[ELEM_DATA_SIZE])

#define NORETURN __attribute__((noreturn))
#define MP_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))


#if defined(MBOOT_LED1) || defined(MICROPY_HW_LED1)
#define MBOOT_ENABLE_DEFAULT_UI (1)
#else
#define MBOOT_ENABLE_DEFAULT_UI (0)
#endif

#ifndef MBOOT_BOARD_EARLY_INIT
#define MBOOT_BOARD_EARLY_INIT(initial_r0)
#endif

#ifndef MBOOT_BOARD_ENTRY_INIT
#define MBOOT_BOARD_ENTRY_INIT(initial_r0) mboot_entry_init_default()
#endif

#ifndef MBOOT_BOARD_GET_RESET_MODE
#if MBOOT_ENABLE_DEFAULT_UI
#define MBOOT_BOARD_GET_RESET_MODE(initial_r0) mboot_get_reset_mode_default()
#else
#define MBOOT_BOARD_GET_RESET_MODE(initial_r0) BOARDCTRL_RESET_MODE_NORMAL
#endif
#endif

#ifndef MBOOT_BOARD_STATE_CHANGE
#if MBOOT_ENABLE_DEFAULT_UI
#define MBOOT_BOARD_STATE_CHANGE(state, arg) mboot_state_change_default((state), (arg))
#else
#define MBOOT_BOARD_STATE_CHANGE(state, arg)
#endif
#endif

#ifndef MBOOT_BOARD_SYSTICK
#if MBOOT_ENABLE_DEFAULT_UI
#define MBOOT_BOARD_SYSTICK() mboot_ui_systick()
#else
#define MBOOT_BOARD_SYSTICK()
#endif
#endif

#ifndef MBOOT_ADDRESS_SPACE_64BIT
#define MBOOT_ADDRESS_SPACE_64BIT (0)
#endif


#define MBOOT_INITIAL_R0_KEY (0x70ad0000)
#define MBOOT_INITIAL_R0_KEY_FSLOAD (MBOOT_INITIAL_R0_KEY | 0x80)


#define MBOOT_LED_STATE_LED0 (0x01)
#define MBOOT_LED_STATE_LED1 (0x02)
#define MBOOT_LED_STATE_LED2 (0x04)
#define MBOOT_LED_STATE_LED3 (0x08)


#ifndef MBOOT_FSLOAD
#define MBOOT_FSLOAD (0)
#endif


#ifndef MBOOT_VFS_FAT
#define MBOOT_VFS_FAT (0)
#endif


#ifndef MBOOT_VFS_LFS1
#define MBOOT_VFS_LFS1 (0)
#endif


#ifndef MBOOT_VFS_LFS2
#define MBOOT_VFS_LFS2 (0)
#endif


#ifndef MBOOT_VFS_RAW
#define MBOOT_VFS_RAW (MBOOT_FSLOAD)
#endif








typedef enum {
    MBOOT_STATE_DFU_START,          
    MBOOT_STATE_DFU_END,            
    MBOOT_STATE_FSLOAD_START,       
    MBOOT_STATE_FSLOAD_END,         
    MBOOT_STATE_FSLOAD_PASS_START,  
    MBOOT_STATE_FSLOAD_PROGRESS,    
    MBOOT_STATE_ERASE_START,        
    MBOOT_STATE_ERASE_END,          
    MBOOT_STATE_READ_START,         
    MBOOT_STATE_READ_END,           
    MBOOT_STATE_WRITE_START,        
    MBOOT_STATE_WRITE_END,          
} mboot_state_t;

enum {
    MBOOT_ERRNO_FLASH_ERASE_DISALLOWED = 200,
    MBOOT_ERRNO_FLASH_ERASE_FAILED,
    MBOOT_ERRNO_FLASH_READ_DISALLOWED,
    MBOOT_ERRNO_FLASH_WRITE_DISALLOWED,

    MBOOT_ERRNO_DFU_INVALID_HEADER = 210,
    MBOOT_ERRNO_DFU_INVALID_TARGET,
    MBOOT_ERRNO_DFU_INVALID_SIZE,
    MBOOT_ERRNO_DFU_TOO_MANY_TARGETS,
    MBOOT_ERRNO_DFU_READ_ERROR,
    MBOOT_ERRNO_DFU_INVALID_CRC,

    MBOOT_ERRNO_FSLOAD_NO_FSLOAD = 220,
    MBOOT_ERRNO_FSLOAD_NO_MOUNT,
    MBOOT_ERRNO_FSLOAD_INVALID_MOUNT,

    MBOOT_ERRNO_PACK_INVALID_ADDR = 230,
    MBOOT_ERRNO_PACK_INVALID_CHUNK,
    MBOOT_ERRNO_PACK_INVALID_VERSION,
    MBOOT_ERRNO_PACK_DECRYPT_FAILED,
    MBOOT_ERRNO_PACK_SIGN_FAILED,

    MBOOT_ERRNO_VFS_FAT_MOUNT_FAILED = 240,
    MBOOT_ERRNO_VFS_FAT_OPEN_FAILED,
    MBOOT_ERRNO_VFS_LFS1_MOUNT_FAILED,
    MBOOT_ERRNO_VFS_LFS1_OPEN_FAILED,
    MBOOT_ERRNO_VFS_LFS2_MOUNT_FAILED,
    MBOOT_ERRNO_VFS_LFS2_OPEN_FAILED,

    MBOOT_ERRNO_GUNZIP_FAILED = 250,
};

enum {
    ELEM_TYPE_END = 1,
    ELEM_TYPE_MOUNT,
    ELEM_TYPE_FSLOAD,
    ELEM_TYPE_STATUS,
};

enum {
    ELEM_MOUNT_FAT = 1,
    ELEM_MOUNT_LFS1,
    ELEM_MOUNT_LFS2,
    ELEM_MOUNT_RAW,
};


#if MBOOT_ADDRESS_SPACE_64BIT
typedef uint64_t mboot_addr_t;
#else
typedef uint32_t mboot_addr_t;
#endif

extern volatile uint32_t systick_ms;
extern uint8_t _estack[ELEM_DATA_SIZE];
extern int32_t first_writable_flash_sector;

void systick_init(void);
void led_init(void);
void mboot_ui_systick(void);
void SystemClock_Config(void);

uint32_t get_le32(const uint8_t *b);
uint64_t get_le64(const uint8_t *b);
void led_state_all(unsigned int mask);

int hw_page_erase(uint32_t addr, uint32_t *next_addr);
void hw_read(mboot_addr_t addr, size_t len, uint8_t *buf);
int hw_write(uint32_t addr, const uint8_t *src8, size_t len);

int do_page_erase(uint32_t addr, uint32_t *next_addr);
void do_read(mboot_addr_t addr, size_t len, uint8_t *buf);
int do_write(uint32_t addr, const uint8_t *src8, size_t len, bool dry_run);

const uint8_t *elem_search(const uint8_t *elem, uint8_t elem_id);
int fsload_process(void);

static inline void mboot_entry_init_default(void) {
    #if MBOOT_ENABLE_DEFAULT_UI
    
    led_init();
    #endif

    
    SystemClock_Config();

    #if defined(STM32H7)
    
    __set_PRIMASK(0);
    #endif
}

int mboot_get_reset_mode_default(void);
void mboot_state_change_default(mboot_state_t state, uint32_t arg);

static inline void mboot_state_change(mboot_state_t state, uint32_t arg) {
    return MBOOT_BOARD_STATE_CHANGE(state, arg);
}

#endif 
