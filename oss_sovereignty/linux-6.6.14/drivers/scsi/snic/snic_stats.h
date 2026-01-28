#ifndef __SNIC_STATS_H
#define __SNIC_STATS_H
struct snic_io_stats {
	atomic64_t active;		 
	atomic64_t max_active;		 
	atomic64_t max_sgl;		 
	atomic64_t max_time;		 
	atomic64_t max_qtime;		 
	atomic64_t max_cmpl_time;	 
	atomic64_t sgl_cnt[SNIC_MAX_SG_DESC_CNT];  
	atomic64_t max_io_sz;		 
	atomic64_t compl;		 
	atomic64_t fail;		 
	atomic64_t req_null;		 
	atomic64_t alloc_fail;		 
	atomic64_t sc_null;
	atomic64_t io_not_found;	 
	atomic64_t num_ios;		 
};
struct snic_abort_stats {
	atomic64_t num;		 
	atomic64_t fail;	 
	atomic64_t drv_tmo;	 
	atomic64_t fw_tmo;	 
	atomic64_t io_not_found; 
	atomic64_t q_fail;	 
};
struct snic_reset_stats {
	atomic64_t dev_resets;		 
	atomic64_t dev_reset_fail;	 
	atomic64_t dev_reset_aborts;	 
	atomic64_t dev_reset_tmo;	 
	atomic64_t dev_reset_terms;	 
	atomic64_t hba_resets;		 
	atomic64_t hba_reset_cmpl;	 
	atomic64_t hba_reset_fail;	 
	atomic64_t snic_resets;		 
	atomic64_t snic_reset_compl;	 
	atomic64_t snic_reset_fail;	 
};
struct snic_fw_stats {
	atomic64_t actv_reqs;		 
	atomic64_t max_actv_reqs;	 
	atomic64_t out_of_res;		 
	atomic64_t io_errs;		 
	atomic64_t scsi_errs;		 
};
struct snic_misc_stats {
	u64	last_isr_time;
	u64	last_ack_time;
	atomic64_t ack_isr_cnt;
	atomic64_t cmpl_isr_cnt;
	atomic64_t errnotify_isr_cnt;
	atomic64_t max_cq_ents;		 
	atomic64_t data_cnt_mismat;	 
	atomic64_t io_tmo;
	atomic64_t io_aborted;
	atomic64_t sgl_inval;		 
	atomic64_t abts_wq_alloc_fail;	 
	atomic64_t devrst_wq_alloc_fail; 
	atomic64_t wq_alloc_fail;	 
	atomic64_t no_icmnd_itmf_cmpls;
	atomic64_t io_under_run;
	atomic64_t qfull;
	atomic64_t qsz_rampup;
	atomic64_t qsz_rampdown;
	atomic64_t last_qsz;
	atomic64_t tgt_not_rdy;
};
struct snic_stats {
	struct snic_io_stats io;
	struct snic_abort_stats abts;
	struct snic_reset_stats reset;
	struct snic_fw_stats fw;
	struct snic_misc_stats misc;
	atomic64_t io_cmpl_skip;
};
void snic_stats_debugfs_init(struct snic *);
void snic_stats_debugfs_remove(struct snic *);
static inline void
snic_stats_update_active_ios(struct snic_stats *s_stats)
{
	struct snic_io_stats *io = &s_stats->io;
	int nr_active_ios;
	nr_active_ios = atomic64_read(&io->active);
	if (atomic64_read(&io->max_active) < nr_active_ios)
		atomic64_set(&io->max_active, nr_active_ios);
	atomic64_inc(&io->num_ios);
}
static inline void
snic_stats_update_io_cmpl(struct snic_stats *s_stats)
{
	atomic64_dec(&s_stats->io.active);
	if (unlikely(atomic64_read(&s_stats->io_cmpl_skip)))
		atomic64_dec(&s_stats->io_cmpl_skip);
	else
		atomic64_inc(&s_stats->io.compl);
}
#endif  
