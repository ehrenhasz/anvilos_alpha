
 
#include <linux/device.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/soc/qcom/smem_state.h>

static LIST_HEAD(smem_states);
static DEFINE_MUTEX(list_lock);

 
struct qcom_smem_state {
	struct kref refcount;
	bool orphan;

	struct list_head list;
	struct device_node *of_node;

	void *priv;

	struct qcom_smem_state_ops ops;
};

 
int qcom_smem_state_update_bits(struct qcom_smem_state *state,
				u32 mask,
				u32 value)
{
	if (state->orphan)
		return -ENXIO;

	if (!state->ops.update_bits)
		return -ENOTSUPP;

	return state->ops.update_bits(state->priv, mask, value);
}
EXPORT_SYMBOL_GPL(qcom_smem_state_update_bits);

static struct qcom_smem_state *of_node_to_state(struct device_node *np)
{
	struct qcom_smem_state *state;

	mutex_lock(&list_lock);

	list_for_each_entry(state, &smem_states, list) {
		if (state->of_node == np) {
			kref_get(&state->refcount);
			goto unlock;
		}
	}
	state = ERR_PTR(-EPROBE_DEFER);

unlock:
	mutex_unlock(&list_lock);

	return state;
}

 
struct qcom_smem_state *qcom_smem_state_get(struct device *dev,
					    const char *con_id,
					    unsigned *bit)
{
	struct qcom_smem_state *state;
	struct of_phandle_args args;
	int index = 0;
	int ret;

	if (con_id) {
		index = of_property_match_string(dev->of_node,
						 "qcom,smem-state-names",
						 con_id);
		if (index < 0) {
			dev_err(dev, "missing qcom,smem-state-names\n");
			return ERR_PTR(index);
		}
	}

	ret = of_parse_phandle_with_args(dev->of_node,
					 "qcom,smem-states",
					 "#qcom,smem-state-cells",
					 index,
					 &args);
	if (ret) {
		dev_err(dev, "failed to parse qcom,smem-states property\n");
		return ERR_PTR(ret);
	}

	if (args.args_count != 1) {
		dev_err(dev, "invalid #qcom,smem-state-cells\n");
		return ERR_PTR(-EINVAL);
	}

	state = of_node_to_state(args.np);
	if (IS_ERR(state))
		goto put;

	*bit = args.args[0];

put:
	of_node_put(args.np);
	return state;
}
EXPORT_SYMBOL_GPL(qcom_smem_state_get);

static void qcom_smem_state_release(struct kref *ref)
{
	struct qcom_smem_state *state = container_of(ref, struct qcom_smem_state, refcount);

	list_del(&state->list);
	of_node_put(state->of_node);
	kfree(state);
}

 
void qcom_smem_state_put(struct qcom_smem_state *state)
{
	mutex_lock(&list_lock);
	kref_put(&state->refcount, qcom_smem_state_release);
	mutex_unlock(&list_lock);
}
EXPORT_SYMBOL_GPL(qcom_smem_state_put);

static void devm_qcom_smem_state_release(struct device *dev, void *res)
{
	qcom_smem_state_put(*(struct qcom_smem_state **)res);
}

 
struct qcom_smem_state *devm_qcom_smem_state_get(struct device *dev,
						 const char *con_id,
						 unsigned *bit)
{
	struct qcom_smem_state **ptr, *state;

	ptr = devres_alloc(devm_qcom_smem_state_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	state = qcom_smem_state_get(dev, con_id, bit);
	if (!IS_ERR(state)) {
		*ptr = state;
		devres_add(dev, ptr);
	} else {
		devres_free(ptr);
	}

	return state;
}
EXPORT_SYMBOL_GPL(devm_qcom_smem_state_get);

 
struct qcom_smem_state *qcom_smem_state_register(struct device_node *of_node,
						 const struct qcom_smem_state_ops *ops,
						 void *priv)
{
	struct qcom_smem_state *state;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return ERR_PTR(-ENOMEM);

	kref_init(&state->refcount);

	state->of_node = of_node_get(of_node);
	state->ops = *ops;
	state->priv = priv;

	mutex_lock(&list_lock);
	list_add(&state->list, &smem_states);
	mutex_unlock(&list_lock);

	return state;
}
EXPORT_SYMBOL_GPL(qcom_smem_state_register);

 
void qcom_smem_state_unregister(struct qcom_smem_state *state)
{
	state->orphan = true;
	qcom_smem_state_put(state);
}
EXPORT_SYMBOL_GPL(qcom_smem_state_unregister);
