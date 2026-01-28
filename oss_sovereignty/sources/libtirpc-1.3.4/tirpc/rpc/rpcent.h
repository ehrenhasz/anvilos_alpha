







#ifndef _RPC_RPCENT_H
#define _RPC_RPCENT_H




#ifdef __cplusplus
extern "C" {
#endif


#if defined(__UCLIBC__) && !defined(__UCLIBC_HAS_RPC__) || !defined(__GLIBC__)
struct rpcent {
	char	*r_name;	
	char	**r_aliases;	
	int	r_number;	
};


extern struct rpcent *getrpcbyname(const char *);
extern struct rpcent *getrpcbynumber(int);
extern struct rpcent *getrpcent(void);

extern void setrpcent(int);
extern void endrpcent(void);
#endif

#ifdef __cplusplus
}
#endif

#endif 
