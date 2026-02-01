







#include <linux/gpio/consumer.h>
#include <linux/string.h>
#include "cs35l41_hda_property.h"

 
static int lenovo_legion_no_acpi(struct cs35l41_hda *cs35l41, struct device *physdev, int id,
				 const char *hid)
{
	struct cs35l41_hw_cfg *hw_cfg = &cs35l41->hw_cfg;

	 
	cs35l41->index = id == 0x40 ? 0 : 1;
	cs35l41->channel_index = 0;
	cs35l41->reset_gpio = gpiod_get_index(physdev, NULL, 0, GPIOD_OUT_HIGH);
	cs35l41->speaker_id = cs35l41_get_speaker_id(physdev, 0, 0, 2);
	hw_cfg->spk_pos = cs35l41->index;
	hw_cfg->gpio2.func = CS35L41_INTERRUPT;
	hw_cfg->gpio2.valid = true;
	hw_cfg->valid = true;

	if (strcmp(hid, "CLSA0100") == 0) {
		hw_cfg->bst_type = CS35L41_EXT_BOOST_NO_VSPK_SWITCH;
	} else if (strcmp(hid, "CLSA0101") == 0) {
		hw_cfg->bst_type = CS35L41_EXT_BOOST;
		hw_cfg->gpio1.func = CS35l41_VSPK_SWITCH;
		hw_cfg->gpio1.valid = true;
	}

	return 0;
}

 
static int hp_vision_acpi_fix(struct cs35l41_hda *cs35l41, struct device *physdev, int id,
			      const char *hid)
{
	struct cs35l41_hw_cfg *hw_cfg = &cs35l41->hw_cfg;

	dev_info(cs35l41->dev, "Adding DSD properties for %s\n", cs35l41->acpi_subsystem_id);

	cs35l41->index = id;
	cs35l41->channel_index = 0;
	cs35l41->reset_gpio = gpiod_get_index(physdev, NULL, 1, GPIOD_OUT_HIGH);
	cs35l41->speaker_id = -ENOENT;
	hw_cfg->spk_pos = cs35l41->index ? 1 : 0; 
	hw_cfg->gpio1.func = CS35L41_NOT_USED;
	hw_cfg->gpio1.valid = true;
	hw_cfg->gpio2.func = CS35L41_INTERRUPT;
	hw_cfg->gpio2.valid = true;
	hw_cfg->bst_type = CS35L41_INT_BOOST;
	hw_cfg->bst_ind = 1000;
	hw_cfg->bst_ipk = 4500;
	hw_cfg->bst_cap = 24;
	hw_cfg->valid = true;

	return 0;
}

struct cs35l41_prop_model {
	const char *hid;
	const char *ssid;
	int (*add_prop)(struct cs35l41_hda *cs35l41, struct device *physdev, int id,
			const char *hid);
};

static const struct cs35l41_prop_model cs35l41_prop_model_table[] = {
	{ "CLSA0100", NULL, lenovo_legion_no_acpi },
	{ "CLSA0101", NULL, lenovo_legion_no_acpi },
	{ "CSC3551", "103C89C6", hp_vision_acpi_fix },
	{}
};

int cs35l41_add_dsd_properties(struct cs35l41_hda *cs35l41, struct device *physdev, int id,
			       const char *hid)
{
	const struct cs35l41_prop_model *model;

	for (model = cs35l41_prop_model_table; model->hid; model++) {
		if (!strcmp(model->hid, hid) &&
		    (!model->ssid ||
		     (cs35l41->acpi_subsystem_id &&
		      !strcmp(model->ssid, cs35l41->acpi_subsystem_id))))
			return model->add_prop(cs35l41, physdev, id, hid);
	}

	return -ENOENT;
}
