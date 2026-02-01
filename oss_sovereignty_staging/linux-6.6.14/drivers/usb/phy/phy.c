
 
#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of.h>

#include <linux/usb/phy.h>

 
#define DEFAULT_SDP_CUR_MIN	2
#define DEFAULT_SDP_CUR_MAX	500
#define DEFAULT_SDP_CUR_MIN_SS	150
#define DEFAULT_SDP_CUR_MAX_SS	900
#define DEFAULT_DCP_CUR_MIN	500
#define DEFAULT_DCP_CUR_MAX	5000
#define DEFAULT_CDP_CUR_MIN	1500
#define DEFAULT_CDP_CUR_MAX	5000
#define DEFAULT_ACA_CUR_MIN	1500
#define DEFAULT_ACA_CUR_MAX	5000

static LIST_HEAD(phy_list);
static DEFINE_SPINLOCK(phy_lock);

struct phy_devm {
	struct usb_phy *phy;
	struct notifier_block *nb;
};

static const char *const usb_chger_type[] = {
	[UNKNOWN_TYPE]			= "USB_CHARGER_UNKNOWN_TYPE",
	[SDP_TYPE]			= "USB_CHARGER_SDP_TYPE",
	[CDP_TYPE]			= "USB_CHARGER_CDP_TYPE",
	[DCP_TYPE]			= "USB_CHARGER_DCP_TYPE",
	[ACA_TYPE]			= "USB_CHARGER_ACA_TYPE",
};

static const char *const usb_chger_state[] = {
	[USB_CHARGER_DEFAULT]	= "USB_CHARGER_DEFAULT",
	[USB_CHARGER_PRESENT]	= "USB_CHARGER_PRESENT",
	[USB_CHARGER_ABSENT]	= "USB_CHARGER_ABSENT",
};

static struct usb_phy *__usb_find_phy(struct list_head *list,
	enum usb_phy_type type)
{
	struct usb_phy  *phy = NULL;

	list_for_each_entry(phy, list, head) {
		if (phy->type != type)
			continue;

		return phy;
	}

	return ERR_PTR(-ENODEV);
}

static struct usb_phy *__of_usb_find_phy(struct device_node *node)
{
	struct usb_phy  *phy;

	if (!of_device_is_available(node))
		return ERR_PTR(-ENODEV);

	list_for_each_entry(phy, &phy_list, head) {
		if (node != phy->dev->of_node)
			continue;

		return phy;
	}

	return ERR_PTR(-EPROBE_DEFER);
}

static struct usb_phy *__device_to_usb_phy(const struct device *dev)
{
	struct usb_phy *usb_phy;

	list_for_each_entry(usb_phy, &phy_list, head) {
		if (usb_phy->dev == dev)
			return usb_phy;
	}

	return NULL;
}

static void usb_phy_set_default_current(struct usb_phy *usb_phy)
{
	usb_phy->chg_cur.sdp_min = DEFAULT_SDP_CUR_MIN;
	usb_phy->chg_cur.sdp_max = DEFAULT_SDP_CUR_MAX;
	usb_phy->chg_cur.dcp_min = DEFAULT_DCP_CUR_MIN;
	usb_phy->chg_cur.dcp_max = DEFAULT_DCP_CUR_MAX;
	usb_phy->chg_cur.cdp_min = DEFAULT_CDP_CUR_MIN;
	usb_phy->chg_cur.cdp_max = DEFAULT_CDP_CUR_MAX;
	usb_phy->chg_cur.aca_min = DEFAULT_ACA_CUR_MIN;
	usb_phy->chg_cur.aca_max = DEFAULT_ACA_CUR_MAX;
}

 
static void usb_phy_notify_charger_work(struct work_struct *work)
{
	struct usb_phy *usb_phy = container_of(work, struct usb_phy, chg_work);
	unsigned int min, max;

	switch (usb_phy->chg_state) {
	case USB_CHARGER_PRESENT:
		usb_phy_get_charger_current(usb_phy, &min, &max);

		atomic_notifier_call_chain(&usb_phy->notifier, max, usb_phy);
		break;
	case USB_CHARGER_ABSENT:
		usb_phy_set_default_current(usb_phy);

		atomic_notifier_call_chain(&usb_phy->notifier, 0, usb_phy);
		break;
	default:
		dev_warn(usb_phy->dev, "Unknown USB charger state: %d\n",
			 usb_phy->chg_state);
		return;
	}

	kobject_uevent(&usb_phy->dev->kobj, KOBJ_CHANGE);
}

