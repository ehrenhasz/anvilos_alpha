 
 
#include <sound/soc.h>

#ifndef __SDW_CADENCE_H
#define __SDW_CADENCE_H

#define SDW_CADENCE_GSYNC_KHZ		4  
#define SDW_CADENCE_GSYNC_HZ		(SDW_CADENCE_GSYNC_KHZ * 1000)

 
#define CDNS_MCP_IP_MAX_CMD_LEN		32

#define SDW_CADENCE_MCP_IP_OFFSET	0x4000

 
struct sdw_cdns_pdi {
	int num;
	int intel_alh_id;
	int l_ch_num;
	int h_ch_num;
	int ch_count;
	enum sdw_data_direction dir;
	enum sdw_stream_type type;
};

 
struct sdw_cdns_streams {
	unsigned int num_bd;
	unsigned int num_in;
	unsigned int num_out;
	unsigned int num_ch_bd;
	unsigned int num_ch_in;
	unsigned int num_ch_out;
	unsigned int num_pdi;
	struct sdw_cdns_pdi *bd;
	struct sdw_cdns_pdi *in;
	struct sdw_cdns_pdi *out;
};

 
struct sdw_cdns_stream_config {
	unsigned int pcm_bd;
	unsigned int pcm_in;
	unsigned int pcm_out;
};

 
struct sdw_cdns_dai_runtime {
	char *name;
	struct sdw_stream_runtime *stream;
	struct sdw_cdns_pdi *pdi;
	struct sdw_bus *bus;
	enum sdw_stream_type stream_type;
	int link_id;
	bool suspended;
	bool paused;
	int direction;
};

 
struct sdw_cdns {
	struct device *dev;
	struct sdw_bus bus;
	unsigned int instance;

	u32 ip_offset;

	 
	u32 response_buf[CDNS_MCP_IP_MAX_CMD_LEN + 2];

	struct completion tx_complete;

	struct sdw_cdns_port *ports;
	int num_ports;

	struct sdw_cdns_streams pcm;

	int pdi_loopback_source;
	int pdi_loopback_target;

	void __iomem *registers;

	bool link_up;
	unsigned int msg_count;
	bool interrupt_enabled;

	struct work_struct work;

	struct list_head list;

	struct sdw_cdns_dai_runtime **dai_runtime_array;
};

#define bus_to_cdns(_bus) container_of(_bus, struct sdw_cdns, bus)

 

int sdw_cdns_probe(struct sdw_cdns *cdns);

irqreturn_t sdw_cdns_irq(int irq, void *dev_id);
irqreturn_t sdw_cdns_thread(int irq, void *dev_id);

int sdw_cdns_init(struct sdw_cdns *cdns);
int sdw_cdns_pdi_init(struct sdw_cdns *cdns,
		      struct sdw_cdns_stream_config config);
int sdw_cdns_exit_reset(struct sdw_cdns *cdns);
int sdw_cdns_enable_interrupt(struct sdw_cdns *cdns, bool state);

bool sdw_cdns_is_clock_stop(struct sdw_cdns *cdns);
int sdw_cdns_clock_stop(struct sdw_cdns *cdns, bool block_wake);
int sdw_cdns_clock_restart(struct sdw_cdns *cdns, bool bus_reset);

#ifdef CONFIG_DEBUG_FS
void sdw_cdns_debugfs_init(struct sdw_cdns *cdns, struct dentry *root);
#endif

struct sdw_cdns_pdi *sdw_cdns_alloc_pdi(struct sdw_cdns *cdns,
					struct sdw_cdns_streams *stream,
					u32 ch, u32 dir, int dai_id);
void sdw_cdns_config_stream(struct sdw_cdns *cdns,
			    u32 ch, u32 dir, struct sdw_cdns_pdi *pdi);

enum sdw_command_response
cdns_xfer_msg(struct sdw_bus *bus, struct sdw_msg *msg);

enum sdw_command_response
cdns_xfer_msg_defer(struct sdw_bus *bus);

u32 cdns_read_ping_status(struct sdw_bus *bus);

int cdns_bus_conf(struct sdw_bus *bus, struct sdw_bus_params *params);

int cdns_set_sdw_stream(struct snd_soc_dai *dai,
			void *stream, int direction);

void sdw_cdns_check_self_clearing_bits(struct sdw_cdns *cdns, const char *string,
				       bool initial_delay, int reset_iterations);

void sdw_cdns_config_update(struct sdw_cdns *cdns);
int sdw_cdns_config_update_set_wait(struct sdw_cdns *cdns);

#endif  
