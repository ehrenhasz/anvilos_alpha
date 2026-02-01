 
 

#ifndef __SNIC_TRC_H
#define __SNIC_TRC_H

#ifdef CONFIG_SCSI_SNIC_DEBUG_FS

extern ssize_t simple_read_from_buffer(void __user *to,
					size_t count,
					loff_t *ppos,
					const void *from,
					size_t available);

extern unsigned int snic_trace_max_pages;

 
struct snic_trc_data {
	u64	ts;		 
	char	*fn;		 
	u32	hno;		 
	u32	tag;		 
	u64 data[5];
} __attribute__((__packed__));

#define SNIC_TRC_ENTRY_SZ  64	 

struct snic_trc {
	spinlock_t lock;
	struct snic_trc_data *buf;	 
	u32	max_idx;		 
	u32	rd_idx;
	u32	wr_idx;
	bool	enable;			 
};

int snic_trc_init(void);
void snic_trc_free(void);
void snic_trc_debugfs_init(void);
void snic_trc_debugfs_term(void);
struct snic_trc_data *snic_get_trc_buf(void);
int snic_get_trc_data(char *buf, int buf_sz);

void snic_debugfs_init(void);
void snic_debugfs_term(void);

static inline void
snic_trace(char *fn, u16 hno, u32 tag, u64 d1, u64 d2, u64 d3, u64 d4, u64 d5)
{
	struct snic_trc_data *tr_rec = snic_get_trc_buf();

	if (!tr_rec)
		return;

	tr_rec->fn = (char *)fn;
	tr_rec->hno = hno;
	tr_rec->tag = tag;
	tr_rec->data[0] = d1;
	tr_rec->data[1] = d2;
	tr_rec->data[2] = d3;
	tr_rec->data[3] = d4;
	tr_rec->data[4] = d5;
	tr_rec->ts = jiffies;  
}

#define SNIC_TRC(_hno, _tag, d1, d2, d3, d4, d5)			\
	do {								\
		if (unlikely(snic_glob->trc.enable))			\
			snic_trace((char *)__func__,			\
				   (u16)(_hno),				\
				   (u32)(_tag),				\
				   (u64)(d1),				\
				   (u64)(d2),				\
				   (u64)(d3),				\
				   (u64)(d4),				\
				   (u64)(d5));				\
	} while (0)
#else

#define SNIC_TRC(_hno, _tag, d1, d2, d3, d4, d5)	\
	do {						\
		if (unlikely(snic_log_level & 0x2))	\
			SNIC_DBG("SnicTrace: %s %2u %2u %llx %llx %llx %llx %llx", \
				 (char *)__func__,	\
				 (u16)(_hno),		\
				 (u32)(_tag),		\
				 (u64)(d1),		\
				 (u64)(d2),		\
				 (u64)(d3),		\
				 (u64)(d4),		\
				 (u64)(d5));		\
	} while (0)
#endif  

#define SNIC_TRC_CMD(sc)	\
	((u64)sc->cmnd[0] << 56 | (u64)sc->cmnd[7] << 40 |	\
	 (u64)sc->cmnd[8] << 32 | (u64)sc->cmnd[2] << 24 |	\
	 (u64)sc->cmnd[3] << 16 | (u64)sc->cmnd[4] << 8 |	\
	 (u64)sc->cmnd[5])

#define SNIC_TRC_CMD_STATE_FLAGS(sc)	\
	((u64) CMD_FLAGS(sc) << 32 | CMD_STATE(sc))

#endif  
