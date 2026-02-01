


 

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/slab.h>
#include <linux/soundwire/sdw_registers.h>
#include <linux/soundwire/sdw.h>
#include <linux/soundwire/sdw_type.h>
#include <sound/soc.h>
#include "bus.h"

 
int sdw_rows[SDW_FRAME_ROWS] = {48, 50, 60, 64, 75, 80, 125, 147,
			96, 100, 120, 128, 150, 160, 250, 0,
			192, 200, 240, 256, 72, 144, 90, 180};
EXPORT_SYMBOL(sdw_rows);

int sdw_cols[SDW_FRAME_COLS] = {2, 4, 6, 8, 10, 12, 14, 16};
EXPORT_SYMBOL(sdw_cols);

int sdw_find_col_index(int col)
{
	int i;

	for (i = 0; i < SDW_FRAME_COLS; i++) {
		if (sdw_cols[i] == col)
			return i;
	}

	pr_warn("Requested column not found, selecting lowest column no: 2\n");
	return 0;
}
EXPORT_SYMBOL(sdw_find_col_index);

int sdw_find_row_index(int row)
{
	int i;

	for (i = 0; i < SDW_FRAME_ROWS; i++) {
		if (sdw_rows[i] == row)
			return i;
	}

	pr_warn("Requested row not found, selecting lowest row no: 48\n");
	return 0;
}
EXPORT_SYMBOL(sdw_find_row_index);

static int _sdw_program_slave_port_params(struct sdw_bus *bus,
					  struct sdw_slave *slave,
					  struct sdw_transport_params *t_params,
					  enum sdw_dpn_type type)
{
	u32 addr1, addr2, addr3, addr4;
	int ret;
	u16 wbuf;

	if (bus->params.next_bank) {
		addr1 = SDW_DPN_OFFSETCTRL2_B1(t_params->port_num);
		addr2 = SDW_DPN_BLOCKCTRL3_B1(t_params->port_num);
		addr3 = SDW_DPN_SAMPLECTRL2_B1(t_params->port_num);
		addr4 = SDW_DPN_HCTRL_B1(t_params->port_num);
	} else {
		addr1 = SDW_DPN_OFFSETCTRL2_B0(t_params->port_num);
		addr2 = SDW_DPN_BLOCKCTRL3_B0(t_params->port_num);
		addr3 = SDW_DPN_SAMPLECTRL2_B0(t_params->port_num);
		addr4 = SDW_DPN_HCTRL_B0(t_params->port_num);
	}

	 
	ret = sdw_write_no_pm(slave, addr1, t_params->offset2);
	if (ret < 0) {
		dev_err(bus->dev, "DPN_OffsetCtrl2 register write failed\n");
		return ret;
	}

	 
	ret = sdw_write_no_pm(slave, addr2, t_params->blk_pkg_mode);
	if (ret < 0) {
		dev_err(bus->dev, "DPN_BlockCtrl3 register write failed\n");
		return ret;
	}

	 
	if (type != SDW_DPN_FULL)
		return ret;

	 
	wbuf = FIELD_GET(SDW_DPN_SAMPLECTRL_HIGH, t_params->sample_interval - 1);

	ret = sdw_write_no_pm(slave, addr3, wbuf);
	if (ret < 0) {
		dev_err(bus->dev, "DPN_SampleCtrl2 register write failed\n");
		return ret;
	}

	 
	wbuf = FIELD_PREP(SDW_DPN_HCTRL_HSTART, t_params->hstart);
	wbuf |= FIELD_PREP(SDW_DPN_HCTRL_HSTOP, t_params->hstop);

	ret = sdw_write_no_pm(slave, addr4, wbuf);
	if (ret < 0)
		dev_err(bus->dev, "DPN_HCtrl register write failed\n");

	return ret;
}

static int sdw_program_slave_port_params(struct sdw_bus *bus,
					 struct sdw_slave_runtime *s_rt,
					 struct sdw_port_runtime *p_rt)
{
	struct sdw_transport_params *t_params = &p_rt->transport_params;
	struct sdw_port_params *p_params = &p_rt->port_params;
	struct sdw_slave_prop *slave_prop = &s_rt->slave->prop;
	u32 addr1, addr2, addr3, addr4, addr5, addr6;
	struct sdw_dpn_prop *dpn_prop;
	int ret;
	u8 wbuf;

	if (s_rt->slave->is_mockup_device)
		return 0;

	dpn_prop = sdw_get_slave_dpn_prop(s_rt->slave,
					  s_rt->direction,
					  t_params->port_num);
	if (!dpn_prop)
		return -EINVAL;

	addr1 = SDW_DPN_PORTCTRL(t_params->port_num);
	addr2 = SDW_DPN_BLOCKCTRL1(t_params->port_num);

	if (bus->params.next_bank) {
		addr3 = SDW_DPN_SAMPLECTRL1_B1(t_params->port_num);
		addr4 = SDW_DPN_OFFSETCTRL1_B1(t_params->port_num);
		addr5 = SDW_DPN_BLOCKCTRL2_B1(t_params->port_num);
		addr6 = SDW_DPN_LANECTRL_B1(t_params->port_num);

	} else {
		addr3 = SDW_DPN_SAMPLECTRL1_B0(t_params->port_num);
		addr4 = SDW_DPN_OFFSETCTRL1_B0(t_params->port_num);
		addr5 = SDW_DPN_BLOCKCTRL2_B0(t_params->port_num);
		addr6 = SDW_DPN_LANECTRL_B0(t_params->port_num);
	}

	 
	wbuf = FIELD_PREP(SDW_DPN_PORTCTRL_DATAMODE, p_params->data_mode);
	wbuf |= FIELD_PREP(SDW_DPN_PORTCTRL_FLOWMODE, p_params->flow_mode);

	ret = sdw_update_no_pm(s_rt->slave, addr1, 0xF, wbuf);
	if (ret < 0) {
		dev_err(&s_rt->slave->dev,
			"DPN_PortCtrl register write failed for port %d\n",
			t_params->port_num);
		return ret;
	}

