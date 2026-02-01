
 

#include <linux/acpi.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pwm.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include <dt-bindings/pwm/pwm.h>

#define CREATE_TRACE_POINTS
#include <trace/events/pwm.h>

#define MAX_PWMS 1024

static DEFINE_MUTEX(pwm_lookup_lock);
static LIST_HEAD(pwm_lookup_list);

 
static DEFINE_MUTEX(pwm_lock);

static LIST_HEAD(pwm_chips);
static DECLARE_BITMAP(allocated_pwms, MAX_PWMS);

 
static int alloc_pwms(unsigned int count)
{
	unsigned int start;

	start = bitmap_find_next_zero_area(allocated_pwms, MAX_PWMS, 0,
					   count, 0);

	if (start + count > MAX_PWMS)
		return -ENOSPC;

	bitmap_set(allocated_pwms, start, count);

	return start;
}

 
static void free_pwms(struct pwm_chip *chip)
{
	bitmap_clear(allocated_pwms, chip->base, chip->npwm);

	kfree(chip->pwms);
	chip->pwms = NULL;
}

static struct pwm_chip *pwmchip_find_by_name(const char *name)
{
	struct pwm_chip *chip;

	if (!name)
		return NULL;

	mutex_lock(&pwm_lock);

	list_for_each_entry(chip, &pwm_chips, list) {
		const char *chip_name = dev_name(chip->dev);

		if (chip_name && strcmp(chip_name, name) == 0) {
			mutex_unlock(&pwm_lock);
			return chip;
		}
	}

	mutex_unlock(&pwm_lock);

	return NULL;
}

static int pwm_device_request(struct pwm_device *pwm, const char *label)
{
	int err;

	if (test_bit(PWMF_REQUESTED, &pwm->flags))
		return -EBUSY;

	if (!try_module_get(pwm->chip->ops->owner))
		return -ENODEV;

	if (pwm->chip->ops->request) {
		err = pwm->chip->ops->request(pwm->chip, pwm);
		if (err) {
			module_put(pwm->chip->ops->owner);
			return err;
		}
	}

	if (pwm->chip->ops->get_state) {
		 
		struct pwm_state state = { 0, };

		err = pwm->chip->ops->get_state(pwm->chip, pwm, &state);
		trace_pwm_get(pwm, &state, err);

		if (!err)
			pwm->state = state;

		if (IS_ENABLED(CONFIG_PWM_DEBUG))
			pwm->last = pwm->state;
	}

	set_bit(PWMF_REQUESTED, &pwm->flags);
	pwm->label = label;

	return 0;
}

struct pwm_device *
of_pwm_xlate_with_flags(struct pwm_chip *chip, const struct of_phandle_args *args)
{
	struct pwm_device *pwm;

	if (chip->of_pwm_n_cells < 2)
		return ERR_PTR(-EINVAL);

	 
	if (args->args_count < 2)
		return ERR_PTR(-EINVAL);

	if (args->args[0] >= chip->npwm)
		return ERR_PTR(-EINVAL);

	pwm = pwm_request_from_chip(chip, args->args[0], NULL);
	if (IS_ERR(pwm))
		return pwm;

	pwm->args.period = args->args[1];
	pwm->args.polarity = PWM_POLARITY_NORMAL;

	if (chip->of_pwm_n_cells >= 3) {
		if (args->args_count > 2 && args->args[2] & PWM_POLARITY_INVERTED)
			pwm->args.polarity = PWM_POLARITY_INVERSED;
	}

	return pwm;
}
EXPORT_SYMBOL_GPL(of_pwm_xlate_with_flags);

struct pwm_device *
of_pwm_single_xlate(struct pwm_chip *chip, const struct of_phandle_args *args)
{
	struct pwm_device *pwm;

	if (chip->of_pwm_n_cells < 1)
		return ERR_PTR(-EINVAL);

	 
	if (args->args_count != 1 && args->args_count != 2)
		return ERR_PTR(-EINVAL);

	pwm = pwm_request_from_chip(chip, 0, NULL);
	if (IS_ERR(pwm))
		return pwm;

	pwm->args.period = args->args[0];
	pwm->args.polarity = PWM_POLARITY_NORMAL;

	if (args->args_count == 2 && args->args[1] & PWM_POLARITY_INVERTED)
		pwm->args.polarity = PWM_POLARITY_INVERSED;

	return pwm;
}
EXPORT_SYMBOL_GPL(of_pwm_single_xlate);

