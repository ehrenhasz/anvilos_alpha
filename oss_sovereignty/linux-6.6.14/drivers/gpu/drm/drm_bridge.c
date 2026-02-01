 

#include <linux/err.h>
#include <linux/media-bus-format.h>
#include <linux/module.h>
#include <linux/mutex.h>

#include <drm/drm_atomic_state_helper.h>
#include <drm/drm_debugfs.h>
#include <drm/drm_bridge.h>
#include <drm/drm_encoder.h>
#include <drm/drm_file.h>
#include <drm/drm_of.h>
#include <drm/drm_print.h>

#include "drm_crtc_internal.h"

 

 

 

 

static DEFINE_MUTEX(bridge_lock);
static LIST_HEAD(bridge_list);

 
void drm_bridge_add(struct drm_bridge *bridge)
{
	mutex_init(&bridge->hpd_mutex);

	mutex_lock(&bridge_lock);
	list_add_tail(&bridge->list, &bridge_list);
	mutex_unlock(&bridge_lock);
}
EXPORT_SYMBOL(drm_bridge_add);

static void drm_bridge_remove_void(void *bridge)
{
	drm_bridge_remove(bridge);
}

 
int devm_drm_bridge_add(struct device *dev, struct drm_bridge *bridge)
{
	drm_bridge_add(bridge);
	return devm_add_action_or_reset(dev, drm_bridge_remove_void, bridge);
}
EXPORT_SYMBOL(devm_drm_bridge_add);

 
void drm_bridge_remove(struct drm_bridge *bridge)
{
	mutex_lock(&bridge_lock);
	list_del_init(&bridge->list);
	mutex_unlock(&bridge_lock);

	mutex_destroy(&bridge->hpd_mutex);
}
EXPORT_SYMBOL(drm_bridge_remove);

static struct drm_private_state *
drm_bridge_atomic_duplicate_priv_state(struct drm_private_obj *obj)
{
	struct drm_bridge *bridge = drm_priv_to_bridge(obj);
	struct drm_bridge_state *state;

	state = bridge->funcs->atomic_duplicate_state(bridge);
	return state ? &state->base : NULL;
}

static void
drm_bridge_atomic_destroy_priv_state(struct drm_private_obj *obj,
				     struct drm_private_state *s)
{
	struct drm_bridge_state *state = drm_priv_to_bridge_state(s);
	struct drm_bridge *bridge = drm_priv_to_bridge(obj);

	bridge->funcs->atomic_destroy_state(bridge, state);
}

static const struct drm_private_state_funcs drm_bridge_priv_state_funcs = {
	.atomic_duplicate_state = drm_bridge_atomic_duplicate_priv_state,
	.atomic_destroy_state = drm_bridge_atomic_destroy_priv_state,
};

 
int drm_bridge_attach(struct drm_encoder *encoder, struct drm_bridge *bridge,
		      struct drm_bridge *previous,
		      enum drm_bridge_attach_flags flags)
{
	int ret;

	if (!encoder || !bridge)
		return -EINVAL;

	if (previous && (!previous->dev || previous->encoder != encoder))
		return -EINVAL;

	if (bridge->dev)
		return -EBUSY;

	bridge->dev = encoder->dev;
	bridge->encoder = encoder;

	if (previous)
		list_add(&bridge->chain_node, &previous->chain_node);
	else
		list_add(&bridge->chain_node, &encoder->bridge_chain);

	if (bridge->funcs->attach) {
		ret = bridge->funcs->attach(bridge, flags);
		if (ret < 0)
			goto err_reset_bridge;
	}

	if (bridge->funcs->atomic_reset) {
		struct drm_bridge_state *state;

		state = bridge->funcs->atomic_reset(bridge);
		if (IS_ERR(state)) {
			ret = PTR_ERR(state);
			goto err_detach_bridge;
		}

		drm_atomic_private_obj_init(bridge->dev, &bridge->base,
					    &state->base,
					    &drm_bridge_priv_state_funcs);
	}

