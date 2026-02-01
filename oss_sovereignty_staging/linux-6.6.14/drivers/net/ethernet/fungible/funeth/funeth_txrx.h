 

#ifndef _FUNETH_TXRX_H
#define _FUNETH_TXRX_H

#include <linux/netdevice.h>
#include <linux/u64_stats_sync.h>
#include <net/xdp.h>

 
#define FUNETH_SQE_SIZE 64U

 
#define FUNETH_FUNOS_HDR_SZ (sizeof(struct fun_eth_tx_req))

 
#define FUNETH_GLE_PER_DESC (FUNETH_SQE_SIZE / sizeof(struct fun_dataop_gl))

 
#define FUNETH_MAX_GL_SZ ((MAX_SKB_FRAGS + 1) * sizeof(struct fun_dataop_gl))

#if IS_ENABLED(CONFIG_TLS_DEVICE)
# define FUNETH_TLS_SZ sizeof(struct fun_eth_tls)
#else
# define FUNETH_TLS_SZ 0
#endif

 
#define FUNETH_MAX_GL_DESC \
	DIV_ROUND_UP((FUNETH_FUNOS_HDR_SZ + FUNETH_MAX_GL_SZ + FUNETH_TLS_SZ), \
		     FUNETH_SQE_SIZE)

 
#define FUNETH_MAX_PKT_DESC FUNETH_MAX_GL_DESC

 
#define FUNETH_CQE_SIZE 64U

 
#define FUNETH_CQE_INFO_OFFSET (FUNETH_CQE_SIZE - sizeof(struct fun_cqe_info))

 
#define FUN_IRQ_CQ_DB(usec, pkts) \
	(FUN_DB_IRQ_ARM_F | ((usec) << FUN_DB_INTCOAL_USEC_S) | \
	 ((pkts) << FUN_DB_INTCOAL_ENTRIES_S))

 
#define FUN_IRQ_SQ_DB(usec, pkts) \
	(FUN_DB_IRQ_ARM_F | \
	 ((usec) << FUN_DB_INTCOAL_USEC_S) | \
	 ((pkts) << FUN_DB_INTCOAL_ENTRIES_S))

 
#define FUN_RX_TAILROOM SKB_DATA_ALIGN(sizeof(struct skb_shared_info))

 
#define FUN_XDP_HEADROOM 192

 
enum {
	FUN_QSTATE_DESTROYED,  
	FUN_QSTATE_INIT_SW,    
	FUN_QSTATE_INIT_FULL,  
};

 
enum {
	FUN_IRQ_INIT,       
	FUN_IRQ_REQUESTED,  
	FUN_IRQ_ENABLED,    
	FUN_IRQ_DISABLED,   
};

struct bpf_prog;

struct funeth_txq_stats {   
	u64 tx_pkts;        
	u64 tx_bytes;       
	u64 tx_cso;         
	u64 tx_tso;         
	u64 tx_encap_tso;   
	u64 tx_uso;         
	u64 tx_more;        
	u64 tx_nstops;      
	u64 tx_nrestarts;   
	u64 tx_map_err;     
	u64 tx_xdp_full;    
	u64 tx_tls_pkts;    
	u64 tx_tls_bytes;   
	u64 tx_tls_fallback;  
	u64 tx_tls_drops;   
};

struct funeth_tx_info {       
	union {
		struct sk_buff *skb;     
		struct xdp_frame *xdpf;  
	};
};

struct funeth_txq {
	 
	u32 mask;                
	u32 hw_qid;              
	void *desc;              
	struct funeth_tx_info *info;
	struct device *dma_dev;  
	volatile __be64 *hw_wb;  
	u32 __iomem *db;         
	struct netdev_queue *ndq;
	dma_addr_t dma_addr;     
	 
	u16 qidx;                
	u16 ethid;
	u32 prod_cnt;            
	struct funeth_txq_stats stats;
	 
	u32 irq_db_val;          
	u32 cons_cnt;            
	struct net_device *netdev;
	struct fun_irq *irq;
	int numa_node;
	u8 init_state;           
	struct u64_stats_sync syncp;
};

struct funeth_rxq_stats {   
	u64 rx_pkts;        
	u64 rx_bytes;       
	u64 rx_cso;         
	u64 rx_bufs;        
	u64 gro_pkts;       
	u64 gro_merged;     
	u64 rx_page_alloc;  
	u64 rx_budget;      
	u64 rx_mem_drops;   
	u64 rx_map_err;     
	u64 xdp_drops;      
	u64 xdp_tx;         
	u64 xdp_redir;      
	u64 xdp_err;        
};

