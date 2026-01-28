#ifndef __IA_CSS_PRBS_H
#define __IA_CSS_PRBS_H
enum ia_css_prbs_id {
	IA_CSS_PRBS_ID0,
	IA_CSS_PRBS_ID1,
	IA_CSS_PRBS_ID2
};
#define N_CSS_PRBS_IDS (IA_CSS_PRBS_ID2 + 1)
struct ia_css_prbs_config {
	enum ia_css_prbs_id	id;
	unsigned int		h_blank;	 
	unsigned int		v_blank;	 
	int			seed;	 
	int			seed1;	 
};
#endif  
