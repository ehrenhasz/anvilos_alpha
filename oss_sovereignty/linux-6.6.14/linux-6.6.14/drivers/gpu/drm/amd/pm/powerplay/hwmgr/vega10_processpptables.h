#ifndef VEGA10_PROCESSPPTABLES_H
#define VEGA10_PROCESSPPTABLES_H
#include "hwmgr.h"
enum Vega10_I2CLineID {
	Vega10_I2CLineID_DDC1 = 0x90,
	Vega10_I2CLineID_DDC2 = 0x91,
	Vega10_I2CLineID_DDC3 = 0x92,
	Vega10_I2CLineID_DDC4 = 0x93,
	Vega10_I2CLineID_DDC5 = 0x94,
	Vega10_I2CLineID_DDC6 = 0x95,
	Vega10_I2CLineID_SCLSDA = 0x96,
	Vega10_I2CLineID_DDCVGA = 0x97
};
#define Vega10_I2C_DDC1DATA          0
#define Vega10_I2C_DDC1CLK           1
#define Vega10_I2C_DDC2DATA          2
#define Vega10_I2C_DDC2CLK           3
#define Vega10_I2C_DDC3DATA          4
#define Vega10_I2C_DDC3CLK           5
#define Vega10_I2C_SDA               40
#define Vega10_I2C_SCL               41
#define Vega10_I2C_DDC4DATA          65
#define Vega10_I2C_DDC4CLK           66
#define Vega10_I2C_DDC5DATA          0x48
#define Vega10_I2C_DDC5CLK           0x49
#define Vega10_I2C_DDC6DATA          0x4a
#define Vega10_I2C_DDC6CLK           0x4b
#define Vega10_I2C_DDCVGADATA        0x4c
#define Vega10_I2C_DDCVGACLK         0x4d
extern const struct pp_table_func vega10_pptable_funcs;
extern int vega10_get_number_of_powerplay_table_entries(struct pp_hwmgr *hwmgr);
extern int vega10_get_powerplay_table_entry(struct pp_hwmgr *hwmgr, uint32_t entry_index,
		struct pp_power_state *power_state, int (*call_back_func)(struct pp_hwmgr *, void *,
				struct pp_power_state *, void *, uint32_t));
extern int vega10_baco_set_cap(struct pp_hwmgr *hwmgr);
#endif
