#ifndef CTDAIO_H
#define CTDAIO_H
#include "ctresource.h"
#include "ctimap.h"
#include <linux/spinlock.h>
#include <linux/list.h>
#include <sound/core.h>
enum DAIOTYP {
	LINEO1,
	LINEO2,
	LINEO3,
	LINEO4,
	SPDIFOO,	 
	LINEIM,
	SPDIFIO,	 
	MIC,		 
	SPDIFI1,	 
	NUM_DAIOTYP
};
struct dao_rsc_ops;
struct dai_rsc_ops;
struct daio_mgr;
struct daio {
	struct rsc rscl;	 
	struct rsc rscr;	 
	enum DAIOTYP type;
};
struct dao {
	struct daio daio;
	const struct dao_rsc_ops *ops;	 
	struct imapper **imappers;
	struct daio_mgr *mgr;
	struct hw *hw;
	void *ctrl_blk;
};
struct dai {
	struct daio daio;
	const struct dai_rsc_ops *ops;	 
	struct hw *hw;
	void *ctrl_blk;
};
struct dao_desc {
	unsigned int msr:4;
	unsigned int passthru:1;
};
struct dao_rsc_ops {
	int (*set_spos)(struct dao *dao, unsigned int spos);
	int (*commit_write)(struct dao *dao);
	int (*get_spos)(struct dao *dao, unsigned int *spos);
	int (*reinit)(struct dao *dao, const struct dao_desc *desc);
	int (*set_left_input)(struct dao *dao, struct rsc *input);
	int (*set_right_input)(struct dao *dao, struct rsc *input);
	int (*clear_left_input)(struct dao *dao);
	int (*clear_right_input)(struct dao *dao);
};
struct dai_rsc_ops {
	int (*set_srt_srcl)(struct dai *dai, struct rsc *src);
	int (*set_srt_srcr)(struct dai *dai, struct rsc *src);
	int (*set_srt_msr)(struct dai *dai, unsigned int msr);
	int (*set_enb_src)(struct dai *dai, unsigned int enb);
	int (*set_enb_srt)(struct dai *dai, unsigned int enb);
	int (*commit_write)(struct dai *dai);
};
struct daio_desc {
	unsigned int type:4;
	unsigned int msr:4;
	unsigned int passthru:1;
};
struct daio_mgr {
	struct rsc_mgr mgr;	 
	struct snd_card *card;	 
	spinlock_t mgr_lock;
	spinlock_t imap_lock;
	struct list_head imappers;
	struct imapper *init_imap;
	unsigned int init_imap_added;
	int (*get_daio)(struct daio_mgr *mgr,
			const struct daio_desc *desc, struct daio **rdaio);
	int (*put_daio)(struct daio_mgr *mgr, struct daio *daio);
	int (*daio_enable)(struct daio_mgr *mgr, struct daio *daio);
	int (*daio_disable)(struct daio_mgr *mgr, struct daio *daio);
	int (*imap_add)(struct daio_mgr *mgr, struct imapper *entry);
	int (*imap_delete)(struct daio_mgr *mgr, struct imapper *entry);
	int (*commit_write)(struct daio_mgr *mgr);
};
int daio_mgr_create(struct hw *hw, struct daio_mgr **rdaio_mgr);
int daio_mgr_destroy(struct daio_mgr *daio_mgr);
#endif  
