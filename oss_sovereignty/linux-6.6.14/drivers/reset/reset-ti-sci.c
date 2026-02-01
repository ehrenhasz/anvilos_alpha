
 

#include <linux/idr.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/reset-controller.h>
#include <linux/soc/ti/ti_sci_protocol.h>

 
struct ti_sci_reset_control {
	u32 dev_id;
	u32 reset_mask;
	struct mutex lock;
};

 
struct ti_sci_reset_data {
	struct reset_controller_dev rcdev;
	struct device *dev;
	const struct ti_sci_handle *sci;
	struct idr idr;
};

#define to_ti_sci_reset_data(p)	\
	container_of((p), struct ti_sci_reset_data, rcdev)

 
static int ti_sci_reset_set(struct reset_controller_dev *rcdev,
			    unsigned long id, bool assert)
{
	struct ti_sci_reset_data *data = to_ti_sci_reset_data(rcdev);
	const struct ti_sci_handle *sci = data->sci;
	const struct ti_sci_dev_ops *dev_ops = &sci->ops.dev_ops;
	struct ti_sci_reset_control *control;
	u32 reset_state;
	int ret;

	control = idr_find(&data->idr, id);
	if (!control)
		return -EINVAL;

	mutex_lock(&control->lock);

	ret = dev_ops->get_device_resets(sci, control->dev_id, &reset_state);
	if (ret)
		goto out;

	if (assert)
		reset_state |= control->reset_mask;
	else
		reset_state &= ~control->reset_mask;

	ret = dev_ops->set_device_resets(sci, control->dev_id, reset_state);
out:
	mutex_unlock(&control->lock);

	return ret;
}

 
static int ti_sci_reset_assert(struct reset_controller_dev *rcdev,
			       unsigned long id)
{
	return ti_sci_reset_set(rcdev, id, true);
}

 
static int ti_sci_reset_deassert(struct reset_controller_dev *rcdev,
				 unsigned long id)
{
	return ti_sci_reset_set(rcdev, id, false);
}

 
static int ti_sci_reset_status(struct reset_controller_dev *rcdev,
			       unsigned long id)
{
	struct ti_sci_reset_data *data = to_ti_sci_reset_data(rcdev);
	const struct ti_sci_handle *sci = data->sci;
	const struct ti_sci_dev_ops *dev_ops = &sci->ops.dev_ops;
	struct ti_sci_reset_control *control;
	u32 reset_state;
	int ret;

	control = idr_find(&data->idr, id);
	if (!control)
		return -EINVAL;

	ret = dev_ops->get_device_resets(sci, control->dev_id, &reset_state);
	if (ret)
		return ret;

	return reset_state & control->reset_mask;
}

static const struct reset_control_ops ti_sci_reset_ops = {
	.assert		= ti_sci_reset_assert,
	.deassert	= ti_sci_reset_deassert,
	.status		= ti_sci_reset_status,
};

 
static int ti_sci_reset_of_xlate(struct reset_controller_dev *rcdev,
				 const struct of_phandle_args *reset_spec)
{
	struct ti_sci_reset_data *data = to_ti_sci_reset_data(rcdev);
	struct ti_sci_reset_control *control;

	if (WARN_ON(reset_spec->args_count != rcdev->of_reset_n_cells))
		return -EINVAL;

	control = devm_kzalloc(data->dev, sizeof(*control), GFP_KERNEL);
	if (!control)
		return -ENOMEM;

	control->dev_id = reset_spec->args[0];
	control->reset_mask = reset_spec->args[1];
	mutex_init(&control->lock);

	return idr_alloc(&data->idr, control, 0, 0, GFP_KERNEL);
}

static const struct of_device_id ti_sci_reset_of_match[] = {
	{ .compatible = "ti,sci-reset", },
	{   },
};
MODULE_DEVICE_TABLE(of, ti_sci_reset_of_match);

static int ti_sci_reset_probe(struct platform_device *pdev)
{
	struct ti_sci_reset_data *data;

	if (!pdev->dev.of_node)
		return -ENODEV;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->sci = devm_ti_sci_get_handle(&pdev->dev);
	if (IS_ERR(data->sci))
		return PTR_ERR(data->sci);

	data->rcdev.ops = &ti_sci_reset_ops;
	data->rcdev.owner = THIS_MODULE;
	data->rcdev.of_node = pdev->dev.of_node;
	data->rcdev.of_reset_n_cells = 2;
	data->rcdev.of_xlate = ti_sci_reset_of_xlate;
	data->dev = &pdev->dev;
	idr_init(&data->idr);

	platform_set_drvdata(pdev, data);

	return reset_controller_register(&data->rcdev);
}

static int ti_sci_reset_remove(struct platform_device *pdev)
{
	struct ti_sci_reset_data *data = platform_get_drvdata(pdev);

	reset_controller_unregister(&data->rcdev);

	idr_destroy(&data->idr);

	return 0;
}

static struct platform_driver ti_sci_reset_driver = {
	.probe = ti_sci_reset_probe,
	.remove = ti_sci_reset_remove,
	.driver = {
		.name = "ti-sci-reset",
		.of_match_table = ti_sci_reset_of_match,
	},
};
module_platform_driver(ti_sci_reset_driver);

MODULE_AUTHOR("Andrew F. Davis <afd@ti.com>");
MODULE_DESCRIPTION("TI System Control Interface (TI SCI) Reset driver");
MODULE_LICENSE("GPL v2");
