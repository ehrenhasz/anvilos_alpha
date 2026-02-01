
 

#include <linux/delay.h>
#include <linux/idr.h>
#include <linux/module.h>
#include <linux/nvmem-provider.h>
#include <linux/pm_runtime.h>
#include <linux/sched/signal.h>
#include <linux/sizes.h>
#include <linux/slab.h>
#include <linux/string_helpers.h>

#include "tb.h"

 

struct nvm_auth_status {
	struct list_head list;
	uuid_t uuid;
	u32 status;
};

 
static LIST_HEAD(nvm_auth_status_cache);
static DEFINE_MUTEX(nvm_auth_status_lock);

static struct nvm_auth_status *__nvm_get_auth_status(const struct tb_switch *sw)
{
	struct nvm_auth_status *st;

	list_for_each_entry(st, &nvm_auth_status_cache, list) {
		if (uuid_equal(&st->uuid, sw->uuid))
			return st;
	}

	return NULL;
}

static void nvm_get_auth_status(const struct tb_switch *sw, u32 *status)
{
	struct nvm_auth_status *st;

	mutex_lock(&nvm_auth_status_lock);
	st = __nvm_get_auth_status(sw);
	mutex_unlock(&nvm_auth_status_lock);

	*status = st ? st->status : 0;
}

static void nvm_set_auth_status(const struct tb_switch *sw, u32 status)
{
	struct nvm_auth_status *st;

	if (WARN_ON(!sw->uuid))
		return;

	mutex_lock(&nvm_auth_status_lock);
	st = __nvm_get_auth_status(sw);

	if (!st) {
		st = kzalloc(sizeof(*st), GFP_KERNEL);
		if (!st)
			goto unlock;

		memcpy(&st->uuid, sw->uuid, sizeof(st->uuid));
		INIT_LIST_HEAD(&st->list);
		list_add_tail(&st->list, &nvm_auth_status_cache);
	}

	st->status = status;
unlock:
	mutex_unlock(&nvm_auth_status_lock);
}

static void nvm_clear_auth_status(const struct tb_switch *sw)
{
	struct nvm_auth_status *st;

	mutex_lock(&nvm_auth_status_lock);
	st = __nvm_get_auth_status(sw);
	if (st) {
		list_del(&st->list);
		kfree(st);
	}
	mutex_unlock(&nvm_auth_status_lock);
}

static int nvm_validate_and_write(struct tb_switch *sw)
{
	unsigned int image_size;
	const u8 *buf;
	int ret;

	ret = tb_nvm_validate(sw->nvm);
	if (ret)
		return ret;

	ret = tb_nvm_write_headers(sw->nvm);
	if (ret)
		return ret;

	buf = sw->nvm->buf_data_start;
	image_size = sw->nvm->buf_data_size;

	if (tb_switch_is_usb4(sw))
		ret = usb4_switch_nvm_write(sw, 0, buf, image_size);
	else
		ret = dma_port_flash_write(sw->dma_port, 0, buf, image_size);
	if (ret)
		return ret;

	sw->nvm->flushed = true;
	return 0;
}

static int nvm_authenticate_host_dma_port(struct tb_switch *sw)
{
	int ret = 0;

	 
	if (!sw->safe_mode) {
		u32 status;

		ret = tb_domain_disconnect_all_paths(sw->tb);
		if (ret)
			return ret;
		 
		ret = dma_port_flash_update_auth(sw->dma_port);
		if (!ret || ret == -ETIMEDOUT)
			return 0;

		 
		tb_sw_warn(sw, "failed to authenticate NVM, power cycling\n");
		if (dma_port_flash_update_auth_status(sw->dma_port, &status) > 0)
			nvm_set_auth_status(sw, status);
	}

	 
	dma_port_power_cycle(sw->dma_port);
	return ret;
}

static int nvm_authenticate_device_dma_port(struct tb_switch *sw)
{
	int ret, retries = 10;

	ret = dma_port_flash_update_auth(sw->dma_port);
	switch (ret) {
	case 0:
	case -ETIMEDOUT:
	case -EACCES:
	case -EINVAL:
		 
		break;
	default:
		return ret;
	}

	 
	do {
		u32 status;

		ret = dma_port_flash_update_auth_status(sw->dma_port, &status);
		if (ret < 0 && ret != -ETIMEDOUT)
			return ret;
		if (ret > 0) {
			if (status) {
				tb_sw_warn(sw, "failed to authenticate NVM\n");
				nvm_set_auth_status(sw, status);
			}

			tb_sw_info(sw, "power cycling the switch now\n");
			dma_port_power_cycle(sw->dma_port);
			return 0;
		}

		msleep(500);
	} while (--retries);

	return -ETIMEDOUT;
}

static void nvm_authenticate_start_dma_port(struct tb_switch *sw)
{
	struct pci_dev *root_port;

	 
	root_port = pcie_find_root_port(sw->tb->nhi->pdev);
	if (root_port)
		pm_runtime_get_noresume(&root_port->dev);
}

static void nvm_authenticate_complete_dma_port(struct tb_switch *sw)
{
	struct pci_dev *root_port;

	root_port = pcie_find_root_port(sw->tb->nhi->pdev);
	if (root_port)
		pm_runtime_put(&root_port->dev);
}

static inline bool nvm_readable(struct tb_switch *sw)
{
	if (tb_switch_is_usb4(sw)) {
		 
		return usb4_switch_nvm_sector_size(sw) > 0;
	}

	 
	return !!sw->dma_port;
}

static inline bool nvm_upgradeable(struct tb_switch *sw)
{
	if (sw->no_nvm_upgrade)
		return false;
	return nvm_readable(sw);
}

static int nvm_authenticate(struct tb_switch *sw, bool auth_only)
{
	int ret;

	if (tb_switch_is_usb4(sw)) {
		if (auth_only) {
			ret = usb4_switch_nvm_set_offset(sw, 0);
			if (ret)
				return ret;
		}
		sw->nvm->authenticating = true;
		return usb4_switch_nvm_authenticate(sw);
	}
	if (auth_only)
		return -EOPNOTSUPP;

	sw->nvm->authenticating = true;
	if (!tb_route(sw)) {
		nvm_authenticate_start_dma_port(sw);
		ret = nvm_authenticate_host_dma_port(sw);
	} else {
		ret = nvm_authenticate_device_dma_port(sw);
	}

	return ret;
}

 
int tb_switch_nvm_read(struct tb_switch *sw, unsigned int address, void *buf,
		       size_t size)
{
	if (tb_switch_is_usb4(sw))
		return usb4_switch_nvm_read(sw, address, buf, size);
	return dma_port_flash_read(sw->dma_port, address, buf, size);
}

static int nvm_read(void *priv, unsigned int offset, void *val, size_t bytes)
{
	struct tb_nvm *nvm = priv;
	struct tb_switch *sw = tb_to_switch(nvm->dev);
	int ret;

	pm_runtime_get_sync(&sw->dev);

	if (!mutex_trylock(&sw->tb->lock)) {
		ret = restart_syscall();
		goto out;
	}

	ret = tb_switch_nvm_read(sw, offset, val, bytes);
	mutex_unlock(&sw->tb->lock);

out:
	pm_runtime_mark_last_busy(&sw->dev);
	pm_runtime_put_autosuspend(&sw->dev);

	return ret;
}

static int nvm_write(void *priv, unsigned int offset, void *val, size_t bytes)
{
	struct tb_nvm *nvm = priv;
	struct tb_switch *sw = tb_to_switch(nvm->dev);
	int ret;

	if (!mutex_trylock(&sw->tb->lock))
		return restart_syscall();

	 
	ret = tb_nvm_write_buf(nvm, offset, val, bytes);
	mutex_unlock(&sw->tb->lock);

	return ret;
}

static int tb_switch_nvm_add(struct tb_switch *sw)
{
	struct tb_nvm *nvm;
	int ret;

	if (!nvm_readable(sw))
		return 0;

	nvm = tb_nvm_alloc(&sw->dev);
	if (IS_ERR(nvm)) {
		ret = PTR_ERR(nvm) == -EOPNOTSUPP ? 0 : PTR_ERR(nvm);
		goto err_nvm;
	}

	ret = tb_nvm_read_version(nvm);
	if (ret)
		goto err_nvm;

	 
	if (!sw->safe_mode) {
		ret = tb_nvm_add_active(nvm, nvm_read);
		if (ret)
			goto err_nvm;
	}

	if (!sw->no_nvm_upgrade) {
		ret = tb_nvm_add_non_active(nvm, nvm_write);
		if (ret)
			goto err_nvm;
	}

	sw->nvm = nvm;
	return 0;

err_nvm:
	tb_sw_dbg(sw, "NVM upgrade disabled\n");
	sw->no_nvm_upgrade = true;
	if (!IS_ERR(nvm))
		tb_nvm_free(nvm);

	return ret;
}

static void tb_switch_nvm_remove(struct tb_switch *sw)
{
	struct tb_nvm *nvm;

	nvm = sw->nvm;
	sw->nvm = NULL;

	if (!nvm)
		return;

	 
	if (!nvm->authenticating)
		nvm_clear_auth_status(sw);

	tb_nvm_free(nvm);
}

 

static const char *tb_port_type(const struct tb_regs_port_header *port)
{
	switch (port->type >> 16) {
	case 0:
		switch ((u8) port->type) {
		case 0:
			return "Inactive";
		case 1:
			return "Port";
		case 2:
			return "NHI";
		default:
			return "unknown";
		}
	case 0x2:
		return "Ethernet";
	case 0x8:
		return "SATA";
	case 0xe:
		return "DP/HDMI";
	case 0x10:
		return "PCIe";
	case 0x20:
		return "USB";
	default:
		return "unknown";
	}
}

