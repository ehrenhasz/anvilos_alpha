
 

#include <linux/cpuhotplug.h>
#include <linux/firmware/xlnx-event-manager.h>
#include <linux/firmware/xlnx-zynqmp.h>
#include <linux/hashtable.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

static DEFINE_PER_CPU_READ_MOSTLY(int, cpu_number1);

static int virq_sgi;
static int event_manager_availability = -EACCES;

 
#define XLNX_EVENT_SGI_NUM	(15)

 
#define MAX_DRIVER_PER_EVENT	(10U)

 
#define REGISTERED_DRIVER_MAX_ORDER	(7)

#define MAX_BITS	(32U)  

#define FIRMWARE_VERSION_MASK			(0xFFFFU)
#define REGISTER_NOTIFIER_FIRMWARE_VERSION	(2U)

static DEFINE_HASHTABLE(reg_driver_map, REGISTERED_DRIVER_MAX_ORDER);
static int sgi_num = XLNX_EVENT_SGI_NUM;

static bool is_need_to_unregister;

 
struct agent_cb {
	void *agent_data;
	event_cb_func_t eve_cb;
	struct list_head list;
};

 
struct registered_event_data {
	u64 key;
	enum pm_api_cb_id cb_type;
	bool wake;
	struct list_head cb_list_head;
	struct hlist_node hentry;
};

static bool xlnx_is_error_event(const u32 node_id)
{
	if (node_id == EVENT_ERROR_PMC_ERR1 ||
	    node_id == EVENT_ERROR_PMC_ERR2 ||
	    node_id == EVENT_ERROR_PSM_ERR1 ||
	    node_id == EVENT_ERROR_PSM_ERR2)
		return true;

	return false;
}

static int xlnx_add_cb_for_notify_event(const u32 node_id, const u32 event, const bool wake,
					event_cb_func_t cb_fun,	void *data)
{
	u64 key = 0;
	bool present_in_hash = false;
	struct registered_event_data *eve_data;
	struct agent_cb *cb_data;
	struct agent_cb *cb_pos;
	struct agent_cb *cb_next;

	key = ((u64)node_id << 32U) | (u64)event;
	 
	hash_for_each_possible(reg_driver_map, eve_data, hentry, key) {
		if (eve_data->key == key) {
			present_in_hash = true;
			break;
		}
	}

	if (!present_in_hash) {
		 
		eve_data = kmalloc(sizeof(*eve_data), GFP_KERNEL);
		if (!eve_data)
			return -ENOMEM;
		eve_data->key = key;
		eve_data->cb_type = PM_NOTIFY_CB;
		eve_data->wake = wake;
		INIT_LIST_HEAD(&eve_data->cb_list_head);

		cb_data = kmalloc(sizeof(*cb_data), GFP_KERNEL);
		if (!cb_data) {
			kfree(eve_data);
			return -ENOMEM;
		}
		cb_data->eve_cb = cb_fun;
		cb_data->agent_data = data;

		 
		list_add(&cb_data->list, &eve_data->cb_list_head);

		 
		hash_add(reg_driver_map, &eve_data->hentry, key);
	} else {
		 
		list_for_each_entry_safe(cb_pos, cb_next, &eve_data->cb_list_head, list) {
			if (cb_pos->eve_cb == cb_fun &&
			    cb_pos->agent_data == data) {
				return 0;
			}
		}

		 
		cb_data = kmalloc(sizeof(*cb_data), GFP_KERNEL);
		if (!cb_data)
			return -ENOMEM;
		cb_data->eve_cb = cb_fun;
		cb_data->agent_data = data;

		list_add(&cb_data->list, &eve_data->cb_list_head);
	}

	return 0;
}

