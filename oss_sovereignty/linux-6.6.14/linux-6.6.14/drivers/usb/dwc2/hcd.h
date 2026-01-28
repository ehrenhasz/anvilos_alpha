#ifndef __DWC2_HCD_H__
#define __DWC2_HCD_H__
struct dwc2_qh;
struct dwc2_host_chan {
	u8 hc_num;
	unsigned dev_addr:7;
	unsigned ep_num:4;
	unsigned ep_is_in:1;
	unsigned speed:4;
	unsigned ep_type:2;
	unsigned max_packet:11;
	unsigned data_pid_start:2;
#define DWC2_HC_PID_DATA0	TSIZ_SC_MC_PID_DATA0
#define DWC2_HC_PID_DATA2	TSIZ_SC_MC_PID_DATA2
#define DWC2_HC_PID_DATA1	TSIZ_SC_MC_PID_DATA1
#define DWC2_HC_PID_MDATA	TSIZ_SC_MC_PID_MDATA
#define DWC2_HC_PID_SETUP	TSIZ_SC_MC_PID_SETUP
	unsigned multi_count:2;
	u8 *xfer_buf;
	dma_addr_t xfer_dma;
	dma_addr_t align_buf;
	u32 xfer_len;
	u32 xfer_count;
	u16 start_pkt_count;
	u8 xfer_started;
	u8 do_ping;
	u8 error_state;
	u8 halt_on_queue;
	u8 halt_pending;
	u8 do_split;
	u8 complete_split;
	u8 hub_addr;
	u8 hub_port;
	u8 xact_pos;
#define DWC2_HCSPLT_XACTPOS_MID	HCSPLT_XACTPOS_MID
#define DWC2_HCSPLT_XACTPOS_END	HCSPLT_XACTPOS_END
#define DWC2_HCSPLT_XACTPOS_BEGIN HCSPLT_XACTPOS_BEGIN
#define DWC2_HCSPLT_XACTPOS_ALL	HCSPLT_XACTPOS_ALL
	u8 requests;
	u8 schinfo;
	u16 ntd;
	enum dwc2_halt_status halt_status;
	u32 hcint;
	struct dwc2_qh *qh;
	struct list_head hc_list_entry;
	dma_addr_t desc_list_addr;
	u32 desc_list_sz;
	struct list_head split_order_list_entry;
};
struct dwc2_hcd_pipe_info {
	u8 dev_addr;
	u8 ep_num;
	u8 pipe_type;
	u8 pipe_dir;
	u16 maxp;
	u16 maxp_mult;
};
struct dwc2_hcd_iso_packet_desc {
	u32 offset;
	u32 length;
	u32 actual_length;
	u32 status;
};
struct dwc2_qtd;
struct dwc2_hcd_urb {
	void *priv;
	struct dwc2_qtd *qtd;
	void *buf;
	dma_addr_t dma;
	void *setup_packet;
	dma_addr_t setup_dma;
	u32 length;
	u32 actual_length;
	u32 status;
	u32 error_count;
	u32 packet_count;
	u32 flags;
	u16 interval;
	struct dwc2_hcd_pipe_info pipe_info;
	struct dwc2_hcd_iso_packet_desc iso_descs[];
};
enum dwc2_control_phase {
	DWC2_CONTROL_SETUP,
	DWC2_CONTROL_DATA,
	DWC2_CONTROL_STATUS,
};
enum dwc2_transaction_type {
	DWC2_TRANSACTION_NONE,
	DWC2_TRANSACTION_PERIODIC,
	DWC2_TRANSACTION_NON_PERIODIC,
	DWC2_TRANSACTION_ALL,
};
#define DWC2_ELEMENTS_PER_LS_BITMAP	DIV_ROUND_UP(DWC2_LS_SCHEDULE_SLICES, \
						     BITS_PER_LONG)