struct funeth_rxbuf {           
	struct page *page;      
	dma_addr_t dma_addr;    
	int pg_refs;            
	int node;               
};

struct funeth_rx_cache {        
	struct funeth_rxbuf *bufs;  
	unsigned int prod_cnt;      
	unsigned int cons_cnt;      
	unsigned int mask;          
};

 
struct funeth_rxq {
	struct net_device *netdev;
	struct napi_struct *napi;
	struct device *dma_dev;     
	void *cqes;                 
	const void *next_cqe_info;  
	u32 __iomem *cq_db;         
	unsigned int cq_head;       
	unsigned int cq_mask;       
	u16 phase;                  
	u16 qidx;                   
	unsigned int irq_db_val;    
	struct fun_eprq_rqbuf *rqes;  
	struct funeth_rxbuf *bufs;  
	struct funeth_rxbuf *cur_buf;  
	u32 __iomem *rq_db;         
	unsigned int rq_cons;       
	unsigned int rq_mask;       
	unsigned int buf_offset;    
	u8 xdp_flush;               
	u8 init_state;              
	u16 headroom;               
	unsigned int rq_cons_db;    
	unsigned int rq_db_thres;   
	struct funeth_rxbuf spare_buf;  
	struct funeth_rx_cache cache;  
	struct bpf_prog *xdp_prog;  
	struct funeth_rxq_stats stats;
	dma_addr_t cq_dma_addr;     
	dma_addr_t rq_dma_addr;     
	u16 irq_cnt;
	u32 hw_cqid;                
	u32 hw_sqid;                
	int numa_node;
	struct u64_stats_sync syncp;
	struct xdp_rxq_info xdp_rxq;
};

#define FUN_QSTAT_INC(q, counter) \
	do { \
		u64_stats_update_begin(&(q)->syncp); \
		(q)->stats.counter++; \
		u64_stats_update_end(&(q)->syncp); \
	} while (0)

#define FUN_QSTAT_READ(q, seq, stats_copy) \
	do { \
		seq = u64_stats_fetch_begin(&(q)->syncp); \
		stats_copy = (q)->stats; \
	} while (u64_stats_fetch_retry(&(q)->syncp, (seq)))

#define FUN_INT_NAME_LEN (IFNAMSIZ + 16)

struct fun_irq {
	struct napi_struct napi;
	struct funeth_txq *txq;
	struct funeth_rxq *rxq;
	u8 state;
	u16 irq_idx;               
	int irq;                   
	cpumask_t affinity_mask;   
	struct irq_affinity_notify aff_notify;
	char name[FUN_INT_NAME_LEN];
} ____cacheline_internodealigned_in_smp;

 
static inline void *fun_tx_desc_addr(const struct funeth_txq *q,
				     unsigned int idx)
{
	return q->desc + idx * FUNETH_SQE_SIZE;
}

static inline void fun_txq_wr_db(const struct funeth_txq *q)
{
	unsigned int tail = q->prod_cnt & q->mask;

	writel(tail, q->db);
}

static inline int fun_irq_node(const struct fun_irq *p)
{
	return cpu_to_mem(cpumask_first(&p->affinity_mask));
}

int fun_rxq_napi_poll(struct napi_struct *napi, int budget);
int fun_txq_napi_poll(struct napi_struct *napi, int budget);
netdev_tx_t fun_start_xmit(struct sk_buff *skb, struct net_device *netdev);
bool fun_xdp_tx(struct funeth_txq *q, struct xdp_frame *xdpf);
int fun_xdp_xmit_frames(struct net_device *dev, int n,
			struct xdp_frame **frames, u32 flags);

int funeth_txq_create(struct net_device *dev, unsigned int qidx,
		      unsigned int ndesc, struct fun_irq *irq, int state,
		      struct funeth_txq **qp);
int fun_txq_create_dev(struct funeth_txq *q, struct fun_irq *irq);
struct funeth_txq *funeth_txq_free(struct funeth_txq *q, int state);
int funeth_rxq_create(struct net_device *dev, unsigned int qidx,
		      unsigned int ncqe, unsigned int nrqe, struct fun_irq *irq,
		      int state, struct funeth_rxq **qp);
int fun_rxq_create_dev(struct funeth_rxq *q, struct fun_irq *irq);
struct funeth_rxq *funeth_rxq_free(struct funeth_rxq *q, int state);
int fun_rxq_set_bpf(struct funeth_rxq *q, struct bpf_prog *prog);

#endif  