static void of_pwmchip_add(struct pwm_chip *chip)
{
	if (!chip->dev || !chip->dev->of_node)
		return;

	if (!chip->of_xlate) {
		u32 pwm_cells;

		if (of_property_read_u32(chip->dev->of_node, "#pwm-cells",
					 &pwm_cells))
			pwm_cells = 2;

		chip->of_xlate = of_pwm_xlate_with_flags;
		chip->of_pwm_n_cells = pwm_cells;
	}

	of_node_get(chip->dev->of_node);
}

static void of_pwmchip_remove(struct pwm_chip *chip)
{
	if (chip->dev)
		of_node_put(chip->dev->of_node);
}

 
int pwm_set_chip_data(struct pwm_device *pwm, void *data)
{
	if (!pwm)
		return -EINVAL;

	pwm->chip_data = data;

	return 0;
}
EXPORT_SYMBOL_GPL(pwm_set_chip_data);

 
void *pwm_get_chip_data(struct pwm_device *pwm)
{
	return pwm ? pwm->chip_data : NULL;
}
EXPORT_SYMBOL_GPL(pwm_get_chip_data);

static bool pwm_ops_check(const struct pwm_chip *chip)
{
	const struct pwm_ops *ops = chip->ops;

	if (!ops->apply)
		return false;

	if (IS_ENABLED(CONFIG_PWM_DEBUG) && !ops->get_state)
		dev_warn(chip->dev,
			 "Please implement the .get_state() callback\n");

	return true;
}

 
int pwmchip_add(struct pwm_chip *chip)
{
	struct pwm_device *pwm;
	unsigned int i;
	int ret;

	if (!chip || !chip->dev || !chip->ops || !chip->npwm)
		return -EINVAL;

	if (!pwm_ops_check(chip))
		return -EINVAL;

	chip->pwms = kcalloc(chip->npwm, sizeof(*pwm), GFP_KERNEL);
	if (!chip->pwms)
		return -ENOMEM;

	mutex_lock(&pwm_lock);

	ret = alloc_pwms(chip->npwm);
	if (ret < 0) {
		mutex_unlock(&pwm_lock);
		kfree(chip->pwms);
		return ret;
	}

	chip->base = ret;

	for (i = 0; i < chip->npwm; i++) {
		pwm = &chip->pwms[i];

		pwm->chip = chip;
		pwm->pwm = chip->base + i;
		pwm->hwpwm = i;
	}

	list_add(&chip->list, &pwm_chips);

	mutex_unlock(&pwm_lock);

	if (IS_ENABLED(CONFIG_OF))
		of_pwmchip_add(chip);

	pwmchip_sysfs_export(chip);

	return 0;
}
EXPORT_SYMBOL_GPL(pwmchip_add);

 
void pwmchip_remove(struct pwm_chip *chip)
{
	pwmchip_sysfs_unexport(chip);

	if (IS_ENABLED(CONFIG_OF))
		of_pwmchip_remove(chip);

	mutex_lock(&pwm_lock);

	list_del_init(&chip->list);

	free_pwms(chip);

	mutex_unlock(&pwm_lock);
}
EXPORT_SYMBOL_GPL(pwmchip_remove);

static void devm_pwmchip_remove(void *data)
{
	struct pwm_chip *chip = data;

	pwmchip_remove(chip);
}

int devm_pwmchip_add(struct device *dev, struct pwm_chip *chip)
{
	int ret;

	ret = pwmchip_add(chip);
	if (ret)
		return ret;

	return devm_add_action_or_reset(dev, devm_pwmchip_remove, chip);
}
EXPORT_SYMBOL_GPL(devm_pwmchip_add);

 
struct pwm_device *pwm_request_from_chip(struct pwm_chip *chip,
					 unsigned int index,
					 const char *label)
{
	struct pwm_device *pwm;
	int err;

	if (!chip || index >= chip->npwm)
		return ERR_PTR(-EINVAL);

	mutex_lock(&pwm_lock);
	pwm = &chip->pwms[index];

	err = pwm_device_request(pwm, label);
	if (err < 0)
		pwm = ERR_PTR(err);

	mutex_unlock(&pwm_lock);
	return pwm;
}
EXPORT_SYMBOL_GPL(pwm_request_from_chip);

