
 

#include <linux/export.h>
#include <linux/of.h>

#include <drm/drm_bridge.h>
#include <drm/drm_bridge_connector.h>
#include <drm/drm_panel.h>

#include "rcar_du_drv.h"
#include "rcar_du_encoder.h"
#include "rcar_lvds.h"

 

static unsigned int rcar_du_encoder_count_ports(struct device_node *node)
{
	struct device_node *ports;
	struct device_node *port;
	unsigned int num_ports = 0;

	ports = of_get_child_by_name(node, "ports");
	if (!ports)
		ports = of_node_get(node);

	for_each_child_of_node(ports, port) {
		if (of_node_name_eq(port, "port"))
			num_ports++;
	}

	of_node_put(ports);

	return num_ports;
}

static const struct drm_encoder_funcs rcar_du_encoder_funcs = {
};

int rcar_du_encoder_init(struct rcar_du_device *rcdu,
			 enum rcar_du_output output,
			 struct device_node *enc_node)
{
	struct rcar_du_encoder *renc;
	struct drm_connector *connector;
	struct drm_bridge *bridge;
	int ret;

	 
	if ((output == RCAR_DU_OUTPUT_DPAD0 ||
	     output == RCAR_DU_OUTPUT_DPAD1) &&
	    rcar_du_encoder_count_ports(enc_node) == 1) {
		struct drm_panel *panel = of_drm_find_panel(enc_node);

		if (IS_ERR(panel))
			return PTR_ERR(panel);

		bridge = devm_drm_panel_bridge_add_typed(rcdu->dev, panel,
							 DRM_MODE_CONNECTOR_DPI);
		if (IS_ERR(bridge))
			return PTR_ERR(bridge);
	} else {
		bridge = of_drm_find_bridge(enc_node);
		if (!bridge)
			return -EPROBE_DEFER;

		if (output == RCAR_DU_OUTPUT_LVDS0 ||
		    output == RCAR_DU_OUTPUT_LVDS1)
			rcdu->lvds[output - RCAR_DU_OUTPUT_LVDS0] = bridge;

		if (output == RCAR_DU_OUTPUT_DSI0 ||
		    output == RCAR_DU_OUTPUT_DSI1)
			rcdu->dsi[output - RCAR_DU_OUTPUT_DSI0] = bridge;
	}

	 
	if (rcdu->info->gen >= 3) {
		if (output == RCAR_DU_OUTPUT_LVDS1 &&
		    rcar_lvds_dual_link(bridge))
			return -ENOLINK;

		if ((output == RCAR_DU_OUTPUT_LVDS0 ||
		     output == RCAR_DU_OUTPUT_LVDS1) &&
		    !rcar_lvds_is_connected(bridge))
			return -ENOLINK;
	}

	dev_dbg(rcdu->dev, "initializing encoder %pOF for output %s\n",
		enc_node, rcar_du_output_name(output));

	renc = drmm_encoder_alloc(&rcdu->ddev, struct rcar_du_encoder, base,
				  &rcar_du_encoder_funcs, DRM_MODE_ENCODER_NONE,
				  NULL);
	if (IS_ERR(renc))
		return PTR_ERR(renc);

	renc->output = output;

	 
	ret = drm_bridge_attach(&renc->base, bridge, NULL,
				DRM_BRIDGE_ATTACH_NO_CONNECTOR);
	if (ret) {
		dev_err(rcdu->dev,
			"failed to attach bridge %pOF for output %s (%d)\n",
			bridge->of_node, rcar_du_output_name(output), ret);
		return ret;
	}

	 
	connector = drm_bridge_connector_init(&rcdu->ddev, &renc->base);
	if (IS_ERR(connector)) {
		dev_err(rcdu->dev,
			"failed to created connector for output %s (%ld)\n",
			rcar_du_output_name(output), PTR_ERR(connector));
		return PTR_ERR(connector);
	}

	return drm_connector_attach_encoder(connector, &renc->base);
}
