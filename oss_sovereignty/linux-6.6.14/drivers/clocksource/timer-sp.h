 
 
#define NR_TIMERS	2
#define TIMER_1_BASE	0x00
#define TIMER_2_BASE	0x20

#define TIMER_LOAD	0x00			 
#define TIMER_VALUE	0x04			 
#define TIMER_CTRL	0x08			 
#define TIMER_CTRL_ONESHOT	(1 << 0)	 
#define TIMER_CTRL_32BIT	(1 << 1)	 
#define TIMER_CTRL_DIV1		(0 << 2)	 
#define TIMER_CTRL_DIV16	(1 << 2)	 
#define TIMER_CTRL_DIV256	(2 << 2)	 
#define TIMER_CTRL_IE		(1 << 5)	 
#define TIMER_CTRL_PERIODIC	(1 << 6)	 
#define TIMER_CTRL_ENABLE	(1 << 7)	 

#define TIMER_INTCLR	0x0c			 
#define TIMER_RIS	0x10			 
#define TIMER_MIS	0x14			 
#define TIMER_BGLOAD	0x18			 

struct sp804_timer {
	int load;
	int load_h;
	int value;
	int value_h;
	int ctrl;
	int intclr;
	int ris;
	int mis;
	int bgload;
	int bgload_h;
	int timer_base[NR_TIMERS];
	int width;
};

struct sp804_clkevt {
	void __iomem *base;
	void __iomem *load;
	void __iomem *load_h;
	void __iomem *value;
	void __iomem *value_h;
	void __iomem *ctrl;
	void __iomem *intclr;
	void __iomem *ris;
	void __iomem *mis;
	void __iomem *bgload;
	void __iomem *bgload_h;
	unsigned long reload;
	int width;
};
