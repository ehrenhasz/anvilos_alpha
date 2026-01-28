#ifndef _CIO_QDIO_H
#define _CIO_QDIO_H
#include <asm/page.h>
#include <asm/schid.h>
#include <asm/debug.h>
#include "chsc.h"
#define QDIO_BUSY_BIT_PATIENCE		(100 << 12)	 
#define QDIO_BUSY_BIT_RETRY_DELAY	10		 
#define QDIO_BUSY_BIT_RETRIES		1000		 
enum qdio_irq_states {
	QDIO_IRQ_STATE_INACTIVE,
	QDIO_IRQ_STATE_ESTABLISHED,
	QDIO_IRQ_STATE_ACTIVE,
	QDIO_IRQ_STATE_STOPPED,
	QDIO_IRQ_STATE_CLEANUP,
	QDIO_IRQ_STATE_ERR,
	NR_QDIO_IRQ_STATES,
};
#define QDIO_DOING_ESTABLISH	1
#define QDIO_DOING_ACTIVATE	2
#define QDIO_DOING_CLEANUP	3
#define SLSB_STATE_NOT_INIT	0x0
#define SLSB_STATE_EMPTY	0x1
#define SLSB_STATE_PRIMED	0x2
#define SLSB_STATE_PENDING	0x3
#define SLSB_STATE_HALTED	0xe
#define SLSB_STATE_ERROR	0xf
#define SLSB_TYPE_INPUT		0x0
#define SLSB_TYPE_OUTPUT	0x20
#define SLSB_OWNER_PROG		0x80
#define SLSB_OWNER_CU		0x40
#define SLSB_P_INPUT_NOT_INIT	\
	(SLSB_OWNER_PROG | SLSB_TYPE_INPUT | SLSB_STATE_NOT_INIT)   
#define SLSB_P_INPUT_ACK	\
	(SLSB_OWNER_PROG | SLSB_TYPE_INPUT | SLSB_STATE_EMPTY)	    
#define SLSB_CU_INPUT_EMPTY	\
	(SLSB_OWNER_CU | SLSB_TYPE_INPUT | SLSB_STATE_EMPTY)	    
#define SLSB_P_INPUT_PRIMED	\
	(SLSB_OWNER_PROG | SLSB_TYPE_INPUT | SLSB_STATE_PRIMED)	    
#define SLSB_P_INPUT_HALTED	\
	(SLSB_OWNER_PROG | SLSB_TYPE_INPUT | SLSB_STATE_HALTED)	    
#define SLSB_P_INPUT_ERROR	\
	(SLSB_OWNER_PROG | SLSB_TYPE_INPUT | SLSB_STATE_ERROR)	    
#define SLSB_P_OUTPUT_NOT_INIT	\
	(SLSB_OWNER_PROG | SLSB_TYPE_OUTPUT | SLSB_STATE_NOT_INIT)  
#define SLSB_P_OUTPUT_EMPTY	\
	(SLSB_OWNER_PROG | SLSB_TYPE_OUTPUT | SLSB_STATE_EMPTY)	    
#define SLSB_P_OUTPUT_PENDING \
	(SLSB_OWNER_PROG | SLSB_TYPE_OUTPUT | SLSB_STATE_PENDING)   
#define SLSB_CU_OUTPUT_PRIMED	\
	(SLSB_OWNER_CU | SLSB_TYPE_OUTPUT | SLSB_STATE_PRIMED)	    
#define SLSB_P_OUTPUT_HALTED	\
	(SLSB_OWNER_PROG | SLSB_TYPE_OUTPUT | SLSB_STATE_HALTED)    
#define SLSB_P_OUTPUT_ERROR	\
	(SLSB_OWNER_PROG | SLSB_TYPE_OUTPUT | SLSB_STATE_ERROR)	    