	return 0;

err_detach_bridge:
	if (bridge->funcs->detach)
		bridge->funcs->detach(bridge);

err_reset_bridge:
	bridge->dev = NULL;
	bridge->encoder = NULL;
	list_del(&bridge->chain_node);

#ifdef CONFIG_OF
	DRM_ERROR("failed to attach bridge %pOF to encoder %s: %d\n",
		  bridge->of_node, encoder->name, ret);
#else
	DRM_ERROR("failed to attach bridge to encoder %s: %d\n",
		  encoder->name, ret);
#endif

	return ret;
}
EXPORT_SYMBOL(drm_bridge_attach);

void drm_bridge_detach(struct drm_bridge *bridge)
{
	if (WARN_ON(!bridge))
		return;

	if (WARN_ON(!bridge->dev))
		return;

	if (bridge->funcs->atomic_reset)
		drm_atomic_private_obj_fini(&bridge->base);

	if (bridge->funcs->detach)
		bridge->funcs->detach(bridge);

	list_del(&bridge->chain_node);
	bridge->dev = NULL;
}

 

 
bool drm_bridge_chain_mode_fixup(struct drm_bridge *bridge,
				 const struct drm_display_mode *mode,
				 struct drm_display_mode *adjusted_mode)
{
	struct drm_encoder *encoder;

	if (!bridge)
		return true;

	encoder = bridge->encoder;
	list_for_each_entry_from(bridge, &encoder->bridge_chain, chain_node) {
		if (!bridge->funcs->mode_fixup)
			continue;

		if (!bridge->funcs->mode_fixup(bridge, mode, adjusted_mode))
			return false;
	}

	return true;
}
EXPORT_SYMBOL(drm_bridge_chain_mode_fixup);

 
enum drm_mode_status
drm_bridge_chain_mode_valid(struct drm_bridge *bridge,
			    const struct drm_display_info *info,
			    const struct drm_display_mode *mode)
{
	struct drm_encoder *encoder;

	if (!bridge)
		return MODE_OK;

	encoder = bridge->encoder;
	list_for_each_entry_from(bridge, &encoder->bridge_chain, chain_node) {
		enum drm_mode_status ret;

		if (!bridge->funcs->mode_valid)
			continue;

		ret = bridge->funcs->mode_valid(bridge, info, mode);
		if (ret != MODE_OK)
			return ret;
	}

	return MODE_OK;
}
EXPORT_SYMBOL(drm_bridge_chain_mode_valid);

 
void drm_bridge_chain_mode_set(struct drm_bridge *bridge,
			       const struct drm_display_mode *mode,
			       const struct drm_display_mode *adjusted_mode)
{
	struct drm_encoder *encoder;

	if (!bridge)
		return;

	encoder = bridge->encoder;
	list_for_each_entry_from(bridge, &encoder->bridge_chain, chain_node) {
		if (bridge->funcs->mode_set)
			bridge->funcs->mode_set(bridge, mode, adjusted_mode);
	}
}
EXPORT_SYMBOL(drm_bridge_chain_mode_set);

 
void drm_atomic_bridge_chain_disable(struct drm_bridge *bridge,
				     struct drm_atomic_state *old_state)
{
	struct drm_encoder *encoder;
	struct drm_bridge *iter;

	if (!bridge)
		return;

	encoder = bridge->encoder;
	list_for_each_entry_reverse(iter, &encoder->bridge_chain, chain_node) {
		if (iter->funcs->atomic_disable) {
			struct drm_bridge_state *old_bridge_state;

			old_bridge_state =
				drm_atomic_get_old_bridge_state(old_state,
								iter);
			if (WARN_ON(!old_bridge_state))
				return;

			iter->funcs->atomic_disable(iter, old_bridge_state);
		} else if (iter->funcs->disable) {
			iter->funcs->disable(iter);
		}

		if (iter == bridge)
			break;
	}
}
EXPORT_SYMBOL(drm_atomic_bridge_chain_disable);

