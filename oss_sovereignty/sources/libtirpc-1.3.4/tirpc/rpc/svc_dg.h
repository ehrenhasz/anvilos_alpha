







struct svc_dg_data {
	
	size_t		su_iosz;		
	u_int32_t	su_xid;			
	XDR		su_xdrs;			
	char		su_verfbody[MAX_AUTH_BYTES];	
	void		*su_cache;		

	struct msghdr	su_msghdr;		
	unsigned char	su_cmsg[64];		
};

#define __rpcb_get_dg_xidp(x)	(&((struct svc_dg_data *)(x)->xp_p2)->su_xid)