#define SLSB_ERROR_DURING_LOOKUP  0xff
#define CIW_TYPE_EQUEUE			0x3  
#define CIW_TYPE_AQUEUE			0x4  
#define CHSC_FLAG_QDIO_CAPABILITY	0x80
#define CHSC_FLAG_VALIDITY		0x40
#define QDIO_SIGA_WRITE		0x00
#define QDIO_SIGA_READ		0x01
#define QDIO_SIGA_SYNC		0x02
#define QDIO_SIGA_WRITEM	0x03
#define QDIO_SIGA_WRITEQ	0x04
#define QDIO_SIGA_QEBSM_FLAG	0x80
static inline int do_sqbs(u64 token, unsigned char state, int queue,
			  int *start, int *count)
{
	unsigned long _queuestart = ((unsigned long)queue << 32) | *start;
	unsigned long _ccq = *count;
	asm volatile(
		"	lgr	1,%[token]\n"
		"	.insn	rsy,0xeb000000008a,%[qs],%[ccq],0(%[state])"
		: [ccq] "+&d" (_ccq), [qs] "+&d" (_queuestart)
		: [state] "a" ((unsigned long)state), [token] "d" (token)
		: "memory", "cc", "1");
	*count = _ccq & 0xff;
	*start = _queuestart & 0xff;
	return (_ccq >> 32) & 0xff;
}
static inline int do_eqbs(u64 token, unsigned char *state, int queue,
			  int *start, int *count, int ack)
{
	unsigned long _queuestart = ((unsigned long)queue << 32) | *start;
	unsigned long _state = (unsigned long)ack << 63;
	unsigned long _ccq = *count;
	asm volatile(
		"	lgr	1,%[token]\n"
		"	.insn	rrf,0xb99c0000,%[qs],%[state],%[ccq],0"
		: [ccq] "+&d" (_ccq), [qs] "+&d" (_queuestart),
		  [state] "+&d" (_state)
		: [token] "d" (token)
		: "memory", "cc", "1");
	*count = _ccq & 0xff;
	*start = _queuestart & 0xff;
	*state = _state & 0xff;
	return (_ccq >> 32) & 0xff;
}
struct qdio_irq;
struct qdio_dev_perf_stat {
	unsigned int adapter_int;
	unsigned int qdio_int;
	unsigned int siga_read;
	unsigned int siga_write;
	unsigned int siga_sync;
	unsigned int inbound_call;
	unsigned int stop_polling;
	unsigned int inbound_queue_full;
	unsigned int outbound_call;
	unsigned int outbound_queue_full;
	unsigned int fast_requeue;
	unsigned int target_full;
	unsigned int eqbs;
	unsigned int eqbs_partial;
	unsigned int sqbs;
	unsigned int sqbs_partial;
	unsigned int int_discarded;
} ____cacheline_aligned;
struct qdio_queue_perf_stat {
	unsigned int nr_sbals[8];
	unsigned int nr_sbal_error;
	unsigned int nr_sbal_nop;
	unsigned int nr_sbal_total;
};
enum qdio_irq_poll_states {
	QDIO_IRQ_DISABLED,
};
struct qdio_input_q {
	unsigned int batch_start;
	unsigned int batch_count;
};
struct qdio_output_q {
};
struct qdio_q {
	struct slsb slsb;
	union {
		struct qdio_input_q in;
		struct qdio_output_q out;
	} u;
	int first_to_check;
	atomic_t nr_buf_used;
	u64 timestamp;
	struct qdio_queue_perf_stat q_stats;
	struct qdio_buffer *sbal[QDIO_MAX_BUFFERS_PER_Q] ____cacheline_aligned;
	int nr;
	int mask;
	int is_input_q;
	qdio_handler_t (*handler);
	struct qdio_irq *irq_ptr;
	struct sl *sl;
	struct slib *slib;
} __attribute__ ((aligned(256)));
struct qdio_irq {
	struct qib qib;
	u32 *dsci;		 
	struct ccw_device *cdev;
	struct list_head entry;		 
	struct dentry *debugfs_dev;
	u64 last_data_irq_time;
	unsigned long int_parm;
	struct subchannel_id schid;
	unsigned long sch_token;	 
	enum qdio_irq_states state;
	u8 qdioac1;
	int nr_input_qs;
	int nr_output_qs;
	struct ccw1 *ccw;
	struct qdio_ssqd_desc ssqd_desc;
	void (*orig_handler) (struct ccw_device *, unsigned long, struct irb *);
	qdio_handler_t (*error_handler);
	int perf_stat_enabled;
	struct qdr *qdr;
	unsigned long chsc_page;
	struct qdio_q *input_qs[QDIO_MAX_QUEUES_PER_IRQ];
	struct qdio_q *output_qs[QDIO_MAX_QUEUES_PER_IRQ];
	unsigned int max_input_qs;
	unsigned int max_output_qs;
	void (*irq_poll)(struct ccw_device *cdev, unsigned long data);
	unsigned long poll_state;
	debug_info_t *debug_area;
	struct mutex setup_mutex;
	struct qdio_dev_perf_stat perf_stat;
};
#define queue_type(q)	q->irq_ptr->qib.qfmt
#define SCH_NO(q)	(q->irq_ptr->schid.sch_no)
#define is_thinint_irq(irq) \
	(irq->qib.qfmt == QDIO_IQDIO_QFMT || \
	 css_general_characteristics.aif_osa)
