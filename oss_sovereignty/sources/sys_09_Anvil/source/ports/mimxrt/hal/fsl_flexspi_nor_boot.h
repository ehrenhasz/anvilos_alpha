

#ifndef __FLEXSPI_NOR_BOOT_H__
#define __FLEXSPI_NOR_BOOT_H__

#include <stdint.h>
#include "board.h"




#define FSL_XIP_DEVICE_DRIVER_VERSION (MAKE_VERSION(2, 0, 0))



typedef struct _ivt_ {
    
    uint32_t hdr;
    
    uint32_t entry;
    
    uint32_t reserved1;
    
    uint32_t dcd;
    
    uint32_t boot_data;
    
    uint32_t self;
    
    uint32_t csf;
    
    uint32_t reserved2;
} ivt;

#define IVT_MAJOR_VERSION           0x4
#define IVT_MAJOR_VERSION_SHIFT     0x4
#define IVT_MAJOR_VERSION_MASK      0xF
#define IVT_MINOR_VERSION           0x3
#define IVT_MINOR_VERSION_SHIFT     0x0
#define IVT_MINOR_VERSION_MASK      0xF

#define IVT_VERSION(major, minor)   \
    ((((major) & IVT_MAJOR_VERSION_MASK) << IVT_MAJOR_VERSION_SHIFT) |  \
    (((minor) & IVT_MINOR_VERSION_MASK) << IVT_MINOR_VERSION_SHIFT))


#define IVT_TAG_HEADER        0xD1       
#define IVT_SIZE              0x2000
#define IVT_PAR               IVT_VERSION(IVT_MAJOR_VERSION, IVT_MINOR_VERSION)
#define IVT_HEADER           (IVT_TAG_HEADER | (IVT_SIZE << 8) | (IVT_PAR << 24))


#if defined(__CC_ARM) || defined(__ARMCC_VERSION)
extern uint32_t __Vectors[];
extern uint32_t Image$$RW_m_config_text$$Base[];
#define IMAGE_ENTRY_ADDRESS ((uint32_t)__Vectors)
#define FLASH_BASE ((uint32_t)Image$$RW_m_config_text$$Base)
#elif defined(__MCUXPRESSO)
extern uint32_t __Vectors[];
extern uint32_t __boot_hdr_start__[];
#define IMAGE_ENTRY_ADDRESS ((uint32_t)__Vectors)
#define FLASH_BASE          ((uint32_t)__boot_hdr_start__)
#elif defined(__ICCARM__)
extern uint32_t __VECTOR_TABLE[];
extern uint32_t m_boot_hdr_conf_start[];
#define IMAGE_ENTRY_ADDRESS ((uint32_t)__VECTOR_TABLE)
#define FLASH_BASE ((uint32_t)m_boot_hdr_conf_start)
#elif defined(__GNUC__)
extern uint32_t __VECTOR_TABLE[];
extern uint32_t __FLASH_BASE[];
#define IMAGE_ENTRY_ADDRESS ((uint32_t)__VECTOR_TABLE)
#define FLASH_BASE ((uint32_t)__FLASH_BASE)
#endif

#define DCD_ADDRESS           dcd_data
#define BOOT_DATA_ADDRESS     &boot_data
#define CSF_ADDRESS           0
#define IVT_RSVD             (uint32_t)(0x00000000)


typedef struct _boot_data_ {
    uint32_t start;         
    uint32_t size;          
    uint32_t plugin;        
    uint32_t placeholder;   
}BOOT_DATA_T;

#if defined(BOARD_FLASH_SIZE)
#define FLASH_SIZE            BOARD_FLASH_SIZE
#else
#error "Please define macro BOARD_FLASH_SIZE"
#endif
#define PLUGIN_FLAG           (uint32_t)0


const BOOT_DATA_T boot_data;
extern const uint8_t dcd_data[];

#endif 
