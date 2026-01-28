#ifndef DRIVERS_FSI_MASTER_H
#define DRIVERS_FSI_MASTER_H
#include <linux/device.h>
#include <linux/mutex.h>
#define FSI_MMODE		0x0		 
#define FSI_MDLYR		0x4		 
#define FSI_MCRSP		0x8		 
#define FSI_MENP0		0x10		 
#define FSI_MLEVP0		0x18		 
#define FSI_MSENP0		0x18		 
#define FSI_MCENP0		0x20		 
#define FSI_MAEB		0x70		 
#define FSI_MVER		0x74		 
#define FSI_MSTAP0		0xd0		 
#define FSI_MRESP0		0xd0		 
#define FSI_MESRB0		0x1d0		 
#define FSI_MRESB0		0x1d0		 
#define FSI_MSCSB0		0x1d4		 
#define FSI_MATRB0		0x1d8		 
#define FSI_MDTRB0		0x1dc		 
#define FSI_MECTRL		0x2e0		 
#define FSI_MMODE_EIP		0x80000000	 
#define FSI_MMODE_ECRC		0x40000000	 
#define FSI_MMODE_RELA		0x20000000	 
#define FSI_MMODE_EPC		0x10000000	 
#define FSI_MMODE_P8_TO_LSB	0x00000010	 
#define FSI_MMODE_CRS0SHFT	18		 
#define FSI_MMODE_CRS0MASK	0x3ff		 
#define FSI_MMODE_CRS1SHFT	8		 
#define FSI_MMODE_CRS1MASK	0x3ff		 
#define FSI_MRESB_RST_GEN	0x80000000	 
#define FSI_MRESB_RST_ERR	0x40000000	 
#define FSI_MRESP_RST_ALL_MASTER 0x20000000	 
#define FSI_MRESP_RST_ALL_LINK	0x10000000	 
#define FSI_MRESP_RST_MCR	0x08000000	 
#define FSI_MRESP_RST_PYE	0x04000000	 
#define FSI_MRESP_RST_ALL	0xfc000000	 
#define FSI_MECTRL_EOAE		0x8000		 
#define FSI_MECTRL_P8_AUTO_TERM	0x4000		 
#define FSI_HUB_LINK_OFFSET		0x80000
#define FSI_HUB_LINK_SIZE		0x80000
#define FSI_HUB_MASTER_MAX_LINKS	8
#define	FSI_ECHO_DELAY_CLOCKS	16	 
#define	FSI_SEND_DELAY_CLOCKS	16	 
#define	FSI_PRE_BREAK_CLOCKS	50	 
#define	FSI_BREAK_CLOCKS	256	 
#define	FSI_POST_BREAK_CLOCKS	16000	 
#define	FSI_INIT_CLOCKS		5000	 
#define	FSI_MASTER_DPOLL_CLOCKS	50       
#define	FSI_MASTER_EPOLL_CLOCKS	50       
#define FSI_CRC_ERR_RETRIES	10
#define	FSI_MASTER_MAX_BUSY	200
#define	FSI_MASTER_MTOE_COUNT	1000
#define	FSI_CMD_DPOLL		0x2
#define	FSI_CMD_EPOLL		0x3
#define	FSI_CMD_TERM		0x3f
#define FSI_CMD_ABS_AR		0x4
#define FSI_CMD_REL_AR		0x5
#define FSI_CMD_SAME_AR		0x3	 
#define	FSI_RESP_ACK		0	 
#define	FSI_RESP_BUSY		1	 
#define	FSI_RESP_ERRA		2	 
#define	FSI_RESP_ERRC		3	 
#define	FSI_CRC_SIZE		4
#define FSI_MASTER_FLAG_SWCLOCK		0x1
struct fsi_master {
	struct device	dev;
	int		idx;
	int		n_links;
	int		flags;
	struct mutex	scan_lock;
	int		(*read)(struct fsi_master *, int link, uint8_t id,
				uint32_t addr, void *val, size_t size);
	int		(*write)(struct fsi_master *, int link, uint8_t id,
				uint32_t addr, const void *val, size_t size);
	int		(*term)(struct fsi_master *, int link, uint8_t id);
	int		(*send_break)(struct fsi_master *, int link);
	int		(*link_enable)(struct fsi_master *, int link,
				       bool enable);
	int		(*link_config)(struct fsi_master *, int link,
				       u8 t_send_delay, u8 t_echo_delay);
};
#define to_fsi_master(d) container_of(d, struct fsi_master, dev)
extern int fsi_master_register(struct fsi_master *master);
extern void fsi_master_unregister(struct fsi_master *master);
extern int fsi_master_rescan(struct fsi_master *master);
#endif  