static void tb_dump_port(struct tb *tb, const struct tb_port *port)
{
	const struct tb_regs_port_header *regs = &port->config;

	tb_dbg(tb,
	       " Port %d: %x:%x (Revision: %d, TB Version: %d, Type: %s (%#x))\n",
	       regs->port_number, regs->vendor_id, regs->device_id,
	       regs->revision, regs->thunderbolt_version, tb_port_type(regs),
	       regs->type);
	tb_dbg(tb, "  Max hop id (in/out): %d/%d\n",
	       regs->max_in_hop_id, regs->max_out_hop_id);
	tb_dbg(tb, "  Max counters: %d\n", regs->max_counters);
	tb_dbg(tb, "  NFC Credits: %#x\n", regs->nfc_credits);
	tb_dbg(tb, "  Credits (total/control): %u/%u\n", port->total_credits,
	       port->ctl_credits);
}

 
int tb_port_state(struct tb_port *port)
{
	struct tb_cap_phy phy;
	int res;
	if (port->cap_phy == 0) {
		tb_port_WARN(port, "does not have a PHY\n");
		return -EINVAL;
	}
	res = tb_port_read(port, &phy, TB_CFG_PORT, port->cap_phy, 2);
	if (res)
		return res;
	return phy.state;
}

 
int tb_wait_for_port(struct tb_port *port, bool wait_if_unplugged)
{
	int retries = 10;
	int state;
	if (!port->cap_phy) {
		tb_port_WARN(port, "does not have PHY\n");
		return -EINVAL;
	}
	if (tb_is_upstream_port(port)) {
		tb_port_WARN(port, "is the upstream port\n");
		return -EINVAL;
	}

	while (retries--) {
		state = tb_port_state(port);
		switch (state) {
		case TB_PORT_DISABLED:
			tb_port_dbg(port, "is disabled (state: 0)\n");
			return 0;

		case TB_PORT_UNPLUGGED:
			if (wait_if_unplugged) {
				 
				tb_port_dbg(port,
					    "is unplugged (state: 7), retrying...\n");
				msleep(100);
				break;
			}
			tb_port_dbg(port, "is unplugged (state: 7)\n");
			return 0;

		case TB_PORT_UP:
		case TB_PORT_TX_CL0S:
		case TB_PORT_RX_CL0S:
		case TB_PORT_CL1:
		case TB_PORT_CL2:
			tb_port_dbg(port, "is connected, link is up (state: %d)\n", state);
			return 1;

		default:
			if (state < 0)
				return state;

			 
			tb_port_dbg(port,
				    "is connected, link is not up (state: %d), retrying...\n",
				    state);
			msleep(100);
		}

	}
	tb_port_warn(port,
		     "failed to reach state TB_PORT_UP. Ignoring port...\n");
	return 0;
}

 
int tb_port_add_nfc_credits(struct tb_port *port, int credits)
{
	u32 nfc_credits;

	if (credits == 0 || port->sw->is_unplugged)
		return 0;

	 
	if (tb_switch_is_usb4(port->sw) && !tb_port_is_null(port))
		return 0;

	nfc_credits = port->config.nfc_credits & ADP_CS_4_NFC_BUFFERS_MASK;
	if (credits < 0)
		credits = max_t(int, -nfc_credits, credits);

	nfc_credits += credits;

	tb_port_dbg(port, "adding %d NFC credits to %lu", credits,
		    port->config.nfc_credits & ADP_CS_4_NFC_BUFFERS_MASK);

	port->config.nfc_credits &= ~ADP_CS_4_NFC_BUFFERS_MASK;
	port->config.nfc_credits |= nfc_credits;

	return tb_port_write(port, &port->config.nfc_credits,
			     TB_CFG_PORT, ADP_CS_4, 1);
}

 
int tb_port_clear_counter(struct tb_port *port, int counter)
{
	u32 zero[3] = { 0, 0, 0 };
	tb_port_dbg(port, "clearing counter %d\n", counter);
	return tb_port_write(port, zero, TB_CFG_COUNTERS, 3 * counter, 3);
}

 
int tb_port_unlock(struct tb_port *port)
{
	if (tb_switch_is_icm(port->sw))
		return 0;
	if (!tb_port_is_null(port))
		return -EINVAL;
	if (tb_switch_is_usb4(port->sw))
		return usb4_port_unlock(port);
	return 0;
}

static int __tb_port_enable(struct tb_port *port, bool enable)
{
	int ret;
	u32 phy;

	if (!tb_port_is_null(port))
		return -EINVAL;

	ret = tb_port_read(port, &phy, TB_CFG_PORT,
			   port->cap_phy + LANE_ADP_CS_1, 1);
	if (ret)
		return ret;

	if (enable)
		phy &= ~LANE_ADP_CS_1_LD;
	else
		phy |= LANE_ADP_CS_1_LD;


	ret = tb_port_write(port, &phy, TB_CFG_PORT,
			    port->cap_phy + LANE_ADP_CS_1, 1);
	if (ret)
		return ret;

	tb_port_dbg(port, "lane %s\n", str_enabled_disabled(enable));
	return 0;
}

 
int tb_port_enable(struct tb_port *port)
{
	return __tb_port_enable(port, true);
}

 
int tb_port_disable(struct tb_port *port)
{
	return __tb_port_enable(port, false);
}

 
static int tb_init_port(struct tb_port *port)
{
	int res;
	int cap;

	INIT_LIST_HEAD(&port->list);

	 
	if (!port->port)
		return 0;

	res = tb_port_read(port, &port->config, TB_CFG_PORT, 0, 8);
	if (res) {
		if (res == -ENODEV) {
			tb_dbg(port->sw->tb, " Port %d: not implemented\n",
			       port->port);
			port->disabled = true;
			return 0;
		}
		return res;
	}

	 
	if (port->config.type == TB_TYPE_PORT) {
		cap = tb_port_find_cap(port, TB_PORT_CAP_PHY);

		if (cap > 0)
			port->cap_phy = cap;
		else
			tb_port_WARN(port, "non switch port without a PHY\n");

		cap = tb_port_find_cap(port, TB_PORT_CAP_USB4);
		if (cap > 0)
			port->cap_usb4 = cap;

		 
		if (port->cap_usb4) {
			struct tb_regs_hop hop;

			if (!tb_port_read(port, &hop, TB_CFG_HOPS, 0, 2))
				port->ctl_credits = hop.initial_credits;
		}
		if (!port->ctl_credits)
			port->ctl_credits = 2;

	} else {
		cap = tb_port_find_cap(port, TB_PORT_CAP_ADAP);
		if (cap > 0)
			port->cap_adap = cap;
	}

	port->total_credits =
		(port->config.nfc_credits & ADP_CS_4_TOTAL_BUFFERS_MASK) >>
		ADP_CS_4_TOTAL_BUFFERS_SHIFT;

	tb_dump_port(port->sw->tb, port);
	return 0;
}

static int tb_port_alloc_hopid(struct tb_port *port, bool in, int min_hopid,
			       int max_hopid)
{
	int port_max_hopid;
	struct ida *ida;

	if (in) {
		port_max_hopid = port->config.max_in_hop_id;
		ida = &port->in_hopids;
	} else {
		port_max_hopid = port->config.max_out_hop_id;
		ida = &port->out_hopids;
	}

	 
	if (!tb_port_is_nhi(port) && min_hopid < TB_PATH_MIN_HOPID)
		min_hopid = TB_PATH_MIN_HOPID;

	if (max_hopid < 0 || max_hopid > port_max_hopid)
		max_hopid = port_max_hopid;

	return ida_simple_get(ida, min_hopid, max_hopid + 1, GFP_KERNEL);
}

 
int tb_port_alloc_in_hopid(struct tb_port *port, int min_hopid, int max_hopid)
{
	return tb_port_alloc_hopid(port, true, min_hopid, max_hopid);
}

 
int tb_port_alloc_out_hopid(struct tb_port *port, int min_hopid, int max_hopid)
{
	return tb_port_alloc_hopid(port, false, min_hopid, max_hopid);
}

 
void tb_port_release_in_hopid(struct tb_port *port, int hopid)
{
	ida_simple_remove(&port->in_hopids, hopid);
}

 
void tb_port_release_out_hopid(struct tb_port *port, int hopid)
{
	ida_simple_remove(&port->out_hopids, hopid);
}

static inline bool tb_switch_is_reachable(const struct tb_switch *parent,
					  const struct tb_switch *sw)
{
	u64 mask = (1ULL << parent->config.depth * 8) - 1;
	return (tb_route(parent) & mask) == (tb_route(sw) & mask);
}

 
struct tb_port *tb_next_port_on_path(struct tb_port *start, struct tb_port *end,
				     struct tb_port *prev)
{
	struct tb_port *next;

	if (!prev)
		return start;

	if (prev->sw == end->sw) {
		if (prev == end)
			return NULL;
		return end;
	}

	if (tb_switch_is_reachable(prev->sw, end->sw)) {
		next = tb_port_at(tb_route(end->sw), prev->sw);
		 
		if (prev->remote &&
		    (next == prev || next->dual_link_port == prev))
			next = prev->remote;
	} else {
		if (tb_is_upstream_port(prev)) {
			next = prev->remote;
		} else {
			next = tb_upstream_port(prev->sw);
			 
			if (next->dual_link_port &&
			    next->link_nr != prev->link_nr) {
				next = next->dual_link_port;
			}
		}
	}

	return next != prev ? next : NULL;
}

 
int tb_port_get_link_speed(struct tb_port *port)
{
	u32 val, speed;
	int ret;

	if (!port->cap_phy)
		return -EINVAL;

	ret = tb_port_read(port, &val, TB_CFG_PORT,
			   port->cap_phy + LANE_ADP_CS_1, 1);
	if (ret)
		return ret;

	speed = (val & LANE_ADP_CS_1_CURRENT_SPEED_MASK) >>
		LANE_ADP_CS_1_CURRENT_SPEED_SHIFT;

	switch (speed) {
	case LANE_ADP_CS_1_CURRENT_SPEED_GEN4:
		return 40;
	case LANE_ADP_CS_1_CURRENT_SPEED_GEN3:
		return 20;
	default:
		return 10;
	}
}

 
int tb_port_get_link_width(struct tb_port *port)
{
	u32 val;
	int ret;

	if (!port->cap_phy)
		return -EINVAL;

	ret = tb_port_read(port, &val, TB_CFG_PORT,
			   port->cap_phy + LANE_ADP_CS_1, 1);
	if (ret)
		return ret;

	 
	return (val & LANE_ADP_CS_1_CURRENT_WIDTH_MASK) >>
		LANE_ADP_CS_1_CURRENT_WIDTH_SHIFT;
}

static bool tb_port_is_width_supported(struct tb_port *port,
				       unsigned int width_mask)
{
	u32 phy, widths;
	int ret;

	if (!port->cap_phy)
		return false;

	ret = tb_port_read(port, &phy, TB_CFG_PORT,
			   port->cap_phy + LANE_ADP_CS_0, 1);
	if (ret)
		return false;

	widths = (phy & LANE_ADP_CS_0_SUPPORTED_WIDTH_MASK) >>
		LANE_ADP_CS_0_SUPPORTED_WIDTH_SHIFT;

	return widths & width_mask;
}