static void drm_atomic_bridge_call_post_disable(struct drm_bridge *bridge,
						struct drm_atomic_state *old_state)
{
	if (old_state && bridge->funcs->atomic_post_disable) {
		struct drm_bridge_state *old_bridge_state;

		old_bridge_state =
			drm_atomic_get_old_bridge_state(old_state,
							bridge);
		if (WARN_ON(!old_bridge_state))
			return;

		bridge->funcs->atomic_post_disable(bridge,
						   old_bridge_state);
	} else if (bridge->funcs->post_disable) {
		bridge->funcs->post_disable(bridge);
	}
}

 
void drm_atomic_bridge_chain_post_disable(struct drm_bridge *bridge,
					  struct drm_atomic_state *old_state)
{
	struct drm_encoder *encoder;
	struct drm_bridge *next, *limit;

	if (!bridge)
		return;

	encoder = bridge->encoder;

	list_for_each_entry_from(bridge, &encoder->bridge_chain, chain_node) {
		limit = NULL;

		if (!list_is_last(&bridge->chain_node, &encoder->bridge_chain)) {
			next = list_next_entry(bridge, chain_node);

			if (next->pre_enable_prev_first) {
				 
				limit = next;

				 
				list_for_each_entry_from(next, &encoder->bridge_chain,
							 chain_node) {
					if (next->pre_enable_prev_first) {
						next = list_prev_entry(next, chain_node);
						limit = next;
						break;
					}
				}

				 
				list_for_each_entry_from_reverse(next, &encoder->bridge_chain,
								 chain_node) {
					if (next == bridge)
						break;

					drm_atomic_bridge_call_post_disable(next,
									    old_state);
				}
			}
		}

		drm_atomic_bridge_call_post_disable(bridge, old_state);

		if (limit)
			 
			bridge = limit;
	}
}
EXPORT_SYMBOL(drm_atomic_bridge_chain_post_disable);

static void drm_atomic_bridge_call_pre_enable(struct drm_bridge *bridge,
					      struct drm_atomic_state *old_state)
{
	if (old_state && bridge->funcs->atomic_pre_enable) {
		struct drm_bridge_state *old_bridge_state;

		old_bridge_state =
			drm_atomic_get_old_bridge_state(old_state,
							bridge);
		if (WARN_ON(!old_bridge_state))
			return;

		bridge->funcs->atomic_pre_enable(bridge, old_bridge_state);
	} else if (bridge->funcs->pre_enable) {
		bridge->funcs->pre_enable(bridge);
	}
}

 
void drm_atomic_bridge_chain_pre_enable(struct drm_bridge *bridge,
					struct drm_atomic_state *old_state)
{
	struct drm_encoder *encoder;
	struct drm_bridge *iter, *next, *limit;

	if (!bridge)
		return;

	encoder = bridge->encoder;

	list_for_each_entry_reverse(iter, &encoder->bridge_chain, chain_node) {
		if (iter->pre_enable_prev_first) {
			next = iter;
			limit = bridge;
			list_for_each_entry_from_reverse(next,
							 &encoder->bridge_chain,
							 chain_node) {
				if (next == bridge)
					break;

				if (!next->pre_enable_prev_first) {
					 
					limit = list_prev_entry(next, chain_node);
					break;
				}
			}

			list_for_each_entry_from(next, &encoder->bridge_chain, chain_node) {
				 
				if (next == iter)
					 
					break;

				drm_atomic_bridge_call_pre_enable(next, old_state);
			}
		}

		drm_atomic_bridge_call_pre_enable(iter, old_state);

		if (iter->pre_enable_prev_first)
			 
			iter = limit;

		if (iter == bridge)
			break;
	}
}
EXPORT_SYMBOL(drm_atomic_bridge_chain_pre_enable);

 
void drm_atomic_bridge_chain_enable(struct drm_bridge *bridge,
				    struct drm_atomic_state *old_state)
{
	struct drm_encoder *encoder;

	if (!bridge)
		return;

	encoder = bridge->encoder;
	list_for_each_entry_from(bridge, &encoder->bridge_chain, chain_node) {
		if (bridge->funcs->atomic_enable) {
			struct drm_bridge_state *old_bridge_state;

			old_bridge_state =
				drm_atomic_get_old_bridge_state(old_state,
								bridge);
			if (WARN_ON(!old_bridge_state))
				return;

			bridge->funcs->atomic_enable(bridge, old_bridge_state);
		} else if (bridge->funcs->enable) {
			bridge->funcs->enable(bridge);
		}
	}
}
EXPORT_SYMBOL(drm_atomic_bridge_chain_enable);

