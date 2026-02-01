
 

#include <linux/device.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/usb/of.h>
#include <linux/usb/onboard_hub.h>

#include "onboard_usb_hub.h"

struct pdev_list_entry {
	struct platform_device *pdev;
	struct list_head node;
};

static bool of_is_onboard_usb_hub(const struct device_node *np)
{
	return !!of_match_node(onboard_hub_match, np);
}

 
void onboard_hub_create_pdevs(struct usb_device *parent_hub, struct list_head *pdev_list)
{
	int i;
	struct usb_hcd *hcd = bus_to_hcd(parent_hub->bus);
	struct device_node *np, *npc;
	struct platform_device *pdev;
	struct pdev_list_entry *pdle;

	if (!parent_hub->dev.of_node)
		return;

	if (!parent_hub->parent && !usb_hcd_is_primary_hcd(hcd))
		return;

	for (i = 1; i <= parent_hub->maxchild; i++) {
		np = usb_of_get_device_node(parent_hub, i);
		if (!np)
			continue;

		if (!of_is_onboard_usb_hub(np))
			goto node_put;

		npc = of_parse_phandle(np, "peer-hub", 0);
		if (npc) {
			if (!usb_hcd_is_primary_hcd(hcd)) {
				of_node_put(npc);
				goto node_put;
			}

			pdev = of_find_device_by_node(npc);
			of_node_put(npc);

			if (pdev) {
				put_device(&pdev->dev);
				goto node_put;
			}
		}

		pdev = of_platform_device_create(np, NULL, &parent_hub->dev);
		if (!pdev) {
			dev_err(&parent_hub->dev,
				"failed to create platform device for onboard hub '%pOF'\n", np);
			goto node_put;
		}

		pdle = kzalloc(sizeof(*pdle), GFP_KERNEL);
		if (!pdle) {
			of_platform_device_destroy(&pdev->dev, NULL);
			goto node_put;
		}

		pdle->pdev = pdev;
		list_add(&pdle->node, pdev_list);

node_put:
		of_node_put(np);
	}
}
EXPORT_SYMBOL_GPL(onboard_hub_create_pdevs);

 
void onboard_hub_destroy_pdevs(struct list_head *pdev_list)
{
	struct pdev_list_entry *pdle, *tmp;

	list_for_each_entry_safe(pdle, tmp, pdev_list, node) {
		list_del(&pdle->node);
		of_platform_device_destroy(&pdle->pdev->dev, NULL);
		kfree(pdle);
	}
}
EXPORT_SYMBOL_GPL(onboard_hub_destroy_pdevs);
