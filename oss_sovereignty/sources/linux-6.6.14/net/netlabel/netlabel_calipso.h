




#ifndef _NETLABEL_CALIPSO
#define _NETLABEL_CALIPSO

#include <net/netlabel.h>
#include <net/calipso.h>




enum {
	NLBL_CALIPSO_C_UNSPEC,
	NLBL_CALIPSO_C_ADD,
	NLBL_CALIPSO_C_REMOVE,
	NLBL_CALIPSO_C_LIST,
	NLBL_CALIPSO_C_LISTALL,
	__NLBL_CALIPSO_C_MAX,
};


enum {
	NLBL_CALIPSO_A_UNSPEC,
	NLBL_CALIPSO_A_DOI,
	
	NLBL_CALIPSO_A_MTYPE,
	
	__NLBL_CALIPSO_A_MAX,
};

#define NLBL_CALIPSO_A_MAX (__NLBL_CALIPSO_A_MAX - 1)


#if IS_ENABLED(CONFIG_IPV6)
int netlbl_calipso_genl_init(void);
#else
static inline int netlbl_calipso_genl_init(void)
{
	return 0;
}
#endif

int calipso_doi_add(struct calipso_doi *doi_def,
		    struct netlbl_audit *audit_info);
void calipso_doi_free(struct calipso_doi *doi_def);
int calipso_doi_remove(u32 doi, struct netlbl_audit *audit_info);
struct calipso_doi *calipso_doi_getdef(u32 doi);
void calipso_doi_putdef(struct calipso_doi *doi_def);
int calipso_doi_walk(u32 *skip_cnt,
		     int (*callback)(struct calipso_doi *doi_def, void *arg),
		     void *cb_arg);
int calipso_sock_getattr(struct sock *sk, struct netlbl_lsm_secattr *secattr);
int calipso_sock_setattr(struct sock *sk,
			 const struct calipso_doi *doi_def,
			 const struct netlbl_lsm_secattr *secattr);
void calipso_sock_delattr(struct sock *sk);
int calipso_req_setattr(struct request_sock *req,
			const struct calipso_doi *doi_def,
			const struct netlbl_lsm_secattr *secattr);
void calipso_req_delattr(struct request_sock *req);
unsigned char *calipso_optptr(const struct sk_buff *skb);
int calipso_getattr(const unsigned char *calipso,
		    struct netlbl_lsm_secattr *secattr);
int calipso_skbuff_setattr(struct sk_buff *skb,
			   const struct calipso_doi *doi_def,
			   const struct netlbl_lsm_secattr *secattr);
int calipso_skbuff_delattr(struct sk_buff *skb);
void calipso_cache_invalidate(void);
int calipso_cache_add(const unsigned char *calipso_ptr,
		      const struct netlbl_lsm_secattr *secattr);

#endif