static int drm_atomic_bridge_check(struct drm_bridge *bridge,
				   struct drm_crtc_state *crtc_state,
				   struct drm_connector_state *conn_state)
{
	if (bridge->funcs->atomic_check) {
		struct drm_bridge_state *bridge_state;
		int ret;

		bridge_state = drm_atomic_get_new_bridge_state(crtc_state->state,
							       bridge);
		if (WARN_ON(!bridge_state))
			return -EINVAL;

		ret = bridge->funcs->atomic_check(bridge, bridge_state,
						  crtc_state, conn_state);
		if (ret)
			return ret;
	} else if (bridge->funcs->mode_fixup) {
		if (!bridge->funcs->mode_fixup(bridge, &crtc_state->mode,
					       &crtc_state->adjusted_mode))
			return -EINVAL;
	}

	return 0;
}

static int select_bus_fmt_recursive(struct drm_bridge *first_bridge,
				    struct drm_bridge *cur_bridge,
				    struct drm_crtc_state *crtc_state,
				    struct drm_connector_state *conn_state,
				    u32 out_bus_fmt)
{
	unsigned int i, num_in_bus_fmts = 0;
	struct drm_bridge_state *cur_state;
	struct drm_bridge *prev_bridge;
	u32 *in_bus_fmts;
	int ret;

	prev_bridge = drm_bridge_get_prev_bridge(cur_bridge);
	cur_state = drm_atomic_get_new_bridge_state(crtc_state->state,
						    cur_bridge);

	 
	if (!cur_bridge->funcs->atomic_get_input_bus_fmts) {
		if (cur_bridge != first_bridge) {
			ret = select_bus_fmt_recursive(first_bridge,
						       prev_bridge, crtc_state,
						       conn_state,
						       MEDIA_BUS_FMT_FIXED);
			if (ret)
				return ret;
		}

		 
		if (cur_state) {
			cur_state->input_bus_cfg.format = MEDIA_BUS_FMT_FIXED;
			cur_state->output_bus_cfg.format = out_bus_fmt;
		}

		return 0;
	}

	 
	if (WARN_ON(!cur_state))
		return -EINVAL;

	in_bus_fmts = cur_bridge->funcs->atomic_get_input_bus_fmts(cur_bridge,
							cur_state,
							crtc_state,
							conn_state,
							out_bus_fmt,
							&num_in_bus_fmts);
	if (!num_in_bus_fmts)
		return -ENOTSUPP;
	else if (!in_bus_fmts)
		return -ENOMEM;

	if (first_bridge == cur_bridge) {
		cur_state->input_bus_cfg.format = in_bus_fmts[0];
		cur_state->output_bus_cfg.format = out_bus_fmt;
		kfree(in_bus_fmts);
		return 0;
	}

	for (i = 0; i < num_in_bus_fmts; i++) {
		ret = select_bus_fmt_recursive(first_bridge, prev_bridge,
					       crtc_state, conn_state,
					       in_bus_fmts[i]);
		if (ret != -ENOTSUPP)
			break;
	}

	if (!ret) {
		cur_state->input_bus_cfg.format = in_bus_fmts[i];
		cur_state->output_bus_cfg.format = out_bus_fmt;
	}

	kfree(in_bus_fmts);
	return ret;
}

 
static int
drm_atomic_bridge_chain_select_bus_fmts(struct drm_bridge *bridge,
					struct drm_crtc_state *crtc_state,
					struct drm_connector_state *conn_state)
{
	struct drm_connector *conn = conn_state->connector;
	struct drm_encoder *encoder = bridge->encoder;
	struct drm_bridge_state *last_bridge_state;
	unsigned int i, num_out_bus_fmts = 0;
	struct drm_bridge *last_bridge;
	u32 *out_bus_fmts;
	int ret = 0;

	last_bridge = list_last_entry(&encoder->bridge_chain,
				      struct drm_bridge, chain_node);
	last_bridge_state = drm_atomic_get_new_bridge_state(crtc_state->state,
							    last_bridge);

