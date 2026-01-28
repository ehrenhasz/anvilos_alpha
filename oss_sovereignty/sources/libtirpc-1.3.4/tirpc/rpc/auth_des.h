







#ifndef _TI_AUTH_DES_
#define _TI_AUTH_DES_

#include <rpc/auth.h>


enum authdes_namekind {
	ADN_FULLNAME, 
	ADN_NICKNAME
};


struct authdes_fullname {
  char *name;		
  union des_block key;		
   
  u_int32_t window;	
};



struct authdes_cred {
	enum authdes_namekind adc_namekind;
  	struct authdes_fullname adc_fullname;
  
 u_int32_t adc_nickname;
}; 




struct authdes_verf {
	union {
		struct timeval adv_ctime;	
	  	des_block adv_xtime;		
	} adv_time_u;
  
  u_int32_t adv_int_u;
};


#define adv_timestamp	adv_time_u.adv_ctime
#define adv_xtimestamp	adv_time_u.adv_xtime
#define adv_winverf	adv_int_u


#define adv_timeverf	adv_time_u.adv_ctime
#define adv_xtimeverf	adv_time_u.adv_xtime
#define adv_nickname	adv_int_u

#ifdef __cplusplus
extern "C" {
#endif
extern int	rtime(struct sockaddr_in *, struct timeval *,
		    struct timeval *);
extern void	kgetnetname(char *);
#ifdef __cplusplus
}
#endif

#endif 