static int xlnx_add_cb_for_suspend(event_cb_func_t cb_fun, void *data)
{
	struct registered_event_data *eve_data;
	struct agent_cb *cb_data;

	 
	hash_for_each_possible(reg_driver_map, eve_data, hentry, PM_INIT_SUSPEND_CB) {
		if (eve_data->cb_type == PM_INIT_SUSPEND_CB) {
			pr_err("Found as already registered\n");
			return -EINVAL;
		}
	}

	 
	eve_data = kmalloc(sizeof(*eve_data), GFP_KERNEL);
	if (!eve_data)
		return -ENOMEM;

	eve_data->key = 0;
	eve_data->cb_type = PM_INIT_SUSPEND_CB;
	INIT_LIST_HEAD(&eve_data->cb_list_head);

	cb_data = kmalloc(sizeof(*cb_data), GFP_KERNEL);
	if (!cb_data)
		return -ENOMEM;
	cb_data->eve_cb = cb_fun;
	cb_data->agent_data = data;

	 
	list_add(&cb_data->list, &eve_data->cb_list_head);

	hash_add(reg_driver_map, &eve_data->hentry, PM_INIT_SUSPEND_CB);

	return 0;
}

static int xlnx_remove_cb_for_suspend(event_cb_func_t cb_fun)
{
	bool is_callback_found = false;
	struct registered_event_data *eve_data;
	struct agent_cb *cb_pos;
	struct agent_cb *cb_next;
	struct hlist_node *tmp;

	is_need_to_unregister = false;

	 
	hash_for_each_possible_safe(reg_driver_map, eve_data, tmp, hentry, PM_INIT_SUSPEND_CB) {
		if (eve_data->cb_type == PM_INIT_SUSPEND_CB) {
			 
			list_for_each_entry_safe(cb_pos, cb_next, &eve_data->cb_list_head, list) {
				if (cb_pos->eve_cb == cb_fun) {
					is_callback_found = true;
					list_del_init(&cb_pos->list);
					kfree(cb_pos);
				}
			}
			 
			hash_del(&eve_data->hentry);
			kfree(eve_data);
			is_need_to_unregister = true;
		}
	}
	if (!is_callback_found) {
		pr_warn("Didn't find any registered callback for suspend event\n");
		return -EINVAL;
	}

	return 0;
}