static bool is_gen4_link(struct tb_port *port)
{
	return tb_port_get_link_speed(port) > 20;
}

 
int tb_port_set_link_width(struct tb_port *port, enum tb_link_width width)
{
	u32 val;
	int ret;

	if (!port->cap_phy)
		return -EINVAL;

	ret = tb_port_read(port, &val, TB_CFG_PORT,
			   port->cap_phy + LANE_ADP_CS_1, 1);
	if (ret)
		return ret;

	val &= ~LANE_ADP_CS_1_TARGET_WIDTH_MASK;
	switch (width) {
	case TB_LINK_WIDTH_SINGLE:
		 
		if (is_gen4_link(port))
			return -EOPNOTSUPP;
		val |= LANE_ADP_CS_1_TARGET_WIDTH_SINGLE <<
			LANE_ADP_CS_1_TARGET_WIDTH_SHIFT;
		break;
	case TB_LINK_WIDTH_DUAL:
		val |= LANE_ADP_CS_1_TARGET_WIDTH_DUAL <<
			LANE_ADP_CS_1_TARGET_WIDTH_SHIFT;
		break;
	default:
		return -EINVAL;
	}

	return tb_port_write(port, &val, TB_CFG_PORT,
			     port->cap_phy + LANE_ADP_CS_1, 1);
}

 
static int tb_port_set_lane_bonding(struct tb_port *port, bool bonding)
{
	u32 val;
	int ret;

	if (!port->cap_phy)
		return -EINVAL;

	ret = tb_port_read(port, &val, TB_CFG_PORT,
			   port->cap_phy + LANE_ADP_CS_1, 1);
	if (ret)
		return ret;

	if (bonding)
		val |= LANE_ADP_CS_1_LB;
	else
		val &= ~LANE_ADP_CS_1_LB;

	return tb_port_write(port, &val, TB_CFG_PORT,
			     port->cap_phy + LANE_ADP_CS_1, 1);
}

 
int tb_port_lane_bonding_enable(struct tb_port *port)
{
	enum tb_link_width width;
	int ret;

	 
	width = tb_port_get_link_width(port);
	if (width == TB_LINK_WIDTH_SINGLE) {
		ret = tb_port_set_link_width(port, TB_LINK_WIDTH_DUAL);
		if (ret)
			goto err_lane0;
	}

	width = tb_port_get_link_width(port->dual_link_port);
	if (width == TB_LINK_WIDTH_SINGLE) {
		ret = tb_port_set_link_width(port->dual_link_port,
					     TB_LINK_WIDTH_DUAL);
		if (ret)
			goto err_lane0;
	}

	 
	if (width == TB_LINK_WIDTH_SINGLE && !tb_is_upstream_port(port)) {
		ret = tb_port_set_lane_bonding(port, true);
		if (ret)
			goto err_lane1;
	}

	 
	port->bonded = true;
	port->dual_link_port->bonded = true;

	return 0;

err_lane1:
	tb_port_set_link_width(port->dual_link_port, TB_LINK_WIDTH_SINGLE);
err_lane0:
	tb_port_set_link_width(port, TB_LINK_WIDTH_SINGLE);

	return ret;
}

 
void tb_port_lane_bonding_disable(struct tb_port *port)
{
	tb_port_set_lane_bonding(port, false);
	tb_port_set_link_width(port->dual_link_port, TB_LINK_WIDTH_SINGLE);
	tb_port_set_link_width(port, TB_LINK_WIDTH_SINGLE);
	port->dual_link_port->bonded = false;
	port->bonded = false;
}

 
int tb_port_wait_for_link_width(struct tb_port *port, unsigned int width_mask,
				int timeout_msec)
{
	ktime_t timeout = ktime_add_ms(ktime_get(), timeout_msec);
	int ret;

	 
	if ((width_mask & TB_LINK_WIDTH_SINGLE) && is_gen4_link(port))
		return -EOPNOTSUPP;

	do {
		ret = tb_port_get_link_width(port);
		if (ret < 0) {
			 
			if (ret != -EACCES)
				return ret;
		} else if (ret & width_mask) {
			return 0;
		}

		usleep_range(1000, 2000);
	} while (ktime_before(ktime_get(), timeout));

	return -ETIMEDOUT;
}

static int tb_port_do_update_credits(struct tb_port *port)
{
	u32 nfc_credits;
	int ret;

	ret = tb_port_read(port, &nfc_credits, TB_CFG_PORT, ADP_CS_4, 1);
	if (ret)
		return ret;

	if (nfc_credits != port->config.nfc_credits) {
		u32 total;

		total = (nfc_credits & ADP_CS_4_TOTAL_BUFFERS_MASK) >>
			ADP_CS_4_TOTAL_BUFFERS_SHIFT;

		tb_port_dbg(port, "total credits changed %u -> %u\n",
			    port->total_credits, total);

		port->config.nfc_credits = nfc_credits;
		port->total_credits = total;
	}

	return 0;
}

 
int tb_port_update_credits(struct tb_port *port)
{
	int ret;

	ret = tb_port_do_update_credits(port);
	if (ret)
		return ret;
	return tb_port_do_update_credits(port->dual_link_port);
}

static int tb_port_start_lane_initialization(struct tb_port *port)
{
	int ret;

	if (tb_switch_is_usb4(port->sw))
		return 0;

	ret = tb_lc_start_lane_initialization(port);
	return ret == -EINVAL ? 0 : ret;
}

 
static bool tb_port_resume(struct tb_port *port)
{
	bool has_remote = tb_port_has_remote(port);

	if (port->usb4) {
		usb4_port_device_resume(port->usb4);
	} else if (!has_remote) {
		 
		if (!tb_is_upstream_port(port) || port->xdomain)
			tb_port_start_lane_initialization(port);
	}

	return has_remote || port->xdomain;
}

 
bool tb_port_is_enabled(struct tb_port *port)
{
	switch (port->config.type) {
	case TB_TYPE_PCIE_UP:
	case TB_TYPE_PCIE_DOWN:
		return tb_pci_port_is_enabled(port);

	case TB_TYPE_DP_HDMI_IN:
	case TB_TYPE_DP_HDMI_OUT:
		return tb_dp_port_is_enabled(port);

	case TB_TYPE_USB3_UP:
	case TB_TYPE_USB3_DOWN:
		return tb_usb3_port_is_enabled(port);

	default:
		return false;
	}
}

 
bool tb_usb3_port_is_enabled(struct tb_port *port)
{
	u32 data;

	if (tb_port_read(port, &data, TB_CFG_PORT,
			 port->cap_adap + ADP_USB3_CS_0, 1))
		return false;

	return !!(data & ADP_USB3_CS_0_PE);
}

 
int tb_usb3_port_enable(struct tb_port *port, bool enable)
{
	u32 word = enable ? (ADP_USB3_CS_0_PE | ADP_USB3_CS_0_V)
			  : ADP_USB3_CS_0_V;

	if (!port->cap_adap)
		return -ENXIO;
	return tb_port_write(port, &word, TB_CFG_PORT,
			     port->cap_adap + ADP_USB3_CS_0, 1);
}

 
bool tb_pci_port_is_enabled(struct tb_port *port)
{
	u32 data;

	if (tb_port_read(port, &data, TB_CFG_PORT,
			 port->cap_adap + ADP_PCIE_CS_0, 1))
		return false;

	return !!(data & ADP_PCIE_CS_0_PE);
}

 
int tb_pci_port_enable(struct tb_port *port, bool enable)
{
	u32 word = enable ? ADP_PCIE_CS_0_PE : 0x0;
	if (!port->cap_adap)
		return -ENXIO;
	return tb_port_write(port, &word, TB_CFG_PORT,
			     port->cap_adap + ADP_PCIE_CS_0, 1);
}

 
int tb_dp_port_hpd_is_active(struct tb_port *port)
{
	u32 data;
	int ret;

	ret = tb_port_read(port, &data, TB_CFG_PORT,
			   port->cap_adap + ADP_DP_CS_2, 1);
	if (ret)
		return ret;

	return !!(data & ADP_DP_CS_2_HDP);
}

 
int tb_dp_port_hpd_clear(struct tb_port *port)
{
	u32 data;
	int ret;

	ret = tb_port_read(port, &data, TB_CFG_PORT,
			   port->cap_adap + ADP_DP_CS_3, 1);
	if (ret)
		return ret;

	data |= ADP_DP_CS_3_HDPC;
	return tb_port_write(port, &data, TB_CFG_PORT,
			     port->cap_adap + ADP_DP_CS_3, 1);
}

 
int tb_dp_port_set_hops(struct tb_port *port, unsigned int video,
			unsigned int aux_tx, unsigned int aux_rx)
{
	u32 data[2];
	int ret;

	if (tb_switch_is_usb4(port->sw))
		return 0;

	ret = tb_port_read(port, data, TB_CFG_PORT,
			   port->cap_adap + ADP_DP_CS_0, ARRAY_SIZE(data));
	if (ret)
		return ret;

	data[0] &= ~ADP_DP_CS_0_VIDEO_HOPID_MASK;
	data[1] &= ~ADP_DP_CS_1_AUX_RX_HOPID_MASK;
	data[1] &= ~ADP_DP_CS_1_AUX_RX_HOPID_MASK;

	data[0] |= (video << ADP_DP_CS_0_VIDEO_HOPID_SHIFT) &
		ADP_DP_CS_0_VIDEO_HOPID_MASK;
	data[1] |= aux_tx & ADP_DP_CS_1_AUX_TX_HOPID_MASK;
	data[1] |= (aux_rx << ADP_DP_CS_1_AUX_RX_HOPID_SHIFT) &
		ADP_DP_CS_1_AUX_RX_HOPID_MASK;

	return tb_port_write(port, data, TB_CFG_PORT,
			     port->cap_adap + ADP_DP_CS_0, ARRAY_SIZE(data));
}

 
bool tb_dp_port_is_enabled(struct tb_port *port)
{
	u32 data[2];

	if (tb_port_read(port, data, TB_CFG_PORT, port->cap_adap + ADP_DP_CS_0,
			 ARRAY_SIZE(data)))
		return false;

	return !!(data[0] & (ADP_DP_CS_0_VE | ADP_DP_CS_0_AE));
}

 
int tb_dp_port_enable(struct tb_port *port, bool enable)
{
	u32 data[2];
	int ret;

	ret = tb_port_read(port, data, TB_CFG_PORT,
			  port->cap_adap + ADP_DP_CS_0, ARRAY_SIZE(data));
	if (ret)
		return ret;

	if (enable)
		data[0] |= ADP_DP_CS_0_VE | ADP_DP_CS_0_AE;
	else
		data[0] &= ~(ADP_DP_CS_0_VE | ADP_DP_CS_0_AE);

	return tb_port_write(port, data, TB_CFG_PORT,
			     port->cap_adap + ADP_DP_CS_0, ARRAY_SIZE(data));
}

 

