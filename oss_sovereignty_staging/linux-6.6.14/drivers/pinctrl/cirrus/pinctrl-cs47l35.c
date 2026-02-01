
 

#include <linux/err.h>
#include <linux/mfd/madera/core.h>

#include "pinctrl-madera.h"

 
static const unsigned int cs47l35_aif3_pins[] = { 0, 1, 2, 3 };
static const unsigned int cs47l35_spk_pins[] = { 4, 5 };
static const unsigned int cs47l35_aif1_pins[] = { 7, 8, 9, 10 };
static const unsigned int cs47l35_aif2_pins[] = { 11, 12, 13, 14 };
static const unsigned int cs47l35_mif1_pins[] = { 6, 15 };

static const struct madera_pin_groups cs47l35_pin_groups[] = {
	{ "aif1", cs47l35_aif1_pins, ARRAY_SIZE(cs47l35_aif1_pins) },
	{ "aif2", cs47l35_aif2_pins, ARRAY_SIZE(cs47l35_aif2_pins) },
	{ "aif3", cs47l35_aif3_pins, ARRAY_SIZE(cs47l35_aif3_pins) },
	{ "mif1", cs47l35_mif1_pins, ARRAY_SIZE(cs47l35_mif1_pins) },
	{ "pdmspk1", cs47l35_spk_pins, ARRAY_SIZE(cs47l35_spk_pins) },
};

const struct madera_pin_chip cs47l35_pin_chip = {
	.n_pins = CS47L35_NUM_GPIOS,
	.pin_groups = cs47l35_pin_groups,
	.n_pin_groups = ARRAY_SIZE(cs47l35_pin_groups),
};
