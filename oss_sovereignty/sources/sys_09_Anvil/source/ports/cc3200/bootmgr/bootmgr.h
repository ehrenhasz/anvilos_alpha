
#ifndef MICROPY_INCLUDED_CC3200_BOOTMGR_BOOTMGR_H
#define MICROPY_INCLUDED_CC3200_BOOTMGR_BOOTMGR_H







#ifdef __cplusplus
extern "C"
{
#endif




#define FACTORY_IMG_TOKEN       0x5555AAAA
#define UPDATE_IMG_TOKEN        0xAA5555AA
#define USER_BOOT_INFO_TOKEN    0xA5A55A5A




#define APP_IMG_SRAM_OFFSET     0x20004000
#define DEVICE_IS_CC3101RS      0x18
#define DEVICE_IS_CC3101S       0x1B




extern void Run(unsigned long);






#ifdef __cplusplus
}
#endif

#endif 