#define qperf(__qdev, __attr)	((__qdev)->perf_stat.(__attr))
#define QDIO_PERF_STAT_INC(__irq, __attr)				\
({									\
	struct qdio_irq *qdev = __irq;					\
	if (qdev->perf_stat_enabled)					\
		(qdev->perf_stat.__attr)++;				\
})
#define qperf_inc(__q, __attr)	QDIO_PERF_STAT_INC((__q)->irq_ptr, __attr)
static inline void account_sbals_error(struct qdio_q *q, int count)
{
	q->q_stats.nr_sbal_error += count;
	q->q_stats.nr_sbal_total += count;
}
static inline int multicast_outbound(struct qdio_q *q)
{
	return (q->irq_ptr->nr_output_qs > 1) &&
	       (q->nr == q->irq_ptr->nr_output_qs - 1);
}
static inline void qdio_deliver_irq(struct qdio_irq *irq)
{
	if (!test_and_set_bit(QDIO_IRQ_DISABLED, &irq->poll_state))
		irq->irq_poll(irq->cdev, irq->int_parm);
	else
		QDIO_PERF_STAT_INC(irq, int_discarded);
}
#define pci_out_supported(irq) ((irq)->qib.ac & QIB_AC_OUTBOUND_PCI_SUPPORTED)
#define is_qebsm(q)			(q->irq_ptr->sch_token != 0)
#define qdio_need_siga_in(irq)		((irq)->qdioac1 & AC1_SIGA_INPUT_NEEDED)
#define qdio_need_siga_out(irq)		((irq)->qdioac1 & AC1_SIGA_OUTPUT_NEEDED)
#define qdio_need_siga_sync(irq)	(unlikely((irq)->qdioac1 & AC1_SIGA_SYNC_NEEDED))
#define for_each_input_queue(irq_ptr, q, i)		\
	for (i = 0; i < irq_ptr->nr_input_qs &&		\
		({ q = irq_ptr->input_qs[i]; 1; }); i++)
#define for_each_output_queue(irq_ptr, q, i)		\
	for (i = 0; i < irq_ptr->nr_output_qs &&	\
		({ q = irq_ptr->output_qs[i]; 1; }); i++)
#define add_buf(bufnr, inc)	QDIO_BUFNR((bufnr) + (inc))
#define next_buf(bufnr)		add_buf(bufnr, 1)
#define sub_buf(bufnr, dec)	QDIO_BUFNR((bufnr) - (dec))
#define prev_buf(bufnr)		sub_buf(bufnr, 1)
extern u64 last_ai_time;
int qdio_establish_thinint(struct qdio_irq *irq_ptr);
void qdio_shutdown_thinint(struct qdio_irq *irq_ptr);
int qdio_thinint_init(void);
void qdio_thinint_exit(void);
int test_nonshared_ind(struct qdio_irq *);
void qdio_int_handler(struct ccw_device *cdev, unsigned long intparm,
		      struct irb *irb);
int qdio_allocate_qs(struct qdio_irq *irq_ptr, int nr_input_qs,
		     int nr_output_qs);
void qdio_setup_ssqd_info(struct qdio_irq *irq_ptr);
int qdio_setup_get_ssqd(struct qdio_irq *irq_ptr,
			struct subchannel_id *schid,
			struct qdio_ssqd_desc *data);
void qdio_setup_irq(struct qdio_irq *irq_ptr, struct qdio_initialize *init_data);
void qdio_shutdown_irq(struct qdio_irq *irq);
void qdio_print_subchannel_info(struct qdio_irq *irq_ptr);
void qdio_free_queues(struct qdio_irq *irq_ptr);
int qdio_setup_init(void);
void qdio_setup_exit(void);
int debug_get_buf_state(struct qdio_q *q, unsigned int bufnr,
			unsigned char *state);
#endif  
