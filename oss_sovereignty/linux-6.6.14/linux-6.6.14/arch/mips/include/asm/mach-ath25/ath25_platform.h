#ifndef __ASM_MACH_ATH25_PLATFORM_H
#define __ASM_MACH_ATH25_PLATFORM_H
#include <linux/etherdevice.h>
struct ath25_boarddata {
	u32 magic;                    
#define ATH25_BD_MAGIC 0x35333131     
	u16 cksum;                    
	u16 rev;                      
#define BD_REV 4
	char board_name[64];          
	u16 major;                    
	u16 minor;                    
	u32 flags;                    
#define BD_ENET0        0x00000001    
#define BD_ENET1        0x00000002    
#define BD_UART1        0x00000004    
#define BD_UART0        0x00000008    
#define BD_RSTFACTORY   0x00000010    
#define BD_SYSLED       0x00000020    
#define BD_EXTUARTCLK   0x00000040    
#define BD_CPUFREQ      0x00000080    
#define BD_SYSFREQ      0x00000100    
#define BD_WLAN0        0x00000200    
#define BD_MEMCAP       0x00000400    
#define BD_DISWATCHDOG  0x00000800    
#define BD_WLAN1        0x00001000    
#define BD_ISCASPER     0x00002000    
#define BD_WLAN0_2G_EN  0x00004000    
#define BD_WLAN0_5G_EN  0x00008000    
#define BD_WLAN1_2G_EN  0x00020000    
#define BD_WLAN1_5G_EN  0x00040000    
	u16 reset_config_gpio;        
	u16 sys_led_gpio;             
	u32 cpu_freq;                 
	u32 sys_freq;                 
	u32 cnt_freq;                 
	u8  wlan0_mac[ETH_ALEN];
	u8  enet0_mac[ETH_ALEN];
	u8  enet1_mac[ETH_ALEN];
	u16 pci_id;                   
	u16 mem_cap;                  
	u8  wlan1_mac[ETH_ALEN];      
};
#define BOARD_CONFIG_BUFSZ		0x1000
struct ar231x_board_config {
	u16 devid;
	struct ath25_boarddata *config;
	const char *radio;
};
#endif  