static void pwm_apply_state_debug(struct pwm_device *pwm,
				  const struct pwm_state *state)
{
	struct pwm_state *last = &pwm->last;
	struct pwm_chip *chip = pwm->chip;
	struct pwm_state s1 = { 0 }, s2 = { 0 };
	int err;

	if (!IS_ENABLED(CONFIG_PWM_DEBUG))
		return;

	 
	if (!chip->ops->get_state)
		return;

	 

	err = chip->ops->get_state(chip, pwm, &s1);
	trace_pwm_get(pwm, &s1, err);
	if (err)
		 
		return;

	 
	if (s1.enabled && s1.polarity != state->polarity) {
		s2.polarity = state->polarity;
		s2.duty_cycle = s1.period - s1.duty_cycle;
		s2.period = s1.period;
		s2.enabled = s1.enabled;
	} else {
		s2 = s1;
	}

	if (s2.polarity != state->polarity &&
	    state->duty_cycle < state->period)
		dev_warn(chip->dev, ".apply ignored .polarity\n");

	if (state->enabled &&
	    last->polarity == state->polarity &&
	    last->period > s2.period &&
	    last->period <= state->period)
		dev_warn(chip->dev,
			 ".apply didn't pick the best available period (requested: %llu, applied: %llu, possible: %llu)\n",
			 state->period, s2.period, last->period);

	if (state->enabled && state->period < s2.period)
		dev_warn(chip->dev,
			 ".apply is supposed to round down period (requested: %llu, applied: %llu)\n",
			 state->period, s2.period);

	if (state->enabled &&
	    last->polarity == state->polarity &&
	    last->period == s2.period &&
	    last->duty_cycle > s2.duty_cycle &&
	    last->duty_cycle <= state->duty_cycle)
		dev_warn(chip->dev,
			 ".apply didn't pick the best available duty cycle (requested: %llu/%llu, applied: %llu/%llu, possible: %llu/%llu)\n",
			 state->duty_cycle, state->period,
			 s2.duty_cycle, s2.period,
			 last->duty_cycle, last->period);

	if (state->enabled && state->duty_cycle < s2.duty_cycle)
		dev_warn(chip->dev,
			 ".apply is supposed to round down duty_cycle (requested: %llu/%llu, applied: %llu/%llu)\n",
			 state->duty_cycle, state->period,
			 s2.duty_cycle, s2.period);

	if (!state->enabled && s2.enabled && s2.duty_cycle > 0)
		dev_warn(chip->dev,
			 "requested disabled, but yielded enabled with duty > 0\n");

	 
	err = chip->ops->apply(chip, pwm, &s1);
	trace_pwm_apply(pwm, &s1, err);
	if (err) {
		*last = s1;
		dev_err(chip->dev, "failed to reapply current setting\n");
		return;
	}

	*last = (struct pwm_state){ 0 };
	err = chip->ops->get_state(chip, pwm, last);
	trace_pwm_get(pwm, last, err);
	if (err)
		return;

	 
	if (s1.enabled != last->enabled ||
	    s1.polarity != last->polarity ||
	    (s1.enabled && s1.period != last->period) ||
	    (s1.enabled && s1.duty_cycle != last->duty_cycle)) {
		dev_err(chip->dev,
			".apply is not idempotent (ena=%d pol=%d %llu/%llu) -> (ena=%d pol=%d %llu/%llu)\n",
			s1.enabled, s1.polarity, s1.duty_cycle, s1.period,
			last->enabled, last->polarity, last->duty_cycle,
			last->period);
	}
}

 
int pwm_apply_state(struct pwm_device *pwm, const struct pwm_state *state)
{
	struct pwm_chip *chip;
	int err;

	 
	might_sleep();

	if (!pwm || !state || !state->period ||
	    state->duty_cycle > state->period)
		return -EINVAL;

	chip = pwm->chip;

	if (state->period == pwm->state.period &&
	    state->duty_cycle == pwm->state.duty_cycle &&
	    state->polarity == pwm->state.polarity &&
	    state->enabled == pwm->state.enabled &&
	    state->usage_power == pwm->state.usage_power)
		return 0;

	err = chip->ops->apply(chip, pwm, state);
	trace_pwm_apply(pwm, state, err);
	if (err)
		return err;

	pwm->state = *state;

	 
	pwm_apply_state_debug(pwm, state);

	return 0;
}
EXPORT_SYMBOL_GPL(pwm_apply_state);

 
int pwm_capture(struct pwm_device *pwm, struct pwm_capture *result,
		unsigned long timeout)
{
	int err;

	if (!pwm || !pwm->chip->ops)
		return -EINVAL;

	if (!pwm->chip->ops->capture)
		return -ENOSYS;

	mutex_lock(&pwm_lock);
	err = pwm->chip->ops->capture(pwm->chip, pwm, result, timeout);
	mutex_unlock(&pwm_lock);

	return err;
}
EXPORT_SYMBOL_GPL(pwm_capture);

 
int pwm_adjust_config(struct pwm_device *pwm)
{
	struct pwm_state state;
	struct pwm_args pargs;

	pwm_get_args(pwm, &pargs);
	pwm_get_state(pwm, &state);

	 
	if (!state.period) {
		state.duty_cycle = 0;
		state.period = pargs.period;
		state.polarity = pargs.polarity;

		return pwm_apply_state(pwm, &state);
	}

	 
	if (pargs.period != state.period) {
		u64 dutycycle = (u64)state.duty_cycle * pargs.period;

		do_div(dutycycle, state.period);
		state.duty_cycle = dutycycle;
		state.period = pargs.period;
	}

	 
	if (pargs.polarity != state.polarity) {
		state.polarity = pargs.polarity;
		state.duty_cycle = state.period - state.duty_cycle;
	}

	return pwm_apply_state(pwm, &state);
}
EXPORT_SYMBOL_GPL(pwm_adjust_config);