static const char *tb_switch_generation_name(const struct tb_switch *sw)
{
	switch (sw->generation) {
	case 1:
		return "Thunderbolt 1";
	case 2:
		return "Thunderbolt 2";
	case 3:
		return "Thunderbolt 3";
	case 4:
		return "USB4";
	default:
		return "Unknown";
	}
}

static void tb_dump_switch(const struct tb *tb, const struct tb_switch *sw)
{
	const struct tb_regs_switch_header *regs = &sw->config;

	tb_dbg(tb, " %s Switch: %x:%x (Revision: %d, TB Version: %d)\n",
	       tb_switch_generation_name(sw), regs->vendor_id, regs->device_id,
	       regs->revision, regs->thunderbolt_version);
	tb_dbg(tb, "  Max Port Number: %d\n", regs->max_port_number);
	tb_dbg(tb, "  Config:\n");
	tb_dbg(tb,
		"   Upstream Port Number: %d Depth: %d Route String: %#llx Enabled: %d, PlugEventsDelay: %dms\n",
	       regs->upstream_port_number, regs->depth,
	       (((u64) regs->route_hi) << 32) | regs->route_lo,
	       regs->enabled, regs->plug_events_delay);
	tb_dbg(tb, "   unknown1: %#x unknown4: %#x\n",
	       regs->__unknown1, regs->__unknown4);
}

 
int tb_switch_reset(struct tb_switch *sw)
{
	struct tb_cfg_result res;

	if (sw->generation > 1)
		return 0;

	tb_sw_dbg(sw, "resetting switch\n");

	res.err = tb_sw_write(sw, ((u32 *) &sw->config) + 2,
			      TB_CFG_SWITCH, 2, 2);
	if (res.err)
		return res.err;
	res = tb_cfg_reset(sw->tb->ctl, tb_route(sw));
	if (res.err > 0)
		return -EIO;
	return res.err;
}

 
int tb_switch_wait_for_bit(struct tb_switch *sw, u32 offset, u32 bit,
			   u32 value, int timeout_msec)
{
	ktime_t timeout = ktime_add_ms(ktime_get(), timeout_msec);

	do {
		u32 val;
		int ret;

		ret = tb_sw_read(sw, &val, TB_CFG_SWITCH, offset, 1);
		if (ret)
			return ret;

		if ((val & bit) == value)
			return 0;

		usleep_range(50, 100);
	} while (ktime_before(ktime_get(), timeout));

	return -ETIMEDOUT;
}

 
static int tb_plug_events_active(struct tb_switch *sw, bool active)
{
	u32 data;
	int res;

	if (tb_switch_is_icm(sw) || tb_switch_is_usb4(sw))
		return 0;

	sw->config.plug_events_delay = 0xff;
	res = tb_sw_write(sw, ((u32 *) &sw->config) + 4, TB_CFG_SWITCH, 4, 1);
	if (res)
		return res;

	res = tb_sw_read(sw, &data, TB_CFG_SWITCH, sw->cap_plug_events + 1, 1);
	if (res)
		return res;

	if (active) {
		data = data & 0xFFFFFF83;
		switch (sw->config.device_id) {
		case PCI_DEVICE_ID_INTEL_LIGHT_RIDGE:
		case PCI_DEVICE_ID_INTEL_EAGLE_RIDGE:
		case PCI_DEVICE_ID_INTEL_PORT_RIDGE:
			break;
		default:
			 
			if (!tb_switch_is_alpine_ridge(sw))
				data |= TB_PLUG_EVENTS_USB_DISABLE;
		}
	} else {
		data = data | 0x7c;
	}
	return tb_sw_write(sw, &data, TB_CFG_SWITCH,
			   sw->cap_plug_events + 1, 1);
}

static ssize_t authorized_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct tb_switch *sw = tb_to_switch(dev);

	return sysfs_emit(buf, "%u\n", sw->authorized);
}

static int disapprove_switch(struct device *dev, void *not_used)
{
	char *envp[] = { "AUTHORIZED=0", NULL };
	struct tb_switch *sw;

	sw = tb_to_switch(dev);
	if (sw && sw->authorized) {
		int ret;

		 
		ret = device_for_each_child_reverse(&sw->dev, NULL, disapprove_switch);
		if (ret)
			return ret;

		ret = tb_domain_disapprove_switch(sw->tb, sw);
		if (ret)
			return ret;

		sw->authorized = 0;
		kobject_uevent_env(&sw->dev.kobj, KOBJ_CHANGE, envp);
	}

	return 0;
}

static int tb_switch_set_authorized(struct tb_switch *sw, unsigned int val)
{
	char envp_string[13];
	int ret = -EINVAL;
	char *envp[] = { envp_string, NULL };

	if (!mutex_trylock(&sw->tb->lock))
		return restart_syscall();

	if (!!sw->authorized == !!val)
		goto unlock;

	switch (val) {
	 
	case 0:
		if (tb_route(sw)) {
			ret = disapprove_switch(&sw->dev, NULL);
			goto unlock;
		}
		break;

	 
	case 1:
		if (sw->key)
			ret = tb_domain_approve_switch_key(sw->tb, sw);
		else
			ret = tb_domain_approve_switch(sw->tb, sw);
		break;

	 
	case 2:
		if (sw->key)
			ret = tb_domain_challenge_switch_key(sw->tb, sw);
		break;

	default:
		break;
	}

	if (!ret) {
		sw->authorized = val;
		 
		sprintf(envp_string, "AUTHORIZED=%u", sw->authorized);
		kobject_uevent_env(&sw->dev.kobj, KOBJ_CHANGE, envp);
	}

unlock:
	mutex_unlock(&sw->tb->lock);
	return ret;
}

static ssize_t authorized_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct tb_switch *sw = tb_to_switch(dev);
	unsigned int val;
	ssize_t ret;

	ret = kstrtouint(buf, 0, &val);
	if (ret)
		return ret;
	if (val > 2)
		return -EINVAL;

	pm_runtime_get_sync(&sw->dev);
	ret = tb_switch_set_authorized(sw, val);
	pm_runtime_mark_last_busy(&sw->dev);
	pm_runtime_put_autosuspend(&sw->dev);

	return ret ? ret : count;
}
static DEVICE_ATTR_RW(authorized);

static ssize_t boot_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct tb_switch *sw = tb_to_switch(dev);

	return sysfs_emit(buf, "%u\n", sw->boot);
}
static DEVICE_ATTR_RO(boot);

static ssize_t device_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct tb_switch *sw = tb_to_switch(dev);

	return sysfs_emit(buf, "%#x\n", sw->device);
}
static DEVICE_ATTR_RO(device);

static ssize_t
device_name_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tb_switch *sw = tb_to_switch(dev);

	return sysfs_emit(buf, "%s\n", sw->device_name ?: "");
}
static DEVICE_ATTR_RO(device_name);

static ssize_t
generation_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tb_switch *sw = tb_to_switch(dev);

	return sysfs_emit(buf, "%u\n", sw->generation);
}
static DEVICE_ATTR_RO(generation);

static ssize_t key_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct tb_switch *sw = tb_to_switch(dev);
	ssize_t ret;

	if (!mutex_trylock(&sw->tb->lock))
		return restart_syscall();

	if (sw->key)
		ret = sysfs_emit(buf, "%*phN\n", TB_SWITCH_KEY_SIZE, sw->key);
	else
		ret = sysfs_emit(buf, "\n");

	mutex_unlock(&sw->tb->lock);
	return ret;
}

static ssize_t key_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct tb_switch *sw = tb_to_switch(dev);
	u8 key[TB_SWITCH_KEY_SIZE];
	ssize_t ret = count;
	bool clear = false;

	if (!strcmp(buf, "\n"))
		clear = true;
	else if (hex2bin(key, buf, sizeof(key)))
		return -EINVAL;

	if (!mutex_trylock(&sw->tb->lock))
		return restart_syscall();

	if (sw->authorized) {
		ret = -EBUSY;
	} else {
		kfree(sw->key);
		if (clear) {
			sw->key = NULL;
		} else {
			sw->key = kmemdup(key, sizeof(key), GFP_KERNEL);
			if (!sw->key)
				ret = -ENOMEM;
		}
	}

	mutex_unlock(&sw->tb->lock);
	return ret;
}
static DEVICE_ATTR(key, 0600, key_show, key_store);

static ssize_t speed_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	struct tb_switch *sw = tb_to_switch(dev);

	return sysfs_emit(buf, "%u.0 Gb/s\n", sw->link_speed);
}

 
static DEVICE_ATTR(rx_speed, 0444, speed_show, NULL);
static DEVICE_ATTR(tx_speed, 0444, speed_show, NULL);

static ssize_t rx_lanes_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct tb_switch *sw = tb_to_switch(dev);
	unsigned int width;

	switch (sw->link_width) {
	case TB_LINK_WIDTH_SINGLE:
	case TB_LINK_WIDTH_ASYM_TX:
		width = 1;
		break;
	case TB_LINK_WIDTH_DUAL:
		width = 2;
		break;
	case TB_LINK_WIDTH_ASYM_RX:
		width = 3;
		break;
	default:
		WARN_ON_ONCE(1);
		return -EINVAL;
	}

	return sysfs_emit(buf, "%u\n", width);
}
static DEVICE_ATTR(rx_lanes, 0444, rx_lanes_show, NULL);

static ssize_t tx_lanes_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct tb_switch *sw = tb_to_switch(dev);
	unsigned int width;

	switch (sw->link_width) {
	case TB_LINK_WIDTH_SINGLE:
	case TB_LINK_WIDTH_ASYM_RX:
		width = 1;
		break;
	case TB_LINK_WIDTH_DUAL:
		width = 2;
		break;
	case TB_LINK_WIDTH_ASYM_TX:
		width = 3;
		break;
	default:
		WARN_ON_ONCE(1);
		return -EINVAL;
	}

	return sysfs_emit(buf, "%u\n", width);
}
static DEVICE_ATTR(tx_lanes, 0444, tx_lanes_show, NULL);

static ssize_t nvm_authenticate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tb_switch *sw = tb_to_switch(dev);
	u32 status;

	nvm_get_auth_status(sw, &status);
	return sysfs_emit(buf, "%#x\n", status);
}

