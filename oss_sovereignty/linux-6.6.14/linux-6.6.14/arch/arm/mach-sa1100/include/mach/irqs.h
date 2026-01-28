#define	IRQ_GPIO0_SC		1
#define	IRQ_GPIO1_SC		2
#define	IRQ_GPIO2_SC		3
#define	IRQ_GPIO3_SC		4
#define	IRQ_GPIO4_SC		5
#define	IRQ_GPIO5_SC		6
#define	IRQ_GPIO6_SC		7
#define	IRQ_GPIO7_SC		8
#define	IRQ_GPIO8_SC		9
#define	IRQ_GPIO9_SC		10
#define	IRQ_GPIO10_SC		11
#define	IRQ_GPIO11_27		12
#define	IRQ_LCD			13	 
#define	IRQ_Ser0UDC		14	 
#define	IRQ_Ser1SDLC		15	 
#define	IRQ_Ser1UART		16	 
#define	IRQ_Ser2ICP		17	 
#define	IRQ_Ser3UART		18	 
#define	IRQ_Ser4MCP		19	 
#define	IRQ_Ser4SSP		20	 
#define	IRQ_DMA0		21	 
#define	IRQ_DMA1		22	 
#define	IRQ_DMA2		23	 
#define	IRQ_DMA3		24	 
#define	IRQ_DMA4		25	 
#define	IRQ_DMA5		26	 
#define	IRQ_OST0		27	 
#define	IRQ_OST1		28	 
#define	IRQ_OST2		29	 
#define	IRQ_OST3		30	 
#define	IRQ_RTC1Hz		31	 
#define	IRQ_RTCAlrm		32	 
#define	IRQ_GPIO0		33
#define	IRQ_GPIO1		34
#define	IRQ_GPIO2		35
#define	IRQ_GPIO3		36
#define	IRQ_GPIO4		37
#define	IRQ_GPIO5		38
#define	IRQ_GPIO6		39
#define	IRQ_GPIO7		40
#define	IRQ_GPIO8		41
#define	IRQ_GPIO9		42
#define	IRQ_GPIO10		43
#define	IRQ_GPIO11		44
#define	IRQ_GPIO12		45
#define	IRQ_GPIO13		46
#define	IRQ_GPIO14		47
#define	IRQ_GPIO15		48
#define	IRQ_GPIO16		49
#define	IRQ_GPIO17		50
#define	IRQ_GPIO18		51
#define	IRQ_GPIO19		52
#define	IRQ_GPIO20		53
#define	IRQ_GPIO21		54
#define	IRQ_GPIO22		55
#define	IRQ_GPIO23		56
#define	IRQ_GPIO24		57
#define	IRQ_GPIO25		58
#define	IRQ_GPIO26		59
#define	IRQ_GPIO27		60
#define IRQ_BOARD_START		61
#define IRQ_BOARD_END		77
#ifdef CONFIG_SHARP_LOCOMO
#define NR_IRQS_LOCOMO		4
#else
#define NR_IRQS_LOCOMO		0
#endif
#ifndef NR_IRQS
#define NR_IRQS (IRQ_BOARD_START + NR_IRQS_LOCOMO)
#endif
#define SA1100_NR_IRQS (IRQ_BOARD_START + NR_IRQS_LOCOMO)