	if (!dpn_prop->read_only_wordlength) {
		 
		ret = sdw_write_no_pm(s_rt->slave, addr2, (p_params->bps - 1));
		if (ret < 0) {
			dev_err(&s_rt->slave->dev,
				"DPN_BlockCtrl1 register write failed for port %d\n",
				t_params->port_num);
			return ret;
		}
	}

	 
	wbuf = (t_params->sample_interval - 1) & SDW_DPN_SAMPLECTRL_LOW;
	ret = sdw_write_no_pm(s_rt->slave, addr3, wbuf);
	if (ret < 0) {
		dev_err(&s_rt->slave->dev,
			"DPN_SampleCtrl1 register write failed for port %d\n",
			t_params->port_num);
		return ret;
	}

	 
	ret = sdw_write_no_pm(s_rt->slave, addr4, t_params->offset1);
	if (ret < 0) {
		dev_err(&s_rt->slave->dev,
			"DPN_OffsetCtrl1 register write failed for port %d\n",
			t_params->port_num);
		return ret;
	}

	 
	if (t_params->blk_grp_ctrl_valid) {
		ret = sdw_write_no_pm(s_rt->slave, addr5, t_params->blk_grp_ctrl);
		if (ret < 0) {
			dev_err(&s_rt->slave->dev,
				"DPN_BlockCtrl2 reg write failed for port %d\n",
				t_params->port_num);
			return ret;
		}
	}

	 
	if (slave_prop->lane_control_support) {
		ret = sdw_write_no_pm(s_rt->slave, addr6, t_params->lane_ctrl);
		if (ret < 0) {
			dev_err(&s_rt->slave->dev,
				"DPN_LaneCtrl register write failed for port %d\n",
				t_params->port_num);
			return ret;
		}
	}

	if (dpn_prop->type != SDW_DPN_SIMPLE) {
		ret = _sdw_program_slave_port_params(bus, s_rt->slave,
						     t_params, dpn_prop->type);
		if (ret < 0)
			dev_err(&s_rt->slave->dev,
				"Transport reg write failed for port: %d\n",
				t_params->port_num);
	}

	return ret;
}

static int sdw_program_master_port_params(struct sdw_bus *bus,
					  struct sdw_port_runtime *p_rt)
{
	int ret;

	 
	ret = bus->port_ops->dpn_set_port_transport_params(bus,
					&p_rt->transport_params,
					bus->params.next_bank);
	if (ret < 0)
		return ret;

	return bus->port_ops->dpn_set_port_params(bus,
						  &p_rt->port_params,
						  bus->params.next_bank);
}

 
static int sdw_program_port_params(struct sdw_master_runtime *m_rt)
{
	struct sdw_slave_runtime *s_rt;
	struct sdw_bus *bus = m_rt->bus;
	struct sdw_port_runtime *p_rt;
	int ret = 0;

	 
	list_for_each_entry(s_rt, &m_rt->slave_rt_list, m_rt_node) {
		list_for_each_entry(p_rt, &s_rt->port_list, port_node) {
			ret = sdw_program_slave_port_params(bus, s_rt, p_rt);
			if (ret < 0)
				return ret;
		}
	}

	 
	list_for_each_entry(p_rt, &m_rt->port_list, port_node) {
		ret = sdw_program_master_port_params(bus, p_rt);
		if (ret < 0)
			return ret;
	}

	return 0;
}

 
static int sdw_enable_disable_slave_ports(struct sdw_bus *bus,
					  struct sdw_slave_runtime *s_rt,
					  struct sdw_port_runtime *p_rt,
					  bool en)
{
	struct sdw_transport_params *t_params = &p_rt->transport_params;
	u32 addr;
	int ret;

	if (bus->params.next_bank)
		addr = SDW_DPN_CHANNELEN_B1(p_rt->num);
	else
		addr = SDW_DPN_CHANNELEN_B0(p_rt->num);

	 
	if (en)
		ret = sdw_write_no_pm(s_rt->slave, addr, p_rt->ch_mask);
	else
		ret = sdw_write_no_pm(s_rt->slave, addr, 0x0);

	if (ret < 0)
		dev_err(&s_rt->slave->dev,
			"Slave chn_en reg write failed:%d port:%d\n",
			ret, t_params->port_num);

	return ret;
}

