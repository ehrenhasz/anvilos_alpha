
#ifndef MICROPY_INCLUDED_ATMEL_SAMD_EXTERNAL_FLASH_DEVICES_H
#define MICROPY_INCLUDED_ATMEL_SAMD_EXTERNAL_FLASH_DEVICES_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t total_size;
    uint16_t start_up_time_us;

    
    uint8_t manufacturer_id;
    uint8_t memory_type;
    uint8_t capacity;

    
    uint8_t max_clock_speed_mhz;

    
    
    uint8_t quad_enable_bit_mask;

    bool has_sector_protection : 1;

    
    bool supports_fast_read : 1;

    
    bool supports_qspi : 1;

    
    
    bool supports_qspi_writes : 1;

    
    
    bool write_status_register_split : 1;

    
    
    bool single_status_byte : 1;
} external_flash_device;




#define AT25DF081A { \
        .total_size = (1 << 20),  \
        .start_up_time_us = 10000, \
        .manufacturer_id = 0x1f, \
        .memory_type = 0x45, \
        .capacity = 0x01, \
        .max_clock_speed_mhz = 85, \
        .quad_enable_bit_mask = 0x00, \
        .has_sector_protection = true, \
        .supports_fast_read = true, \
        .supports_qspi = false, \
        .supports_qspi_writes = false, \
        .write_status_register_split = false, \
        .single_status_byte = false, \
}



#define GD25Q16C { \
        .total_size = (1 << 21),  \
        .start_up_time_us = 5000, \
        .manufacturer_id = 0xc8, \
        .memory_type = 0x40, \
        .capacity = 0x15, \
        .max_clock_speed_mhz = 104,  \
        .quad_enable_bit_mask = 0x02, \
        .has_sector_protection = false, \
        .supports_fast_read = true, \
        .supports_qspi = true, \
        .supports_qspi_writes = true, \
        .write_status_register_split = false, \
        .single_status_byte = false, \
}



#define GD25Q64C { \
        .total_size = (1 << 23),  \
        .start_up_time_us = 5000, \
        .manufacturer_id = 0xc8, \
        .memory_type = 0x40, \
        .capacity = 0x17, \
        .max_clock_speed_mhz = 104,  \
        .quad_enable_bit_mask = 0x02, \
        .has_sector_protection = false, \
        .supports_fast_read = true, \
        .supports_qspi = true, \
        .supports_qspi_writes = true, \
        .write_status_register_split = true, \
        .single_status_byte = false, \
}



#define S25FL064L { \
        .total_size = (1 << 23),  \
        .start_up_time_us = 300, \
        .manufacturer_id = 0x01, \
        .memory_type = 0x60, \
        .capacity = 0x17, \
        .max_clock_speed_mhz = 108, \
        .quad_enable_bit_mask = 0x02, \
        .has_sector_protection = false, \
        .supports_fast_read = true, \
        .supports_qspi = true, \
        .supports_qspi_writes = true, \
        .write_status_register_split = false, \
        .single_status_byte = false, \
}



#define S25FL116K { \
        .total_size = (1 << 21),  \
        .start_up_time_us = 10000, \
        .manufacturer_id = 0x01, \
        .memory_type = 0x40, \
        .capacity = 0x15, \
        .max_clock_speed_mhz = 108, \
        .quad_enable_bit_mask = 0x02, \
        .has_sector_protection = false, \
        .supports_fast_read = true, \
        .supports_qspi = true, \
        .supports_qspi_writes = false, \
        .write_status_register_split = false, \
        .single_status_byte = false, \
}



#define S25FL216K { \
        .total_size = (1 << 21),  \
        .start_up_time_us = 10000, \
        .manufacturer_id = 0x01, \
        .memory_type = 0x40, \
        .capacity = 0x15, \
        .max_clock_speed_mhz = 65, \
        .quad_enable_bit_mask = 0x02, \
        .has_sector_protection = false, \
        .supports_fast_read = true, \
        .supports_qspi = false, \
        .supports_qspi_writes = false, \
        .write_status_register_split = false, \
        .single_status_byte = false, \
}



#define W25Q16FW { \
        .total_size = (1 << 21),  \
        .start_up_time_us = 5000, \
        .manufacturer_id = 0xef, \
        .memory_type = 0x60, \
        .capacity = 0x15, \
        .max_clock_speed_mhz = 133, \
        .quad_enable_bit_mask = 0x02, \
        .has_sector_protection = false, \
        .supports_fast_read = true, \
        .supports_qspi = true, \
        .supports_qspi_writes = true, \
        .write_status_register_split = false, \
        .single_status_byte = false, \
}