static int usb_phy_uevent(const struct device *dev, struct kobj_uevent_env *env)
{
	const struct usb_phy *usb_phy;
	char uchger_state[50] = { 0 };
	char uchger_type[50] = { 0 };
	unsigned long flags;

	spin_lock_irqsave(&phy_lock, flags);
	usb_phy = __device_to_usb_phy(dev);
	spin_unlock_irqrestore(&phy_lock, flags);

	if (!usb_phy)
		return -ENODEV;

	snprintf(uchger_state, ARRAY_SIZE(uchger_state),
		 "USB_CHARGER_STATE=%s", usb_chger_state[usb_phy->chg_state]);

	snprintf(uchger_type, ARRAY_SIZE(uchger_type),
		 "USB_CHARGER_TYPE=%s", usb_chger_type[usb_phy->chg_type]);

	if (add_uevent_var(env, uchger_state))
		return -ENOMEM;

	if (add_uevent_var(env, uchger_type))
		return -ENOMEM;

	return 0;
}

static void __usb_phy_get_charger_type(struct usb_phy *usb_phy)
{
	if (extcon_get_state(usb_phy->edev, EXTCON_CHG_USB_SDP) > 0) {
		usb_phy->chg_type = SDP_TYPE;
		usb_phy->chg_state = USB_CHARGER_PRESENT;
	} else if (extcon_get_state(usb_phy->edev, EXTCON_CHG_USB_CDP) > 0) {
		usb_phy->chg_type = CDP_TYPE;
		usb_phy->chg_state = USB_CHARGER_PRESENT;
	} else if (extcon_get_state(usb_phy->edev, EXTCON_CHG_USB_DCP) > 0) {
		usb_phy->chg_type = DCP_TYPE;
		usb_phy->chg_state = USB_CHARGER_PRESENT;
	} else if (extcon_get_state(usb_phy->edev, EXTCON_CHG_USB_ACA) > 0) {
		usb_phy->chg_type = ACA_TYPE;
		usb_phy->chg_state = USB_CHARGER_PRESENT;
	} else {
		usb_phy->chg_type = UNKNOWN_TYPE;
		usb_phy->chg_state = USB_CHARGER_ABSENT;
	}

	schedule_work(&usb_phy->chg_work);
}

 
static int usb_phy_get_charger_type(struct notifier_block *nb,
				    unsigned long state, void *data)
{
	struct usb_phy *usb_phy = container_of(nb, struct usb_phy, type_nb);

	__usb_phy_get_charger_type(usb_phy);
	return NOTIFY_OK;
}

 
void usb_phy_set_charger_current(struct usb_phy *usb_phy, unsigned int mA)
{
	switch (usb_phy->chg_type) {
	case SDP_TYPE:
		if (usb_phy->chg_cur.sdp_max == mA)
			return;

		usb_phy->chg_cur.sdp_max = (mA > DEFAULT_SDP_CUR_MAX_SS) ?
			DEFAULT_SDP_CUR_MAX_SS : mA;
		break;
	case DCP_TYPE:
		if (usb_phy->chg_cur.dcp_max == mA)
			return;

		usb_phy->chg_cur.dcp_max = (mA > DEFAULT_DCP_CUR_MAX) ?
			DEFAULT_DCP_CUR_MAX : mA;
		break;
	case CDP_TYPE:
		if (usb_phy->chg_cur.cdp_max == mA)
			return;

		usb_phy->chg_cur.cdp_max = (mA > DEFAULT_CDP_CUR_MAX) ?
			DEFAULT_CDP_CUR_MAX : mA;
		break;
	case ACA_TYPE:
		if (usb_phy->chg_cur.aca_max == mA)
			return;

		usb_phy->chg_cur.aca_max = (mA > DEFAULT_ACA_CUR_MAX) ?
			DEFAULT_ACA_CUR_MAX : mA;
		break;
	default:
		return;
	}

	schedule_work(&usb_phy->chg_work);
}
EXPORT_SYMBOL_GPL(usb_phy_set_charger_current);

 
void usb_phy_get_charger_current(struct usb_phy *usb_phy,
				 unsigned int *min, unsigned int *max)
{
	switch (usb_phy->chg_type) {
	case SDP_TYPE:
		*min = usb_phy->chg_cur.sdp_min;
		*max = usb_phy->chg_cur.sdp_max;
		break;
	case DCP_TYPE:
		*min = usb_phy->chg_cur.dcp_min;
		*max = usb_phy->chg_cur.dcp_max;
		break;
	case CDP_TYPE:
		*min = usb_phy->chg_cur.cdp_min;
		*max = usb_phy->chg_cur.cdp_max;
		break;
	case ACA_TYPE:
		*min = usb_phy->chg_cur.aca_min;
		*max = usb_phy->chg_cur.aca_max;
		break;
	default:
		*min = 0;
		*max = 0;
		break;
	}
}
EXPORT_SYMBOL_GPL(usb_phy_get_charger_current);

 
void usb_phy_set_charger_state(struct usb_phy *usb_phy,
			       enum usb_charger_state state)
{
	if (usb_phy->chg_state == state || !usb_phy->charger_detect)
		return;