static int xlnx_remove_cb_for_notify_event(const u32 node_id, const u32 event,
					   event_cb_func_t cb_fun, void *data)
{
	bool is_callback_found = false;
	struct registered_event_data *eve_data;
	u64 key = ((u64)node_id << 32U) | (u64)event;
	struct agent_cb *cb_pos;
	struct agent_cb *cb_next;
	struct hlist_node *tmp;

	is_need_to_unregister = false;

	 
	hash_for_each_possible_safe(reg_driver_map, eve_data, tmp, hentry, key) {
		if (eve_data->key == key) {
			 
			list_for_each_entry_safe(cb_pos, cb_next, &eve_data->cb_list_head, list) {
				if (cb_pos->eve_cb == cb_fun &&
				    cb_pos->agent_data == data) {
					is_callback_found = true;
					list_del_init(&cb_pos->list);
					kfree(cb_pos);
				}
			}

			 
			if (list_empty(&eve_data->cb_list_head)) {
				 
				hash_del(&eve_data->hentry);
				kfree(eve_data);
				is_need_to_unregister = true;
			}
		}
	}
	if (!is_callback_found) {
		pr_warn("Didn't find any registered callback for 0x%x 0x%x\n",
			node_id, event);
		return -EINVAL;
	}

	return 0;
}

 
int xlnx_register_event(const enum pm_api_cb_id cb_type, const u32 node_id, const u32 event,
			const bool wake, event_cb_func_t cb_fun, void *data)
{
	int ret = 0;
	u32 eve;
	int pos;

	if (event_manager_availability)
		return event_manager_availability;

	if (cb_type != PM_NOTIFY_CB && cb_type != PM_INIT_SUSPEND_CB) {
		pr_err("%s() Unsupported Callback 0x%x\n", __func__, cb_type);
		return -EINVAL;
	}

	if (!cb_fun)
		return -EFAULT;

	if (cb_type == PM_INIT_SUSPEND_CB) {
		ret = xlnx_add_cb_for_suspend(cb_fun, data);
	} else {
		if (!xlnx_is_error_event(node_id)) {
			 
			ret = xlnx_add_cb_for_notify_event(node_id, event, wake, cb_fun, data);
		} else {
			 
			for (pos = 0; pos < MAX_BITS; pos++) {
				eve = event & (1 << pos);
				if (!eve)
					continue;

				 
				ret = xlnx_add_cb_for_notify_event(node_id, eve, wake, cb_fun,
								   data);
				 
				if (ret)
					break;
			}
			if (ret) {
				 
				pos--;
				 
				for ( ; pos >= 0; pos--) {
					eve = event & (1 << pos);
					if (!eve)
						continue;
					xlnx_remove_cb_for_notify_event(node_id, eve, cb_fun, data);
				}
			}
		}

		if (ret) {
			pr_err("%s() failed for 0x%x and 0x%x: %d\r\n", __func__, node_id,
			       event, ret);
			return ret;
		}

		 
		ret = zynqmp_pm_register_notifier(node_id, event, wake, true);
		if (ret) {
			pr_err("%s() failed for 0x%x and 0x%x: %d\r\n", __func__, node_id,
			       event, ret);
			 
			if (xlnx_is_error_event(node_id)) {
				for (pos = 0; pos < MAX_BITS; pos++) {
					eve = event & (1 << pos);
					if (!eve)
						continue;
					xlnx_remove_cb_for_notify_event(node_id, eve, cb_fun, data);
				}
			} else {
				xlnx_remove_cb_for_notify_event(node_id, event, cb_fun, data);
			}
			return ret;
		}
	}

	return ret;
}
EXPORT_SYMBOL_GPL(xlnx_register_event);

 
int xlnx_unregister_event(const enum pm_api_cb_id cb_type, const u32 node_id, const u32 event,
			  event_cb_func_t cb_fun, void *data)
{
	int ret = 0;
	u32 eve, pos;

	is_need_to_unregister = false;

	if (event_manager_availability)
		return event_manager_availability;

	if (cb_type != PM_NOTIFY_CB && cb_type != PM_INIT_SUSPEND_CB) {
		pr_err("%s() Unsupported Callback 0x%x\n", __func__, cb_type);
		return -EINVAL;
	}

	if (!cb_fun)
		return -EFAULT;

	if (cb_type == PM_INIT_SUSPEND_CB) {
		ret = xlnx_remove_cb_for_suspend(cb_fun);
	} else {
		 
		if (!xlnx_is_error_event(node_id)) {
			xlnx_remove_cb_for_notify_event(node_id, event, cb_fun, data);
		} else {
			for (pos = 0; pos < MAX_BITS; pos++) {
				eve = event & (1 << pos);
				if (!eve)
					continue;

				xlnx_remove_cb_for_notify_event(node_id, eve, cb_fun, data);
			}
		}

		 
		if (is_need_to_unregister) {
			 
			ret = zynqmp_pm_register_notifier(node_id, event, false, false);
			if (ret) {
				pr_err("%s() failed for 0x%x and 0x%x: %d\n",
				       __func__, node_id, event, ret);
				return ret;
			}
		}
	}

	return ret;
}
EXPORT_SYMBOL_GPL(xlnx_unregister_event);

static void xlnx_call_suspend_cb_handler(const u32 *payload)
{
	bool is_callback_found = false;
	struct registered_event_data *eve_data;
	u32 cb_type = payload[0];
	struct agent_cb *cb_pos;
	struct agent_cb *cb_next;

	 
	hash_for_each_possible(reg_driver_map, eve_data, hentry, cb_type) {
		if (eve_data->cb_type == cb_type) {
			list_for_each_entry_safe(cb_pos, cb_next, &eve_data->cb_list_head, list) {
				cb_pos->eve_cb(&payload[0], cb_pos->agent_data);
				is_callback_found = true;
			}
		}
	}
	if (!is_callback_found)
		pr_warn("Didn't find any registered callback for suspend event\n");
}

