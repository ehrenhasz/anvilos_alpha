
 
#include "sja1105.h"

#define SJA1105_TAS_CLKSRC_DISABLED	0
#define SJA1105_TAS_CLKSRC_STANDALONE	1
#define SJA1105_TAS_CLKSRC_AS6802	2
#define SJA1105_TAS_CLKSRC_PTP		3
#define SJA1105_GATE_MASK		GENMASK_ULL(SJA1105_NUM_TC - 1, 0)

#define work_to_sja1105_tas(d) \
	container_of((d), struct sja1105_tas_data, tas_work)
#define tas_to_sja1105(d) \
	container_of((d), struct sja1105_private, tas_data)

static int sja1105_tas_set_runtime_params(struct sja1105_private *priv)
{
	struct sja1105_tas_data *tas_data = &priv->tas_data;
	struct sja1105_gating_config *gating_cfg = &tas_data->gating_cfg;
	struct dsa_switch *ds = priv->ds;
	s64 earliest_base_time = S64_MAX;
	s64 latest_base_time = 0;
	s64 its_cycle_time = 0;
	s64 max_cycle_time = 0;
	int port;

	tas_data->enabled = false;

	for (port = 0; port < ds->num_ports; port++) {
		const struct tc_taprio_qopt_offload *offload;

		offload = tas_data->offload[port];
		if (!offload)
			continue;

		tas_data->enabled = true;

		if (max_cycle_time < offload->cycle_time)
			max_cycle_time = offload->cycle_time;
		if (latest_base_time < offload->base_time)
			latest_base_time = offload->base_time;
		if (earliest_base_time > offload->base_time) {
			earliest_base_time = offload->base_time;
			its_cycle_time = offload->cycle_time;
		}
	}

	if (!list_empty(&gating_cfg->entries)) {
		tas_data->enabled = true;

		if (max_cycle_time < gating_cfg->cycle_time)
			max_cycle_time = gating_cfg->cycle_time;
		if (latest_base_time < gating_cfg->base_time)
			latest_base_time = gating_cfg->base_time;
		if (earliest_base_time > gating_cfg->base_time) {
			earliest_base_time = gating_cfg->base_time;
			its_cycle_time = gating_cfg->cycle_time;
		}
	}

	if (!tas_data->enabled)
		return 0;

	 
	earliest_base_time = future_base_time(earliest_base_time,
					      its_cycle_time,
					      latest_base_time);
	while (earliest_base_time > latest_base_time)
		earliest_base_time -= its_cycle_time;
	if (latest_base_time - earliest_base_time >
	    sja1105_delta_to_ns(SJA1105_TAS_MAX_DELTA)) {
		dev_err(ds->dev,
			"Base times too far apart: min %llu max %llu\n",
			earliest_base_time, latest_base_time);
		return -ERANGE;
	}

	tas_data->earliest_base_time = earliest_base_time;
	tas_data->max_cycle_time = max_cycle_time;

	dev_dbg(ds->dev, "earliest base time %lld ns\n", earliest_base_time);
	dev_dbg(ds->dev, "latest base time %lld ns\n", latest_base_time);
	dev_dbg(ds->dev, "longest cycle time %lld ns\n", max_cycle_time);

	return 0;
}

 
int sja1105_init_scheduling(struct sja1105_private *priv)
{
	struct sja1105_schedule_entry_points_entry *schedule_entry_points;
	struct sja1105_schedule_entry_points_params_entry
					*schedule_entry_points_params;
	struct sja1105_schedule_params_entry *schedule_params;
	struct sja1105_tas_data *tas_data = &priv->tas_data;
	struct sja1105_gating_config *gating_cfg = &tas_data->gating_cfg;
	struct sja1105_schedule_entry *schedule;
	struct dsa_switch *ds = priv->ds;
	struct sja1105_table *table;
	int schedule_start_idx;
	s64 entry_point_delta;
	int schedule_end_idx;
	int num_entries = 0;
	int num_cycles = 0;
	int cycle = 0;
	int i, k = 0;
	int port, rc;

	rc = sja1105_tas_set_runtime_params(priv);
	if (rc < 0)
		return rc;

	 
	table = &priv->static_config.tables[BLK_IDX_SCHEDULE];
	if (table->entry_count) {
		kfree(table->entries);
		table->entry_count = 0;
	}

	 
	table = &priv->static_config.tables[BLK_IDX_SCHEDULE_ENTRY_POINTS_PARAMS];
	if (table->entry_count) {
		kfree(table->entries);
		table->entry_count = 0;
	}

	 
	table = &priv->static_config.tables[BLK_IDX_SCHEDULE_PARAMS];
	if (table->entry_count) {
		kfree(table->entries);
		table->entry_count = 0;
	}

	 
	table = &priv->static_config.tables[BLK_IDX_SCHEDULE_ENTRY_POINTS];
	if (table->entry_count) {
		kfree(table->entries);
		table->entry_count = 0;
	}

	 
	for (port = 0; port < ds->num_ports; port++) {
		if (tas_data->offload[port]) {
			num_entries += tas_data->offload[port]->num_entries;
			num_cycles++;
		}
	}

	if (!list_empty(&gating_cfg->entries)) {
		num_entries += gating_cfg->num_entries;
		num_cycles++;
	}

	 
	if (!num_cycles)
		return 0;

	 

	 
	table = &priv->static_config.tables[BLK_IDX_SCHEDULE];
	table->entries = kcalloc(num_entries, table->ops->unpacked_entry_size,
				 GFP_KERNEL);
	if (!table->entries)
		return -ENOMEM;
	table->entry_count = num_entries;
	schedule = table->entries;

	 
	table = &priv->static_config.tables[BLK_IDX_SCHEDULE_ENTRY_POINTS_PARAMS];
	table->entries = kcalloc(SJA1105_MAX_SCHEDULE_ENTRY_POINTS_PARAMS_COUNT,
				 table->ops->unpacked_entry_size, GFP_KERNEL);
	if (!table->entries)
		 
		return -ENOMEM;
	table->entry_count = SJA1105_MAX_SCHEDULE_ENTRY_POINTS_PARAMS_COUNT;
	schedule_entry_points_params = table->entries;

	 
	table = &priv->static_config.tables[BLK_IDX_SCHEDULE_PARAMS];
	table->entries = kcalloc(SJA1105_MAX_SCHEDULE_PARAMS_COUNT,
				 table->ops->unpacked_entry_size, GFP_KERNEL);
	if (!table->entries)
		return -ENOMEM;
	table->entry_count = SJA1105_MAX_SCHEDULE_PARAMS_COUNT;
	schedule_params = table->entries;

	 
	table = &priv->static_config.tables[BLK_IDX_SCHEDULE_ENTRY_POINTS];
	table->entries = kcalloc(num_cycles, table->ops->unpacked_entry_size,
				 GFP_KERNEL);
	if (!table->entries)
		return -ENOMEM;
	table->entry_count = num_cycles;
	schedule_entry_points = table->entries;

	 
	schedule_entry_points_params->clksrc = SJA1105_TAS_CLKSRC_PTP;
	schedule_entry_points_params->actsubsch = num_cycles - 1;

	for (port = 0; port < ds->num_ports; port++) {
		const struct tc_taprio_qopt_offload *offload;
		 
		s64 rbt;

		offload = tas_data->offload[port];
		if (!offload)
			continue;

		schedule_start_idx = k;
		schedule_end_idx = k + offload->num_entries - 1;
		 
		rbt = future_base_time(offload->base_time,
				       offload->cycle_time,
				       tas_data->earliest_base_time);
		rbt -= tas_data->earliest_base_time;
		 
		entry_point_delta = ns_to_sja1105_delta(rbt) + 1;

		schedule_entry_points[cycle].subschindx = cycle;
		schedule_entry_points[cycle].delta = entry_point_delta;
		schedule_entry_points[cycle].address = schedule_start_idx;

		 
		for (i = cycle; i < 8; i++)
			schedule_params->subscheind[i] = schedule_end_idx;

		for (i = 0; i < offload->num_entries; i++, k++) {
			s64 delta_ns = offload->entries[i].interval;

			schedule[k].delta = ns_to_sja1105_delta(delta_ns);
			schedule[k].destports = BIT(port);
			schedule[k].resmedia_en = true;
			schedule[k].resmedia = SJA1105_GATE_MASK &
					~offload->entries[i].gate_mask;
		}
		cycle++;
	}

	if (!list_empty(&gating_cfg->entries)) {
		struct sja1105_gate_entry *e;

		 
		s64 rbt;

		schedule_start_idx = k;
		schedule_end_idx = k + gating_cfg->num_entries - 1;
		rbt = future_base_time(gating_cfg->base_time,
				       gating_cfg->cycle_time,
				       tas_data->earliest_base_time);
		rbt -= tas_data->earliest_base_time;
		entry_point_delta = ns_to_sja1105_delta(rbt) + 1;

		schedule_entry_points[cycle].subschindx = cycle;
		schedule_entry_points[cycle].delta = entry_point_delta;
		schedule_entry_points[cycle].address = schedule_start_idx;

		for (i = cycle; i < 8; i++)
			schedule_params->subscheind[i] = schedule_end_idx;

		list_for_each_entry(e, &gating_cfg->entries, list) {
			schedule[k].delta = ns_to_sja1105_delta(e->interval);
			schedule[k].destports = e->rule->vl.destports;
			schedule[k].setvalid = true;
			schedule[k].txen = true;
			schedule[k].vlindex = e->rule->vl.sharindx;
			schedule[k].winstindex = e->rule->vl.sharindx;
			if (e->gate_state)  
				schedule[k].winst = true;
			else  
				schedule[k].winend = true;
			k++;
		}
	}

	return 0;
}

 
static bool
sja1105_tas_check_conflicts(struct sja1105_private *priv, int port,
			    const struct tc_taprio_qopt_offload *admin)
{
	struct sja1105_tas_data *tas_data = &priv->tas_data;
	const struct tc_taprio_qopt_offload *offload;
	s64 max_cycle_time, min_cycle_time;
	s64 delta1, delta2;
	s64 rbt1, rbt2;
	s64 stop_time;
	s64 t1, t2;
	int i, j;
	s32 rem;

	offload = tas_data->offload[port];
	if (!offload)
		return false;

	 
	max_cycle_time = max(offload->cycle_time, admin->cycle_time);
	min_cycle_time = min(offload->cycle_time, admin->cycle_time);
	div_s64_rem(max_cycle_time, min_cycle_time, &rem);
	if (rem)
		return true;

	 
	div_s64_rem(offload->base_time, offload->cycle_time, &rem);
	rbt1 = rem;

	div_s64_rem(admin->base_time, admin->cycle_time, &rem);
	rbt2 = rem;

	stop_time = max_cycle_time + max(rbt1, rbt2);

	 
	for (i = 0, delta1 = 0;
	     i < offload->num_entries;
	     delta1 += offload->entries[i].interval, i++) {
		 
		for (j = 0, delta2 = 0;
		     j < admin->num_entries;
		     delta2 += admin->entries[j].interval, j++) {
			 
			for (t1 = rbt1 + delta1;
			     t1 <= stop_time;
			     t1 += offload->cycle_time) {
				 
				for (t2 = rbt2 + delta2;
				     t2 <= stop_time;
				     t2 += admin->cycle_time) {
					if (t1 == t2) {
						dev_warn(priv->ds->dev,
							 "GCL entry %d collides with entry %d of port %d\n",
							 j, i, port);
						return true;
					}
				}
			}
		}
	}

	return false;
}

 
bool sja1105_gating_check_conflicts(struct sja1105_private *priv, int port,
				    struct netlink_ext_ack *extack)
{
	struct sja1105_gating_config *gating_cfg = &priv->tas_data.gating_cfg;
	size_t num_entries = gating_cfg->num_entries;
	struct tc_taprio_qopt_offload *dummy;
	struct dsa_switch *ds = priv->ds;
	struct sja1105_gate_entry *e;
	bool conflict;
	int i = 0;