	if (last_bridge->funcs->atomic_get_output_bus_fmts) {
		const struct drm_bridge_funcs *funcs = last_bridge->funcs;

		 
		if (WARN_ON(!last_bridge_state))
			return -EINVAL;

		out_bus_fmts = funcs->atomic_get_output_bus_fmts(last_bridge,
							last_bridge_state,
							crtc_state,
							conn_state,
							&num_out_bus_fmts);
		if (!num_out_bus_fmts)
			return -ENOTSUPP;
		else if (!out_bus_fmts)
			return -ENOMEM;
	} else {
		num_out_bus_fmts = 1;
		out_bus_fmts = kmalloc(sizeof(*out_bus_fmts), GFP_KERNEL);
		if (!out_bus_fmts)
			return -ENOMEM;

		if (conn->display_info.num_bus_formats &&
		    conn->display_info.bus_formats)
			out_bus_fmts[0] = conn->display_info.bus_formats[0];
		else
			out_bus_fmts[0] = MEDIA_BUS_FMT_FIXED;
	}

	for (i = 0; i < num_out_bus_fmts; i++) {
		ret = select_bus_fmt_recursive(bridge, last_bridge, crtc_state,
					       conn_state, out_bus_fmts[i]);
		if (ret != -ENOTSUPP)
			break;
	}

	kfree(out_bus_fmts);

	return ret;
}

static void
drm_atomic_bridge_propagate_bus_flags(struct drm_bridge *bridge,
				      struct drm_connector *conn,
				      struct drm_atomic_state *state)
{
	struct drm_bridge_state *bridge_state, *next_bridge_state;
	struct drm_bridge *next_bridge;
	u32 output_flags = 0;

	bridge_state = drm_atomic_get_new_bridge_state(state, bridge);

	 
	if (!bridge_state)
		return;

	next_bridge = drm_bridge_get_next_bridge(bridge);

	 
	if (!next_bridge) {
		output_flags = conn->display_info.bus_flags;
	} else {
		next_bridge_state = drm_atomic_get_new_bridge_state(state,
								next_bridge);
		 
		if (next_bridge_state)
			output_flags = next_bridge_state->input_bus_cfg.flags;
	}

	bridge_state->output_bus_cfg.flags = output_flags;

	 
	bridge_state->input_bus_cfg.flags = output_flags;
}

 
int drm_atomic_bridge_chain_check(struct drm_bridge *bridge,
				  struct drm_crtc_state *crtc_state,
				  struct drm_connector_state *conn_state)
{
	struct drm_connector *conn = conn_state->connector;
	struct drm_encoder *encoder;
	struct drm_bridge *iter;
	int ret;

	if (!bridge)
		return 0;

	ret = drm_atomic_bridge_chain_select_bus_fmts(bridge, crtc_state,
						      conn_state);
	if (ret)
		return ret;

	encoder = bridge->encoder;
	list_for_each_entry_reverse(iter, &encoder->bridge_chain, chain_node) {
		int ret;

		 
		drm_atomic_bridge_propagate_bus_flags(iter, conn,
						      crtc_state->state);

		ret = drm_atomic_bridge_check(iter, crtc_state, conn_state);
		if (ret)
			return ret;

		if (iter == bridge)
			break;
	}

	return 0;
}
EXPORT_SYMBOL(drm_atomic_bridge_chain_check);

 
enum drm_connector_status drm_bridge_detect(struct drm_bridge *bridge)
{
	if (!(bridge->ops & DRM_BRIDGE_OP_DETECT))
		return connector_status_unknown;

	return bridge->funcs->detect(bridge);
}
EXPORT_SYMBOL_GPL(drm_bridge_detect);

 
int drm_bridge_get_modes(struct drm_bridge *bridge,
			 struct drm_connector *connector)
{
	if (!(bridge->ops & DRM_BRIDGE_OP_MODES))
		return 0;

	return bridge->funcs->get_modes(bridge, connector);
}
EXPORT_SYMBOL_GPL(drm_bridge_get_modes);

 
struct edid *drm_bridge_get_edid(struct drm_bridge *bridge,
				 struct drm_connector *connector)
{
	if (!(bridge->ops & DRM_BRIDGE_OP_EDID))
		return NULL;