static struct pwm_chip *fwnode_to_pwmchip(struct fwnode_handle *fwnode)
{
	struct pwm_chip *chip;

	mutex_lock(&pwm_lock);

	list_for_each_entry(chip, &pwm_chips, list)
		if (chip->dev && device_match_fwnode(chip->dev, fwnode)) {
			mutex_unlock(&pwm_lock);
			return chip;
		}

	mutex_unlock(&pwm_lock);

	return ERR_PTR(-EPROBE_DEFER);
}

static struct device_link *pwm_device_link_add(struct device *dev,
					       struct pwm_device *pwm)
{
	struct device_link *dl;

	if (!dev) {
		 
		dev_warn(pwm->chip->dev,
			 "No consumer device specified to create a link to\n");
		return NULL;
	}

	dl = device_link_add(dev, pwm->chip->dev, DL_FLAG_AUTOREMOVE_CONSUMER);
	if (!dl) {
		dev_err(dev, "failed to create device link to %s\n",
			dev_name(pwm->chip->dev));
		return ERR_PTR(-EINVAL);
	}

	return dl;
}

 
static struct pwm_device *of_pwm_get(struct device *dev, struct device_node *np,
				     const char *con_id)
{
	struct pwm_device *pwm = NULL;
	struct of_phandle_args args;
	struct device_link *dl;
	struct pwm_chip *chip;
	int index = 0;
	int err;

	if (con_id) {
		index = of_property_match_string(np, "pwm-names", con_id);
		if (index < 0)
			return ERR_PTR(index);
	}

	err = of_parse_phandle_with_args(np, "pwms", "#pwm-cells", index,
					 &args);
	if (err) {
		pr_err("%s(): can't parse \"pwms\" property\n", __func__);
		return ERR_PTR(err);
	}

	chip = fwnode_to_pwmchip(of_fwnode_handle(args.np));
	if (IS_ERR(chip)) {
		if (PTR_ERR(chip) != -EPROBE_DEFER)
			pr_err("%s(): PWM chip not found\n", __func__);

		pwm = ERR_CAST(chip);
		goto put;
	}

	pwm = chip->of_xlate(chip, &args);
	if (IS_ERR(pwm))
		goto put;

	dl = pwm_device_link_add(dev, pwm);
	if (IS_ERR(dl)) {
		 
		pwm_put(pwm);
		pwm = ERR_CAST(dl);
		goto put;
	}

	 
	if (!con_id) {
		err = of_property_read_string_index(np, "pwm-names", index,
						    &con_id);
		if (err < 0)
			con_id = np->name;
	}

	pwm->label = con_id;

put:
	of_node_put(args.np);

	return pwm;
}

 
static struct pwm_device *acpi_pwm_get(const struct fwnode_handle *fwnode)
{
	struct pwm_device *pwm;
	struct fwnode_reference_args args;
	struct pwm_chip *chip;
	int ret;

