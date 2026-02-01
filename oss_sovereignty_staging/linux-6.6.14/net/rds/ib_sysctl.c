 
#include <linux/kernel.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>

#include "ib.h"

static struct ctl_table_header *rds_ib_sysctl_hdr;

unsigned long rds_ib_sysctl_max_send_wr = RDS_IB_DEFAULT_SEND_WR;
unsigned long rds_ib_sysctl_max_recv_wr = RDS_IB_DEFAULT_RECV_WR;
unsigned long rds_ib_sysctl_max_recv_allocation = (128 * 1024 * 1024) / RDS_FRAG_SIZE;
static unsigned long rds_ib_sysctl_max_wr_min = 1;
 
static unsigned long rds_ib_sysctl_max_wr_max = (u32)~0;

unsigned long rds_ib_sysctl_max_unsig_wrs = 16;
static unsigned long rds_ib_sysctl_max_unsig_wr_min = 1;
static unsigned long rds_ib_sysctl_max_unsig_wr_max = 64;

 
unsigned int rds_ib_sysctl_flow_control = 0;

static struct ctl_table rds_ib_sysctl_table[] = {
	{
		.procname       = "max_send_wr",
		.data		= &rds_ib_sysctl_max_send_wr,
		.maxlen         = sizeof(unsigned long),
		.mode           = 0644,
		.proc_handler   = proc_doulongvec_minmax,
		.extra1		= &rds_ib_sysctl_max_wr_min,
		.extra2		= &rds_ib_sysctl_max_wr_max,
	},
	{
		.procname       = "max_recv_wr",
		.data		= &rds_ib_sysctl_max_recv_wr,
		.maxlen         = sizeof(unsigned long),
		.mode           = 0644,
		.proc_handler   = proc_doulongvec_minmax,
		.extra1		= &rds_ib_sysctl_max_wr_min,
		.extra2		= &rds_ib_sysctl_max_wr_max,
	},
	{
		.procname       = "max_unsignaled_wr",
		.data		= &rds_ib_sysctl_max_unsig_wrs,
		.maxlen         = sizeof(unsigned long),
		.mode           = 0644,
		.proc_handler   = proc_doulongvec_minmax,
		.extra1		= &rds_ib_sysctl_max_unsig_wr_min,
		.extra2		= &rds_ib_sysctl_max_unsig_wr_max,
	},
	{
		.procname       = "max_recv_allocation",
		.data		= &rds_ib_sysctl_max_recv_allocation,
		.maxlen         = sizeof(unsigned long),
		.mode           = 0644,
		.proc_handler   = proc_doulongvec_minmax,
	},
	{
		.procname	= "flow_control",
		.data		= &rds_ib_sysctl_flow_control,
		.maxlen		= sizeof(rds_ib_sysctl_flow_control),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{ }
};

void rds_ib_sysctl_exit(void)
{
	if (rds_ib_sysctl_hdr)
		unregister_net_sysctl_table(rds_ib_sysctl_hdr);
}

int rds_ib_sysctl_init(void)
{
	rds_ib_sysctl_hdr = register_net_sysctl(&init_net, "net/rds/ib", rds_ib_sysctl_table);
	if (!rds_ib_sysctl_hdr)
		return -ENOMEM;
	return 0;
}
