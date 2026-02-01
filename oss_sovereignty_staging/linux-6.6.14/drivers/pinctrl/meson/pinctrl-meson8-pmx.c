
 

 
#include <linux/device.h>
#include <linux/regmap.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>

#include "pinctrl-meson.h"
#include "pinctrl-meson8-pmx.h"

 
static void meson8_pmx_disable_other_groups(struct meson_pinctrl *pc,
					    unsigned int pin, int sel_group)
{
	struct meson_pmx_group *group;
	struct meson8_pmx_data *pmx_data;
	int i, j;

	for (i = 0; i < pc->data->num_groups; i++) {
		group = &pc->data->groups[i];
		pmx_data = (struct meson8_pmx_data *)group->data;
		if (pmx_data->is_gpio || i == sel_group)
			continue;

		for (j = 0; j < group->num_pins; j++) {
			if (group->pins[j] == pin) {
				 
				regmap_update_bits(pc->reg_mux,
						   pmx_data->reg * 4,
						   BIT(pmx_data->bit), 0);
			}
		}
	}
}

static int meson8_pmx_set_mux(struct pinctrl_dev *pcdev, unsigned func_num,
			      unsigned group_num)
{
	struct meson_pinctrl *pc = pinctrl_dev_get_drvdata(pcdev);
	struct meson_pmx_func *func = &pc->data->funcs[func_num];
	struct meson_pmx_group *group = &pc->data->groups[group_num];
	struct meson8_pmx_data *pmx_data =
		(struct meson8_pmx_data *)group->data;
	int i, ret = 0;

	dev_dbg(pc->dev, "enable function %s, group %s\n", func->name,
		group->name);

	 
	for (i = 0; i < group->num_pins; i++)
		meson8_pmx_disable_other_groups(pc, group->pins[i], group_num);

	 
	if (func_num)
		ret = regmap_update_bits(pc->reg_mux, pmx_data->reg * 4,
					 BIT(pmx_data->bit),
					 BIT(pmx_data->bit));

	return ret;
}

static int meson8_pmx_request_gpio(struct pinctrl_dev *pcdev,
				   struct pinctrl_gpio_range *range,
				   unsigned offset)
{
	struct meson_pinctrl *pc = pinctrl_dev_get_drvdata(pcdev);

	meson8_pmx_disable_other_groups(pc, offset, -1);

	return 0;
}

const struct pinmux_ops meson8_pmx_ops = {
	.set_mux = meson8_pmx_set_mux,
	.get_functions_count = meson_pmx_get_funcs_count,
	.get_function_name = meson_pmx_get_func_name,
	.get_function_groups = meson_pmx_get_groups,
	.gpio_request_enable = meson8_pmx_request_gpio,
};
EXPORT_SYMBOL_GPL(meson8_pmx_ops);
MODULE_LICENSE("GPL v2");