#define W25Q16JV_IQ { \
        .total_size = (1 << 21),  \
        .start_up_time_us = 5000, \
        .manufacturer_id = 0xef, \
        .memory_type = 0x40, \
        .capacity = 0x15, \
        .max_clock_speed_mhz = 133, \
        .quad_enable_bit_mask = 0x02, \
        .has_sector_protection = false, \
        .supports_fast_read = true, \
        .supports_qspi = true, \
        .supports_qspi_writes = true, \
        .write_status_register_split = false, \
        .single_status_byte = false, \
}



#define W25Q16JV_IM { \
        .total_size = (1 << 21),  \
        .start_up_time_us = 5000, \
        .manufacturer_id = 0xef, \
        .memory_type = 0x70, \
        .capacity = 0x15, \
        .max_clock_speed_mhz = 133, \
        .quad_enable_bit_mask = 0x02, \
        .has_sector_protection = false, \
        .supports_fast_read = true, \
        .supports_qspi = true, \
        .supports_qspi_writes = true, \
        .write_status_register_split = false, \
}



#define W25Q32BV { \
        .total_size = (1 << 22),  \
        .start_up_time_us = 10000, \
        .manufacturer_id = 0xef, \
        .memory_type = 0x60, \
        .capacity = 0x16, \
        .max_clock_speed_mhz = 104, \
        .quad_enable_bit_mask = 0x02, \
        .has_sector_protection = false, \
        .supports_fast_read = true, \
        .supports_qspi = true, \
        .supports_qspi_writes = false, \
        .write_status_register_split = false, \
        .single_status_byte = false, \
}


#define W25Q32JV_IM { \
        .total_size = (1 << 22),  \
        .start_up_time_us = 5000, \
        .manufacturer_id = 0xef, \
        .memory_type = 0x70, \
        .capacity = 0x16, \
        .max_clock_speed_mhz = 133, \
        .quad_enable_bit_mask = 0x02, \
        .has_sector_protection = false, \
        .supports_fast_read = true, \
        .supports_qspi = true, \
        .supports_qspi_writes = true, \
        .write_status_register_split = false, \
}



#define W25Q32JV_IQ { \
        .total_size = (1 << 22),  \
        .start_up_time_us = 5000, \
        .manufacturer_id = 0xef, \
        .memory_type = 0x40, \
        .capacity = 0x16, \
        .max_clock_speed_mhz = 133, \
        .quad_enable_bit_mask = 0x02, \
        .has_sector_protection = false, \
        .supports_fast_read = true, \
        .supports_qspi = true, \
        .supports_qspi_writes = true, \
        .write_status_register_split = false, \
}



#define W25Q64JV_IM { \
        .total_size = (1 << 23),  \
        .start_up_time_us = 5000, \
        .manufacturer_id = 0xef, \
        .memory_type = 0x70, \
        .capacity = 0x17, \
        .max_clock_speed_mhz = 133, \
        .quad_enable_bit_mask = 0x02, \
        .has_sector_protection = false, \
        .supports_fast_read = true, \
        .supports_qspi = true, \
        .supports_qspi_writes = true, \
        .write_status_register_split = false, \
        .single_status_byte = false, \
}



#define W25Q64JV_IQ { \
        .total_size = (1 << 23),  \
        .start_up_time_us = 5000, \
        .manufacturer_id = 0xef, \
        .memory_type = 0x40, \
        .capacity = 0x17, \
        .max_clock_speed_mhz = 133, \
        .quad_enable_bit_mask = 0x02, \
        .has_sector_protection = false, \
        .supports_fast_read = true, \
        .supports_qspi = true, \
        .supports_qspi_writes = true, \
        .write_status_register_split = false, \
        .single_status_byte = false, \
}



#define W25Q80DL { \
        .total_size = (1 << 20),  \
        .start_up_time_us = 5000, \
        .manufacturer_id = 0xef, \
        .memory_type = 0x60, \
        .capacity = 0x14, \
        .max_clock_speed_mhz = 104, \
        .quad_enable_bit_mask = 0x02, \
        .has_sector_protection = false, \
        .supports_fast_read = true, \
        .supports_qspi = true, \
        .supports_qspi_writes = false, \
        .write_status_register_split = false, \
        .single_status_byte = false, \
}




