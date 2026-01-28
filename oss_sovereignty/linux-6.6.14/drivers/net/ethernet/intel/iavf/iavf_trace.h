#undef TRACE_SYSTEM
#define TRACE_SYSTEM iavf
#if !defined(_IAVF_TRACE_H_) || defined(TRACE_HEADER_MULTI_READ)
#define _IAVF_TRACE_H_
#include <linux/tracepoint.h>
#define _IAVF_TRACE_NAME(trace_name) (trace_ ## iavf ## _ ## trace_name)
#define IAVF_TRACE_NAME(trace_name) _IAVF_TRACE_NAME(trace_name)
#define iavf_trace(trace_name, args...) IAVF_TRACE_NAME(trace_name)(args)
#define iavf_trace_enabled(trace_name) IAVF_TRACE_NAME(trace_name##_enabled)()
DECLARE_EVENT_CLASS(
	iavf_tx_template,
	TP_PROTO(struct iavf_ring *ring,
		 struct iavf_tx_desc *desc,
		 struct iavf_tx_buffer *buf),
	TP_ARGS(ring, desc, buf),
	TP_STRUCT__entry(
		__field(void*, ring)
		__field(void*, desc)
		__field(void*, buf)
		__string(devname, ring->netdev->name)
	),
	TP_fast_assign(
		__entry->ring = ring;
		__entry->desc = desc;
		__entry->buf = buf;
		__assign_str(devname, ring->netdev->name);
	),
	TP_printk(
		"netdev: %s ring: %p desc: %p buf %p",
		__get_str(devname), __entry->ring,
		__entry->desc, __entry->buf)
);
DEFINE_EVENT(
	iavf_tx_template, iavf_clean_tx_irq,
	TP_PROTO(struct iavf_ring *ring,
		 struct iavf_tx_desc *desc,
		 struct iavf_tx_buffer *buf),
	TP_ARGS(ring, desc, buf));
DEFINE_EVENT(
	iavf_tx_template, iavf_clean_tx_irq_unmap,
	TP_PROTO(struct iavf_ring *ring,
		 struct iavf_tx_desc *desc,
		 struct iavf_tx_buffer *buf),
	TP_ARGS(ring, desc, buf));
DECLARE_EVENT_CLASS(
	iavf_rx_template,
	TP_PROTO(struct iavf_ring *ring,
		 union iavf_32byte_rx_desc *desc,
		 struct sk_buff *skb),
	TP_ARGS(ring, desc, skb),
	TP_STRUCT__entry(
		__field(void*, ring)
		__field(void*, desc)
		__field(void*, skb)
		__string(devname, ring->netdev->name)
	),
	TP_fast_assign(
		__entry->ring = ring;
		__entry->desc = desc;
		__entry->skb = skb;
		__assign_str(devname, ring->netdev->name);
	),
	TP_printk(
		"netdev: %s ring: %p desc: %p skb %p",
		__get_str(devname), __entry->ring,
		__entry->desc, __entry->skb)
);
DEFINE_EVENT(
	iavf_rx_template, iavf_clean_rx_irq,
	TP_PROTO(struct iavf_ring *ring,
		 union iavf_32byte_rx_desc *desc,
		 struct sk_buff *skb),
	TP_ARGS(ring, desc, skb));
DEFINE_EVENT(
	iavf_rx_template, iavf_clean_rx_irq_rx,
	TP_PROTO(struct iavf_ring *ring,
		 union iavf_32byte_rx_desc *desc,
		 struct sk_buff *skb),
	TP_ARGS(ring, desc, skb));
DECLARE_EVENT_CLASS(
	iavf_xmit_template,
	TP_PROTO(struct sk_buff *skb,
		 struct iavf_ring *ring),
	TP_ARGS(skb, ring),
	TP_STRUCT__entry(
		__field(void*, skb)
		__field(void*, ring)
		__string(devname, ring->netdev->name)
	),
	TP_fast_assign(
		__entry->skb = skb;
		__entry->ring = ring;
		__assign_str(devname, ring->netdev->name);
	),
	TP_printk(
		"netdev: %s skb: %p ring: %p",
		__get_str(devname), __entry->skb,
		__entry->ring)
);
DEFINE_EVENT(
	iavf_xmit_template, iavf_xmit_frame_ring,
	TP_PROTO(struct sk_buff *skb,
		 struct iavf_ring *ring),
	TP_ARGS(skb, ring));
DEFINE_EVENT(
	iavf_xmit_template, iavf_xmit_frame_ring_drop,
	TP_PROTO(struct sk_buff *skb,
		 struct iavf_ring *ring),
	TP_ARGS(skb, ring));
#endif  
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE iavf_trace
#include <trace/define_trace.h>
