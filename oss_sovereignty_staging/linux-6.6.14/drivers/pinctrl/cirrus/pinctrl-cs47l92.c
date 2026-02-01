
 

#include <linux/err.h>
#include <linux/mfd/madera/core.h>

#include "pinctrl-madera.h"

 
static const unsigned int cs47l92_spk1_pins[] = { 2, 3 };
static const unsigned int cs47l92_aif1_pins[] = { 4, 5, 6, 7 };
static const unsigned int cs47l92_aif2_pins[] = { 8, 9, 10, 11 };
static const unsigned int cs47l92_aif3_pins[] = { 12, 13, 14, 15 };

static const struct madera_pin_groups cs47l92_pin_groups[] = {
	{ "aif1", cs47l92_aif1_pins, ARRAY_SIZE(cs47l92_aif1_pins) },
	{ "aif2", cs47l92_aif2_pins, ARRAY_SIZE(cs47l92_aif2_pins) },
	{ "aif3", cs47l92_aif3_pins, ARRAY_SIZE(cs47l92_aif3_pins) },
	{ "pdmspk1", cs47l92_spk1_pins, ARRAY_SIZE(cs47l92_spk1_pins) },
};

const struct madera_pin_chip cs47l92_pin_chip = {
	.n_pins = CS47L92_NUM_GPIOS,
	.pin_groups = cs47l92_pin_groups,
	.n_pin_groups = ARRAY_SIZE(cs47l92_pin_groups),
};