struct dwc2_tt {
	int refcount;
	struct usb_tt *usb_tt;
	unsigned long periodic_bitmaps[];
};
struct dwc2_hs_transfer_time {
	u32 start_schedule_us;
	u16 duration_us;
};
struct dwc2_qh {
	struct dwc2_hsotg *hsotg;
	u8 ep_type;
	u8 ep_is_in;
	u16 maxp;
	u16 maxp_mult;
	u8 dev_speed;
	u8 data_toggle;
	u8 ping_state;
	u8 do_split;
	u8 td_first;
	u8 td_last;
	u16 host_us;
	u16 device_us;
	u16 host_interval;
	u16 device_interval;
	u16 next_active_frame;
	u16 start_active_frame;
	s16 num_hs_transfers;
	struct dwc2_hs_transfer_time hs_transfers[DWC2_HS_SCHEDULE_UFRAMES];
	u32 ls_start_schedule_slice;
	u16 ntd;
	u8 *dw_align_buf;
	dma_addr_t dw_align_buf_dma;
	struct list_head qtd_list;
	struct dwc2_host_chan *channel;
	struct list_head qh_list_entry;
	struct dwc2_dma_desc *desc_list;
	dma_addr_t desc_list_dma;
	u32 desc_list_sz;
	u32 *n_bytes;
	struct timer_list unreserve_timer;
	struct hrtimer wait_timer;
	struct dwc2_tt *dwc_tt;
	int ttport;
	unsigned tt_buffer_dirty:1;
	unsigned unreserve_pending:1;
	unsigned schedule_low_speed:1;
	unsigned want_wait:1;
	unsigned wait_timer_cancel:1;
};
struct dwc2_qtd {
	enum dwc2_control_phase control_phase;
	u8 in_process;
	u8 data_toggle;
	u8 complete_split;
	u8 isoc_split_pos;
	u16 isoc_frame_index;
	u16 isoc_split_offset;
	u16 isoc_td_last;
	u16 isoc_td_first;
	u32 ssplit_out_xfer_count;
	u8 error_count;
	u8 n_desc;
	u16 isoc_frame_index_last;
	u16 num_naks;
	struct dwc2_hcd_urb *urb;
	struct dwc2_qh *qh;
	struct list_head qtd_list_entry;
};
#ifdef DEBUG
struct hc_xfer_info {
	struct dwc2_hsotg *hsotg;
	struct dwc2_host_chan *chan;
};
#endif
u32 dwc2_calc_frame_interval(struct dwc2_hsotg *hsotg);
static inline struct usb_hcd *dwc2_hsotg_to_hcd(struct dwc2_hsotg *hsotg)
{
	return (struct usb_hcd *)hsotg->priv;
}
static inline void disable_hc_int(struct dwc2_hsotg *hsotg, int chnum, u32 intr)
{
	u32 mask = dwc2_readl(hsotg, HCINTMSK(chnum));
	mask &= ~intr;
	dwc2_writel(hsotg, mask, HCINTMSK(chnum));
}
void dwc2_hc_cleanup(struct dwc2_hsotg *hsotg, struct dwc2_host_chan *chan);
void dwc2_hc_halt(struct dwc2_hsotg *hsotg, struct dwc2_host_chan *chan,
		  enum dwc2_halt_status halt_status);
void dwc2_hc_start_transfer_ddma(struct dwc2_hsotg *hsotg,
				 struct dwc2_host_chan *chan);
static inline u32 dwc2_read_hprt0(struct dwc2_hsotg *hsotg)
{
	u32 hprt0 = dwc2_readl(hsotg, HPRT0);
	hprt0 &= ~(HPRT0_ENA | HPRT0_CONNDET | HPRT0_ENACHG | HPRT0_OVRCURRCHG);
	return hprt0;
}
static inline u8 dwc2_hcd_get_ep_num(struct dwc2_hcd_pipe_info *pipe)
{
	return pipe->ep_num;
}
static inline u8 dwc2_hcd_get_pipe_type(struct dwc2_hcd_pipe_info *pipe)
{
	return pipe->pipe_type;
}
static inline u16 dwc2_hcd_get_maxp(struct dwc2_hcd_pipe_info *pipe)
{
	return pipe->maxp;
}
static inline u16 dwc2_hcd_get_maxp_mult(struct dwc2_hcd_pipe_info *pipe)
{
	return pipe->maxp_mult;
}
static inline u8 dwc2_hcd_get_dev_addr(struct dwc2_hcd_pipe_info *pipe)
{
	return pipe->dev_addr;
}
static inline u8 dwc2_hcd_is_pipe_isoc(struct dwc2_hcd_pipe_info *pipe)
{
	return pipe->pipe_type == USB_ENDPOINT_XFER_ISOC;
}
static inline u8 dwc2_hcd_is_pipe_int(struct dwc2_hcd_pipe_info *pipe)
{
	return pipe->pipe_type == USB_ENDPOINT_XFER_INT;
}
static inline u8 dwc2_hcd_is_pipe_bulk(struct dwc2_hcd_pipe_info *pipe)
{
	return pipe->pipe_type == USB_ENDPOINT_XFER_BULK;
}
static inline u8 dwc2_hcd_is_pipe_control(struct dwc2_hcd_pipe_info *pipe)
{
	return pipe->pipe_type == USB_ENDPOINT_XFER_CONTROL;
}
static inline u8 dwc2_hcd_is_pipe_in(struct dwc2_hcd_pipe_info *pipe)
{
	return pipe->pipe_dir == USB_DIR_IN;
}
static inline u8 dwc2_hcd_is_pipe_out(struct dwc2_hcd_pipe_info *pipe)
{
	return !dwc2_hcd_is_pipe_in(pipe);
}
int dwc2_hcd_init(struct dwc2_hsotg *hsotg);
void dwc2_hcd_remove(struct dwc2_hsotg *hsotg);
enum dwc2_transaction_type dwc2_hcd_select_transactions(
						struct dwc2_hsotg *hsotg);