	if (list_empty(&gating_cfg->entries))
		return false;

	dummy = kzalloc(struct_size(dummy, entries, num_entries), GFP_KERNEL);
	if (!dummy) {
		NL_SET_ERR_MSG_MOD(extack, "Failed to allocate memory");
		return true;
	}

	dummy->num_entries = num_entries;
	dummy->base_time = gating_cfg->base_time;
	dummy->cycle_time = gating_cfg->cycle_time;

	list_for_each_entry(e, &gating_cfg->entries, list)
		dummy->entries[i++].interval = e->interval;

	if (port != -1) {
		conflict = sja1105_tas_check_conflicts(priv, port, dummy);
	} else {
		for (port = 0; port < ds->num_ports; port++) {
			conflict = sja1105_tas_check_conflicts(priv, port,
							       dummy);
			if (conflict)
				break;
		}
	}

	kfree(dummy);

	return conflict;
}

int sja1105_setup_tc_taprio(struct dsa_switch *ds, int port,
			    struct tc_taprio_qopt_offload *admin)
{
	struct sja1105_private *priv = ds->priv;
	struct sja1105_tas_data *tas_data = &priv->tas_data;
	int other_port, rc, i;

	 
	if ((!!tas_data->offload[port] && admin->cmd == TAPRIO_CMD_REPLACE) ||
	    (!tas_data->offload[port] && admin->cmd == TAPRIO_CMD_DESTROY))
		return -EINVAL;