	memset(&args, 0, sizeof(args));

	ret = __acpi_node_get_property_reference(fwnode, "pwms", 0, 3, &args);
	if (ret < 0)
		return ERR_PTR(ret);

	if (args.nargs < 2)
		return ERR_PTR(-EPROTO);

	chip = fwnode_to_pwmchip(args.fwnode);
	if (IS_ERR(chip))
		return ERR_CAST(chip);

	pwm = pwm_request_from_chip(chip, args.args[0], NULL);
	if (IS_ERR(pwm))
		return pwm;

	pwm->args.period = args.args[1];
	pwm->args.polarity = PWM_POLARITY_NORMAL;

	if (args.nargs > 2 && args.args[2] & PWM_POLARITY_INVERTED)
		pwm->args.polarity = PWM_POLARITY_INVERSED;

	return pwm;
}

 
void pwm_add_table(struct pwm_lookup *table, size_t num)
{
	mutex_lock(&pwm_lookup_lock);

	while (num--) {
		list_add_tail(&table->list, &pwm_lookup_list);
		table++;
	}

	mutex_unlock(&pwm_lookup_lock);
}

 
void pwm_remove_table(struct pwm_lookup *table, size_t num)
{
	mutex_lock(&pwm_lookup_lock);

	while (num--) {
		list_del(&table->list);
		table++;
	}

	mutex_unlock(&pwm_lookup_lock);
}

 
struct pwm_device *pwm_get(struct device *dev, const char *con_id)
{
	const struct fwnode_handle *fwnode = dev ? dev_fwnode(dev) : NULL;
	const char *dev_id = dev ? dev_name(dev) : NULL;
	struct pwm_device *pwm;
	struct pwm_chip *chip;
	struct device_link *dl;
	unsigned int best = 0;
	struct pwm_lookup *p, *chosen = NULL;
	unsigned int match;
	int err;

	 
	if (is_of_node(fwnode))
		return of_pwm_get(dev, to_of_node(fwnode), con_id);

	 
	if (is_acpi_node(fwnode)) {
		pwm = acpi_pwm_get(fwnode);
		if (!IS_ERR(pwm) || PTR_ERR(pwm) != -ENOENT)
			return pwm;
	}

	 
	mutex_lock(&pwm_lookup_lock);

	list_for_each_entry(p, &pwm_lookup_list, list) {
		match = 0;

		if (p->dev_id) {
			if (!dev_id || strcmp(p->dev_id, dev_id))
				continue;

			match += 2;
		}

		if (p->con_id) {
			if (!con_id || strcmp(p->con_id, con_id))
				continue;

			match += 1;
		}

		if (match > best) {
			chosen = p;

			if (match != 3)
				best = match;
			else
				break;
		}
	}

	mutex_unlock(&pwm_lookup_lock);

	if (!chosen)
		return ERR_PTR(-ENODEV);

	chip = pwmchip_find_by_name(chosen->provider);

	 
	if (!chip && chosen->module) {
		err = request_module(chosen->module);
		if (err == 0)
			chip = pwmchip_find_by_name(chosen->provider);
	}

	if (!chip)
		return ERR_PTR(-EPROBE_DEFER);

	pwm = pwm_request_from_chip(chip, chosen->index, con_id ?: dev_id);
	if (IS_ERR(pwm))
		return pwm;

	dl = pwm_device_link_add(dev, pwm);
	if (IS_ERR(dl)) {
		pwm_put(pwm);
		return ERR_CAST(dl);
	}

	pwm->args.period = chosen->period;
	pwm->args.polarity = chosen->polarity;

