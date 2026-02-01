 
 

#ifndef PT3_H
#define PT3_H

#include <linux/atomic.h>
#include <linux/types.h>

#include <media/dvb_demux.h>
#include <media/dvb_frontend.h>
#include <media/dmxdev.h>

#include "tc90522.h"
#include "mxl301rf.h"
#include "qm1d1c0042.h"

#define DRV_NAME KBUILD_MODNAME

#define PT3_NUM_FE 4

 
#define REG_VERSION	0x00
#define REG_BUS		0x04
#define REG_SYSTEM_W	0x08
#define REG_SYSTEM_R	0x0c
#define REG_I2C_W	0x10
#define REG_I2C_R	0x14
#define REG_RAM_W	0x18
#define REG_RAM_R	0x1c
#define REG_DMA_BASE	0x40	 
#define OFST_DMA_DESC_L	0x00
#define OFST_DMA_DESC_H	0x04
#define OFST_DMA_CTL	0x08
#define OFST_TS_CTL	0x0c
#define OFST_STATUS	0x10
#define OFST_TS_ERR	0x14

 
#define PT3_I2C_MAX 4091
struct pt3_i2cbuf {
	u8  data[PT3_I2C_MAX];
	u8  tmp;
	u32 num_cmds;
};

 
#define TS_PACKET_SZ  188
 
#define DATA_XFER_SZ   4096
#define DATA_BUF_XFERS 47
 
#define DATA_BUF_SZ    (DATA_BUF_XFERS * DATA_XFER_SZ)
#define MAX_DATA_BUFS  16
#define MIN_DATA_BUFS   2

#define DESCS_IN_PAGE (PAGE_SIZE / sizeof(struct xfer_desc))
#define MAX_NUM_XFERS (MAX_DATA_BUFS * DATA_BUF_XFERS)
#define MAX_DESC_BUFS DIV_ROUND_UP(MAX_NUM_XFERS, DESCS_IN_PAGE)

 
struct xfer_desc {
	u32 addr_l;  
	u32 addr_h;
	u32 size;
	u32 next_l;  
	u32 next_h;
};

 
struct xfer_desc_buffer {
	dma_addr_t b_addr;
	struct xfer_desc *descs;  
};

 
struct dma_data_buffer {
	dma_addr_t b_addr;
	u8 *data;  
};

 
struct pt3_adap_config {
	struct i2c_board_info demod_info;
	struct tc90522_config demod_cfg;

	struct i2c_board_info tuner_info;
	union tuner_config {
		struct qm1d1c0042_config qm1d1c0042;
		struct mxl301rf_config   mxl301rf;
	} tuner_cfg;
	u32 init_freq;
};

struct pt3_adapter {
	struct dvb_adapter  dvb_adap;   
	int adap_idx;

	struct dvb_demux    demux;
	struct dmxdev       dmxdev;
	struct dvb_frontend *fe;
	struct i2c_client   *i2c_demod;
	struct i2c_client   *i2c_tuner;

	 
	struct task_struct *thread;
	int num_feeds;

	bool cur_lna;
	bool cur_lnb;  

	 
	struct dma_data_buffer buffer[MAX_DATA_BUFS];
	int buf_idx;
	int buf_ofs;
	int num_bufs;   
	int num_discard;  

	struct xfer_desc_buffer desc_buf[MAX_DESC_BUFS];
	int num_desc_bufs;   
};


struct pt3_board {
	struct pci_dev *pdev;
	void __iomem *regs[2];
	 

	struct mutex lock;

	 
	int lnb_on_cnt;  

	 
	int lna_on_cnt;  

	int num_bufs;   

	struct i2c_adapter i2c_adap;
	struct pt3_i2cbuf *i2c_buf;

	struct pt3_adapter *adaps[PT3_NUM_FE];
};


 
extern int  pt3_alloc_dmabuf(struct pt3_adapter *adap);
extern void pt3_init_dmabuf(struct pt3_adapter *adap);
extern void pt3_free_dmabuf(struct pt3_adapter *adap);
extern int  pt3_start_dma(struct pt3_adapter *adap);
extern int  pt3_stop_dma(struct pt3_adapter *adap);
extern int  pt3_proc_dma(struct pt3_adapter *adap);

extern int  pt3_i2c_master_xfer(struct i2c_adapter *adap,
				struct i2c_msg *msgs, int num);
extern u32  pt3_i2c_functionality(struct i2c_adapter *adap);
extern void pt3_i2c_reset(struct pt3_board *pt3);
extern int  pt3_init_all_demods(struct pt3_board *pt3);
extern int  pt3_init_all_mxl301rf(struct pt3_board *pt3);
#endif  