	usb_phy->chg_state = state;
	if (usb_phy->chg_state == USB_CHARGER_PRESENT)
		usb_phy->chg_type = usb_phy->charger_detect(usb_phy);
	else
		usb_phy->chg_type = UNKNOWN_TYPE;

	schedule_work(&usb_phy->chg_work);
}
EXPORT_SYMBOL_GPL(usb_phy_set_charger_state);

static void devm_usb_phy_release(struct device *dev, void *res)
{
	struct usb_phy *phy = *(struct usb_phy **)res;

	usb_put_phy(phy);
}

static void devm_usb_phy_release2(struct device *dev, void *_res)
{
	struct phy_devm *res = _res;

	if (res->nb)
		usb_unregister_notifier(res->phy, res->nb);
	usb_put_phy(res->phy);
}

static int devm_usb_phy_match(struct device *dev, void *res, void *match_data)
{
	struct usb_phy **phy = res;

	return *phy == match_data;
}

static void usb_charger_init(struct usb_phy *usb_phy)
{
	usb_phy->chg_type = UNKNOWN_TYPE;
	usb_phy->chg_state = USB_CHARGER_DEFAULT;
	usb_phy_set_default_current(usb_phy);
	INIT_WORK(&usb_phy->chg_work, usb_phy_notify_charger_work);
}