	return pwm;
}
EXPORT_SYMBOL_GPL(pwm_get);

 
void pwm_put(struct pwm_device *pwm)
{
	if (!pwm)
		return;

	mutex_lock(&pwm_lock);

	if (!test_and_clear_bit(PWMF_REQUESTED, &pwm->flags)) {
		pr_warn("PWM device already freed\n");
		goto out;
	}

	if (pwm->chip->ops->free)
		pwm->chip->ops->free(pwm->chip, pwm);

	pwm_set_chip_data(pwm, NULL);
	pwm->label = NULL;

	module_put(pwm->chip->ops->owner);
out:
	mutex_unlock(&pwm_lock);
}
EXPORT_SYMBOL_GPL(pwm_put);

static void devm_pwm_release(void *pwm)
{
	pwm_put(pwm);
}

 
struct pwm_device *devm_pwm_get(struct device *dev, const char *con_id)
{
	struct pwm_device *pwm;
	int ret;

	pwm = pwm_get(dev, con_id);
	if (IS_ERR(pwm))
		return pwm;

	ret = devm_add_action_or_reset(dev, devm_pwm_release, pwm);
	if (ret)
		return ERR_PTR(ret);

	return pwm;
}
EXPORT_SYMBOL_GPL(devm_pwm_get);

 
struct pwm_device *devm_fwnode_pwm_get(struct device *dev,
				       struct fwnode_handle *fwnode,
				       const char *con_id)
{
	struct pwm_device *pwm = ERR_PTR(-ENODEV);
	int ret;

	if (is_of_node(fwnode))
		pwm = of_pwm_get(dev, to_of_node(fwnode), con_id);
	else if (is_acpi_node(fwnode))
		pwm = acpi_pwm_get(fwnode);
	if (IS_ERR(pwm))
		return pwm;

	ret = devm_add_action_or_reset(dev, devm_pwm_release, pwm);
	if (ret)
		return ERR_PTR(ret);

	return pwm;
}
EXPORT_SYMBOL_GPL(devm_fwnode_pwm_get);

#ifdef CONFIG_DEBUG_FS
static void pwm_dbg_show(struct pwm_chip *chip, struct seq_file *s)
{
	unsigned int i;

	for (i = 0; i < chip->npwm; i++) {
		struct pwm_device *pwm = &chip->pwms[i];
		struct pwm_state state;

		pwm_get_state(pwm, &state);

		seq_printf(s, " pwm-%-3d (%-20.20s):", i, pwm->label);

		if (test_bit(PWMF_REQUESTED, &pwm->flags))
			seq_puts(s, " requested");

		if (state.enabled)
			seq_puts(s, " enabled");

		seq_printf(s, " period: %llu ns", state.period);
		seq_printf(s, " duty: %llu ns", state.duty_cycle);
		seq_printf(s, " polarity: %s",
			   state.polarity ? "inverse" : "normal");

		if (state.usage_power)
			seq_puts(s, " usage_power");

		seq_puts(s, "\n");
	}
}

static void *pwm_seq_start(struct seq_file *s, loff_t *pos)
{
	mutex_lock(&pwm_lock);
	s->private = "";

	return seq_list_start(&pwm_chips, *pos);
}

static void *pwm_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	s->private = "\n";

	return seq_list_next(v, &pwm_chips, pos);
}

static void pwm_seq_stop(struct seq_file *s, void *v)
{
	mutex_unlock(&pwm_lock);
}

static int pwm_seq_show(struct seq_file *s, void *v)
{
	struct pwm_chip *chip = list_entry(v, struct pwm_chip, list);

	seq_printf(s, "%s%s/%s, %d PWM device%s\n", (char *)s->private,
		   chip->dev->bus ? chip->dev->bus->name : "no-bus",
		   dev_name(chip->dev), chip->npwm,
		   (chip->npwm != 1) ? "s" : "");

	pwm_dbg_show(chip, s);

	return 0;
}

static const struct seq_operations pwm_debugfs_sops = {
	.start = pwm_seq_start,
	.next = pwm_seq_next,
	.stop = pwm_seq_stop,
	.show = pwm_seq_show,
};

DEFINE_SEQ_ATTRIBUTE(pwm_debugfs);

static int __init pwm_debugfs_init(void)
{
	debugfs_create_file("pwm", 0444, NULL, NULL, &pwm_debugfs_fops);

	return 0;
}
subsys_initcall(pwm_debugfs_init);
#endif  
