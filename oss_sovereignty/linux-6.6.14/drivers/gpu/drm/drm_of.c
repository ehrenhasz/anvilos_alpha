
#include <linux/component.h>
#include <linux/export.h>
#include <linux/list.h>
#include <linux/media-bus-format.h>
#include <linux/of.h>
#include <linux/of_graph.h>

#include <drm/drm_bridge.h>
#include <drm/drm_crtc.h>
#include <drm/drm_device.h>
#include <drm/drm_encoder.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_of.h>
#include <drm/drm_panel.h>

 

 
uint32_t drm_of_crtc_port_mask(struct drm_device *dev,
			    struct device_node *port)
{
	unsigned int index = 0;
	struct drm_crtc *tmp;

	drm_for_each_crtc(tmp, dev) {
		if (tmp->port == port)
			return 1 << index;

		index++;
	}

	return 0;
}
EXPORT_SYMBOL(drm_of_crtc_port_mask);

 
uint32_t drm_of_find_possible_crtcs(struct drm_device *dev,
				    struct device_node *port)
{
	struct device_node *remote_port, *ep;
	uint32_t possible_crtcs = 0;

	for_each_endpoint_of_node(port, ep) {
		remote_port = of_graph_get_remote_port(ep);
		if (!remote_port) {
			of_node_put(ep);
			return 0;
		}

		possible_crtcs |= drm_of_crtc_port_mask(dev, remote_port);

		of_node_put(remote_port);
	}

	return possible_crtcs;
}
EXPORT_SYMBOL(drm_of_find_possible_crtcs);

 
void drm_of_component_match_add(struct device *master,
				struct component_match **matchptr,
				int (*compare)(struct device *, void *),
				struct device_node *node)
{
	of_node_get(node);
	component_match_add_release(master, matchptr, component_release_of,
				    compare, node);
}
EXPORT_SYMBOL_GPL(drm_of_component_match_add);

 
int drm_of_component_probe(struct device *dev,
			   int (*compare_of)(struct device *, void *),
			   const struct component_master_ops *m_ops)
{
	struct device_node *ep, *port, *remote;
	struct component_match *match = NULL;
	int i;

	if (!dev->of_node)
		return -EINVAL;

	 
	for (i = 0; ; i++) {
		port = of_parse_phandle(dev->of_node, "ports", i);
		if (!port)
			break;

		if (of_device_is_available(port->parent))
			drm_of_component_match_add(dev, &match, compare_of,
						   port);

		of_node_put(port);
	}

	if (i == 0) {
		dev_err(dev, "missing 'ports' property\n");
		return -ENODEV;
	}

	if (!match) {
		dev_err(dev, "no available port\n");
		return -ENODEV;
	}

	 
	for (i = 0; ; i++) {
		port = of_parse_phandle(dev->of_node, "ports", i);
		if (!port)
			break;

		if (!of_device_is_available(port->parent)) {
			of_node_put(port);
			continue;
		}

		for_each_child_of_node(port, ep) {
			remote = of_graph_get_remote_port_parent(ep);
			if (!remote || !of_device_is_available(remote)) {
				of_node_put(remote);
				continue;
			} else if (!of_device_is_available(remote->parent)) {
				dev_warn(dev, "parent device of %pOF is not available\n",
					 remote);
				of_node_put(remote);
				continue;
			}

			drm_of_component_match_add(dev, &match, compare_of,
						   remote);
			of_node_put(remote);
		}
		of_node_put(port);
	}

	return component_master_add_with_match(dev, m_ops, match);
}
EXPORT_SYMBOL(drm_of_component_probe);

 
int drm_of_encoder_active_endpoint(struct device_node *node,
				   struct drm_encoder *encoder,
				   struct of_endpoint *endpoint)
{
	struct device_node *ep;
	struct drm_crtc *crtc = encoder->crtc;
	struct device_node *port;
	int ret;

	if (!node || !crtc)
		return -EINVAL;

	for_each_endpoint_of_node(node, ep) {
		port = of_graph_get_remote_port(ep);
		of_node_put(port);
		if (port == crtc->port) {
			ret = of_graph_parse_endpoint(ep, endpoint);
			of_node_put(ep);
			return ret;
		}
	}

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(drm_of_encoder_active_endpoint);

 
int drm_of_find_panel_or_bridge(const struct device_node *np,
				int port, int endpoint,
				struct drm_panel **panel,
				struct drm_bridge **bridge)
{
	int ret = -EPROBE_DEFER;
	struct device_node *remote;

	if (!panel && !bridge)
		return -EINVAL;
	if (panel)
		*panel = NULL;

	 
	if (!of_graph_is_present(np))
		return -ENODEV;

	remote = of_graph_get_remote_node(np, port, endpoint);
	if (!remote)
		return -ENODEV;

	if (panel) {
		*panel = of_drm_find_panel(remote);
		if (!IS_ERR(*panel))
			ret = 0;
		else
			*panel = NULL;
	}

	 
	if (bridge) {
		if (ret) {
			*bridge = of_drm_find_bridge(remote);
			if (*bridge)
				ret = 0;
		} else {
			*bridge = NULL;
		}

	}

