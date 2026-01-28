#ifndef __ASM_POWERPC_MPC5121_H__
#define __ASM_POWERPC_MPC5121_H__
struct mpc512x_reset_module {
	u32	rcwlr;	 
	u32	rcwhr;	 
	u32	reserved1;
	u32	reserved2;
	u32	rsr;	 
	u32	rmr;	 
	u32	rpr;	 
	u32	rcr;	 
	u32	rcer;	 
};
struct mpc512x_ccm {
	u32	spmr;	 
	u32	sccr1;	 
	u32	sccr2;	 
	u32	scfr1;	 
	u32	scfr2;	 
	u32	scfr2s;	 
	u32	bcr;	 
	u32	psc_ccr[12];	 
	u32	spccr;	 
	u32	cccr;	 
	u32	dccr;	 
	u32	mscan_ccr[4];	 
	u32	out_ccr[4];	 
	u32	rsv0[2];	 
	u32	scfr3;		 
	u32	rsv1[3];	 
	u32	spll_lock_cnt;	 
	u8	res[0x6c];	 
};
struct mpc512x_lpc {
	u32	cs_cfg[8];	 
	u32	cs_ctrl;	 
	u32	cs_status;	 
	u32	burst_ctrl;	 
	u32	deadcycle_ctrl;	 
	u32	holdcycle_ctrl;	 
	u32	alt;		 
};
int mpc512x_cs_config(unsigned int cs, u32 val);
struct mpc512x_lpbfifo {
	u32	pkt_size;	 
	u32	start_addr;	 
	u32	ctrl;		 
	u32	enable;		 
	u32	reserved1;
	u32	status;		 
	u32	bytes_done;	 
	u32	emb_sc;		 
	u32	emb_pc;		 
	u32	reserved2[7];
	u32	data_word;	 
	u32	fifo_status;	 
	u32	fifo_ctrl;	 
	u32	fifo_alarm;	 
};
#define MPC512X_SCLPC_START		(1 << 31)
#define MPC512X_SCLPC_CS(x)		(((x) & 0x7) << 24)
#define MPC512X_SCLPC_FLUSH		(1 << 17)
#define MPC512X_SCLPC_READ		(1 << 16)
#define MPC512X_SCLPC_DAI		(1 << 8)
#define MPC512X_SCLPC_BPT(x)		((x) & 0x3f)
#define MPC512X_SCLPC_RESET		(1 << 24)
#define MPC512X_SCLPC_FIFO_RESET	(1 << 16)
#define MPC512X_SCLPC_ABORT_INT_ENABLE	(1 << 9)
#define MPC512X_SCLPC_NORM_INT_ENABLE	(1 << 8)
#define MPC512X_SCLPC_ENABLE		(1 << 0)
#define MPC512X_SCLPC_SUCCESS		(1 << 24)
#define MPC512X_SCLPC_FIFO_CTRL(x)	(((x) & 0x7) << 24)
#define MPC512X_SCLPC_FIFO_ALARM(x)	((x) & 0x3ff)
enum lpb_dev_portsize {
	LPB_DEV_PORTSIZE_UNDEFINED = 0,
	LPB_DEV_PORTSIZE_1_BYTE = 1,
	LPB_DEV_PORTSIZE_2_BYTES = 2,
	LPB_DEV_PORTSIZE_4_BYTES = 4,
	LPB_DEV_PORTSIZE_8_BYTES = 8
};
enum mpc512x_lpbfifo_req_dir {
	MPC512X_LPBFIFO_REQ_DIR_READ,
	MPC512X_LPBFIFO_REQ_DIR_WRITE
};
struct mpc512x_lpbfifo_request {
	phys_addr_t dev_phys_addr;  
	void *ram_virt_addr;  
	u32 size;
	enum lpb_dev_portsize portsize;
	enum mpc512x_lpbfifo_req_dir dir;
	void (*callback)(struct mpc512x_lpbfifo_request *);
};
int mpc512x_lpbfifo_submit(struct mpc512x_lpbfifo_request *req);
#endif  
