


#ifndef _CCU_SUN8I_A23_A33_H_
#define _CCU_SUN8I_A23_A33_H_

#include <dt-bindings/clock/sun8i-a23-a33-ccu.h>
#include <dt-bindings/reset/sun8i-a23-a33-ccu.h>

#define CLK_PLL_CPUX		0
#define CLK_PLL_AUDIO_BASE	1
#define CLK_PLL_AUDIO		2
#define CLK_PLL_AUDIO_2X	3
#define CLK_PLL_AUDIO_4X	4
#define CLK_PLL_AUDIO_8X	5
#define CLK_PLL_VIDEO		6
#define CLK_PLL_VIDEO_2X	7
#define CLK_PLL_VE		8
#define CLK_PLL_DDR0		9
#define CLK_PLL_PERIPH		10
#define CLK_PLL_PERIPH_2X	11
#define CLK_PLL_GPU		12



#define CLK_PLL_HSIC		14
#define CLK_PLL_DE		15
#define CLK_PLL_DDR1		16
#define CLK_PLL_DDR		17



#define CLK_AXI			19
#define CLK_AHB1		20
#define CLK_APB1		21
#define CLK_APB2		22





#define CLK_DRAM		79



#define CLK_MBUS		95



#define CLK_NUMBER		(CLK_ATS + 1)

#endif 