	if (admin->cmd == TAPRIO_CMD_DESTROY) {
		taprio_offload_free(tas_data->offload[port]);
		tas_data->offload[port] = NULL;

		rc = sja1105_init_scheduling(priv);
		if (rc < 0)
			return rc;

		return sja1105_static_config_reload(priv, SJA1105_SCHEDULING);
	} else if (admin->cmd != TAPRIO_CMD_REPLACE) {
		return -EOPNOTSUPP;
	}

	 
	if (admin->cycle_time_extension)
		return -ENOTSUPP;

	for (i = 0; i < admin->num_entries; i++) {
		s64 delta_ns = admin->entries[i].interval;
		s64 delta_cycles = ns_to_sja1105_delta(delta_ns);
		bool too_long, too_short;

		too_long = (delta_cycles >= SJA1105_TAS_MAX_DELTA);
		too_short = (delta_cycles == 0);
		if (too_long || too_short) {
			dev_err(priv->ds->dev,
				"Interval %llu too %s for GCL entry %d\n",
				delta_ns, too_long ? "long" : "short", i);
			return -ERANGE;
		}
	}

	for (other_port = 0; other_port < ds->num_ports; other_port++) {
		if (other_port == port)
			continue;

		if (sja1105_tas_check_conflicts(priv, other_port, admin))
			return -ERANGE;
	}