static int sdw_enable_disable_master_ports(struct sdw_master_runtime *m_rt,
					   struct sdw_port_runtime *p_rt,
					   bool en)
{
	struct sdw_transport_params *t_params = &p_rt->transport_params;
	struct sdw_bus *bus = m_rt->bus;
	struct sdw_enable_ch enable_ch;
	int ret;

	enable_ch.port_num = p_rt->num;
	enable_ch.ch_mask = p_rt->ch_mask;
	enable_ch.enable = en;

	 
	if (bus->port_ops->dpn_port_enable_ch) {
		ret = bus->port_ops->dpn_port_enable_ch(bus,
							&enable_ch,
							bus->params.next_bank);
		if (ret < 0) {
			dev_err(bus->dev,
				"Master chn_en write failed:%d port:%d\n",
				ret, t_params->port_num);
			return ret;
		}
	} else {
		dev_err(bus->dev,
			"dpn_port_enable_ch not supported, %s failed\n",
			en ? "enable" : "disable");
		return -EINVAL;
	}

	return 0;
}

 
static int sdw_enable_disable_ports(struct sdw_master_runtime *m_rt, bool en)
{
	struct sdw_port_runtime *s_port, *m_port;
	struct sdw_slave_runtime *s_rt;
	int ret = 0;

	 
	list_for_each_entry(s_rt, &m_rt->slave_rt_list, m_rt_node) {
		list_for_each_entry(s_port, &s_rt->port_list, port_node) {
			ret = sdw_enable_disable_slave_ports(m_rt->bus, s_rt,
							     s_port, en);
			if (ret < 0)
				return ret;
		}
	}

	 
	list_for_each_entry(m_port, &m_rt->port_list, port_node) {
		ret = sdw_enable_disable_master_ports(m_rt, m_port, en);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int sdw_do_port_prep(struct sdw_slave_runtime *s_rt,
			    struct sdw_prepare_ch prep_ch,
			    enum sdw_port_prep_ops cmd)
{
	int ret = 0;
	struct sdw_slave *slave = s_rt->slave;

	mutex_lock(&slave->sdw_dev_lock);

	if (slave->probed) {
		struct device *dev = &slave->dev;
		struct sdw_driver *drv = drv_to_sdw_driver(dev->driver);

		if (drv->ops && drv->ops->port_prep) {
			ret = drv->ops->port_prep(slave, &prep_ch, cmd);
			if (ret < 0)
				dev_err(dev, "Slave Port Prep cmd %d failed: %d\n",
					cmd, ret);
		}
	}

	mutex_unlock(&slave->sdw_dev_lock);

	return ret;
}

static int sdw_prep_deprep_slave_ports(struct sdw_bus *bus,
				       struct sdw_slave_runtime *s_rt,
				       struct sdw_port_runtime *p_rt,
				       bool prep)
{
	struct completion *port_ready;
	struct sdw_dpn_prop *dpn_prop;
	struct sdw_prepare_ch prep_ch;
	bool intr = false;
	int ret = 0, val;
	u32 addr;

	prep_ch.num = p_rt->num;
	prep_ch.ch_mask = p_rt->ch_mask;

	dpn_prop = sdw_get_slave_dpn_prop(s_rt->slave,
					  s_rt->direction,
					  prep_ch.num);
	if (!dpn_prop) {
		dev_err(bus->dev,
			"Slave Port:%d properties not found\n", prep_ch.num);
		return -EINVAL;
	}

	prep_ch.prepare = prep;

	prep_ch.bank = bus->params.next_bank;

	if (dpn_prop->imp_def_interrupts || !dpn_prop->simple_ch_prep_sm ||
	    bus->params.s_data_mode != SDW_PORT_DATA_MODE_NORMAL)
		intr = true;

	 
	if (prep && intr) {
		ret = sdw_configure_dpn_intr(s_rt->slave, p_rt->num, prep,
					     dpn_prop->imp_def_interrupts);
		if (ret < 0)
			return ret;
	}

	 
	sdw_do_port_prep(s_rt, prep_ch, prep ? SDW_OPS_PORT_PRE_PREP : SDW_OPS_PORT_PRE_DEPREP);

	 
	if (!dpn_prop->simple_ch_prep_sm) {
		addr = SDW_DPN_PREPARECTRL(p_rt->num);

		if (prep)
			ret = sdw_write_no_pm(s_rt->slave, addr, p_rt->ch_mask);
		else
			ret = sdw_write_no_pm(s_rt->slave, addr, 0x0);

		if (ret < 0) {
			dev_err(&s_rt->slave->dev,
				"Slave prep_ctrl reg write failed\n");
			return ret;
		}

		 
		port_ready = &s_rt->slave->port_ready[prep_ch.num];
		wait_for_completion_timeout(port_ready,
			msecs_to_jiffies(dpn_prop->ch_prep_timeout));

		val = sdw_read_no_pm(s_rt->slave, SDW_DPN_PREPARESTATUS(p_rt->num));
		if ((val < 0) || (val & p_rt->ch_mask)) {
			ret = (val < 0) ? val : -ETIMEDOUT;
			dev_err(&s_rt->slave->dev,
				"Chn prep failed for port %d: %d\n", prep_ch.num, ret);
			return ret;
		}
	}

	 
	sdw_do_port_prep(s_rt, prep_ch, prep ? SDW_OPS_PORT_POST_PREP : SDW_OPS_PORT_POST_DEPREP);

	 
	if (!prep && intr)
		ret = sdw_configure_dpn_intr(s_rt->slave, p_rt->num, prep,
					     dpn_prop->imp_def_interrupts);

	return ret;
}

static int sdw_prep_deprep_master_ports(struct sdw_master_runtime *m_rt,
					struct sdw_port_runtime *p_rt,
					bool prep)
{
	struct sdw_transport_params *t_params = &p_rt->transport_params;
	struct sdw_bus *bus = m_rt->bus;
	const struct sdw_master_port_ops *ops = bus->port_ops;
	struct sdw_prepare_ch prep_ch;
	int ret = 0;

	prep_ch.num = p_rt->num;
	prep_ch.ch_mask = p_rt->ch_mask;
	prep_ch.prepare = prep;  
	prep_ch.bank = bus->params.next_bank;

	 
	if (ops->dpn_port_prep) {
		ret = ops->dpn_port_prep(bus, &prep_ch);
		if (ret < 0) {
			dev_err(bus->dev, "Port prepare failed for port:%d\n",
				t_params->port_num);
			return ret;
		}
	}

	return ret;
}

 
static int sdw_prep_deprep_ports(struct sdw_master_runtime *m_rt, bool prep)
{
	struct sdw_slave_runtime *s_rt;
	struct sdw_port_runtime *p_rt;
	int ret = 0;

	 
	list_for_each_entry(s_rt, &m_rt->slave_rt_list, m_rt_node) {
		list_for_each_entry(p_rt, &s_rt->port_list, port_node) {
			ret = sdw_prep_deprep_slave_ports(m_rt->bus, s_rt,
							  p_rt, prep);
			if (ret < 0)
				return ret;
		}
	}

	 
	list_for_each_entry(p_rt, &m_rt->port_list, port_node) {
		ret = sdw_prep_deprep_master_ports(m_rt, p_rt, prep);
		if (ret < 0)
			return ret;
	}

	return ret;
}

 
static int sdw_notify_config(struct sdw_master_runtime *m_rt)
{
	struct sdw_slave_runtime *s_rt;
	struct sdw_bus *bus = m_rt->bus;
	struct sdw_slave *slave;
	int ret;

	if (bus->ops->set_bus_conf) {
		ret = bus->ops->set_bus_conf(bus, &bus->params);
		if (ret < 0)
			return ret;
	}

	list_for_each_entry(s_rt, &m_rt->slave_rt_list, m_rt_node) {
		slave = s_rt->slave;

		mutex_lock(&slave->sdw_dev_lock);

		if (slave->probed) {
			struct device *dev = &slave->dev;
			struct sdw_driver *drv = drv_to_sdw_driver(dev->driver);

			if (drv->ops && drv->ops->bus_config) {
				ret = drv->ops->bus_config(slave, &bus->params);
				if (ret < 0) {
					dev_err(dev, "Notify Slave: %d failed\n",
						slave->dev_num);
					mutex_unlock(&slave->sdw_dev_lock);
					return ret;
				}
			}
		}

		mutex_unlock(&slave->sdw_dev_lock);
	}

	return 0;
}

 
static int sdw_program_params(struct sdw_bus *bus, bool prepare)
{
	struct sdw_master_runtime *m_rt;
	int ret = 0;

	list_for_each_entry(m_rt, &bus->m_rt_list, bus_node) {

		 
		if (!prepare &&
		    m_rt->stream->state == SDW_STREAM_CONFIGURED)
			continue;

		ret = sdw_program_port_params(m_rt);
		if (ret < 0) {
			dev_err(bus->dev,
				"Program transport params failed: %d\n", ret);
			return ret;
		}

		ret = sdw_notify_config(m_rt);
		if (ret < 0) {
			dev_err(bus->dev,
				"Notify bus config failed: %d\n", ret);
			return ret;
		}

		 
		if (m_rt->stream->state != SDW_STREAM_ENABLED)
			continue;

		ret = sdw_enable_disable_ports(m_rt, true);
		if (ret < 0) {
			dev_err(bus->dev, "Enable channel failed: %d\n", ret);
			return ret;
		}
	}

	return ret;
}

static int sdw_bank_switch(struct sdw_bus *bus, int m_rt_count)
{
	int col_index, row_index;
	bool multi_link;
	struct sdw_msg *wr_msg;
	u8 *wbuf;
	int ret;
	u16 addr;

	wr_msg = kzalloc(sizeof(*wr_msg), GFP_KERNEL);
	if (!wr_msg)
		return -ENOMEM;

	wbuf = kzalloc(sizeof(*wbuf), GFP_KERNEL);
	if (!wbuf) {
		ret = -ENOMEM;
		goto error_1;
	}

	 
	col_index = sdw_find_col_index(bus->params.col);
	row_index = sdw_find_row_index(bus->params.row);
	wbuf[0] = col_index | (row_index << 3);

	if (bus->params.next_bank)
		addr = SDW_SCP_FRAMECTRL_B1;
	else
		addr = SDW_SCP_FRAMECTRL_B0;

	sdw_fill_msg(wr_msg, NULL, addr, 1, SDW_BROADCAST_DEV_NUM,
		     SDW_MSG_FLAG_WRITE, wbuf);
	wr_msg->ssp_sync = true;

	 
	multi_link = bus->multi_link && (m_rt_count >= bus->hw_sync_min_links);

	if (multi_link)
		ret = sdw_transfer_defer(bus, wr_msg);
	else
		ret = sdw_transfer(bus, wr_msg);

	if (ret < 0 && ret != -ENODATA) {
		dev_err(bus->dev, "Slave frame_ctrl reg write failed\n");
		goto error;
	}

	if (!multi_link) {
		kfree(wbuf);
		kfree(wr_msg);
		bus->defer_msg.msg = NULL;
		bus->params.curr_bank = !bus->params.curr_bank;
		bus->params.next_bank = !bus->params.next_bank;
	}

	return 0;

error:
	kfree(wbuf);
error_1:
	kfree(wr_msg);
	bus->defer_msg.msg = NULL;
	return ret;
}

 
static int sdw_ml_sync_bank_switch(struct sdw_bus *bus, bool multi_link)
{
	unsigned long time_left;

	if (!multi_link)
		return 0;

	 
	time_left = wait_for_completion_timeout(&bus->defer_msg.complete,
						bus->bank_switch_timeout);

	if (!time_left) {
		dev_err(bus->dev, "Controller Timed out on bank switch\n");
		return -ETIMEDOUT;
	}

	bus->params.curr_bank = !bus->params.curr_bank;
	bus->params.next_bank = !bus->params.next_bank;

	if (bus->defer_msg.msg) {
		kfree(bus->defer_msg.msg->buf);
		kfree(bus->defer_msg.msg);
		bus->defer_msg.msg = NULL;
	}

	return 0;
}

static int do_bank_switch(struct sdw_stream_runtime *stream)
{
	struct sdw_master_runtime *m_rt;
	const struct sdw_master_ops *ops;
	struct sdw_bus *bus;
	bool multi_link = false;
	int m_rt_count;
	int ret = 0;

	m_rt_count = stream->m_rt_count;

	list_for_each_entry(m_rt, &stream->master_list, stream_node) {
		bus = m_rt->bus;
		ops = bus->ops;

		if (bus->multi_link && m_rt_count >= bus->hw_sync_min_links) {
			multi_link = true;
			mutex_lock(&bus->msg_lock);
		}

		 
		if (ops->pre_bank_switch) {
			ret = ops->pre_bank_switch(bus);
			if (ret < 0) {
				dev_err(bus->dev,
					"Pre bank switch op failed: %d\n", ret);
				goto msg_unlock;
			}
		}

		 
		ret = sdw_bank_switch(bus, m_rt_count);
		if (ret < 0) {
			dev_err(bus->dev, "Bank switch failed: %d\n", ret);
			goto error;
		}
	}

	 
	list_for_each_entry(m_rt, &stream->master_list, stream_node) {
		bus = m_rt->bus;
		ops = bus->ops;

		 
		if (ops->post_bank_switch) {
			ret = ops->post_bank_switch(bus);
			if (ret < 0) {
				dev_err(bus->dev,
					"Post bank switch op failed: %d\n",
					ret);
				goto error;
			}
		} else if (multi_link) {
			dev_err(bus->dev,
				"Post bank switch ops not implemented\n");
			ret = -EINVAL;
			goto error;
		}

		 
		if (!bus->bank_switch_timeout)
			bus->bank_switch_timeout = DEFAULT_BANK_SWITCH_TIMEOUT;

		 
		ret = sdw_ml_sync_bank_switch(bus, multi_link);
		if (ret < 0) {
			dev_err(bus->dev,
				"multi link bank switch failed: %d\n", ret);
			goto error;
		}

		if (multi_link)
			mutex_unlock(&bus->msg_lock);
	}

	return ret;

error:
	list_for_each_entry(m_rt, &stream->master_list, stream_node) {
		bus = m_rt->bus;
		if (bus->defer_msg.msg) {
			kfree(bus->defer_msg.msg->buf);
			kfree(bus->defer_msg.msg);
			bus->defer_msg.msg = NULL;
		}
	}

msg_unlock:

	if (multi_link) {
		list_for_each_entry(m_rt, &stream->master_list, stream_node) {
			bus = m_rt->bus;
			if (mutex_is_locked(&bus->msg_lock))
				mutex_unlock(&bus->msg_lock);
		}
	}

	return ret;
}

static struct sdw_port_runtime *sdw_port_alloc(struct list_head *port_list)
{
	struct sdw_port_runtime *p_rt;

	p_rt = kzalloc(sizeof(*p_rt), GFP_KERNEL);
	if (!p_rt)
		return NULL;

	list_add_tail(&p_rt->port_node, port_list);

	return p_rt;
}

static int sdw_port_config(struct sdw_port_runtime *p_rt,
			   struct sdw_port_config *port_config,
			   int port_index)
{
	p_rt->ch_mask = port_config[port_index].ch_mask;
	p_rt->num = port_config[port_index].num;

	 

	return 0;
}

static void sdw_port_free(struct sdw_port_runtime *p_rt)
{
	list_del(&p_rt->port_node);
	kfree(p_rt);
}

static bool sdw_slave_port_allocated(struct sdw_slave_runtime *s_rt)
{
	return !list_empty(&s_rt->port_list);
}

static void sdw_slave_port_free(struct sdw_slave *slave,
				struct sdw_stream_runtime *stream)
{
	struct sdw_port_runtime *p_rt, *_p_rt;
	struct sdw_master_runtime *m_rt;
	struct sdw_slave_runtime *s_rt;

	list_for_each_entry(m_rt, &stream->master_list, stream_node) {
		list_for_each_entry(s_rt, &m_rt->slave_rt_list, m_rt_node) {
			if (s_rt->slave != slave)
				continue;

			list_for_each_entry_safe(p_rt, _p_rt,
						 &s_rt->port_list, port_node) {
				sdw_port_free(p_rt);
			}
		}
	}
}

static int sdw_slave_port_alloc(struct sdw_slave *slave,
				struct sdw_slave_runtime *s_rt,
				unsigned int num_config)
{
	struct sdw_port_runtime *p_rt;
	int i;

	 
	for (i = 0; i < num_config; i++) {
		p_rt = sdw_port_alloc(&s_rt->port_list);
		if (!p_rt)
			return -ENOMEM;
	}

	return 0;
}

static int sdw_slave_port_is_valid_range(struct device *dev, int num)
{
	if (!SDW_VALID_PORT_RANGE(num)) {
		dev_err(dev, "SoundWire: Invalid port number :%d\n", num);
		return -EINVAL;
	}

	return 0;
}

static int sdw_slave_port_config(struct sdw_slave *slave,
				 struct sdw_slave_runtime *s_rt,
				 struct sdw_port_config *port_config)
{
	struct sdw_port_runtime *p_rt;
	int ret;
	int i;

	i = 0;
	list_for_each_entry(p_rt, &s_rt->port_list, port_node) {
		 
		ret = sdw_slave_port_is_valid_range(&slave->dev, port_config[i].num);
		if (ret < 0)
			return ret;

		ret = sdw_port_config(p_rt, port_config, i);
		if (ret < 0)
			return ret;
		i++;
	}

	return 0;
}

static bool sdw_master_port_allocated(struct sdw_master_runtime *m_rt)
{
	return !list_empty(&m_rt->port_list);
}

static void sdw_master_port_free(struct sdw_master_runtime *m_rt)
{
	struct sdw_port_runtime *p_rt, *_p_rt;

	list_for_each_entry_safe(p_rt, _p_rt, &m_rt->port_list, port_node) {
		sdw_port_free(p_rt);
	}
}

static int sdw_master_port_alloc(struct sdw_master_runtime *m_rt,
				 unsigned int num_ports)
{
	struct sdw_port_runtime *p_rt;
	int i;

	 
	for (i = 0; i < num_ports; i++) {
		p_rt = sdw_port_alloc(&m_rt->port_list);
		if (!p_rt)
			return -ENOMEM;
	}

	return 0;
}

static int sdw_master_port_config(struct sdw_master_runtime *m_rt,
				  struct sdw_port_config *port_config)
{
	struct sdw_port_runtime *p_rt;
	int ret;
	int i;

	i = 0;
	list_for_each_entry(p_rt, &m_rt->port_list, port_node) {
		ret = sdw_port_config(p_rt, port_config, i);
		if (ret < 0)
			return ret;
		i++;
	}

	return 0;
}

 
static struct sdw_slave_runtime
*sdw_slave_rt_alloc(struct sdw_slave *slave,
		    struct sdw_master_runtime *m_rt)
{
	struct sdw_slave_runtime *s_rt;

	s_rt = kzalloc(sizeof(*s_rt), GFP_KERNEL);
	if (!s_rt)
		return NULL;

	INIT_LIST_HEAD(&s_rt->port_list);
	s_rt->slave = slave;

	list_add_tail(&s_rt->m_rt_node, &m_rt->slave_rt_list);

	return s_rt;
}

 
static int sdw_slave_rt_config(struct sdw_slave_runtime *s_rt,
			       struct sdw_stream_config *stream_config)
{
	s_rt->ch_count = stream_config->ch_count;
	s_rt->direction = stream_config->direction;

	return 0;
}

static struct sdw_slave_runtime *sdw_slave_rt_find(struct sdw_slave *slave,
						   struct sdw_stream_runtime *stream)
{
	struct sdw_slave_runtime *s_rt, *_s_rt;
	struct sdw_master_runtime *m_rt;

	list_for_each_entry(m_rt, &stream->master_list, stream_node) {
		 
		list_for_each_entry_safe(s_rt, _s_rt,
					 &m_rt->slave_rt_list, m_rt_node) {
			if (s_rt->slave == slave)
				return s_rt;
		}
	}
	return NULL;
}

 
static void sdw_slave_rt_free(struct sdw_slave *slave,
			      struct sdw_stream_runtime *stream)
{
	struct sdw_slave_runtime *s_rt;

	s_rt = sdw_slave_rt_find(slave, stream);
	if (s_rt) {
		list_del(&s_rt->m_rt_node);
		kfree(s_rt);
	}
}

static struct sdw_master_runtime
*sdw_master_rt_find(struct sdw_bus *bus,
		    struct sdw_stream_runtime *stream)
{
	struct sdw_master_runtime *m_rt;

	 
	list_for_each_entry(m_rt, &stream->master_list, stream_node) {
		if (m_rt->bus == bus)
			return m_rt;
	}

	return NULL;
}

 
static struct sdw_master_runtime
*sdw_master_rt_alloc(struct sdw_bus *bus,
		     struct sdw_stream_runtime *stream)
{
	struct sdw_master_runtime *m_rt, *walk_m_rt;
	struct list_head *insert_after;

	m_rt = kzalloc(sizeof(*m_rt), GFP_KERNEL);
	if (!m_rt)
		return NULL;

	 
	INIT_LIST_HEAD(&m_rt->port_list);
	INIT_LIST_HEAD(&m_rt->slave_rt_list);

	 
	insert_after = &stream->master_list;
	list_for_each_entry_reverse(walk_m_rt, &stream->master_list, stream_node) {
		if (walk_m_rt->bus->id < bus->id) {
			insert_after = &walk_m_rt->stream_node;
			break;
		}
	}
	list_add(&m_rt->stream_node, insert_after);

	list_add_tail(&m_rt->bus_node, &bus->m_rt_list);

	m_rt->bus = bus;
	m_rt->stream = stream;

	return m_rt;
}

 

static int sdw_master_rt_config(struct sdw_master_runtime *m_rt,
				struct sdw_stream_config *stream_config)
{
	m_rt->ch_count = stream_config->ch_count;
	m_rt->direction = stream_config->direction;

	return 0;
}

 
static void sdw_master_rt_free(struct sdw_master_runtime *m_rt,
			       struct sdw_stream_runtime *stream)
{
	struct sdw_slave_runtime *s_rt, *_s_rt;

	list_for_each_entry_safe(s_rt, _s_rt, &m_rt->slave_rt_list, m_rt_node) {
		sdw_slave_port_free(s_rt->slave, stream);
		sdw_slave_rt_free(s_rt->slave, stream);
	}

	list_del(&m_rt->stream_node);
	list_del(&m_rt->bus_node);
	kfree(m_rt);
}

 
static int sdw_config_stream(struct device *dev,
			     struct sdw_stream_runtime *stream,
			     struct sdw_stream_config *stream_config,
			     bool is_slave)
{
	 
	if (stream->params.rate &&
	    stream->params.rate != stream_config->frame_rate) {
		dev_err(dev, "rate not matching, stream:%s\n", stream->name);
		return -EINVAL;
	}

	if (stream->params.bps &&
	    stream->params.bps != stream_config->bps) {
		dev_err(dev, "bps not matching, stream:%s\n", stream->name);
		return -EINVAL;
	}

	stream->type = stream_config->type;
	stream->params.rate = stream_config->frame_rate;
	stream->params.bps = stream_config->bps;

	 
	if (is_slave)
		stream->params.ch_count += stream_config->ch_count;

	return 0;
}

 
struct sdw_dpn_prop *sdw_get_slave_dpn_prop(struct sdw_slave *slave,
					    enum sdw_data_direction direction,
					    unsigned int port_num)
{
	struct sdw_dpn_prop *dpn_prop;
	u8 num_ports;
	int i;

	if (direction == SDW_DATA_DIR_TX) {
		num_ports = hweight32(slave->prop.source_ports);
		dpn_prop = slave->prop.src_dpn_prop;
	} else {
		num_ports = hweight32(slave->prop.sink_ports);
		dpn_prop = slave->prop.sink_dpn_prop;
	}

	for (i = 0; i < num_ports; i++) {
		if (dpn_prop[i].num == port_num)
			return &dpn_prop[i];
	}

	return NULL;
}

 
static void sdw_acquire_bus_lock(struct sdw_stream_runtime *stream)
{
	struct sdw_master_runtime *m_rt;
	struct sdw_bus *bus;

	 
	list_for_each_entry(m_rt, &stream->master_list, stream_node) {
		bus = m_rt->bus;

		mutex_lock(&bus->bus_lock);
	}
}

 
static void sdw_release_bus_lock(struct sdw_stream_runtime *stream)
{
	struct sdw_master_runtime *m_rt;
	struct sdw_bus *bus;

	 
	list_for_each_entry_reverse(m_rt, &stream->master_list, stream_node) {
		bus = m_rt->bus;
		mutex_unlock(&bus->bus_lock);
	}
}

static int _sdw_prepare_stream(struct sdw_stream_runtime *stream,
			       bool update_params)
{
	struct sdw_master_runtime *m_rt;
	struct sdw_bus *bus;
	struct sdw_master_prop *prop;
	struct sdw_bus_params params;
	int ret;

	 
	list_for_each_entry(m_rt, &stream->master_list, stream_node) {
		bus = m_rt->bus;
		prop = &bus->prop;
		memcpy(&params, &bus->params, sizeof(params));

		 
		if ((prop->max_clk_freq % stream->params.rate) != 0) {
			dev_err(bus->dev, "Async mode not supported\n");
			return -EINVAL;
		}

		if (update_params) {
			 
			 
			bus->params.bandwidth += m_rt->stream->params.rate *
				m_rt->ch_count * m_rt->stream->params.bps;

			 
			if (bus->compute_params) {
				ret = bus->compute_params(bus);
				if (ret < 0) {
					dev_err(bus->dev, "Compute params failed: %d\n",
						ret);
					goto restore_params;
				}
			}
		}

		 
		ret = sdw_program_params(bus, true);
		if (ret < 0) {
			dev_err(bus->dev, "Program params failed: %d\n", ret);
			goto restore_params;
		}
	}

	ret = do_bank_switch(stream);
	if (ret < 0) {
		pr_err("%s: do_bank_switch failed: %d\n", __func__, ret);
		goto restore_params;
	}

	list_for_each_entry(m_rt, &stream->master_list, stream_node) {
		bus = m_rt->bus;

		 
		ret = sdw_prep_deprep_ports(m_rt, true);
		if (ret < 0) {
			dev_err(bus->dev, "Prepare port(s) failed ret = %d\n",
				ret);
			return ret;
		}
	}

	stream->state = SDW_STREAM_PREPARED;

	return ret;

restore_params:
	memcpy(&bus->params, &params, sizeof(params));
	return ret;
}

 
int sdw_prepare_stream(struct sdw_stream_runtime *stream)
{
	bool update_params = true;
	int ret;

	if (!stream) {
		pr_err("SoundWire: Handle not found for stream\n");
		return -EINVAL;
	}

	sdw_acquire_bus_lock(stream);

	if (stream->state == SDW_STREAM_PREPARED) {
		ret = 0;
		goto state_err;
	}

	if (stream->state != SDW_STREAM_CONFIGURED &&
	    stream->state != SDW_STREAM_DEPREPARED &&
	    stream->state != SDW_STREAM_DISABLED) {
		pr_err("%s: %s: inconsistent state state %d\n",
		       __func__, stream->name, stream->state);
		ret = -EINVAL;
		goto state_err;
	}

	 
	if (stream->state == SDW_STREAM_DISABLED)
		update_params = false;

	ret = _sdw_prepare_stream(stream, update_params);

state_err:
	sdw_release_bus_lock(stream);
	return ret;
}
EXPORT_SYMBOL(sdw_prepare_stream);

static int _sdw_enable_stream(struct sdw_stream_runtime *stream)
{
	struct sdw_master_runtime *m_rt;
	struct sdw_bus *bus;
	int ret;

	 
	list_for_each_entry(m_rt, &stream->master_list, stream_node) {
		bus = m_rt->bus;

		 
		ret = sdw_program_params(bus, false);
		if (ret < 0) {
			dev_err(bus->dev, "%s: Program params failed: %d\n", __func__, ret);
			return ret;
		}

		 
		ret = sdw_enable_disable_ports(m_rt, true);
		if (ret < 0) {
			dev_err(bus->dev,
				"Enable port(s) failed ret: %d\n", ret);
			return ret;
		}
	}

	ret = do_bank_switch(stream);
	if (ret < 0) {
		pr_err("%s: do_bank_switch failed: %d\n", __func__, ret);
		return ret;
	}

	stream->state = SDW_STREAM_ENABLED;
	return 0;
}

 
int sdw_enable_stream(struct sdw_stream_runtime *stream)
{
	int ret;

	if (!stream) {
		pr_err("SoundWire: Handle not found for stream\n");
		return -EINVAL;
	}

	sdw_acquire_bus_lock(stream);

	if (stream->state == SDW_STREAM_ENABLED) {
		ret = 0;
		goto state_err;
	}

	if (stream->state != SDW_STREAM_PREPARED &&
	    stream->state != SDW_STREAM_DISABLED) {
		pr_err("%s: %s: inconsistent state state %d\n",
		       __func__, stream->name, stream->state);
		ret = -EINVAL;
		goto state_err;
	}

	ret = _sdw_enable_stream(stream);

state_err:
	sdw_release_bus_lock(stream);
	return ret;
}
EXPORT_SYMBOL(sdw_enable_stream);

static int _sdw_disable_stream(struct sdw_stream_runtime *stream)
{
	struct sdw_master_runtime *m_rt;
	int ret;

	list_for_each_entry(m_rt, &stream->master_list, stream_node) {
		struct sdw_bus *bus = m_rt->bus;

		 
		ret = sdw_enable_disable_ports(m_rt, false);
		if (ret < 0) {
			dev_err(bus->dev, "Disable port(s) failed: %d\n", ret);
			return ret;
		}
	}
	stream->state = SDW_STREAM_DISABLED;

	list_for_each_entry(m_rt, &stream->master_list, stream_node) {
		struct sdw_bus *bus = m_rt->bus;

		 
		ret = sdw_program_params(bus, false);
		if (ret < 0) {
			dev_err(bus->dev, "%s: Program params failed: %d\n", __func__, ret);
			return ret;
		}
	}

	ret = do_bank_switch(stream);
	if (ret < 0) {
		pr_err("%s: do_bank_switch failed: %d\n", __func__, ret);
		return ret;
	}

	 
	list_for_each_entry(m_rt, &stream->master_list, stream_node) {
		struct sdw_bus *bus = m_rt->bus;

		 
		ret = sdw_enable_disable_ports(m_rt, false);
		if (ret < 0) {
			dev_err(bus->dev, "Disable port(s) failed: %d\n", ret);
			return ret;
		}
	}

	return 0;
}

 
int sdw_disable_stream(struct sdw_stream_runtime *stream)
{
	int ret;

	if (!stream) {
		pr_err("SoundWire: Handle not found for stream\n");
		return -EINVAL;
	}

	sdw_acquire_bus_lock(stream);

	if (stream->state == SDW_STREAM_DISABLED) {
		ret = 0;
		goto state_err;
	}

	if (stream->state != SDW_STREAM_ENABLED) {
		pr_err("%s: %s: inconsistent state state %d\n",
		       __func__, stream->name, stream->state);
		ret = -EINVAL;
		goto state_err;
	}

	ret = _sdw_disable_stream(stream);

state_err:
	sdw_release_bus_lock(stream);
	return ret;
}
EXPORT_SYMBOL(sdw_disable_stream);

static int _sdw_deprepare_stream(struct sdw_stream_runtime *stream)
{
	struct sdw_master_runtime *m_rt;
	struct sdw_bus *bus;
	int ret = 0;

	list_for_each_entry(m_rt, &stream->master_list, stream_node) {
		bus = m_rt->bus;
		 
		ret = sdw_prep_deprep_ports(m_rt, false);
		if (ret < 0) {
			dev_err(bus->dev,
				"De-prepare port(s) failed: %d\n", ret);
			return ret;
		}

		 
		bus->params.bandwidth -= m_rt->stream->params.rate *
			m_rt->ch_count * m_rt->stream->params.bps;

		 
		if (bus->compute_params) {
			ret = bus->compute_params(bus);
			if (ret < 0) {
				dev_err(bus->dev, "Compute params failed: %d\n",
					ret);
				return ret;
			}
		}

		 
		ret = sdw_program_params(bus, false);
		if (ret < 0) {
			dev_err(bus->dev, "%s: Program params failed: %d\n", __func__, ret);
			return ret;
		}
	}

	stream->state = SDW_STREAM_DEPREPARED;
	return do_bank_switch(stream);
}

 
int sdw_deprepare_stream(struct sdw_stream_runtime *stream)
{
	int ret;

	if (!stream) {
		pr_err("SoundWire: Handle not found for stream\n");
		return -EINVAL;
	}

	sdw_acquire_bus_lock(stream);

	if (stream->state == SDW_STREAM_DEPREPARED) {
		ret = 0;
		goto state_err;
	}

	if (stream->state != SDW_STREAM_PREPARED &&
	    stream->state != SDW_STREAM_DISABLED) {
		pr_err("%s: %s: inconsistent state state %d\n",
		       __func__, stream->name, stream->state);
		ret = -EINVAL;
		goto state_err;
	}

	ret = _sdw_deprepare_stream(stream);

state_err:
	sdw_release_bus_lock(stream);
	return ret;
}
EXPORT_SYMBOL(sdw_deprepare_stream);

static int set_stream(struct snd_pcm_substream *substream,
		      struct sdw_stream_runtime *sdw_stream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *dai;
	int ret = 0;
	int i;

	 
	for_each_rtd_dais(rtd, i, dai) {
		ret = snd_soc_dai_set_stream(dai, sdw_stream, substream->stream);
		if (ret < 0) {
			dev_err(rtd->dev, "failed to set stream pointer on dai %s\n", dai->name);
			break;
		}
	}

	return ret;
}

 
struct sdw_stream_runtime *sdw_alloc_stream(const char *stream_name)
{
	struct sdw_stream_runtime *stream;

	stream = kzalloc(sizeof(*stream), GFP_KERNEL);
	if (!stream)
		return NULL;

	stream->name = stream_name;
	INIT_LIST_HEAD(&stream->master_list);
	stream->state = SDW_STREAM_ALLOCATED;
	stream->m_rt_count = 0;

	return stream;
}
EXPORT_SYMBOL(sdw_alloc_stream);

 
int sdw_startup_stream(void *sdw_substream)
{
	struct snd_pcm_substream *substream = sdw_substream;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct sdw_stream_runtime *sdw_stream;
	char *name;
	int ret;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		name = kasprintf(GFP_KERNEL, "%s-Playback", substream->name);
	else
		name = kasprintf(GFP_KERNEL, "%s-Capture", substream->name);

	if (!name)
		return -ENOMEM;

	sdw_stream = sdw_alloc_stream(name);
	if (!sdw_stream) {
		dev_err(rtd->dev, "alloc stream failed for substream DAI %s\n", substream->name);
		ret = -ENOMEM;
		goto error;
	}

	ret = set_stream(substream, sdw_stream);
	if (ret < 0)
		goto release_stream;
	return 0;

release_stream:
	sdw_release_stream(sdw_stream);
	set_stream(substream, NULL);
error:
	kfree(name);
	return ret;
}
EXPORT_SYMBOL(sdw_startup_stream);

 
void sdw_shutdown_stream(void *sdw_substream)
{
	struct snd_pcm_substream *substream = sdw_substream;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct sdw_stream_runtime *sdw_stream;
	struct snd_soc_dai *dai;

	 
	dai = asoc_rtd_to_cpu(rtd, 0);

	sdw_stream = snd_soc_dai_get_stream(dai, substream->stream);

	if (IS_ERR(sdw_stream)) {
		dev_err(rtd->dev, "no stream found for DAI %s\n", dai->name);
		return;
	}

	 
	kfree(sdw_stream->name);
	sdw_release_stream(sdw_stream);

	 
	set_stream(substream, NULL);
}
EXPORT_SYMBOL(sdw_shutdown_stream);

 
void sdw_release_stream(struct sdw_stream_runtime *stream)
{
	kfree(stream);
}
EXPORT_SYMBOL(sdw_release_stream);

 
int sdw_stream_add_master(struct sdw_bus *bus,
			  struct sdw_stream_config *stream_config,
			  struct sdw_port_config *port_config,
			  unsigned int num_ports,
			  struct sdw_stream_runtime *stream)
{
	struct sdw_master_runtime *m_rt;
	bool alloc_master_rt = false;
	int ret;

	mutex_lock(&bus->bus_lock);

	 
	if (!bus->multi_link && stream->m_rt_count > 0) {
		dev_err(bus->dev,
			"Multilink not supported, link %d\n", bus->link_id);
		ret = -EINVAL;
		goto unlock;
	}

	 
	m_rt = sdw_master_rt_find(bus, stream);
	if (!m_rt) {
		m_rt = sdw_master_rt_alloc(bus, stream);
		if (!m_rt) {
			dev_err(bus->dev, "%s: Master runtime alloc failed for stream:%s\n",
				__func__, stream->name);
			ret = -ENOMEM;
			goto unlock;
		}

		alloc_master_rt = true;
	}

	if (!sdw_master_port_allocated(m_rt)) {
		ret = sdw_master_port_alloc(m_rt, num_ports);
		if (ret)
			goto alloc_error;

		stream->m_rt_count++;
	}

	ret = sdw_master_rt_config(m_rt, stream_config);
	if (ret < 0)
		goto unlock;

	ret = sdw_config_stream(bus->dev, stream, stream_config, false);
	if (ret)
		goto unlock;

	ret = sdw_master_port_config(m_rt, port_config);

	goto unlock;

alloc_error:
	 
	if (alloc_master_rt)
		sdw_master_rt_free(m_rt, stream);
unlock:
	mutex_unlock(&bus->bus_lock);
	return ret;
}
EXPORT_SYMBOL(sdw_stream_add_master);

 
int sdw_stream_remove_master(struct sdw_bus *bus,
			     struct sdw_stream_runtime *stream)
{
	struct sdw_master_runtime *m_rt, *_m_rt;

	mutex_lock(&bus->bus_lock);

	list_for_each_entry_safe(m_rt, _m_rt,
				 &stream->master_list, stream_node) {
		if (m_rt->bus != bus)
			continue;

		sdw_master_port_free(m_rt);
		sdw_master_rt_free(m_rt, stream);
		stream->m_rt_count--;
	}

	if (list_empty(&stream->master_list))
		stream->state = SDW_STREAM_RELEASED;

	mutex_unlock(&bus->bus_lock);

	return 0;
}
EXPORT_SYMBOL(sdw_stream_remove_master);

 
int sdw_stream_add_slave(struct sdw_slave *slave,
			 struct sdw_stream_config *stream_config,
			 struct sdw_port_config *port_config,
			 unsigned int num_ports,
			 struct sdw_stream_runtime *stream)
{
	struct sdw_slave_runtime *s_rt;
	struct sdw_master_runtime *m_rt;
	bool alloc_master_rt = false;
	bool alloc_slave_rt = false;

	int ret;

	mutex_lock(&slave->bus->bus_lock);

	 
	m_rt = sdw_master_rt_find(slave->bus, stream);
	if (!m_rt) {
		 
		m_rt = sdw_master_rt_alloc(slave->bus, stream);
		if (!m_rt) {
			dev_err(&slave->dev, "%s: Master runtime alloc failed for stream:%s\n",
				__func__, stream->name);
			ret = -ENOMEM;
			goto unlock;
		}

		alloc_master_rt = true;
	}

	s_rt = sdw_slave_rt_find(slave, stream);
	if (!s_rt) {
		s_rt = sdw_slave_rt_alloc(slave, m_rt);
		if (!s_rt) {
			dev_err(&slave->dev, "Slave runtime alloc failed for stream:%s\n",
				stream->name);
			ret = -ENOMEM;
			goto alloc_error;
		}

		alloc_slave_rt = true;
	}

	if (!sdw_slave_port_allocated(s_rt)) {
		ret = sdw_slave_port_alloc(slave, s_rt, num_ports);
		if (ret)
			goto alloc_error;
	}

	ret =  sdw_master_rt_config(m_rt, stream_config);
	if (ret)
		goto unlock;

	ret = sdw_slave_rt_config(s_rt, stream_config);
	if (ret)
		goto unlock;

	ret = sdw_config_stream(&slave->dev, stream, stream_config, true);
	if (ret)
		goto unlock;

	ret = sdw_slave_port_config(slave, s_rt, port_config);
	if (ret)
		goto unlock;

	 
	stream->state = SDW_STREAM_CONFIGURED;
	goto unlock;

alloc_error:
	 
	if (alloc_master_rt)
		sdw_master_rt_free(m_rt, stream);
	else if (alloc_slave_rt)
		sdw_slave_rt_free(slave, stream);
unlock:
	mutex_unlock(&slave->bus->bus_lock);
	return ret;
}
EXPORT_SYMBOL(sdw_stream_add_slave);

 
int sdw_stream_remove_slave(struct sdw_slave *slave,
			    struct sdw_stream_runtime *stream)
{
	mutex_lock(&slave->bus->bus_lock);

	sdw_slave_port_free(slave, stream);
	sdw_slave_rt_free(slave, stream);

	mutex_unlock(&slave->bus->bus_lock);

	return 0;
}
EXPORT_SYMBOL(sdw_stream_remove_slave);