static void xlnx_call_notify_cb_handler(const u32 *payload)
{
	bool is_callback_found = false;
	struct registered_event_data *eve_data;
	u64 key = ((u64)payload[1] << 32U) | (u64)payload[2];
	int ret;
	struct agent_cb *cb_pos;
	struct agent_cb *cb_next;

	 
	hash_for_each_possible(reg_driver_map, eve_data, hentry, key) {
		if (eve_data->key == key) {
			list_for_each_entry_safe(cb_pos, cb_next, &eve_data->cb_list_head, list) {
				cb_pos->eve_cb(&payload[0], cb_pos->agent_data);
				is_callback_found = true;
			}

			 
			ret = zynqmp_pm_register_notifier(payload[1], payload[2],
							  eve_data->wake, true);
			if (ret) {
				pr_err("%s() failed for 0x%x and 0x%x: %d\r\n", __func__,
				       payload[1], payload[2], ret);
				list_for_each_entry_safe(cb_pos, cb_next, &eve_data->cb_list_head,
							 list) {
					 
					xlnx_remove_cb_for_notify_event(payload[1], payload[2],
									cb_pos->eve_cb,
									cb_pos->agent_data);
				}
			}
		}
	}
	if (!is_callback_found)
		pr_warn("Didn't find any registered callback for 0x%x 0x%x\n",
			payload[1], payload[2]);
}

static void xlnx_get_event_callback_data(u32 *buf)
{
	zynqmp_pm_invoke_fn(GET_CALLBACK_DATA, 0, 0, 0, 0, buf);
}

static irqreturn_t xlnx_event_handler(int irq, void *dev_id)
{
	u32 cb_type, node_id, event, pos;
	u32 payload[CB_MAX_PAYLOAD_SIZE] = {0};
	u32 event_data[CB_MAX_PAYLOAD_SIZE] = {0};

	 
	xlnx_get_event_callback_data(payload);

	 
	cb_type = payload[0];

	if (cb_type == PM_NOTIFY_CB) {
		node_id = payload[1];
		event = payload[2];
		if (!xlnx_is_error_event(node_id)) {
			xlnx_call_notify_cb_handler(payload);
		} else {
			 
			memcpy(event_data, payload, (4 * CB_MAX_PAYLOAD_SIZE));
			 
			for (pos = 0; pos < MAX_BITS; pos++) {
				if ((0 == (event & (1 << pos))))
					continue;
				event_data[2] = (event & (1 << pos));
				xlnx_call_notify_cb_handler(event_data);
			}
		}
	} else if (cb_type == PM_INIT_SUSPEND_CB) {
		xlnx_call_suspend_cb_handler(payload);
	} else {
		pr_err("%s() Unsupported Callback %d\n", __func__, cb_type);
	}

	return IRQ_HANDLED;
}

static int xlnx_event_cpuhp_start(unsigned int cpu)
{
	enable_percpu_irq(virq_sgi, IRQ_TYPE_NONE);

	return 0;
}

static int xlnx_event_cpuhp_down(unsigned int cpu)
{
	disable_percpu_irq(virq_sgi);

	return 0;
}

static void xlnx_disable_percpu_irq(void *data)
{
	disable_percpu_irq(virq_sgi);
}

static int xlnx_event_init_sgi(struct platform_device *pdev)
{
	int ret = 0;
	int cpu = smp_processor_id();
	 
	struct irq_domain *domain;
	struct irq_fwspec sgi_fwspec;
	struct device_node *interrupt_parent = NULL;
	struct device *parent = pdev->dev.parent;

	 
	interrupt_parent = of_irq_find_parent(parent->of_node);
	if (!interrupt_parent) {
		dev_err(&pdev->dev, "Failed to find property for Interrupt parent\n");
		return -EINVAL;
	}

	 
	domain = irq_find_host(interrupt_parent);
	of_node_put(interrupt_parent);

	 
	sgi_fwspec.fwnode = domain->fwnode;

	 
	sgi_fwspec.param_count = 1;

	 
	sgi_fwspec.param[0] = sgi_num;
	virq_sgi = irq_create_fwspec_mapping(&sgi_fwspec);

	per_cpu(cpu_number1, cpu) = cpu;
	ret = request_percpu_irq(virq_sgi, xlnx_event_handler, "xlnx_event_mgmt",
				 &cpu_number1);
	WARN_ON(ret);
	if (ret) {
		irq_dispose_mapping(virq_sgi);
		return ret;
	}

	irq_to_desc(virq_sgi);
	irq_set_status_flags(virq_sgi, IRQ_PER_CPU);

	return ret;
}