	if (sja1105_gating_check_conflicts(priv, port, NULL)) {
		dev_err(ds->dev, "Conflict with tc-gate schedule\n");
		return -ERANGE;
	}

	tas_data->offload[port] = taprio_offload_get(admin);

	rc = sja1105_init_scheduling(priv);
	if (rc < 0)
		return rc;

	return sja1105_static_config_reload(priv, SJA1105_SCHEDULING);
}

static int sja1105_tas_check_running(struct sja1105_private *priv)
{
	struct sja1105_tas_data *tas_data = &priv->tas_data;
	struct dsa_switch *ds = priv->ds;
	struct sja1105_ptp_cmd cmd = {0};
	int rc;

	rc = sja1105_ptp_commit(ds, &cmd, SPI_READ);
	if (rc < 0)
		return rc;

	if (cmd.ptpstrtsch == 1)
		 
		tas_data->state = SJA1105_TAS_STATE_RUNNING;
	else if (cmd.ptpstopsch == 1)
		 
		tas_data->state = SJA1105_TAS_STATE_DISABLED;
	else
		 
		rc = -EINVAL;

	return rc;
}

 
static int sja1105_tas_adjust_drift(struct sja1105_private *priv,
				    u64 correction)
{
	const struct sja1105_regs *regs = priv->info->regs;
	u32 ptpclkcorp = ns_to_sja1105_ticks(correction);

	return sja1105_xfer_u32(priv, SPI_WRITE, regs->ptpclkcorp,
				&ptpclkcorp, NULL);
}

 
static int sja1105_tas_set_base_time(struct sja1105_private *priv,
				     u64 base_time)
{
	const struct sja1105_regs *regs = priv->info->regs;
	u64 ptpschtm = ns_to_sja1105_ticks(base_time);

	return sja1105_xfer_u64(priv, SPI_WRITE, regs->ptpschtm,
				&ptpschtm, NULL);
}

