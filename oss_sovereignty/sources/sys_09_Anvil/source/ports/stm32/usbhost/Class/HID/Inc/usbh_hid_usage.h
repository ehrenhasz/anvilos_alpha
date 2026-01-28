 
#ifndef _HID_USAGE_H_
#define _HID_USAGE_H_






  
 








#define HID_USAGE_PAGE_UNDEFINED  uint16_t (0x00)   

#define HID_USAGE_PAGE_GEN_DES    uint16_t (0x01)   
#define HID_USAGE_PAGE_SIM_CTR    uint16_t (0x02)   
#define HID_USAGE_PAGE_VR_CTR     uint16_t (0x03)   
#define HID_USAGE_PAGE_SPORT_CTR  uint16_t (0x04)   
#define HID_USAGE_PAGE_GAME_CTR   uint16_t (0x05)   
#define HID_USAGE_PAGE_GEN_DEV    uint16_t (0x06)   
#define HID_USAGE_PAGE_KEYB       uint16_t (0x07)   
#define HID_USAGE_PAGE_LED        uint16_t (0x08)   
#define HID_USAGE_PAGE_BUTTON     uint16_t (0x09)   
#define HID_USAGE_PAGE_ORDINAL    uint16_t (0x0A)   
#define HID_USAGE_PAGE_PHONE      uint16_t (0x0B)   
#define HID_USAGE_PAGE_CONSUMER   uint16_t (0x0C)   
#define HID_USAGE_PAGE_DIGITIZER  uint16_t (0x0D)   

#define HID_USAGE_PAGE_PID        uint16_t (0x0F)   
#define HID_USAGE_PAGE_UNICODE    uint16_t (0x10)   

#define HID_USAGE_PAGE_ALNUM_DISP uint16_t (0x14)   



#define HID_USAGE_PAGE_MEDICAL    uint16_t (0x40)   



#define HID_USAGE_PAGE_BARCODE    uint16_t (0x8C)   
#define HID_USAGE_PAGE_SCALE      uint16_t (0x8D)   
#define HID_USAGE_PAGE_MSR        uint16_t (0x8E)   
#define HID_USAGE_PAGE_POS        uint16_t (0x8F)   
#define HID_USAGE_PAGE_CAMERA_CTR uint16_t (0x90)   
#define HID_USAGE_PAGE_ARCADE     uint16_t (0x91)   




#define HID_USAGE_UNDEFINED     uint16_t (0x00)   
#define HID_USAGE_POINTER       uint16_t (0x01)   
#define HID_USAGE_MOUSE         uint16_t (0x02)   

#define HID_USAGE_JOYSTICK      uint16_t (0x04)   
#define HID_USAGE_GAMEPAD       uint16_t (0x05)   
#define HID_USAGE_KBD           uint16_t (0x06)   
#define HID_USAGE_KEYPAD        uint16_t (0x07)   
#define HID_USAGE_MAX_CTR       uint16_t (0x08)   

#define HID_USAGE_X             uint16_t (0x30)   
#define HID_USAGE_Y             uint16_t (0x31)   
#define HID_USAGE_Z             uint16_t (0x32)   
#define HID_USAGE_RX            uint16_t (0x33)   
#define HID_USAGE_RY            uint16_t (0x34)   
#define HID_USAGE_RZ            uint16_t (0x35)   
#define HID_USAGE_SLIDER        uint16_t (0x36)   
#define HID_USAGE_DIAL          uint16_t (0x37)   
#define HID_USAGE_WHEEL         uint16_t (0x38)   
#define HID_USAGE_HATSW         uint16_t (0x39)   
#define HID_USAGE_COUNTEDBUF    uint16_t (0x3A)   
#define HID_USAGE_BYTECOUNT     uint16_t (0x3B)   
#define HID_USAGE_MOTIONWAKE    uint16_t (0x3C)   
#define HID_USAGE_START         uint16_t (0x3D)   
#define HID_USAGE_SELECT        uint16_t (0x3E)   

#define HID_USAGE_VX            uint16_t (0x40)   
#define HID_USAGE_VY            uint16_t (0x41)   
#define HID_USAGE_VZ            uint16_t (0x42)   
#define HID_USAGE_VBRX          uint16_t (0x43)   
#define HID_USAGE_VBRY          uint16_t (0x44)   
#define HID_USAGE_VBRZ          uint16_t (0x45)   
#define HID_USAGE_VNO           uint16_t (0x46)   
#define HID_USAGE_FEATNOTIF     uint16_t (0x47)   

#define HID_USAGE_SYSCTL        uint16_t (0x80)   
#define HID_USAGE_PWDOWN        uint16_t (0x81)   
#define HID_USAGE_SLEEP         uint16_t (0x82)   
#define HID_USAGE_WAKEUP        uint16_t (0x83)   
#define HID_USAGE_CONTEXTM      uint16_t (0x84)   
#define HID_USAGE_MAINM         uint16_t (0x85)   
#define HID_USAGE_APPM          uint16_t (0x86)   
#define HID_USAGE_MENUHELP      uint16_t (0x87)   
#define HID_USAGE_MENUEXIT      uint16_t (0x88)   
#define HID_USAGE_MENUSELECT    uint16_t (0x89)   
#define HID_USAGE_SYSM_RIGHT    uint16_t (0x8A)   
#define HID_USAGE_SYSM_LEFT     uint16_t (0x8B)   
#define HID_USAGE_SYSM_UP       uint16_t (0x8C)   
#define HID_USAGE_SYSM_DOWN     uint16_t (0x8D)   
#define HID_USAGE_COLDRESET     uint16_t (0x8E)   
#define HID_USAGE_WARMRESET     uint16_t (0x8F)   
#define HID_USAGE_DUP           uint16_t (0x90)   
#define HID_USAGE_DDOWN         uint16_t (0x91)   
#define HID_USAGE_DRIGHT        uint16_t (0x92)   
#define HID_USAGE_DLEFT         uint16_t (0x93)   

#define HID_USAGE_SYS_DOCK      uint16_t (0xA0)   
#define HID_USAGE_SYS_UNDOCK    uint16_t (0xA1)   
#define HID_USAGE_SYS_SETUP     uint16_t (0xA2)   
#define HID_USAGE_SYS_BREAK     uint16_t (0xA3)   
#define HID_USAGE_SYS_DBGBRK    uint16_t (0xA4)   
#define HID_USAGE_APP_BRK       uint16_t (0xA5)   
#define HID_USAGE_APP_DBGBRK    uint16_t (0xA6)   
#define HID_USAGE_SYS_SPKMUTE   uint16_t (0xA7)   
#define HID_USAGE_SYS_HIBERN    uint16_t (0xA8)   

#define HID_USAGE_SYS_SIDPINV   uint16_t (0xB0)   
#define HID_USAGE_SYS_DISPINT   uint16_t (0xB1)   
#define HID_USAGE_SYS_DISPEXT   uint16_t (0xB2)   
#define HID_USAGE_SYS_DISPBOTH  uint16_t (0xB3)   
#define HID_USAGE_SYS_DISPDUAL  uint16_t (0xB4)   
#define HID_USAGE_SYS_DISPTGLIE uint16_t (0xB5)   
#define HID_USAGE_SYS_DISP_SWAP uint16_t (0xB6)   
#define HID_USAGE_SYS_DIPS_LCDA uint16_t (0xB7)   


 

#endif 

 

 

 

 

