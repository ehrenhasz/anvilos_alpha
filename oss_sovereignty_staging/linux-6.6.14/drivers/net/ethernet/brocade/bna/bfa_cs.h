 
 
 

 

#ifndef __BFA_CS_H__
#define __BFA_CS_H__

#include "cna.h"

 

 
#define BFA_SM_TABLE(n, s, e, t)				\
struct s;							\
enum e;								\
typedef void (*t)(struct s *, enum e);				\
								\
struct n ## _sm_table_s {					\
	t		sm;	 	\
	int		state;	 	\
	char		*name;	 	\
};								\
								\
static inline int						\
n ## _sm_to_state(struct n ## _sm_table_s *smt, t sm)		\
{								\
	int	i = 0;						\
								\
	while (smt[i].sm && smt[i].sm != sm)			\
		i++;						\
	return smt[i].state;					\
}

BFA_SM_TABLE(iocpf,	bfa_iocpf,	iocpf_event,	bfa_fsm_iocpf_t)
BFA_SM_TABLE(ioc,	bfa_ioc,	ioc_event,	bfa_fsm_ioc_t)
BFA_SM_TABLE(cmdq,	bfa_msgq_cmdq,	cmdq_event,	bfa_fsm_msgq_cmdq_t)
BFA_SM_TABLE(rspq,	bfa_msgq_rspq,	rspq_event,	bfa_fsm_msgq_rspq_t)

BFA_SM_TABLE(ioceth,	bna_ioceth,	bna_ioceth_event, bna_fsm_ioceth_t)
BFA_SM_TABLE(enet,	bna_enet,	bna_enet_event, bna_fsm_enet_t)
BFA_SM_TABLE(ethport,	bna_ethport,	bna_ethport_event, bna_fsm_ethport_t)
BFA_SM_TABLE(tx,	bna_tx,		bna_tx_event,	bna_fsm_tx_t)
BFA_SM_TABLE(rxf,	bna_rxf,	bna_rxf_event, bna_fsm_rxf_t)
BFA_SM_TABLE(rx,	bna_rx,		bna_rx_event,	bna_fsm_rx_t)

#undef BFA_SM_TABLE

#define BFA_SM(_sm)	(_sm)

 
typedef void (*bfa_fsm_t)(void *fsm, int event);

 
#define bfa_fsm_state_decl(oc, st, otype, etype)			\
	static void oc ## _sm_ ## st(otype * fsm, etype event);		\
	static void oc ## _sm_ ## st ## _entry(otype * fsm)

#define bfa_fsm_set_state(_fsm, _state) do {				\
	(_fsm)->fsm = (_state);						\
	_state ## _entry(_fsm);						\
} while (0)

#define bfa_fsm_send_event(_fsm, _event)	((_fsm)->fsm((_fsm), (_event)))
#define bfa_fsm_cmp_state(_fsm, _state)		((_fsm)->fsm == (_state))
 

typedef void (*bfa_wc_resume_t) (void *cbarg);

struct bfa_wc {
	bfa_wc_resume_t wc_resume;
	void		*wc_cbarg;
	int		wc_count;
};

static inline void
bfa_wc_up(struct bfa_wc *wc)
{
	wc->wc_count++;
}

static inline void
bfa_wc_down(struct bfa_wc *wc)
{
	wc->wc_count--;
	if (wc->wc_count == 0)
		wc->wc_resume(wc->wc_cbarg);
}

 
static inline void
bfa_wc_init(struct bfa_wc *wc, bfa_wc_resume_t wc_resume, void *wc_cbarg)
{
	wc->wc_resume = wc_resume;
	wc->wc_cbarg = wc_cbarg;
	wc->wc_count = 0;
	bfa_wc_up(wc);
}

 
static inline void
bfa_wc_wait(struct bfa_wc *wc)
{
	bfa_wc_down(wc);
}

#endif  