void dwc2_hcd_queue_transactions(struct dwc2_hsotg *hsotg,
				 enum dwc2_transaction_type tr_type);
struct dwc2_qh *dwc2_hcd_qh_create(struct dwc2_hsotg *hsotg,
				   struct dwc2_hcd_urb *urb,
					  gfp_t mem_flags);
void dwc2_hcd_qh_free(struct dwc2_hsotg *hsotg, struct dwc2_qh *qh);
int dwc2_hcd_qh_add(struct dwc2_hsotg *hsotg, struct dwc2_qh *qh);
void dwc2_hcd_qh_unlink(struct dwc2_hsotg *hsotg, struct dwc2_qh *qh);
void dwc2_hcd_qh_deactivate(struct dwc2_hsotg *hsotg, struct dwc2_qh *qh,
			    int sched_csplit);
void dwc2_hcd_qtd_init(struct dwc2_qtd *qtd, struct dwc2_hcd_urb *urb);
int dwc2_hcd_qtd_add(struct dwc2_hsotg *hsotg, struct dwc2_qtd *qtd,
		     struct dwc2_qh *qh);
static inline void dwc2_hcd_qtd_unlink_and_free(struct dwc2_hsotg *hsotg,
						struct dwc2_qtd *qtd,
						struct dwc2_qh *qh)
{
	list_del(&qtd->qtd_list_entry);
	kfree(qtd);
}
void dwc2_hcd_start_xfer_ddma(struct dwc2_hsotg *hsotg,
			      struct dwc2_qh *qh);
void dwc2_hcd_complete_xfer_ddma(struct dwc2_hsotg *hsotg,
				 struct dwc2_host_chan *chan, int chnum,
					enum dwc2_halt_status halt_status);
int dwc2_hcd_qh_init_ddma(struct dwc2_hsotg *hsotg, struct dwc2_qh *qh,
			  gfp_t mem_flags);
void dwc2_hcd_qh_free_ddma(struct dwc2_hsotg *hsotg, struct dwc2_qh *qh);
#define dwc2_qh_is_non_per(_qh_ptr_) \
	((_qh_ptr_)->ep_type == USB_ENDPOINT_XFER_BULK || \
	 (_qh_ptr_)->ep_type == USB_ENDPOINT_XFER_CONTROL)
