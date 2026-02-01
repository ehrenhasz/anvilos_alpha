 
 

#ifndef __STM32_DAC_CORE_H
#define __STM32_DAC_CORE_H

#include <linux/regmap.h>

 
#define STM32_DAC_CR		0x00
#define STM32_DAC_DHR12R1	0x08
#define STM32_DAC_DHR12R2	0x14
#define STM32_DAC_DOR1		0x2C
#define STM32_DAC_DOR2		0x30

 
#define STM32_DAC_CR_EN1		BIT(0)
#define STM32H7_DAC_CR_HFSEL		BIT(15)
#define STM32_DAC_CR_EN2		BIT(16)

 
struct stm32_dac_common {
	struct regmap			*regmap;
	int				vref_mv;
	bool				hfsel;
};

#endif