	return bridge->funcs->get_edid(bridge, connector);
}
EXPORT_SYMBOL_GPL(drm_bridge_get_edid);

 
void drm_bridge_hpd_enable(struct drm_bridge *bridge,
			   void (*cb)(void *data,
				      enum drm_connector_status status),
			   void *data)
{
	if (!(bridge->ops & DRM_BRIDGE_OP_HPD))
		return;

	mutex_lock(&bridge->hpd_mutex);

	if (WARN(bridge->hpd_cb, "Hot plug detection already enabled\n"))
		goto unlock;

	bridge->hpd_cb = cb;
	bridge->hpd_data = data;

	if (bridge->funcs->hpd_enable)
		bridge->funcs->hpd_enable(bridge);

unlock:
	mutex_unlock(&bridge->hpd_mutex);
}
EXPORT_SYMBOL_GPL(drm_bridge_hpd_enable);

 
void drm_bridge_hpd_disable(struct drm_bridge *bridge)
{
	if (!(bridge->ops & DRM_BRIDGE_OP_HPD))
		return;

	mutex_lock(&bridge->hpd_mutex);
	if (bridge->funcs->hpd_disable)
		bridge->funcs->hpd_disable(bridge);

	bridge->hpd_cb = NULL;
	bridge->hpd_data = NULL;
	mutex_unlock(&bridge->hpd_mutex);
}
EXPORT_SYMBOL_GPL(drm_bridge_hpd_disable);

 
void drm_bridge_hpd_notify(struct drm_bridge *bridge,
			   enum drm_connector_status status)
{
	mutex_lock(&bridge->hpd_mutex);
	if (bridge->hpd_cb)
		bridge->hpd_cb(bridge->hpd_data, status);
	mutex_unlock(&bridge->hpd_mutex);
}
EXPORT_SYMBOL_GPL(drm_bridge_hpd_notify);

#ifdef CONFIG_OF
 
struct drm_bridge *of_drm_find_bridge(struct device_node *np)
{
	struct drm_bridge *bridge;

	mutex_lock(&bridge_lock);

	list_for_each_entry(bridge, &bridge_list, list) {
		if (bridge->of_node == np) {
			mutex_unlock(&bridge_lock);
			return bridge;
		}
	}

	mutex_unlock(&bridge_lock);
	return NULL;
}
EXPORT_SYMBOL(of_drm_find_bridge);
#endif

#ifdef CONFIG_DEBUG_FS
static int drm_bridge_chains_info(struct seq_file *m, void *data)
{
	struct drm_debugfs_entry *entry = m->private;
	struct drm_device *dev = entry->dev;
	struct drm_printer p = drm_seq_file_printer(m);
	struct drm_mode_config *config = &dev->mode_config;
	struct drm_encoder *encoder;
	unsigned int bridge_idx = 0;

	list_for_each_entry(encoder, &config->encoder_list, head) {
		struct drm_bridge *bridge;

		drm_printf(&p, "encoder[%u]\n", encoder->base.id);

		drm_for_each_bridge_in_chain(encoder, bridge) {
			drm_printf(&p, "\tbridge[%u] type: %u, ops: %#x",
				   bridge_idx, bridge->type, bridge->ops);

#ifdef CONFIG_OF
			if (bridge->of_node)
				drm_printf(&p, ", OF: %pOFfc", bridge->of_node);
#endif

			drm_printf(&p, "\n");

			bridge_idx++;
		}
	}

	return 0;
}

static const struct drm_debugfs_info drm_bridge_debugfs_list[] = {
	{ "bridge_chains", drm_bridge_chains_info, 0 },
};

void drm_bridge_debugfs_init(struct drm_minor *minor)
{
	drm_debugfs_add_files(minor->dev, drm_bridge_debugfs_list,
			      ARRAY_SIZE(drm_bridge_debugfs_list));
}
#endif

MODULE_AUTHOR("Ajay Kumar <ajaykumar.rs@samsung.com>");
MODULE_DESCRIPTION("DRM bridge infrastructure");
MODULE_LICENSE("GPL and additional rights");
