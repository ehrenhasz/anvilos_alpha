


#ifndef __SDW_BUS_H
#define __SDW_BUS_H

#define DEFAULT_BANK_SWITCH_TIMEOUT 3000
#define DEFAULT_PROBE_TIMEOUT       2000

u64 sdw_dmi_override_adr(struct sdw_bus *bus, u64 addr);

#if IS_ENABLED(CONFIG_ACPI)
int sdw_acpi_find_slaves(struct sdw_bus *bus);
#else
static inline int sdw_acpi_find_slaves(struct sdw_bus *bus)
{
	return -ENOTSUPP;
}
#endif

int sdw_of_find_slaves(struct sdw_bus *bus);
void sdw_extract_slave_id(struct sdw_bus *bus,
			  u64 addr, struct sdw_slave_id *id);
int sdw_slave_add(struct sdw_bus *bus, struct sdw_slave_id *id,
		  struct fwnode_handle *fwnode);
int sdw_master_device_add(struct sdw_bus *bus, struct device *parent,
			  struct fwnode_handle *fwnode);
int sdw_master_device_del(struct sdw_bus *bus);

#ifdef CONFIG_DEBUG_FS
void sdw_bus_debugfs_init(struct sdw_bus *bus);
void sdw_bus_debugfs_exit(struct sdw_bus *bus);
void sdw_slave_debugfs_init(struct sdw_slave *slave);
void sdw_slave_debugfs_exit(struct sdw_slave *slave);
void sdw_debugfs_init(void);
void sdw_debugfs_exit(void);
#else
static inline void sdw_bus_debugfs_init(struct sdw_bus *bus) {}
static inline void sdw_bus_debugfs_exit(struct sdw_bus *bus) {}
static inline void sdw_slave_debugfs_init(struct sdw_slave *slave) {}
static inline void sdw_slave_debugfs_exit(struct sdw_slave *slave) {}
static inline void sdw_debugfs_init(void) {}
static inline void sdw_debugfs_exit(void) {}
#endif

enum {
	SDW_MSG_FLAG_READ = 0,
	SDW_MSG_FLAG_WRITE,
};


struct sdw_msg {
	u16 addr;
	u16 len;
	u8 dev_num;
	u8 addr_page1;
	u8 addr_page2;
	u8 flags;
	u8 *buf;
	bool ssp_sync;
	bool page;
};

#define SDW_DOUBLE_RATE_FACTOR		2
#define SDW_STRM_RATE_GROUPING		1

extern int sdw_rows[SDW_FRAME_ROWS];
extern int sdw_cols[SDW_FRAME_COLS];

int sdw_find_row_index(int row);
int sdw_find_col_index(int col);


struct sdw_port_runtime {
	int num;
	int ch_mask;
	struct sdw_transport_params transport_params;
	struct sdw_port_params port_params;
	struct list_head port_node;
};


struct sdw_slave_runtime {
	struct sdw_slave *slave;
	enum sdw_data_direction direction;
	unsigned int ch_count;
	struct list_head m_rt_node;
	struct list_head port_list;
};


struct sdw_master_runtime {
	struct sdw_bus *bus;
	struct sdw_stream_runtime *stream;
	enum sdw_data_direction direction;
	unsigned int ch_count;
	struct list_head slave_rt_list;
	struct list_head port_list;
	struct list_head stream_node;
	struct list_head bus_node;
};

struct sdw_transport_data {
	int hstart;
	int hstop;
	int block_offset;
	int sub_block_offset;
};

struct sdw_dpn_prop *sdw_get_slave_dpn_prop(struct sdw_slave *slave,
					    enum sdw_data_direction direction,
					    unsigned int port_num);
int sdw_configure_dpn_intr(struct sdw_slave *slave, int port,
			   bool enable, int mask);

int sdw_transfer(struct sdw_bus *bus, struct sdw_msg *msg);
int sdw_transfer_defer(struct sdw_bus *bus, struct sdw_msg *msg);

#define SDW_READ_INTR_CLEAR_RETRY	10

int sdw_fill_msg(struct sdw_msg *msg, struct sdw_slave *slave,
		 u32 addr, size_t count, u16 dev_num, u8 flags, u8 *buf);


static inline void sdw_fill_xport_params(struct sdw_transport_params *params,
					 int port_num, bool grp_ctrl_valid,
					 int grp_ctrl, int sample_int,
					 int off1, int off2,
					 int hstart, int hstop,
					 int pack_mode, int lane_ctrl)
{
	params->port_num = port_num;
	params->blk_grp_ctrl_valid = grp_ctrl_valid;
	params->blk_grp_ctrl = grp_ctrl;
	params->sample_interval = sample_int;
	params->offset1 = off1;
	params->offset2 = off2;
	params->hstart = hstart;
	params->hstop = hstop;
	params->blk_pkg_mode = pack_mode;
	params->lane_ctrl = lane_ctrl;
}


static inline void sdw_fill_port_params(struct sdw_port_params *params,
					int port_num, int bps,
					int flow_mode, int data_mode)
{
	params->num = port_num;
	params->bps = bps;
	params->flow_mode = flow_mode;
	params->data_mode = data_mode;
}


int sdw_bread_no_pm_unlocked(struct sdw_bus *bus, u16 dev_num, u32 addr);
int sdw_bwrite_no_pm_unlocked(struct sdw_bus *bus, u16 dev_num, u32 addr, u8 value);


#define SDW_UNATTACH_REQUEST_MASTER_RESET	BIT(0)

void sdw_clear_slave_status(struct sdw_bus *bus, u32 request);
int sdw_slave_modalias(const struct sdw_slave *slave, char *buf, size_t size);
void sdw_compute_slave_ports(struct sdw_master_runtime *m_rt,
			     struct sdw_transport_data *t_data);

#endif 