#ifdef CONFIG_USB_DWC2_DEBUG_PERIODIC
static inline bool dbg_hc(struct dwc2_host_chan *hc) { return true; }
static inline bool dbg_qh(struct dwc2_qh *qh) { return true; }
static inline bool dbg_urb(struct urb *urb) { return true; }
static inline bool dbg_perio(void) { return true; }
#else  
static inline bool dbg_hc(struct dwc2_host_chan *hc)
{
	return hc->ep_type == USB_ENDPOINT_XFER_BULK ||
	       hc->ep_type == USB_ENDPOINT_XFER_CONTROL;
}
static inline bool dbg_qh(struct dwc2_qh *qh)
{
	return qh->ep_type == USB_ENDPOINT_XFER_BULK ||
	       qh->ep_type == USB_ENDPOINT_XFER_CONTROL;
}
static inline bool dbg_urb(struct urb *urb)
{
	return usb_pipetype(urb->pipe) == PIPE_BULK ||
	       usb_pipetype(urb->pipe) == PIPE_CONTROL;
}
static inline bool dbg_perio(void) { return false; }
#endif
static inline bool dwc2_frame_idx_num_gt(u16 fr_idx1, u16 fr_idx2)
{
	u16 diff = fr_idx1 - fr_idx2;
	u16 sign = diff & (FRLISTEN_64_SIZE >> 1);
	return diff && !sign;
}
static inline int dwc2_frame_num_le(u16 frame1, u16 frame2)
{
	return ((frame2 - frame1) & HFNUM_MAX_FRNUM) <= (HFNUM_MAX_FRNUM >> 1);
}
static inline int dwc2_frame_num_gt(u16 frame1, u16 frame2)
{
	return (frame1 != frame2) &&
	       ((frame1 - frame2) & HFNUM_MAX_FRNUM) < (HFNUM_MAX_FRNUM >> 1);
}
static inline u16 dwc2_frame_num_inc(u16 frame, u16 inc)
{
	return (frame + inc) & HFNUM_MAX_FRNUM;
}
static inline u16 dwc2_frame_num_dec(u16 frame, u16 dec)
{
	return (frame + HFNUM_MAX_FRNUM + 1 - dec) & HFNUM_MAX_FRNUM;
}
static inline u16 dwc2_full_frame_num(u16 frame)
{
	return (frame & HFNUM_MAX_FRNUM) >> 3;
}
static inline u16 dwc2_micro_frame_num(u16 frame)
{
	return frame & 0x7;
}
static inline u32 dwc2_read_core_intr(struct dwc2_hsotg *hsotg)
{
	return dwc2_readl(hsotg, GINTSTS) &
	       dwc2_readl(hsotg, GINTMSK);
}
static inline u32 dwc2_hcd_urb_get_status(struct dwc2_hcd_urb *dwc2_urb)
{
	return dwc2_urb->status;
}
static inline u32 dwc2_hcd_urb_get_actual_length(
		struct dwc2_hcd_urb *dwc2_urb)
{
	return dwc2_urb->actual_length;
}
static inline u32 dwc2_hcd_urb_get_error_count(struct dwc2_hcd_urb *dwc2_urb)
{
	return dwc2_urb->error_count;
}
static inline void dwc2_hcd_urb_set_iso_desc_params(
		struct dwc2_hcd_urb *dwc2_urb, int desc_num, u32 offset,
		u32 length)
{
	dwc2_urb->iso_descs[desc_num].offset = offset;
	dwc2_urb->iso_descs[desc_num].length = length;
}
static inline u32 dwc2_hcd_urb_get_iso_desc_status(
		struct dwc2_hcd_urb *dwc2_urb, int desc_num)
{
	return dwc2_urb->iso_descs[desc_num].status;
}
static inline u32 dwc2_hcd_urb_get_iso_desc_actual_length(
		struct dwc2_hcd_urb *dwc2_urb, int desc_num)
{
	return dwc2_urb->iso_descs[desc_num].actual_length;
}
static inline int dwc2_hcd_is_bandwidth_allocated(struct dwc2_hsotg *hsotg,
						  struct usb_host_endpoint *ep)
{
	struct dwc2_qh *qh = ep->hcpriv;
	if (qh && !list_empty(&qh->qh_list_entry))
		return 1;
	return 0;
}
static inline u16 dwc2_hcd_get_ep_bandwidth(struct dwc2_hsotg *hsotg,
					    struct usb_host_endpoint *ep)
{
	struct dwc2_qh *qh = ep->hcpriv;
	if (!qh) {
		WARN_ON(1);
		return 0;
	}
	return qh->host_us;
}
void dwc2_hcd_save_data_toggle(struct dwc2_hsotg *hsotg,
			       struct dwc2_host_chan *chan, int chnum,
				      struct dwc2_qtd *qtd);
irqreturn_t dwc2_handle_hcd_intr(struct dwc2_hsotg *hsotg);
void dwc2_hcd_stop(struct dwc2_hsotg *hsotg);
int dwc2_hcd_is_b_host(struct dwc2_hsotg *hsotg);
void dwc2_hcd_dump_state(struct dwc2_hsotg *hsotg);
#define URB_GIVEBACK_ASAP	0x1
#define URB_SEND_ZERO_PACKET	0x2
struct dwc2_tt *dwc2_host_get_tt_info(struct dwc2_hsotg *hsotg,
				      void *context, gfp_t mem_flags,
				      int *ttport);
void dwc2_host_put_tt_info(struct dwc2_hsotg *hsotg,
			   struct dwc2_tt *dwc_tt);
int dwc2_host_get_speed(struct dwc2_hsotg *hsotg, void *context);
void dwc2_host_complete(struct dwc2_hsotg *hsotg, struct dwc2_qtd *qtd,
			int status);
#endif  