static ssize_t nvm_authenticate_sysfs(struct device *dev, const char *buf,
				      bool disconnect)
{
	struct tb_switch *sw = tb_to_switch(dev);
	int val, ret;

	pm_runtime_get_sync(&sw->dev);

	if (!mutex_trylock(&sw->tb->lock)) {
		ret = restart_syscall();
		goto exit_rpm;
	}

	if (sw->no_nvm_upgrade) {
		ret = -EOPNOTSUPP;
		goto exit_unlock;
	}

	 
	if (!sw->nvm) {
		ret = -EAGAIN;
		goto exit_unlock;
	}

	ret = kstrtoint(buf, 10, &val);
	if (ret)
		goto exit_unlock;

	 
	nvm_clear_auth_status(sw);

	if (val > 0) {
		if (val == AUTHENTICATE_ONLY) {
			if (disconnect)
				ret = -EINVAL;
			else
				ret = nvm_authenticate(sw, true);
		} else {
			if (!sw->nvm->flushed) {
				if (!sw->nvm->buf) {
					ret = -EINVAL;
					goto exit_unlock;
				}

				ret = nvm_validate_and_write(sw);
				if (ret || val == WRITE_ONLY)
					goto exit_unlock;
			}
			if (val == WRITE_AND_AUTHENTICATE) {
				if (disconnect)
					ret = tb_lc_force_power(sw);
				else
					ret = nvm_authenticate(sw, false);
			}
		}
	}

exit_unlock:
	mutex_unlock(&sw->tb->lock);
exit_rpm:
	pm_runtime_mark_last_busy(&sw->dev);
	pm_runtime_put_autosuspend(&sw->dev);

	return ret;
}

static ssize_t nvm_authenticate_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = nvm_authenticate_sysfs(dev, buf, false);
	if (ret)
		return ret;
	return count;
}
static DEVICE_ATTR_RW(nvm_authenticate);

static ssize_t nvm_authenticate_on_disconnect_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return nvm_authenticate_show(dev, attr, buf);
}

static ssize_t nvm_authenticate_on_disconnect_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;

	ret = nvm_authenticate_sysfs(dev, buf, true);
	return ret ? ret : count;
}
static DEVICE_ATTR_RW(nvm_authenticate_on_disconnect);

static ssize_t nvm_version_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tb_switch *sw = tb_to_switch(dev);
	int ret;

	if (!mutex_trylock(&sw->tb->lock))
		return restart_syscall();

	if (sw->safe_mode)
		ret = -ENODATA;
	else if (!sw->nvm)
		ret = -EAGAIN;
	else
		ret = sysfs_emit(buf, "%x.%x\n", sw->nvm->major, sw->nvm->minor);

	mutex_unlock(&sw->tb->lock);

	return ret;
}
static DEVICE_ATTR_RO(nvm_version);

static ssize_t vendor_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct tb_switch *sw = tb_to_switch(dev);

	return sysfs_emit(buf, "%#x\n", sw->vendor);
}
static DEVICE_ATTR_RO(vendor);

static ssize_t
vendor_name_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tb_switch *sw = tb_to_switch(dev);

	return sysfs_emit(buf, "%s\n", sw->vendor_name ?: "");
}
static DEVICE_ATTR_RO(vendor_name);

static ssize_t unique_id_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct tb_switch *sw = tb_to_switch(dev);

	return sysfs_emit(buf, "%pUb\n", sw->uuid);
}
static DEVICE_ATTR_RO(unique_id);

static struct attribute *switch_attrs[] = {
	&dev_attr_authorized.attr,
	&dev_attr_boot.attr,
	&dev_attr_device.attr,
	&dev_attr_device_name.attr,
	&dev_attr_generation.attr,
	&dev_attr_key.attr,
	&dev_attr_nvm_authenticate.attr,
	&dev_attr_nvm_authenticate_on_disconnect.attr,
	&dev_attr_nvm_version.attr,
	&dev_attr_rx_speed.attr,
	&dev_attr_rx_lanes.attr,
	&dev_attr_tx_speed.attr,
	&dev_attr_tx_lanes.attr,
	&dev_attr_vendor.attr,
	&dev_attr_vendor_name.attr,
	&dev_attr_unique_id.attr,
	NULL,
};

static umode_t switch_attr_is_visible(struct kobject *kobj,
				      struct attribute *attr, int n)
{
	struct device *dev = kobj_to_dev(kobj);
	struct tb_switch *sw = tb_to_switch(dev);

	if (attr == &dev_attr_authorized.attr) {
		if (sw->tb->security_level == TB_SECURITY_NOPCIE ||
		    sw->tb->security_level == TB_SECURITY_DPONLY)
			return 0;
	} else if (attr == &dev_attr_device.attr) {
		if (!sw->device)
			return 0;
	} else if (attr == &dev_attr_device_name.attr) {
		if (!sw->device_name)
			return 0;
	} else if (attr == &dev_attr_vendor.attr)  {
		if (!sw->vendor)
			return 0;
	} else if (attr == &dev_attr_vendor_name.attr)  {
		if (!sw->vendor_name)
			return 0;
	} else if (attr == &dev_attr_key.attr) {
		if (tb_route(sw) &&
		    sw->tb->security_level == TB_SECURITY_SECURE &&
		    sw->security_level == TB_SECURITY_SECURE)
			return attr->mode;
		return 0;
	} else if (attr == &dev_attr_rx_speed.attr ||
		   attr == &dev_attr_rx_lanes.attr ||
		   attr == &dev_attr_tx_speed.attr ||
		   attr == &dev_attr_tx_lanes.attr) {
		if (tb_route(sw))
			return attr->mode;
		return 0;
	} else if (attr == &dev_attr_nvm_authenticate.attr) {
		if (nvm_upgradeable(sw))
			return attr->mode;
		return 0;
	} else if (attr == &dev_attr_nvm_version.attr) {
		if (nvm_readable(sw))
			return attr->mode;
		return 0;
	} else if (attr == &dev_attr_boot.attr) {
		if (tb_route(sw))
			return attr->mode;
		return 0;
	} else if (attr == &dev_attr_nvm_authenticate_on_disconnect.attr) {
		if (sw->quirks & QUIRK_FORCE_POWER_LINK_CONTROLLER)
			return attr->mode;
		return 0;
	}

	return sw->safe_mode ? 0 : attr->mode;
}

static const struct attribute_group switch_group = {
	.is_visible = switch_attr_is_visible,
	.attrs = switch_attrs,
};

static const struct attribute_group *switch_groups[] = {
	&switch_group,
	NULL,
};

static void tb_switch_release(struct device *dev)
{
	struct tb_switch *sw = tb_to_switch(dev);
	struct tb_port *port;

	dma_port_free(sw->dma_port);

	tb_switch_for_each_port(sw, port) {
		ida_destroy(&port->in_hopids);
		ida_destroy(&port->out_hopids);
	}

	kfree(sw->uuid);
	kfree(sw->device_name);
	kfree(sw->vendor_name);
	kfree(sw->ports);
	kfree(sw->drom);
	kfree(sw->key);
	kfree(sw);
}

static int tb_switch_uevent(const struct device *dev, struct kobj_uevent_env *env)
{
	const struct tb_switch *sw = tb_to_switch(dev);
	const char *type;

	if (tb_switch_is_usb4(sw)) {
		if (add_uevent_var(env, "USB4_VERSION=%u.0",
				   usb4_switch_version(sw)))
			return -ENOMEM;
	}

	if (!tb_route(sw)) {
		type = "host";
	} else {
		const struct tb_port *port;
		bool hub = false;

		 
		tb_switch_for_each_port(sw, port) {
			if (!port->disabled && !tb_is_upstream_port(port) &&
			     tb_port_is_null(port)) {
				hub = true;
				break;
			}
		}

		type = hub ? "hub" : "device";
	}

	if (add_uevent_var(env, "USB4_TYPE=%s", type))
		return -ENOMEM;
	return 0;
}

 
static int __maybe_unused tb_switch_runtime_suspend(struct device *dev)
{
	struct tb_switch *sw = tb_to_switch(dev);
	const struct tb_cm_ops *cm_ops = sw->tb->cm_ops;

	if (cm_ops->runtime_suspend_switch)
		return cm_ops->runtime_suspend_switch(sw);

	return 0;
}

static int __maybe_unused tb_switch_runtime_resume(struct device *dev)
{
	struct tb_switch *sw = tb_to_switch(dev);
	const struct tb_cm_ops *cm_ops = sw->tb->cm_ops;

	if (cm_ops->runtime_resume_switch)
		return cm_ops->runtime_resume_switch(sw);
	return 0;
}

static const struct dev_pm_ops tb_switch_pm_ops = {
	SET_RUNTIME_PM_OPS(tb_switch_runtime_suspend, tb_switch_runtime_resume,
			   NULL)
};

struct device_type tb_switch_type = {
	.name = "thunderbolt_device",
	.release = tb_switch_release,
	.uevent = tb_switch_uevent,
	.pm = &tb_switch_pm_ops,
};

static int tb_switch_get_generation(struct tb_switch *sw)
{
	if (tb_switch_is_usb4(sw))
		return 4;

	if (sw->config.vendor_id == PCI_VENDOR_ID_INTEL) {
		switch (sw->config.device_id) {
		case PCI_DEVICE_ID_INTEL_LIGHT_RIDGE:
		case PCI_DEVICE_ID_INTEL_EAGLE_RIDGE:
		case PCI_DEVICE_ID_INTEL_LIGHT_PEAK:
		case PCI_DEVICE_ID_INTEL_CACTUS_RIDGE_2C:
		case PCI_DEVICE_ID_INTEL_CACTUS_RIDGE_4C:
		case PCI_DEVICE_ID_INTEL_PORT_RIDGE:
		case PCI_DEVICE_ID_INTEL_REDWOOD_RIDGE_2C_BRIDGE:
		case PCI_DEVICE_ID_INTEL_REDWOOD_RIDGE_4C_BRIDGE:
			return 1;

		case PCI_DEVICE_ID_INTEL_WIN_RIDGE_2C_BRIDGE:
		case PCI_DEVICE_ID_INTEL_FALCON_RIDGE_2C_BRIDGE:
		case PCI_DEVICE_ID_INTEL_FALCON_RIDGE_4C_BRIDGE:
			return 2;

		case PCI_DEVICE_ID_INTEL_ALPINE_RIDGE_LP_BRIDGE:
		case PCI_DEVICE_ID_INTEL_ALPINE_RIDGE_2C_BRIDGE:
		case PCI_DEVICE_ID_INTEL_ALPINE_RIDGE_4C_BRIDGE:
		case PCI_DEVICE_ID_INTEL_ALPINE_RIDGE_C_2C_BRIDGE:
		case PCI_DEVICE_ID_INTEL_ALPINE_RIDGE_C_4C_BRIDGE:
		case PCI_DEVICE_ID_INTEL_TITAN_RIDGE_2C_BRIDGE:
		case PCI_DEVICE_ID_INTEL_TITAN_RIDGE_4C_BRIDGE:
		case PCI_DEVICE_ID_INTEL_TITAN_RIDGE_DD_BRIDGE:
		case PCI_DEVICE_ID_INTEL_ICL_NHI0:
		case PCI_DEVICE_ID_INTEL_ICL_NHI1:
			return 3;
		}
	}

	 
	tb_sw_warn(sw, "unsupported switch device id %#x\n",
		   sw->config.device_id);
	return 1;
}

