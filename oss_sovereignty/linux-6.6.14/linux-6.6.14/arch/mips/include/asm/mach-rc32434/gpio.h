#ifndef _RC32434_GPIO_H_
#define _RC32434_GPIO_H_
struct rb532_gpio_reg {
	u32   gpiofunc;	   
	u32   gpiocfg;	   
	u32   gpiod;	   
	u32   gpioilevel;  
	u32   gpioistat;   
	u32   gpionmien;   
};
#define RC32434_UART0_SOUT	(1 << 0)
#define RC32434_UART0_SIN	(1 << 1)
#define RC32434_UART0_RTS	(1 << 2)
#define RC32434_UART0_CTS	(1 << 3)
#define RC32434_MP_BIT_22	(1 << 4)
#define RC32434_MP_BIT_23	(1 << 5)
#define RC32434_MP_BIT_24	(1 << 6)
#define RC32434_MP_BIT_25	(1 << 7)
#define RC32434_CPU_GPIO	(1 << 8)
#define RC32434_AF_SPARE_6	(1 << 9)
#define RC32434_AF_SPARE_4	(1 << 10)
#define RC32434_AF_SPARE_3	(1 << 11)
#define RC32434_AF_SPARE_2	(1 << 12)
#define RC32434_PCI_MSU_GPIO	(1 << 13)
#define GPIO_RDY		8
#define GPIO_WPX	9
#define GPIO_ALE		10
#define GPIO_CLE		11
#define CF_GPIO_NUM		13
#define GPIO_BTN_S1		1
extern void rb532_gpio_set_ilevel(int bit, unsigned gpio);
extern void rb532_gpio_set_istat(int bit, unsigned gpio);
extern void rb532_gpio_set_func(unsigned gpio);
#endif  
