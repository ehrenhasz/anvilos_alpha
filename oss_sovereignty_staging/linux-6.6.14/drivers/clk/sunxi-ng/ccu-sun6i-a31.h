 
 

#ifndef _CCU_SUN6I_A31_H_
#define _CCU_SUN6I_A31_H_

#include <dt-bindings/clock/sun6i-a31-ccu.h>
#include <dt-bindings/reset/sun6i-a31-ccu.h>

#define CLK_PLL_CPU		0
#define CLK_PLL_AUDIO_BASE	1
#define CLK_PLL_AUDIO		2
#define CLK_PLL_AUDIO_2X	3
#define CLK_PLL_AUDIO_4X	4
#define CLK_PLL_AUDIO_8X	5
#define CLK_PLL_VIDEO0		6

 

#define CLK_PLL_VE		8
#define CLK_PLL_DDR		9

 

#define CLK_PLL_PERIPH_2X	11
#define CLK_PLL_VIDEO1		12

 

#define CLK_PLL_GPU		14

 

#define CLK_PLL9		16
#define CLK_PLL10		17

 

#define CLK_AXI			19
#define CLK_AHB1		20
#define CLK_APB1		21
#define CLK_APB2		22

 

 

 

#define CLK_MDFS		107
#define CLK_SDRAM0		108
#define CLK_SDRAM1		109

 

 

#define CLK_MBUS0		141
#define CLK_MBUS1		142

 

#define CLK_NUMBER		(CLK_OUT_C + 1)

#endif  
