





#ifndef _TIRPC_AUTH_UNIX_H
#define _TIRPC_AUTH_UNIX_H


#define MAX_MACHINE_NAME 255


#define NGRPS 16


struct authunix_parms {
	u_long	 aup_time;
	char	*aup_machname;
	uid_t 	 aup_uid;
	gid_t  	 aup_gid;
	u_int	 aup_len;
	gid_t 	*aup_gids;
};

#define authsys_parms authunix_parms

#ifdef __cplusplus
extern "C" {
#endif
extern bool_t xdr_authunix_parms(XDR *, struct authunix_parms *);
#ifdef __cplusplus
}
#endif


struct short_hand_verf {
	struct opaque_auth new_cred;
};

#endif 
