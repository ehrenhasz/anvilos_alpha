




#ifndef __BFA_MODULES_H__
#define __BFA_MODULES_H__

#include "bfa_cs.h"
#include "bfa.h"
#include "bfa_svc.h"
#include "bfa_fcpim.h"
#include "bfa_port.h"

struct bfa_modules_s {
	struct bfa_fcdiag_s	fcdiag;		
	struct bfa_fcport_s	fcport;		
	struct bfa_fcxp_mod_s	fcxp_mod;	
	struct bfa_lps_mod_s	lps_mod;	
	struct bfa_uf_mod_s	uf_mod;		
	struct bfa_rport_mod_s	rport_mod;	
	struct bfa_fcp_mod_s	fcp_mod;	
	struct bfa_sgpg_mod_s	sgpg_mod;	
	struct bfa_port_s	port;		
	struct bfa_ablk_s	ablk;		
	struct bfa_cee_s	cee;		
	struct bfa_sfp_s	sfp;		
	struct bfa_flash_s	flash;		
	struct bfa_diag_s	diag_mod;	
	struct bfa_phy_s	phy;		
	struct bfa_dconf_mod_s	dconf_mod;	
	struct bfa_fru_s	fru;		
};


enum {
	BFA_TRC_HAL_CORE	= 1,
	BFA_TRC_HAL_FCXP	= 2,
	BFA_TRC_HAL_FCPIM	= 3,
	BFA_TRC_HAL_IOCFC_CT	= 4,
	BFA_TRC_HAL_IOCFC_CB	= 5,
};

#define BFA_CACHELINE_SZ	(256)

struct bfa_s {
	void			*bfad;		
	struct bfa_plog_s	*plog;		
	struct bfa_trc_mod_s	*trcmod;	
	struct bfa_ioc_s	ioc;		
	struct bfa_iocfc_s	iocfc;		
	struct bfa_timer_mod_s	timer_mod;	
	struct bfa_modules_s	modules;	
	struct list_head	comp_q;		
	bfa_boolean_t		queue_process;	
	struct list_head	reqq_waitq[BFI_IOC_MAX_CQS];
	bfa_boolean_t		fcs;		
	struct bfa_msix_s	msix;
	int			bfa_aen_seq;
	bfa_boolean_t		intr_enabled;	
};

extern bfa_boolean_t bfa_auto_recover;

void bfa_dconf_attach(struct bfa_s *, void *, struct bfa_iocfc_cfg_s *);
void bfa_dconf_meminfo(struct bfa_iocfc_cfg_s *, struct bfa_meminfo_s *,
		  struct bfa_s *);
void bfa_dconf_iocdisable(struct bfa_s *);
void bfa_fcp_attach(struct bfa_s *, void *, struct bfa_iocfc_cfg_s *,
		struct bfa_pcidev_s *);
void bfa_fcp_iocdisable(struct bfa_s *bfa);
void bfa_fcp_meminfo(struct bfa_iocfc_cfg_s *, struct bfa_meminfo_s *,
		struct bfa_s *);
void bfa_fcpim_iocdisable(struct bfa_fcp_mod_s *);
void bfa_fcport_start(struct bfa_s *);
void bfa_fcport_iocdisable(struct bfa_s *);
void bfa_fcport_meminfo(struct bfa_iocfc_cfg_s *, struct bfa_meminfo_s *,
		   struct bfa_s *);
void bfa_fcport_attach(struct bfa_s *, void *, struct bfa_iocfc_cfg_s *,
		struct bfa_pcidev_s *);
void bfa_fcxp_iocdisable(struct bfa_s *);
void bfa_fcxp_meminfo(struct bfa_iocfc_cfg_s *, struct bfa_meminfo_s *,
		struct bfa_s *);
void bfa_fcxp_attach(struct bfa_s *, void *, struct bfa_iocfc_cfg_s *,
		struct bfa_pcidev_s *);
void bfa_fcdiag_iocdisable(struct bfa_s *);
void bfa_fcdiag_attach(struct bfa_s *bfa, void *, struct bfa_iocfc_cfg_s *,
		struct bfa_pcidev_s *);
void bfa_ioim_lm_init(struct bfa_s *);
void bfa_lps_iocdisable(struct bfa_s *bfa);
void bfa_lps_meminfo(struct bfa_iocfc_cfg_s *, struct bfa_meminfo_s *,
		struct bfa_s *);
void bfa_lps_attach(struct bfa_s *, void *, struct bfa_iocfc_cfg_s *,
	struct bfa_pcidev_s *);
void bfa_rport_iocdisable(struct bfa_s *bfa);
void bfa_rport_meminfo(struct bfa_iocfc_cfg_s *, struct bfa_meminfo_s *,
		struct bfa_s *);
void bfa_rport_attach(struct bfa_s *, void *, struct bfa_iocfc_cfg_s *,
		struct bfa_pcidev_s *);
void bfa_sgpg_meminfo(struct bfa_iocfc_cfg_s *, struct bfa_meminfo_s *,
		struct bfa_s *);
void bfa_sgpg_attach(struct bfa_s *, void *bfad, struct bfa_iocfc_cfg_s *,
		struct bfa_pcidev_s *);
void bfa_uf_iocdisable(struct bfa_s *);
void bfa_uf_meminfo(struct bfa_iocfc_cfg_s *, struct bfa_meminfo_s *,
		struct bfa_s *);
void bfa_uf_attach(struct bfa_s *, void *, struct bfa_iocfc_cfg_s *,
		struct bfa_pcidev_s *);
void bfa_uf_start(struct bfa_s *);

#endif 
