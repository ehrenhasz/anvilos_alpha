 
 

#ifndef __FMAN_PORT_H
#define __FMAN_PORT_H

#include "fman.h"

 

 
 
 
#define FM_PORT_FRM_ERR_UNSUPPORTED_FORMAT	FM_FD_ERR_UNSUPPORTED_FORMAT
 
#define FM_PORT_FRM_ERR_LENGTH			FM_FD_ERR_LENGTH
 
#define FM_PORT_FRM_ERR_DMA			FM_FD_ERR_DMA
 
#define FM_PORT_FRM_ERR_NON_FM			FM_FD_RX_STATUS_ERR_NON_FM
  
#define FM_PORT_FRM_ERR_IPRE			(FM_FD_ERR_IPR & ~FM_FD_IPR)
 
#define FM_PORT_FRM_ERR_IPR_NCSP		(FM_FD_ERR_IPR_NCSP &	\
						~FM_FD_IPR)

 
#define FM_PORT_FRM_ERR_PHYSICAL                FM_FD_ERR_PHYSICAL
 
#define FM_PORT_FRM_ERR_SIZE                    FM_FD_ERR_SIZE
 
#define FM_PORT_FRM_ERR_CLS_DISCARD             FM_FD_ERR_CLS_DISCARD
 
#define FM_PORT_FRM_ERR_EXTRACTION              FM_FD_ERR_EXTRACTION
 
#define FM_PORT_FRM_ERR_NO_SCHEME               FM_FD_ERR_NO_SCHEME
 
#define FM_PORT_FRM_ERR_KEYSIZE_OVERFLOW        FM_FD_ERR_KEYSIZE_OVERFLOW
 
#define FM_PORT_FRM_ERR_COLOR_RED               FM_FD_ERR_COLOR_RED
 
#define FM_PORT_FRM_ERR_COLOR_YELLOW            FM_FD_ERR_COLOR_YELLOW
 
#define FM_PORT_FRM_ERR_PRS_TIMEOUT             FM_FD_ERR_PRS_TIMEOUT
 
#define FM_PORT_FRM_ERR_PRS_ILL_INSTRUCT        FM_FD_ERR_PRS_ILL_INSTRUCT
 
#define FM_PORT_FRM_ERR_PRS_HDR_ERR             FM_FD_ERR_PRS_HDR_ERR
 
#define FM_PORT_FRM_ERR_BLOCK_LIMIT_EXCEEDED    FM_FD_ERR_BLOCK_LIMIT_EXCEEDED
 
#define FM_PORT_FRM_ERR_PROCESS_TIMEOUT         0x00000001

struct fman_port;

 
struct fman_port_rx_params {
	u32 err_fqid;			 
	u32 dflt_fqid;			 
	u32 pcd_base_fqid;		 
	u32 pcd_fqs_count;		 

	 
	struct fman_ext_pools ext_buf_pools;
};

 
struct fman_port_non_rx_params {
	 
	u32 err_fqid;
	 
	u32 dflt_fqid;
};

 
union fman_port_specific_params {
	 
	struct fman_port_rx_params rx_params;
	 
	struct fman_port_non_rx_params non_rx_params;
};

 
struct fman_port_params {
	 
	void *fm;
	union fman_port_specific_params specific_params;
	 
};

int fman_port_config(struct fman_port *port, struct fman_port_params *params);

void fman_port_use_kg_hash(struct fman_port *port, bool enable);

int fman_port_init(struct fman_port *port);

int fman_port_cfg_buf_prefix_content(struct fman_port *port,
				     struct fman_buffer_prefix_content
				     *buffer_prefix_content);

int fman_port_disable(struct fman_port *port);

int fman_port_enable(struct fman_port *port);

u32 fman_port_get_qman_channel_id(struct fman_port *port);

int fman_port_get_hash_result_offset(struct fman_port *port, u32 *offset);

int fman_port_get_tstamp(struct fman_port *port, const void *data, u64 *tstamp);

struct fman_port *fman_port_bind(struct device *dev);

struct device *fman_port_get_device(struct fman_port *port);

#endif  