	of_node_put(remote);
	return ret;
}
EXPORT_SYMBOL_GPL(drm_of_find_panel_or_bridge);

enum drm_of_lvds_pixels {
	DRM_OF_LVDS_EVEN = BIT(0),
	DRM_OF_LVDS_ODD = BIT(1),
};

static int drm_of_lvds_get_port_pixels_type(struct device_node *port_node)
{
	bool even_pixels =
		of_property_read_bool(port_node, "dual-lvds-even-pixels");
	bool odd_pixels =
		of_property_read_bool(port_node, "dual-lvds-odd-pixels");

	return (even_pixels ? DRM_OF_LVDS_EVEN : 0) |
	       (odd_pixels ? DRM_OF_LVDS_ODD : 0);
}

static int drm_of_lvds_get_remote_pixels_type(
			const struct device_node *port_node)
{
	struct device_node *endpoint = NULL;
	int pixels_type = -EPIPE;

	for_each_child_of_node(port_node, endpoint) {
		struct device_node *remote_port;
		int current_pt;

		if (!of_node_name_eq(endpoint, "endpoint"))
			continue;

		remote_port = of_graph_get_remote_port(endpoint);
		if (!remote_port) {
			of_node_put(endpoint);
			return -EPIPE;
		}

		current_pt = drm_of_lvds_get_port_pixels_type(remote_port);
		of_node_put(remote_port);
		if (pixels_type < 0)
			pixels_type = current_pt;

		 
		if (!current_pt || pixels_type != current_pt) {
			of_node_put(endpoint);
			return -EINVAL;
		}
	}

	return pixels_type;
}

 
int drm_of_lvds_get_dual_link_pixel_order(const struct device_node *port1,
					  const struct device_node *port2)
{
	int remote_p1_pt, remote_p2_pt;

	if (!port1 || !port2)
		return -EINVAL;

	remote_p1_pt = drm_of_lvds_get_remote_pixels_type(port1);
	if (remote_p1_pt < 0)
		return remote_p1_pt;

	remote_p2_pt = drm_of_lvds_get_remote_pixels_type(port2);
	if (remote_p2_pt < 0)
		return remote_p2_pt;

	 
	if (remote_p1_pt + remote_p2_pt != DRM_OF_LVDS_EVEN + DRM_OF_LVDS_ODD)
		return -EINVAL;

	return remote_p1_pt == DRM_OF_LVDS_EVEN ?
		DRM_LVDS_DUAL_LINK_EVEN_ODD_PIXELS :
		DRM_LVDS_DUAL_LINK_ODD_EVEN_PIXELS;
}
EXPORT_SYMBOL_GPL(drm_of_lvds_get_dual_link_pixel_order);

 
int drm_of_lvds_get_data_mapping(const struct device_node *port)
{
	const char *mapping;
	int ret;

	ret = of_property_read_string(port, "data-mapping", &mapping);
	if (ret < 0)
		return -ENODEV;

	if (!strcmp(mapping, "jeida-18"))
		return MEDIA_BUS_FMT_RGB666_1X7X3_SPWG;
	if (!strcmp(mapping, "jeida-24"))
		return MEDIA_BUS_FMT_RGB888_1X7X4_JEIDA;
	if (!strcmp(mapping, "vesa-24"))
		return MEDIA_BUS_FMT_RGB888_1X7X4_SPWG;

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(drm_of_lvds_get_data_mapping);

 
int drm_of_get_data_lanes_count(const struct device_node *endpoint,
				const unsigned int min, const unsigned int max)
{
	int ret;

	ret = of_property_count_u32_elems(endpoint, "data-lanes");
	if (ret < 0)
		return ret;

	if (ret < min || ret > max)
		return -EINVAL;

	return ret;
}
EXPORT_SYMBOL_GPL(drm_of_get_data_lanes_count);

 
int drm_of_get_data_lanes_count_ep(const struct device_node *port,
				   int port_reg, int reg,
				   const unsigned int min,
				   const unsigned int max)
{
	struct device_node *endpoint;
	int ret;

	endpoint = of_graph_get_endpoint_by_regs(port, port_reg, reg);
	ret = drm_of_get_data_lanes_count(endpoint, min, max);
	of_node_put(endpoint);

	return ret;
}
EXPORT_SYMBOL_GPL(drm_of_get_data_lanes_count_ep);

#if IS_ENABLED(CONFIG_DRM_MIPI_DSI)

 
struct mipi_dsi_host *drm_of_get_dsi_bus(struct device *dev)
{
	struct mipi_dsi_host *dsi_host;
	struct device_node *endpoint, *dsi_host_node;

	 
	endpoint = of_graph_get_next_endpoint(dev->of_node, NULL);
	if (!endpoint)
		return ERR_PTR(-ENODEV);

	 
	dsi_host_node = of_graph_get_remote_port_parent(endpoint);
	of_node_put(endpoint);
	if (!dsi_host_node)
		return ERR_PTR(-ENODEV);

	 
	dsi_host = of_find_mipi_dsi_host_by_node(dsi_host_node);
	of_node_put(dsi_host_node);
	if (IS_ERR_OR_NULL(dsi_host))
		return ERR_PTR(-EPROBE_DEFER);

	return dsi_host;
}
EXPORT_SYMBOL_GPL(drm_of_get_dsi_bus);

#endif  