static bool tb_switch_exceeds_max_depth(const struct tb_switch *sw, int depth)
{
	int max_depth;

	if (tb_switch_is_usb4(sw) ||
	    (sw->tb->root_switch && tb_switch_is_usb4(sw->tb->root_switch)))
		max_depth = USB4_SWITCH_MAX_DEPTH;
	else
		max_depth = TB_SWITCH_MAX_DEPTH;

	return depth > max_depth;
}

 
struct tb_switch *tb_switch_alloc(struct tb *tb, struct device *parent,
				  u64 route)
{
	struct tb_switch *sw;
	int upstream_port;
	int i, ret, depth;

	 
	if (route) {
		struct tb_switch *parent_sw = tb_to_switch(parent);
		struct tb_port *down;

		down = tb_port_at(route, parent_sw);
		tb_port_unlock(down);
	}

	depth = tb_route_length(route);

	upstream_port = tb_cfg_get_upstream_port(tb->ctl, route);
	if (upstream_port < 0)
		return ERR_PTR(upstream_port);

	sw = kzalloc(sizeof(*sw), GFP_KERNEL);
	if (!sw)
		return ERR_PTR(-ENOMEM);

	sw->tb = tb;
	ret = tb_cfg_read(tb->ctl, &sw->config, route, 0, TB_CFG_SWITCH, 0, 5);
	if (ret)
		goto err_free_sw_ports;

	sw->generation = tb_switch_get_generation(sw);

	tb_dbg(tb, "current switch config:\n");
	tb_dump_switch(tb, sw);

	 
	sw->config.upstream_port_number = upstream_port;
	sw->config.depth = depth;
	sw->config.route_hi = upper_32_bits(route);
	sw->config.route_lo = lower_32_bits(route);
	sw->config.enabled = 0;

	 
	if (tb_switch_exceeds_max_depth(sw, depth)) {
		ret = -EADDRNOTAVAIL;
		goto err_free_sw_ports;
	}

	 
	sw->ports = kcalloc(sw->config.max_port_number + 1, sizeof(*sw->ports),
				GFP_KERNEL);
	if (!sw->ports) {
		ret = -ENOMEM;
		goto err_free_sw_ports;
	}

	for (i = 0; i <= sw->config.max_port_number; i++) {
		 
		sw->ports[i].sw = sw;
		sw->ports[i].port = i;

		 
		if (i) {
			ida_init(&sw->ports[i].in_hopids);
			ida_init(&sw->ports[i].out_hopids);
		}
	}

	ret = tb_switch_find_vse_cap(sw, TB_VSE_CAP_PLUG_EVENTS);
	if (ret > 0)
		sw->cap_plug_events = ret;

	ret = tb_switch_find_vse_cap(sw, TB_VSE_CAP_TIME2);
	if (ret > 0)
		sw->cap_vsec_tmu = ret;

	ret = tb_switch_find_vse_cap(sw, TB_VSE_CAP_LINK_CONTROLLER);
	if (ret > 0)
		sw->cap_lc = ret;

	ret = tb_switch_find_vse_cap(sw, TB_VSE_CAP_CP_LP);
	if (ret > 0)
		sw->cap_lp = ret;

	 
	if (!route)
		sw->authorized = true;

	device_initialize(&sw->dev);
	sw->dev.parent = parent;
	sw->dev.bus = &tb_bus_type;
	sw->dev.type = &tb_switch_type;
	sw->dev.groups = switch_groups;
	dev_set_name(&sw->dev, "%u-%llx", tb->index, tb_route(sw));

	return sw;

err_free_sw_ports:
	kfree(sw->ports);
	kfree(sw);

	return ERR_PTR(ret);
}

 
struct tb_switch *
tb_switch_alloc_safe_mode(struct tb *tb, struct device *parent, u64 route)
{
	struct tb_switch *sw;

	sw = kzalloc(sizeof(*sw), GFP_KERNEL);
	if (!sw)
		return ERR_PTR(-ENOMEM);

	sw->tb = tb;
	sw->config.depth = tb_route_length(route);
	sw->config.route_hi = upper_32_bits(route);
	sw->config.route_lo = lower_32_bits(route);
	sw->safe_mode = true;

	device_initialize(&sw->dev);
	sw->dev.parent = parent;
	sw->dev.bus = &tb_bus_type;
	sw->dev.type = &tb_switch_type;
	sw->dev.groups = switch_groups;
	dev_set_name(&sw->dev, "%u-%llx", tb->index, tb_route(sw));

	return sw;
}

 
int tb_switch_configure(struct tb_switch *sw)
{
	struct tb *tb = sw->tb;
	u64 route;
	int ret;

	route = tb_route(sw);

	tb_dbg(tb, "%s Switch at %#llx (depth: %d, up port: %d)\n",
	       sw->config.enabled ? "restoring" : "initializing", route,
	       tb_route_length(route), sw->config.upstream_port_number);

	sw->config.enabled = 1;

	if (tb_switch_is_usb4(sw)) {
		 
		if (usb4_switch_version(sw) < 2)
			sw->config.cmuv = ROUTER_CS_4_CMUV_V1;
		else
			sw->config.cmuv = ROUTER_CS_4_CMUV_V2;
		sw->config.plug_events_delay = 0xa;

		 
		ret = tb_sw_write(sw, (u32 *)&sw->config + 1, TB_CFG_SWITCH,
				  ROUTER_CS_1, 4);
		if (ret)
			return ret;

		ret = usb4_switch_setup(sw);
	} else {
		if (sw->config.vendor_id != PCI_VENDOR_ID_INTEL)
			tb_sw_warn(sw, "unknown switch vendor id %#x\n",
				   sw->config.vendor_id);

		if (!sw->cap_plug_events) {
			tb_sw_warn(sw, "cannot find TB_VSE_CAP_PLUG_EVENTS aborting\n");
			return -ENODEV;
		}

		 
		ret = tb_sw_write(sw, (u32 *)&sw->config + 1, TB_CFG_SWITCH,
				  ROUTER_CS_1, 3);
	}
	if (ret)
		return ret;

	return tb_plug_events_active(sw, true);
}

 
int tb_switch_configuration_valid(struct tb_switch *sw)
{
	if (tb_switch_is_usb4(sw))
		return usb4_switch_configuration_valid(sw);
	return 0;
}

static int tb_switch_set_uuid(struct tb_switch *sw)
{
	bool uid = false;
	u32 uuid[4];
	int ret;

	if (sw->uuid)
		return 0;

	if (tb_switch_is_usb4(sw)) {
		ret = usb4_switch_read_uid(sw, &sw->uid);
		if (ret)
			return ret;
		uid = true;
	} else {
		 
		ret = tb_lc_read_uuid(sw, uuid);
		if (ret) {
			if (ret != -EINVAL)
				return ret;
			uid = true;
		}
	}

	if (uid) {
		 
		uuid[0] = sw->uid & 0xffffffff;
		uuid[1] = (sw->uid >> 32) & 0xffffffff;
		uuid[2] = 0xffffffff;
		uuid[3] = 0xffffffff;
	}

	sw->uuid = kmemdup(uuid, sizeof(uuid), GFP_KERNEL);
	if (!sw->uuid)
		return -ENOMEM;
	return 0;
}

static int tb_switch_add_dma_port(struct tb_switch *sw)
{
	u32 status;
	int ret;

	switch (sw->generation) {
	case 2:
		 
		if (tb_route(sw))
			return 0;

		fallthrough;
	case 3:
	case 4:
		ret = tb_switch_set_uuid(sw);
		if (ret)
			return ret;
		break;

	default:
		 
		if (!sw->safe_mode)
			return 0;
		break;
	}

	if (sw->no_nvm_upgrade)
		return 0;

	if (tb_switch_is_usb4(sw)) {
		ret = usb4_switch_nvm_authenticate_status(sw, &status);
		if (ret)
			return ret;

		if (status) {
			tb_sw_info(sw, "switch flash authentication failed\n");
			nvm_set_auth_status(sw, status);
		}

		return 0;
	}

	 
	if (!tb_route(sw) && !tb_switch_is_icm(sw))
		return 0;

	sw->dma_port = dma_port_alloc(sw);
	if (!sw->dma_port)
		return 0;

	 
	nvm_get_auth_status(sw, &status);
	if (status) {
		if (!tb_route(sw))
			nvm_authenticate_complete_dma_port(sw);
		return 0;
	}

	 
	ret = dma_port_flash_update_auth_status(sw->dma_port, &status);
	if (ret <= 0)
		return ret;

	 
	if (!tb_route(sw))
		nvm_authenticate_complete_dma_port(sw);

	if (status) {
		tb_sw_info(sw, "switch flash authentication failed\n");
		nvm_set_auth_status(sw, status);
	}

	tb_sw_info(sw, "power cycling the switch now\n");
	dma_port_power_cycle(sw->dma_port);

	 
	return -ESHUTDOWN;
}

static void tb_switch_default_link_ports(struct tb_switch *sw)
{
	int i;

	for (i = 1; i <= sw->config.max_port_number; i++) {
		struct tb_port *port = &sw->ports[i];
		struct tb_port *subordinate;

		if (!tb_port_is_null(port))
			continue;

		 
		if (i == sw->config.max_port_number ||
		    !tb_port_is_null(&sw->ports[i + 1]))
			continue;

		 
		subordinate = &sw->ports[i + 1];
		if (!port->dual_link_port && !subordinate->dual_link_port) {
			port->link_nr = 0;
			port->dual_link_port = subordinate;
			subordinate->link_nr = 1;
			subordinate->dual_link_port = port;

			tb_sw_dbg(sw, "linked ports %d <-> %d\n",
				  port->port, subordinate->port);
		}
	}
}

static bool tb_switch_lane_bonding_possible(struct tb_switch *sw)
{
	const struct tb_port *up = tb_upstream_port(sw);

	if (!up->dual_link_port || !up->dual_link_port->remote)
		return false;

	if (tb_switch_is_usb4(sw))
		return usb4_switch_lane_bonding_possible(sw);
	return tb_lc_lane_bonding_possible(sw);
}

