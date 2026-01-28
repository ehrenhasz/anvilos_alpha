#ifndef __CTUCANFD__
#define __CTUCANFD__
#include <linux/netdevice.h>
#include <linux/can/dev.h>
#include <linux/list.h>
enum ctu_can_fd_can_registers;
struct ctucan_priv {
	struct can_priv can;  
	void __iomem *mem_base;
	u32 (*read_reg)(struct ctucan_priv *priv,
			enum ctu_can_fd_can_registers reg);
	void (*write_reg)(struct ctucan_priv *priv,
			  enum ctu_can_fd_can_registers reg, u32 val);
	unsigned int txb_head;
	unsigned int txb_tail;
	u32 txb_prio;
	unsigned int ntxbufs;
	spinlock_t tx_lock;  
	struct napi_struct napi;
	struct device *dev;
	struct clk *can_clk;
	int irq_flags;
	unsigned long drv_flags;
	u32 rxfrm_first_word;
	struct list_head peers_on_pdev;
};
int ctucan_probe_common(struct device *dev, void __iomem *addr,
			int irq, unsigned int ntxbufs,
			unsigned long can_clk_rate,
			int pm_enable_call,
			void (*set_drvdata_fnc)(struct device *dev,
						struct net_device *ndev));
int ctucan_suspend(struct device *dev) __maybe_unused;
int ctucan_resume(struct device *dev) __maybe_unused;
#endif  
