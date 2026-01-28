#ifndef __MUSB_DMA_H__
#define __MUSB_DMA_H__
struct musb_hw_ep;
#define MUSB_HSDMA_BASE		0x200
#define MUSB_HSDMA_INTR		(MUSB_HSDMA_BASE + 0)
#define MUSB_HSDMA_CONTROL	0x4
#define MUSB_HSDMA_ADDRESS	0x8
#define MUSB_HSDMA_COUNT	0xc
#define	DMA_ADDR_INVALID	(~(dma_addr_t)0)
#ifdef CONFIG_MUSB_PIO_ONLY
#define	is_dma_capable()	(0)
#else
#define	is_dma_capable()	(1)
#endif
#ifdef CONFIG_USB_UX500_DMA
#define musb_dma_ux500(musb)		(musb->ops->quirks & MUSB_DMA_UX500)
#else
#define musb_dma_ux500(musb)		0
#endif
#ifdef CONFIG_USB_TI_CPPI41_DMA
#define musb_dma_cppi41(musb)		(musb->ops->quirks & MUSB_DMA_CPPI41)
#else
#define musb_dma_cppi41(musb)		0
#endif
#ifdef CONFIG_USB_TUSB_OMAP_DMA
#define tusb_dma_omap(musb)		(musb->ops->quirks & MUSB_DMA_TUSB_OMAP)
#else
#define tusb_dma_omap(musb)		0
#endif
#ifdef CONFIG_USB_INVENTRA_DMA
#define musb_dma_inventra(musb)		(musb->ops->quirks & MUSB_DMA_INVENTRA)
#else
#define musb_dma_inventra(musb)		0
#endif
#if defined(CONFIG_USB_TI_CPPI41_DMA)
#define	is_cppi_enabled(musb)		musb_dma_cppi41(musb)
#else
#define	is_cppi_enabled(musb)		0
#endif
enum dma_channel_status {
	MUSB_DMA_STATUS_UNKNOWN,
	MUSB_DMA_STATUS_FREE,
	MUSB_DMA_STATUS_BUSY,
	MUSB_DMA_STATUS_BUS_ABORT,
	MUSB_DMA_STATUS_CORE_ABORT
};
struct dma_controller;
struct dma_channel {
	void			*private_data;
	size_t			max_len;
	size_t			actual_len;
	enum dma_channel_status	status;
	bool			desired_mode;
	bool			rx_packet_done;
};
static inline enum dma_channel_status
dma_channel_status(struct dma_channel *c)
{
	return (is_dma_capable() && c) ? c->status : MUSB_DMA_STATUS_UNKNOWN;
}
struct dma_controller {
	struct musb *musb;
	struct dma_channel	*(*channel_alloc)(struct dma_controller *,
					struct musb_hw_ep *, u8 is_tx);
	void			(*channel_release)(struct dma_channel *);
	int			(*channel_program)(struct dma_channel *channel,
							u16 maxpacket, u8 mode,
							dma_addr_t dma_addr,
							u32 length);
	int			(*channel_abort)(struct dma_channel *);
	int			(*is_compatible)(struct dma_channel *channel,
							u16 maxpacket,
							void *buf, u32 length);
	void			(*dma_callback)(struct dma_controller *);
};
extern void musb_dma_completion(struct musb *musb, u8 epnum, u8 transmit);
#ifdef CONFIG_MUSB_PIO_ONLY
static inline struct dma_controller *
musb_dma_controller_create(struct musb *m, void __iomem *io)
{
	return NULL;
}
static inline void musb_dma_controller_destroy(struct dma_controller *d) { }
#else
extern struct dma_controller *
(*musb_dma_controller_create)(struct musb *, void __iomem *);
extern void (*musb_dma_controller_destroy)(struct dma_controller *);
#endif
extern struct dma_controller *
musbhs_dma_controller_create(struct musb *musb, void __iomem *base);
extern void musbhs_dma_controller_destroy(struct dma_controller *c);
extern struct dma_controller *
musbhs_dma_controller_create_noirq(struct musb *musb, void __iomem *base);
extern irqreturn_t dma_controller_irq(int irq, void *private_data);
extern struct dma_controller *
tusb_dma_controller_create(struct musb *musb, void __iomem *base);
extern void tusb_dma_controller_destroy(struct dma_controller *c);
extern struct dma_controller *
cppi41_dma_controller_create(struct musb *musb, void __iomem *base);
extern void cppi41_dma_controller_destroy(struct dma_controller *c);
extern struct dma_controller *
ux500_dma_controller_create(struct musb *musb, void __iomem *base);
extern void ux500_dma_controller_destroy(struct dma_controller *c);
#endif	 
