

#ifndef __CXGB4_SRQ_H
#define __CXGB4_SRQ_H

struct adapter;
struct cpl_srq_table_rpl;

#define SRQ_WAIT_TO	(HZ * 5)

struct srq_entry {
	u8 valid;
	u8 idx;
	u8 qlen;
	u16 pdid;
	u16 cur_msn;
	u16 max_msn;
	u32 qbase;
};

struct srq_data {
	unsigned int srq_size;
	struct srq_entry *entryp;
	struct completion comp;
	struct mutex lock; 
};

struct srq_data *t4_init_srq(int srq_size);
int cxgb4_get_srq_entry(struct net_device *dev,
			int srq_idx, struct srq_entry *entryp);
void do_srq_table_rpl(struct adapter *adap,
		      const struct cpl_srq_table_rpl *rpl);
#endif  