static void xlnx_event_cleanup_sgi(struct platform_device *pdev)
{
	int cpu = smp_processor_id();

	per_cpu(cpu_number1, cpu) = cpu;

	cpuhp_remove_state(CPUHP_AP_ONLINE_DYN);

	on_each_cpu(xlnx_disable_percpu_irq, NULL, 1);

	irq_clear_status_flags(virq_sgi, IRQ_PER_CPU);
	free_percpu_irq(virq_sgi, &cpu_number1);
	irq_dispose_mapping(virq_sgi);
}

static int xlnx_event_manager_probe(struct platform_device *pdev)
{
	int ret;

	ret = zynqmp_pm_feature(PM_REGISTER_NOTIFIER);
	if (ret < 0) {
		dev_err(&pdev->dev, "Feature check failed with %d\n", ret);
		return ret;
	}

	if ((ret & FIRMWARE_VERSION_MASK) <
	    REGISTER_NOTIFIER_FIRMWARE_VERSION) {
		dev_err(&pdev->dev, "Register notifier version error. Expected Firmware: v%d - Found: v%d\n",
			REGISTER_NOTIFIER_FIRMWARE_VERSION,
			ret & FIRMWARE_VERSION_MASK);
		return -EOPNOTSUPP;
	}

	 
	ret = xlnx_event_init_sgi(pdev);
	if (ret) {
		dev_err(&pdev->dev, "SGI Init has been failed with %d\n", ret);
		return ret;
	}

	 
	cpuhp_setup_state(CPUHP_AP_ONLINE_DYN, "soc/event:starting",
			  xlnx_event_cpuhp_start, xlnx_event_cpuhp_down);

	ret = zynqmp_pm_register_sgi(sgi_num, 0);
	if (ret) {
		dev_err(&pdev->dev, "SGI %d Registration over TF-A failed with %d\n", sgi_num, ret);
		xlnx_event_cleanup_sgi(pdev);
		return ret;
	}

	event_manager_availability = 0;

	dev_info(&pdev->dev, "SGI %d Registered over TF-A\n", sgi_num);
	dev_info(&pdev->dev, "Xilinx Event Management driver probed\n");

	return ret;
}

static void xlnx_event_manager_remove(struct platform_device *pdev)
{
	int i;
	struct registered_event_data *eve_data;
	struct hlist_node *tmp;
	int ret;
	struct agent_cb *cb_pos;
	struct agent_cb *cb_next;

	hash_for_each_safe(reg_driver_map, i, tmp, eve_data, hentry) {
		list_for_each_entry_safe(cb_pos, cb_next, &eve_data->cb_list_head, list) {
			list_del_init(&cb_pos->list);
			kfree(cb_pos);
		}
		hash_del(&eve_data->hentry);
		kfree(eve_data);
	}

	ret = zynqmp_pm_register_sgi(0, 1);
	if (ret)
		dev_err(&pdev->dev, "SGI unregistration over TF-A failed with %d\n", ret);

	xlnx_event_cleanup_sgi(pdev);

	event_manager_availability = -EACCES;
}

static struct platform_driver xlnx_event_manager_driver = {
	.probe = xlnx_event_manager_probe,
	.remove_new = xlnx_event_manager_remove,
	.driver = {
		.name = "xlnx_event_manager",
	},
};
module_param(sgi_num, uint, 0);
module_platform_driver(xlnx_event_manager_driver);