static int sja1105_tas_start(struct sja1105_private *priv)
{
	struct sja1105_tas_data *tas_data = &priv->tas_data;
	struct sja1105_ptp_cmd *cmd = &priv->ptp_data.cmd;
	struct dsa_switch *ds = priv->ds;
	int rc;

	dev_dbg(ds->dev, "Starting the TAS\n");

	if (tas_data->state == SJA1105_TAS_STATE_ENABLED_NOT_RUNNING ||
	    tas_data->state == SJA1105_TAS_STATE_RUNNING) {
		dev_err(ds->dev, "TAS already started\n");
		return -EINVAL;
	}

	cmd->ptpstrtsch = 1;
	cmd->ptpstopsch = 0;

	rc = sja1105_ptp_commit(ds, cmd, SPI_WRITE);
	if (rc < 0)
		return rc;

	tas_data->state = SJA1105_TAS_STATE_ENABLED_NOT_RUNNING;

	return 0;
}

static int sja1105_tas_stop(struct sja1105_private *priv)
{
	struct sja1105_tas_data *tas_data = &priv->tas_data;
	struct sja1105_ptp_cmd *cmd = &priv->ptp_data.cmd;
	struct dsa_switch *ds = priv->ds;
	int rc;

	dev_dbg(ds->dev, "Stopping the TAS\n");

	if (tas_data->state == SJA1105_TAS_STATE_DISABLED) {
		dev_err(ds->dev, "TAS already disabled\n");
		return -EINVAL;
	}

	cmd->ptpstopsch = 1;
	cmd->ptpstrtsch = 0;

	rc = sja1105_ptp_commit(ds, cmd, SPI_WRITE);
	if (rc < 0)
		return rc;

	tas_data->state = SJA1105_TAS_STATE_DISABLED;

	return 0;
}

 
static void sja1105_tas_state_machine(struct work_struct *work)
{
	struct sja1105_tas_data *tas_data = work_to_sja1105_tas(work);
	struct sja1105_private *priv = tas_to_sja1105(tas_data);
	struct sja1105_ptp_data *ptp_data = &priv->ptp_data;
	struct timespec64 base_time_ts, now_ts;
	struct dsa_switch *ds = priv->ds;
	struct timespec64 diff;
	s64 base_time, now;
	int rc = 0;

	mutex_lock(&ptp_data->lock);

	switch (tas_data->state) {
	case SJA1105_TAS_STATE_DISABLED:
		 
		if (tas_data->last_op != SJA1105_PTP_ADJUSTFREQ)
			break;

		rc = sja1105_tas_adjust_drift(priv, tas_data->max_cycle_time);
		if (rc < 0)
			break;

		rc = __sja1105_ptp_gettimex(ds, &now, NULL);
		if (rc < 0)
			break;

		 
		base_time = future_base_time(tas_data->earliest_base_time,
					     tas_data->max_cycle_time,
					     now + 1ull * NSEC_PER_SEC);
		base_time -= sja1105_delta_to_ns(1);

		rc = sja1105_tas_set_base_time(priv, base_time);
		if (rc < 0)
			break;

		tas_data->oper_base_time = base_time;

		rc = sja1105_tas_start(priv);
		if (rc < 0)
			break;

		base_time_ts = ns_to_timespec64(base_time);
		now_ts = ns_to_timespec64(now);

		dev_dbg(ds->dev, "OPER base time %lld.%09ld (now %lld.%09ld)\n",
			base_time_ts.tv_sec, base_time_ts.tv_nsec,
			now_ts.tv_sec, now_ts.tv_nsec);

		break;

	case SJA1105_TAS_STATE_ENABLED_NOT_RUNNING:
		if (tas_data->last_op != SJA1105_PTP_ADJUSTFREQ) {
			 
			sja1105_tas_stop(priv);
			break;
		}

		 
		rc = __sja1105_ptp_gettimex(ds, &now, NULL);
		if (rc < 0)
			break;

		if (now < tas_data->oper_base_time) {
			 
			diff = ns_to_timespec64(tas_data->oper_base_time - now);
			dev_dbg(ds->dev, "time to start: [%lld.%09ld]",
				diff.tv_sec, diff.tv_nsec);
			break;
		}

		 
		rc = sja1105_tas_check_running(priv);
		if (rc < 0)
			break;

		if (tas_data->state != SJA1105_TAS_STATE_RUNNING)
			 
			dev_err(ds->dev,
				"TAS not started despite time elapsed\n");

		break;

	case SJA1105_TAS_STATE_RUNNING:
		 
		if (tas_data->last_op != SJA1105_PTP_ADJUSTFREQ) {
			sja1105_tas_stop(priv);
			break;
		}

		rc = sja1105_tas_check_running(priv);
		if (rc < 0)
			break;

		if (tas_data->state != SJA1105_TAS_STATE_RUNNING)
			dev_err(ds->dev, "TAS surprisingly stopped\n");

		break;

	default:
		if (net_ratelimit())
			dev_err(ds->dev, "TAS in an invalid state (incorrect use of API)!\n");
	}

	if (rc && net_ratelimit())
		dev_err(ds->dev, "An operation returned %d\n", rc);

	mutex_unlock(&ptp_data->lock);
}