#define W25Q128JV_SQ { \
        .total_size = (1 << 24),  \
        .start_up_time_us = 5000, \
        .manufacturer_id = 0xef, \
        .memory_type = 0x40, \
        .capacity = 0x18, \
        .max_clock_speed_mhz = 133, \
        .quad_enable_bit_mask = 0x02, \
        .has_sector_protection = false, \
        .supports_fast_read = true, \
        .supports_qspi = true, \
        .supports_qspi_writes = true, \
        .write_status_register_split = false, \
        .single_status_byte = false, \
}



#define MX25L1606  { \
        .total_size = (1 << 21),  \
        .start_up_time_us = 5000, \
        .manufacturer_id = 0xc2, \
        .memory_type = 0x20, \
        .capacity = 0x15, \
        .max_clock_speed_mhz = 8, \
        .quad_enable_bit_mask = 0x40, \
        .has_sector_protection = false, \
        .supports_fast_read = true, \
        .supports_qspi = true, \
        .supports_qspi_writes = true, \
        .write_status_register_split = false, \
        .single_status_byte = true, \
}



#define MX25L3233F  { \
        .total_size = (1 << 22),  \
        .start_up_time_us = 5000, \
        .manufacturer_id = 0xc2, \
        .memory_type = 0x20, \
        .capacity = 0x16, \
        .max_clock_speed_mhz = 133, \
        .quad_enable_bit_mask = 0x40, \
        .has_sector_protection = false, \
        .supports_fast_read = true, \
        .supports_qspi = true, \
        .supports_qspi_writes = true, \
        .write_status_register_split = false, \
        .single_status_byte = true, \
}




#define MX25R6435F  { \
        .total_size = (1 << 23),  \
        .start_up_time_us = 5000, \
        .manufacturer_id = 0xc2, \
        .memory_type = 0x28, \
        .capacity = 0x17, \
        .max_clock_speed_mhz = 8, \
        .quad_enable_bit_mask = 0x40, \
        .has_sector_protection = false, \
        .supports_fast_read = true, \
        .supports_qspi = true, \
        .supports_qspi_writes = true, \
        .write_status_register_split = false, \
        .single_status_byte = true, \
}



#define W25Q128JV_PM { \
        .total_size = (1 << 24),  \
        .start_up_time_us = 5000, \
        .manufacturer_id = 0xef, \
        .memory_type = 0x70, \
        .capacity = 0x18, \
        .max_clock_speed_mhz = 133, \
        .quad_enable_bit_mask = 0x02, \
        .has_sector_protection = false, \
        .supports_fast_read = true, \
        .supports_qspi = true, \
        .supports_qspi_writes = true, \
        .write_status_register_split = false, \
}



#define W25Q32FV { \
        .total_size = (1 << 22),  \
        .start_up_time_us = 5000, \
        .manufacturer_id = 0xef, \
        .memory_type = 0x40, \
        .capacity = 0x16, \
        .max_clock_speed_mhz = 104, \
        .quad_enable_bit_mask = 0x00, \
        .has_sector_protection = false, \
        .supports_fast_read = true, \
        .supports_qspi = false, \
        .supports_qspi_writes = false, \
        .write_status_register_split = false, \
        .single_status_byte = false, \
}


#define IS25LPWP064D { \
        .total_size = (1 << 23),  \
        .start_up_time_us = 5000, \
        .manufacturer_id = 0x9D, \
        .memory_type = 0x60, \
        .capacity = 0x17, \
        .max_clock_speed_mhz = 80, \
        .quad_enable_bit_mask = 0x40, \
        .has_sector_protection = false, \
        .supports_fast_read = true, \
        .supports_qspi = true, \
        .supports_qspi_writes = true, \
        .write_status_register_split = false, \
        .single_status_byte = true, \
}


#define GENERIC { \
        .total_size = (1 << 21),  \
        .start_up_time_us = 5000, \
        .manufacturer_id = 0x00, \
        .memory_type = 0x40, \
        .capacity = 0x15, \
        .max_clock_speed_mhz = 48, \
        .quad_enable_bit_mask = 0x02, \
        .has_sector_protection = false, \
        .supports_fast_read = true, \
        .supports_qspi = true, \
        .supports_qspi_writes = true, \
        .write_status_register_split = false, \
        .single_status_byte = false, \
}
#endif  
