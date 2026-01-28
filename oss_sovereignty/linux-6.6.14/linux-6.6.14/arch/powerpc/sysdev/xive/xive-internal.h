#ifndef __XIVE_INTERNAL_H
#define __XIVE_INTERNAL_H
#define XIVE_BAD_IRQ		0x7fffffff
#define XIVE_MAX_IRQ		(XIVE_BAD_IRQ - 1)
struct xive_cpu {
#ifdef CONFIG_SMP
	u32 hw_ipi;
	struct xive_irq_data ipi_data;
#endif  
	int chip_id;
#define XIVE_MAX_QUEUES	8
	struct xive_q queue[XIVE_MAX_QUEUES];
	u8 pending_prio;
	u8 cppr;
};
struct xive_ops {
	int	(*populate_irq_data)(u32 hw_irq, struct xive_irq_data *data);
	int 	(*configure_irq)(u32 hw_irq, u32 target, u8 prio, u32 sw_irq);
	int	(*get_irq_config)(u32 hw_irq, u32 *target, u8 *prio,
				  u32 *sw_irq);
	int	(*setup_queue)(unsigned int cpu, struct xive_cpu *xc, u8 prio);
	void	(*cleanup_queue)(unsigned int cpu, struct xive_cpu *xc, u8 prio);
	void	(*prepare_cpu)(unsigned int cpu, struct xive_cpu *xc);
	void	(*setup_cpu)(unsigned int cpu, struct xive_cpu *xc);
	void	(*teardown_cpu)(unsigned int cpu, struct xive_cpu *xc);
	bool	(*match)(struct device_node *np);
	void	(*shutdown)(void);
	void	(*update_pending)(struct xive_cpu *xc);
	void	(*sync_source)(u32 hw_irq);
	u64	(*esb_rw)(u32 hw_irq, u32 offset, u64 data, bool write);
#ifdef CONFIG_SMP
	int	(*get_ipi)(unsigned int cpu, struct xive_cpu *xc);
	void	(*put_ipi)(unsigned int cpu, struct xive_cpu *xc);
#endif
	int	(*debug_show)(struct seq_file *m, void *private);
	int	(*debug_create)(struct dentry *xive_dir);
	const char *name;
};
bool xive_core_init(struct device_node *np, const struct xive_ops *ops,
		    void __iomem *area, u32 offset, u8 max_prio);
__be32 *xive_queue_page_alloc(unsigned int cpu, u32 queue_shift);
int xive_core_debug_init(void);
static inline u32 xive_alloc_order(u32 queue_shift)
{
	return (queue_shift > PAGE_SHIFT) ? (queue_shift - PAGE_SHIFT) : 0;
}
extern bool xive_cmdline_disabled;
extern bool xive_has_save_restore;
#endif  