void sja1105_tas_clockstep(struct dsa_switch *ds)
{
	struct sja1105_private *priv = ds->priv;
	struct sja1105_tas_data *tas_data = &priv->tas_data;

	if (!tas_data->enabled)
		return;

	tas_data->last_op = SJA1105_PTP_CLOCKSTEP;
	schedule_work(&tas_data->tas_work);
}

void sja1105_tas_adjfreq(struct dsa_switch *ds)
{
	struct sja1105_private *priv = ds->priv;
	struct sja1105_tas_data *tas_data = &priv->tas_data;

	if (!tas_data->enabled)
		return;

	 
	if (tas_data->state == SJA1105_TAS_STATE_RUNNING)
		return;

	tas_data->last_op = SJA1105_PTP_ADJUSTFREQ;
	schedule_work(&tas_data->tas_work);
}

void sja1105_tas_setup(struct dsa_switch *ds)
{
	struct sja1105_private *priv = ds->priv;
	struct sja1105_tas_data *tas_data = &priv->tas_data;

	INIT_WORK(&tas_data->tas_work, sja1105_tas_state_machine);
	tas_data->state = SJA1105_TAS_STATE_DISABLED;
	tas_data->last_op = SJA1105_PTP_NONE;

	INIT_LIST_HEAD(&tas_data->gating_cfg.entries);
}

void sja1105_tas_teardown(struct dsa_switch *ds)
{
	struct sja1105_private *priv = ds->priv;
	struct tc_taprio_qopt_offload *offload;
	int port;

	cancel_work_sync(&priv->tas_data.tas_work);

	for (port = 0; port < ds->num_ports; port++) {
		offload = priv->tas_data.offload[port];
		if (!offload)
			continue;

		taprio_offload_free(offload);
	}
}