static int tb_switch_update_link_attributes(struct tb_switch *sw)
{
	struct tb_port *up;
	bool change = false;
	int ret;

	if (!tb_route(sw) || tb_switch_is_icm(sw))
		return 0;

	up = tb_upstream_port(sw);

	ret = tb_port_get_link_speed(up);
	if (ret < 0)
		return ret;
	if (sw->link_speed != ret)
		change = true;
	sw->link_speed = ret;

	ret = tb_port_get_link_width(up);
	if (ret < 0)
		return ret;
	if (sw->link_width != ret)
		change = true;
	sw->link_width = ret;

	 
	if (device_is_registered(&sw->dev) && change)
		kobject_uevent(&sw->dev.kobj, KOBJ_CHANGE);

	return 0;
}

 
int tb_switch_lane_bonding_enable(struct tb_switch *sw)
{
	struct tb_port *up, *down;
	u64 route = tb_route(sw);
	unsigned int width_mask;
	int ret;

	if (!route)
		return 0;

	if (!tb_switch_lane_bonding_possible(sw))
		return 0;

	up = tb_upstream_port(sw);
	down = tb_switch_downstream_port(sw);

	if (!tb_port_is_width_supported(up, TB_LINK_WIDTH_DUAL) ||
	    !tb_port_is_width_supported(down, TB_LINK_WIDTH_DUAL))
		return 0;

	 
	if (tb_wait_for_port(down->dual_link_port, false) <= 0)
		return -ENOTCONN;

	ret = tb_port_lane_bonding_enable(up);
	if (ret) {
		tb_port_warn(up, "failed to enable lane bonding\n");
		return ret;
	}

	ret = tb_port_lane_bonding_enable(down);
	if (ret) {
		tb_port_warn(down, "failed to enable lane bonding\n");
		tb_port_lane_bonding_disable(up);
		return ret;
	}

	 
	width_mask = TB_LINK_WIDTH_DUAL | TB_LINK_WIDTH_ASYM_TX |
		     TB_LINK_WIDTH_ASYM_RX;

	ret = tb_port_wait_for_link_width(down, width_mask, 100);
	if (ret) {
		tb_port_warn(down, "timeout enabling lane bonding\n");
		return ret;
	}

	tb_port_update_credits(down);
	tb_port_update_credits(up);
	tb_switch_update_link_attributes(sw);

	tb_sw_dbg(sw, "lane bonding enabled\n");
	return ret;
}

 
void tb_switch_lane_bonding_disable(struct tb_switch *sw)
{
	struct tb_port *up, *down;
	int ret;

	if (!tb_route(sw))
		return;

	up = tb_upstream_port(sw);
	if (!up->bonded)
		return;

	down = tb_switch_downstream_port(sw);

	tb_port_lane_bonding_disable(up);
	tb_port_lane_bonding_disable(down);

	 
	ret = tb_port_wait_for_link_width(down, TB_LINK_WIDTH_SINGLE, 100);
	if (ret == -ETIMEDOUT)
		tb_sw_warn(sw, "timeout disabling lane bonding\n");

	tb_port_update_credits(down);
	tb_port_update_credits(up);
	tb_switch_update_link_attributes(sw);

	tb_sw_dbg(sw, "lane bonding disabled\n");
}

 
int tb_switch_configure_link(struct tb_switch *sw)
{
	struct tb_port *up, *down;
	int ret;

	if (!tb_route(sw) || tb_switch_is_icm(sw))
		return 0;

	up = tb_upstream_port(sw);
	if (tb_switch_is_usb4(up->sw))
		ret = usb4_port_configure(up);
	else
		ret = tb_lc_configure_port(up);
	if (ret)
		return ret;

	down = up->remote;
	if (tb_switch_is_usb4(down->sw))
		return usb4_port_configure(down);
	return tb_lc_configure_port(down);
}

 
void tb_switch_unconfigure_link(struct tb_switch *sw)
{
	struct tb_port *up, *down;

	if (sw->is_unplugged)
		return;
	if (!tb_route(sw) || tb_switch_is_icm(sw))
		return;

	up = tb_upstream_port(sw);
	if (tb_switch_is_usb4(up->sw))
		usb4_port_unconfigure(up);
	else
		tb_lc_unconfigure_port(up);

	down = up->remote;
	if (tb_switch_is_usb4(down->sw))
		usb4_port_unconfigure(down);
	else
		tb_lc_unconfigure_port(down);
}

static void tb_switch_credits_init(struct tb_switch *sw)
{
	if (tb_switch_is_icm(sw))
		return;
	if (!tb_switch_is_usb4(sw))
		return;
	if (usb4_switch_credits_init(sw))
		tb_sw_info(sw, "failed to determine preferred buffer allocation, using defaults\n");
}

static int tb_switch_port_hotplug_enable(struct tb_switch *sw)
{
	struct tb_port *port;

	if (tb_switch_is_icm(sw))
		return 0;

	tb_switch_for_each_port(sw, port) {
		int res;

		if (!port->cap_usb4)
			continue;

		res = usb4_port_hotplug_enable(port);
		if (res)
			return res;
	}
	return 0;
}

 
int tb_switch_add(struct tb_switch *sw)
{
	int i, ret;

	 
	ret = tb_switch_add_dma_port(sw);
	if (ret) {
		dev_err(&sw->dev, "failed to add DMA port\n");
		return ret;
	}

	if (!sw->safe_mode) {
		tb_switch_credits_init(sw);

		 
		ret = tb_drom_read(sw);
		if (ret)
			dev_warn(&sw->dev, "reading DROM failed: %d\n", ret);
		tb_sw_dbg(sw, "uid: %#llx\n", sw->uid);

		ret = tb_switch_set_uuid(sw);
		if (ret) {
			dev_err(&sw->dev, "failed to set UUID\n");
			return ret;
		}

		for (i = 0; i <= sw->config.max_port_number; i++) {
			if (sw->ports[i].disabled) {
				tb_port_dbg(&sw->ports[i], "disabled by eeprom\n");
				continue;
			}
			ret = tb_init_port(&sw->ports[i]);
			if (ret) {
				dev_err(&sw->dev, "failed to initialize port %d\n", i);
				return ret;
			}
		}

		tb_check_quirks(sw);

		tb_switch_default_link_ports(sw);

		ret = tb_switch_update_link_attributes(sw);
		if (ret)
			return ret;

		ret = tb_switch_clx_init(sw);
		if (ret)
			return ret;

		ret = tb_switch_tmu_init(sw);
		if (ret)
			return ret;
	}

	ret = tb_switch_port_hotplug_enable(sw);
	if (ret)
		return ret;

	ret = device_add(&sw->dev);
	if (ret) {
		dev_err(&sw->dev, "failed to add device: %d\n", ret);
		return ret;
	}

	if (tb_route(sw)) {
		dev_info(&sw->dev, "new device found, vendor=%#x device=%#x\n",
			 sw->vendor, sw->device);
		if (sw->vendor_name && sw->device_name)
			dev_info(&sw->dev, "%s %s\n", sw->vendor_name,
				 sw->device_name);
	}

	ret = usb4_switch_add_ports(sw);
	if (ret) {
		dev_err(&sw->dev, "failed to add USB4 ports\n");
		goto err_del;
	}

	ret = tb_switch_nvm_add(sw);
	if (ret) {
		dev_err(&sw->dev, "failed to add NVM devices\n");
		goto err_ports;
	}

	 
	device_init_wakeup(&sw->dev, true);

	pm_runtime_set_active(&sw->dev);
	if (sw->rpm) {
		pm_runtime_set_autosuspend_delay(&sw->dev, TB_AUTOSUSPEND_DELAY);
		pm_runtime_use_autosuspend(&sw->dev);
		pm_runtime_mark_last_busy(&sw->dev);
		pm_runtime_enable(&sw->dev);
		pm_request_autosuspend(&sw->dev);
	}

	tb_switch_debugfs_init(sw);
	return 0;

err_ports:
	usb4_switch_remove_ports(sw);
err_del:
	device_del(&sw->dev);

	return ret;
}

 
void tb_switch_remove(struct tb_switch *sw)
{
	struct tb_port *port;

	tb_switch_debugfs_remove(sw);

	if (sw->rpm) {
		pm_runtime_get_sync(&sw->dev);
		pm_runtime_disable(&sw->dev);
	}

	 
	tb_switch_for_each_port(sw, port) {
		if (tb_port_has_remote(port)) {
			tb_switch_remove(port->remote->sw);
			port->remote = NULL;
		} else if (port->xdomain) {
			tb_xdomain_remove(port->xdomain);
			port->xdomain = NULL;
		}

		 
		tb_retimer_remove_all(port);
	}

	if (!sw->is_unplugged)
		tb_plug_events_active(sw, false);

	tb_switch_nvm_remove(sw);
	usb4_switch_remove_ports(sw);

	if (tb_route(sw))
		dev_info(&sw->dev, "device disconnected\n");
	device_unregister(&sw->dev);
}

 
void tb_sw_set_unplugged(struct tb_switch *sw)
{
	struct tb_port *port;

	if (sw == sw->tb->root_switch) {
		tb_sw_WARN(sw, "cannot unplug root switch\n");
		return;
	}
	if (sw->is_unplugged) {
		tb_sw_WARN(sw, "is_unplugged already set\n");
		return;
	}
	sw->is_unplugged = true;
	tb_switch_for_each_port(sw, port) {
		if (tb_port_has_remote(port))
			tb_sw_set_unplugged(port->remote->sw);
		else if (port->xdomain)
			port->xdomain->is_unplugged = true;
	}
}

static int tb_switch_set_wake(struct tb_switch *sw, unsigned int flags)
{
	if (flags)
		tb_sw_dbg(sw, "enabling wakeup: %#x\n", flags);
	else
		tb_sw_dbg(sw, "disabling wakeup\n");

	if (tb_switch_is_usb4(sw))
		return usb4_switch_set_wake(sw, flags);
	return tb_lc_set_wake(sw, flags);
}

