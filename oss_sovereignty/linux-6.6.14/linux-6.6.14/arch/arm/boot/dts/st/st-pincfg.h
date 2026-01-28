#ifndef _ST_PINCFG_H_
#define _ST_PINCFG_H_
#define ALT1	1
#define ALT2	2
#define ALT3	3
#define ALT4	4
#define ALT5	5
#define ALT6	6
#define ALT7	7
#define OE			(1 << 27)
#define PU			(1 << 26)
#define OD			(1 << 25)
#define RT			(1 << 23)
#define INVERTCLK		(1 << 22)
#define CLKNOTDATA		(1 << 21)
#define DOUBLE_EDGE		(1 << 20)
#define CLK_A			(0 << 18)
#define CLK_B			(1 << 18)
#define CLK_C			(2 << 18)
#define CLK_D			(3 << 18)
#define IN			(0)
#define IN_PU			(PU)
#define OUT			(OE)
#define BIDIR			(OE | OD)
#define BIDIR_PU		(OE | PU | OD)
#define BYPASS		(0)
#define SE_NICLK_IO	(RT)
#define SE_ICLK_IO	(RT | INVERTCLK)
#define DE_IO		(RT | DOUBLE_EDGE)
#define ICLK		(RT | CLKNOTDATA | INVERTCLK)
#define NICLK		(RT | CLKNOTDATA)
#endif  