static int usb_add_extcon(struct usb_phy *x)
{
	int ret;

	if (of_property_read_bool(x->dev->of_node, "extcon")) {
		x->edev = extcon_get_edev_by_phandle(x->dev, 0);
		if (IS_ERR(x->edev))
			return PTR_ERR(x->edev);

		x->id_edev = extcon_get_edev_by_phandle(x->dev, 1);
		if (IS_ERR(x->id_edev)) {
			x->id_edev = NULL;
			dev_info(x->dev, "No separate ID extcon device\n");
		}

		if (x->vbus_nb.notifier_call) {
			ret = devm_extcon_register_notifier(x->dev, x->edev,
							    EXTCON_USB,
							    &x->vbus_nb);
			if (ret < 0) {
				dev_err(x->dev,
					"register VBUS notifier failed\n");
				return ret;
			}
		} else {
			x->type_nb.notifier_call = usb_phy_get_charger_type;

			ret = devm_extcon_register_notifier(x->dev, x->edev,
							    EXTCON_CHG_USB_SDP,
							    &x->type_nb);
			if (ret) {
				dev_err(x->dev,
					"register extcon USB SDP failed.\n");
				return ret;
			}

			ret = devm_extcon_register_notifier(x->dev, x->edev,
							    EXTCON_CHG_USB_CDP,
							    &x->type_nb);
			if (ret) {
				dev_err(x->dev,
					"register extcon USB CDP failed.\n");
				return ret;
			}

			ret = devm_extcon_register_notifier(x->dev, x->edev,
							    EXTCON_CHG_USB_DCP,
							    &x->type_nb);
			if (ret) {
				dev_err(x->dev,
					"register extcon USB DCP failed.\n");
				return ret;
			}

			ret = devm_extcon_register_notifier(x->dev, x->edev,
							    EXTCON_CHG_USB_ACA,
							    &x->type_nb);
			if (ret) {
				dev_err(x->dev,
					"register extcon USB ACA failed.\n");
				return ret;
			}
		}

		if (x->id_nb.notifier_call) {
			struct extcon_dev *id_ext;

			if (x->id_edev)
				id_ext = x->id_edev;
			else
				id_ext = x->edev;

			ret = devm_extcon_register_notifier(x->dev, id_ext,
							    EXTCON_USB_HOST,
							    &x->id_nb);
			if (ret < 0) {
				dev_err(x->dev,
					"register ID notifier failed\n");
				return ret;
			}
		}
	}

	if (x->type_nb.notifier_call)
		__usb_phy_get_charger_type(x);

	return 0;
}

 
struct usb_phy *devm_usb_get_phy(struct device *dev, enum usb_phy_type type)
{
	struct usb_phy **ptr, *phy;

	ptr = devres_alloc(devm_usb_phy_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	phy = usb_get_phy(type);
	if (!IS_ERR(phy)) {
		*ptr = phy;
		devres_add(dev, ptr);
	} else
		devres_free(ptr);

	return phy;
}
EXPORT_SYMBOL_GPL(devm_usb_get_phy);

 
struct usb_phy *usb_get_phy(enum usb_phy_type type)
{
	struct usb_phy	*phy = NULL;
	unsigned long	flags;

	spin_lock_irqsave(&phy_lock, flags);

	phy = __usb_find_phy(&phy_list, type);
	if (IS_ERR(phy) || !try_module_get(phy->dev->driver->owner)) {
		pr_debug("PHY: unable to find transceiver of type %s\n",
			usb_phy_type_string(type));
		if (!IS_ERR(phy))
			phy = ERR_PTR(-ENODEV);

		goto err0;
	}

	get_device(phy->dev);

err0:
	spin_unlock_irqrestore(&phy_lock, flags);

	return phy;
}
EXPORT_SYMBOL_GPL(usb_get_phy);

 
struct  usb_phy *devm_usb_get_phy_by_node(struct device *dev,
					  struct device_node *node,
					  struct notifier_block *nb)
{
	struct usb_phy	*phy = ERR_PTR(-ENOMEM);
	struct phy_devm	*ptr;
	unsigned long	flags;

	ptr = devres_alloc(devm_usb_phy_release2, sizeof(*ptr), GFP_KERNEL);
	if (!ptr) {
		dev_dbg(dev, "failed to allocate memory for devres\n");
		goto err0;
	}

	spin_lock_irqsave(&phy_lock, flags);

	phy = __of_usb_find_phy(node);
	if (IS_ERR(phy)) {
		devres_free(ptr);
		goto err1;
	}

	if (!try_module_get(phy->dev->driver->owner)) {
		phy = ERR_PTR(-ENODEV);
		devres_free(ptr);
		goto err1;
	}
	if (nb)
		usb_register_notifier(phy, nb);
	ptr->phy = phy;
	ptr->nb = nb;
	devres_add(dev, ptr);

	get_device(phy->dev);

err1:
	spin_unlock_irqrestore(&phy_lock, flags);

err0:

	return phy;
}
EXPORT_SYMBOL_GPL(devm_usb_get_phy_by_node);

 
struct usb_phy *devm_usb_get_phy_by_phandle(struct device *dev,
	const char *phandle, u8 index)
{
	struct device_node *node;
	struct usb_phy	*phy;

	if (!dev->of_node) {
		dev_dbg(dev, "device does not have a device node entry\n");
		return ERR_PTR(-EINVAL);
	}

	node = of_parse_phandle(dev->of_node, phandle, index);
	if (!node) {
		dev_dbg(dev, "failed to get %s phandle in %pOF node\n", phandle,
			dev->of_node);
		return ERR_PTR(-ENODEV);
	}
	phy = devm_usb_get_phy_by_node(dev, node, NULL);
	of_node_put(node);
	return phy;
}
EXPORT_SYMBOL_GPL(devm_usb_get_phy_by_phandle);

 
void devm_usb_put_phy(struct device *dev, struct usb_phy *phy)
{
	int r;

	r = devres_destroy(dev, devm_usb_phy_release, devm_usb_phy_match, phy);
	dev_WARN_ONCE(dev, r, "couldn't find PHY resource\n");
}
EXPORT_SYMBOL_GPL(devm_usb_put_phy);

 
void usb_put_phy(struct usb_phy *x)
{
	if (x) {
		struct module *owner = x->dev->driver->owner;

		put_device(x->dev);
		module_put(owner);
	}
}
EXPORT_SYMBOL_GPL(usb_put_phy);

 
int usb_add_phy(struct usb_phy *x, enum usb_phy_type type)
{
	int		ret = 0;
	unsigned long	flags;
	struct usb_phy	*phy;

	if (x->type != USB_PHY_TYPE_UNDEFINED) {
		dev_err(x->dev, "not accepting initialized PHY %s\n", x->label);
		return -EINVAL;
	}

	usb_charger_init(x);
	ret = usb_add_extcon(x);
	if (ret)
		return ret;

	ATOMIC_INIT_NOTIFIER_HEAD(&x->notifier);

	spin_lock_irqsave(&phy_lock, flags);

	list_for_each_entry(phy, &phy_list, head) {
		if (phy->type == type) {
			ret = -EBUSY;
			dev_err(x->dev, "transceiver type %s already exists\n",
						usb_phy_type_string(type));
			goto out;
		}
	}

	x->type = type;
	list_add_tail(&x->head, &phy_list);

out:
	spin_unlock_irqrestore(&phy_lock, flags);
	return ret;
}
EXPORT_SYMBOL_GPL(usb_add_phy);

static struct device_type usb_phy_dev_type = {
	.name = "usb_phy",
	.uevent = usb_phy_uevent,
};

 
int usb_add_phy_dev(struct usb_phy *x)
{
	unsigned long flags;
	int ret;

	if (!x->dev) {
		dev_err(x->dev, "no device provided for PHY\n");
		return -EINVAL;
	}

	usb_charger_init(x);
	ret = usb_add_extcon(x);
	if (ret)
		return ret;

	x->dev->type = &usb_phy_dev_type;

	ATOMIC_INIT_NOTIFIER_HEAD(&x->notifier);

	spin_lock_irqsave(&phy_lock, flags);
	list_add_tail(&x->head, &phy_list);
	spin_unlock_irqrestore(&phy_lock, flags);

	return 0;
}
EXPORT_SYMBOL_GPL(usb_add_phy_dev);

 
void usb_remove_phy(struct usb_phy *x)
{
	unsigned long	flags;

	spin_lock_irqsave(&phy_lock, flags);
	if (x)
		list_del(&x->head);
	spin_unlock_irqrestore(&phy_lock, flags);
}
EXPORT_SYMBOL_GPL(usb_remove_phy);

 
void usb_phy_set_event(struct usb_phy *x, unsigned long event)
{
	x->last_event = event;
}
EXPORT_SYMBOL_GPL(usb_phy_set_event);