int tb_switch_resume(struct tb_switch *sw)
{
	struct tb_port *port;
	int err;

	tb_sw_dbg(sw, "resuming switch\n");

	 
	if (tb_route(sw)) {
		u64 uid;

		 
		err = tb_cfg_get_upstream_port(sw->tb->ctl, tb_route(sw));
		if (err < 0) {
			tb_sw_info(sw, "switch not present anymore\n");
			return err;
		}

		 
		if (!sw->uid)
			return -ENODEV;

		if (tb_switch_is_usb4(sw))
			err = usb4_switch_read_uid(sw, &uid);
		else
			err = tb_drom_read_uid_only(sw, &uid);
		if (err) {
			tb_sw_warn(sw, "uid read failed\n");
			return err;
		}
		if (sw->uid != uid) {
			tb_sw_info(sw,
				"changed while suspended (uid %#llx -> %#llx)\n",
				sw->uid, uid);
			return -ENODEV;
		}
	}

	err = tb_switch_configure(sw);
	if (err)
		return err;

	 
	tb_switch_set_wake(sw, 0);

	err = tb_switch_tmu_init(sw);
	if (err)
		return err;

	 
	tb_switch_for_each_port(sw, port) {
		if (!tb_port_is_null(port))
			continue;

		if (!tb_port_resume(port))
			continue;

		if (tb_wait_for_port(port, true) <= 0) {
			tb_port_warn(port,
				     "lost during suspend, disconnecting\n");
			if (tb_port_has_remote(port))
				tb_sw_set_unplugged(port->remote->sw);
			else if (port->xdomain)
				port->xdomain->is_unplugged = true;
		} else {
			 
			if (tb_port_unlock(port))
				tb_port_warn(port, "failed to unlock port\n");
			if (port->remote && tb_switch_resume(port->remote->sw)) {
				tb_port_warn(port,
					     "lost during suspend, disconnecting\n");
				tb_sw_set_unplugged(port->remote->sw);
			}
		}
	}
	return 0;
}

 
void tb_switch_suspend(struct tb_switch *sw, bool runtime)
{
	unsigned int flags = 0;
	struct tb_port *port;
	int err;

	tb_sw_dbg(sw, "suspending switch\n");

	 
	tb_switch_clx_disable(sw);

	err = tb_plug_events_active(sw, false);
	if (err)
		return;

	tb_switch_for_each_port(sw, port) {
		if (tb_port_has_remote(port))
			tb_switch_suspend(port->remote->sw, runtime);
	}

	if (runtime) {
		 
		flags |= TB_WAKE_ON_CONNECT | TB_WAKE_ON_DISCONNECT;
		flags |= TB_WAKE_ON_USB4;
		flags |= TB_WAKE_ON_USB3 | TB_WAKE_ON_PCIE | TB_WAKE_ON_DP;
	} else if (device_may_wakeup(&sw->dev)) {
		flags |= TB_WAKE_ON_USB4 | TB_WAKE_ON_USB3 | TB_WAKE_ON_PCIE;
	}

	tb_switch_set_wake(sw, flags);

	if (tb_switch_is_usb4(sw))
		usb4_switch_set_sleep(sw);
	else
		tb_lc_set_sleep(sw);
}

 
bool tb_switch_query_dp_resource(struct tb_switch *sw, struct tb_port *in)
{
	if (tb_switch_is_usb4(sw))
		return usb4_switch_query_dp_resource(sw, in);
	return tb_lc_dp_sink_query(sw, in);
}

 
int tb_switch_alloc_dp_resource(struct tb_switch *sw, struct tb_port *in)
{
	int ret;

	if (tb_switch_is_usb4(sw))
		ret = usb4_switch_alloc_dp_resource(sw, in);
	else
		ret = tb_lc_dp_sink_alloc(sw, in);

	if (ret)
		tb_sw_warn(sw, "failed to allocate DP resource for port %d\n",
			   in->port);
	else
		tb_sw_dbg(sw, "allocated DP resource for port %d\n", in->port);

	return ret;
}

 
void tb_switch_dealloc_dp_resource(struct tb_switch *sw, struct tb_port *in)
{
	int ret;

	if (tb_switch_is_usb4(sw))
		ret = usb4_switch_dealloc_dp_resource(sw, in);
	else
		ret = tb_lc_dp_sink_dealloc(sw, in);

	if (ret)
		tb_sw_warn(sw, "failed to de-allocate DP resource for port %d\n",
			   in->port);
	else
		tb_sw_dbg(sw, "released DP resource for port %d\n", in->port);
}

struct tb_sw_lookup {
	struct tb *tb;
	u8 link;
	u8 depth;
	const uuid_t *uuid;
	u64 route;
};

static int tb_switch_match(struct device *dev, const void *data)
{
	struct tb_switch *sw = tb_to_switch(dev);
	const struct tb_sw_lookup *lookup = data;

	if (!sw)
		return 0;
	if (sw->tb != lookup->tb)
		return 0;

	if (lookup->uuid)
		return !memcmp(sw->uuid, lookup->uuid, sizeof(*lookup->uuid));

	if (lookup->route) {
		return sw->config.route_lo == lower_32_bits(lookup->route) &&
		       sw->config.route_hi == upper_32_bits(lookup->route);
	}

	 
	if (!lookup->depth)
		return !sw->depth;

	return sw->link == lookup->link && sw->depth == lookup->depth;
}

 
struct tb_switch *tb_switch_find_by_link_depth(struct tb *tb, u8 link, u8 depth)
{
	struct tb_sw_lookup lookup;
	struct device *dev;

	memset(&lookup, 0, sizeof(lookup));
	lookup.tb = tb;
	lookup.link = link;
	lookup.depth = depth;

	dev = bus_find_device(&tb_bus_type, NULL, &lookup, tb_switch_match);
	if (dev)
		return tb_to_switch(dev);

	return NULL;
}

 
struct tb_switch *tb_switch_find_by_uuid(struct tb *tb, const uuid_t *uuid)
{
	struct tb_sw_lookup lookup;
	struct device *dev;

	memset(&lookup, 0, sizeof(lookup));
	lookup.tb = tb;
	lookup.uuid = uuid;

	dev = bus_find_device(&tb_bus_type, NULL, &lookup, tb_switch_match);
	if (dev)
		return tb_to_switch(dev);

	return NULL;
}

 
struct tb_switch *tb_switch_find_by_route(struct tb *tb, u64 route)
{
	struct tb_sw_lookup lookup;
	struct device *dev;

	if (!route)
		return tb_switch_get(tb->root_switch);

	memset(&lookup, 0, sizeof(lookup));
	lookup.tb = tb;
	lookup.route = route;

	dev = bus_find_device(&tb_bus_type, NULL, &lookup, tb_switch_match);
	if (dev)
		return tb_to_switch(dev);

	return NULL;
}

 
struct tb_port *tb_switch_find_port(struct tb_switch *sw,
				    enum tb_port_type type)
{
	struct tb_port *port;

	tb_switch_for_each_port(sw, port) {
		if (port->config.type == type)
			return port;
	}

	return NULL;
}

 
static int tb_switch_pcie_bridge_write(struct tb_switch *sw, unsigned int bridge,
				       unsigned int pcie_offset, u32 value)
{
	u32 offset, command, val;
	int ret;

	if (sw->generation != 3)
		return -EOPNOTSUPP;

	offset = sw->cap_plug_events + TB_PLUG_EVENTS_PCIE_WR_DATA;
	ret = tb_sw_write(sw, &value, TB_CFG_SWITCH, offset, 1);
	if (ret)
		return ret;

	command = pcie_offset & TB_PLUG_EVENTS_PCIE_CMD_DW_OFFSET_MASK;
	command |= BIT(bridge + TB_PLUG_EVENTS_PCIE_CMD_BR_SHIFT);
	command |= TB_PLUG_EVENTS_PCIE_CMD_RD_WR_MASK;
	command |= TB_PLUG_EVENTS_PCIE_CMD_COMMAND_VAL
			<< TB_PLUG_EVENTS_PCIE_CMD_COMMAND_SHIFT;
	command |= TB_PLUG_EVENTS_PCIE_CMD_REQ_ACK_MASK;

	offset = sw->cap_plug_events + TB_PLUG_EVENTS_PCIE_CMD;

	ret = tb_sw_write(sw, &command, TB_CFG_SWITCH, offset, 1);
	if (ret)
		return ret;

	ret = tb_switch_wait_for_bit(sw, offset,
				     TB_PLUG_EVENTS_PCIE_CMD_REQ_ACK_MASK, 0, 100);
	if (ret)
		return ret;

	ret = tb_sw_read(sw, &val, TB_CFG_SWITCH, offset, 1);
	if (ret)
		return ret;

	if (val & TB_PLUG_EVENTS_PCIE_CMD_TIMEOUT_MASK)
		return -ETIMEDOUT;

	return 0;
}

 
int tb_switch_pcie_l1_enable(struct tb_switch *sw)
{
	struct tb_switch *parent = tb_switch_parent(sw);
	int ret;

	if (!tb_route(sw))
		return 0;

	if (!tb_switch_is_titan_ridge(sw))
		return 0;

	 
	if (tb_route(parent))
		return 0;

	 
	ret = tb_switch_pcie_bridge_write(sw, 5, 0x143, 0x0c7806b1);
	if (ret)
		return ret;

	 
	return tb_switch_pcie_bridge_write(sw, 0, 0x143, 0x0c5806b1);
}

 
int tb_switch_xhci_connect(struct tb_switch *sw)
{
	struct tb_port *port1, *port3;
	int ret;

	if (sw->generation != 3)
		return 0;

	port1 = &sw->ports[1];
	port3 = &sw->ports[3];

	if (tb_switch_is_alpine_ridge(sw)) {
		bool usb_port1, usb_port3, xhci_port1, xhci_port3;

		usb_port1 = tb_lc_is_usb_plugged(port1);
		usb_port3 = tb_lc_is_usb_plugged(port3);
		xhci_port1 = tb_lc_is_xhci_connected(port1);
		xhci_port3 = tb_lc_is_xhci_connected(port3);

		 
		if (usb_port1 && !xhci_port1) {
			ret = tb_lc_xhci_connect(port1);
			if (ret)
				return ret;
		}
		if (usb_port3 && !xhci_port3)
			return tb_lc_xhci_connect(port3);
	} else if (tb_switch_is_titan_ridge(sw)) {
		ret = tb_lc_xhci_connect(port1);
		if (ret)
			return ret;
		return tb_lc_xhci_connect(port3);
	}

	return 0;
}

 
void tb_switch_xhci_disconnect(struct tb_switch *sw)
{
	if (sw->generation == 3) {
		struct tb_port *port1 = &sw->ports[1];
		struct tb_port *port3 = &sw->ports[3];

		tb_lc_xhci_disconnect(port1);
		tb_port_dbg(port1, "disconnected xHCI\n");
		tb_lc_xhci_disconnect(port3);
		tb_port_dbg(port3, "disconnected xHCI\n");
	}
}
