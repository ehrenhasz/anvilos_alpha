#ifndef __CVMX_WQE_H__
#define __CVMX_WQE_H__
#include <asm/octeon/cvmx-packet.h>
#define OCT_TAG_TYPE_STRING(x)						\
	(((x) == CVMX_POW_TAG_TYPE_ORDERED) ?  "ORDERED" :		\
		(((x) == CVMX_POW_TAG_TYPE_ATOMIC) ?  "ATOMIC" :	\
			(((x) == CVMX_POW_TAG_TYPE_NULL) ?  "NULL" :	\
				"NULL_NULL")))
typedef union {
	uint64_t u64;
	struct {
#ifdef __BIG_ENDIAN_BITFIELD
		uint64_t bufs:8;
		uint64_t ip_offset:8;
		uint64_t vlan_valid:1;
		uint64_t vlan_stacked:1;
		uint64_t unassigned:1;
		uint64_t vlan_cfi:1;
		uint64_t vlan_id:12;
		uint64_t pr:4;
		uint64_t unassigned2:8;
		uint64_t dec_ipcomp:1;
		uint64_t tcp_or_udp:1;
		uint64_t dec_ipsec:1;
		uint64_t is_v6:1;
		uint64_t software:1;
		uint64_t L4_error:1;
		uint64_t is_frag:1;
		uint64_t IP_exc:1;
		uint64_t is_bcast:1;
		uint64_t is_mcast:1;
		uint64_t not_IP:1;
		uint64_t rcv_error:1;
		uint64_t err_code:8;
#else
	        uint64_t err_code:8;
	        uint64_t rcv_error:1;
	        uint64_t not_IP:1;
	        uint64_t is_mcast:1;
	        uint64_t is_bcast:1;
	        uint64_t IP_exc:1;
	        uint64_t is_frag:1;
	        uint64_t L4_error:1;
	        uint64_t software:1;
	        uint64_t is_v6:1;
	        uint64_t dec_ipsec:1;
	        uint64_t tcp_or_udp:1;
	        uint64_t dec_ipcomp:1;
	        uint64_t unassigned2:4;
	        uint64_t unassigned2a:4;
	        uint64_t pr:4;
	        uint64_t vlan_id:12;
	        uint64_t vlan_cfi:1;
	        uint64_t unassigned:1;
	        uint64_t vlan_stacked:1;
	        uint64_t vlan_valid:1;
	        uint64_t ip_offset:8;
	        uint64_t bufs:8;
#endif
	} s;
	struct {
#ifdef __BIG_ENDIAN_BITFIELD
		uint64_t bufs:8;
		uint64_t ip_offset:8;
		uint64_t vlan_valid:1;
		uint64_t vlan_stacked:1;
		uint64_t unassigned:1;
		uint64_t vlan_cfi:1;
		uint64_t vlan_id:12;
		uint64_t port:12;		 
		uint64_t dec_ipcomp:1;
		uint64_t tcp_or_udp:1;
		uint64_t dec_ipsec:1;
		uint64_t is_v6:1;
		uint64_t software:1;
		uint64_t L4_error:1;
		uint64_t is_frag:1;
		uint64_t IP_exc:1;
		uint64_t is_bcast:1;
		uint64_t is_mcast:1;
		uint64_t not_IP:1;
		uint64_t rcv_error:1;
		uint64_t err_code:8;
#else
		uint64_t err_code:8;
		uint64_t rcv_error:1;
		uint64_t not_IP:1;
		uint64_t is_mcast:1;
		uint64_t is_bcast:1;
		uint64_t IP_exc:1;
		uint64_t is_frag:1;
		uint64_t L4_error:1;
		uint64_t software:1;
		uint64_t is_v6:1;
		uint64_t dec_ipsec:1;
		uint64_t tcp_or_udp:1;
		uint64_t dec_ipcomp:1;
		uint64_t port:12;
		uint64_t vlan_id:12;
		uint64_t vlan_cfi:1;
		uint64_t unassigned:1;
		uint64_t vlan_stacked:1;
		uint64_t vlan_valid:1;
		uint64_t ip_offset:8;
		uint64_t bufs:8;
#endif
	} s_cn68xx;
	struct {
#ifdef __BIG_ENDIAN_BITFIELD
		uint64_t unused1:16;
		uint64_t vlan:16;
		uint64_t unused2:32;
#else
	        uint64_t unused2:32;
	        uint64_t vlan:16;
	        uint64_t unused1:16;
#endif
	} svlan;
	struct {
#ifdef __BIG_ENDIAN_BITFIELD
		uint64_t bufs:8;
		uint64_t unused:8;
		uint64_t vlan_valid:1;
		uint64_t vlan_stacked:1;
		uint64_t unassigned:1;
		uint64_t vlan_cfi:1;
		uint64_t vlan_id:12;
		uint64_t pr:4;
		uint64_t unassigned2:12;
		uint64_t software:1;
		uint64_t unassigned3:1;
		uint64_t is_rarp:1;
		uint64_t is_arp:1;
		uint64_t is_bcast:1;
		uint64_t is_mcast:1;
		uint64_t not_IP:1;
		uint64_t rcv_error:1;
		uint64_t err_code:8;
#else
	        uint64_t err_code:8;
	        uint64_t rcv_error:1;
	        uint64_t not_IP:1;
	        uint64_t is_mcast:1;
	        uint64_t is_bcast:1;
	        uint64_t is_arp:1;
	        uint64_t is_rarp:1;
	        uint64_t unassigned3:1;
	        uint64_t software:1;
	        uint64_t unassigned2:4;
	        uint64_t unassigned2a:8;
	        uint64_t pr:4;
	        uint64_t vlan_id:12;
	        uint64_t vlan_cfi:1;
	        uint64_t unassigned:1;
	        uint64_t vlan_stacked:1;
	        uint64_t vlan_valid:1;
	        uint64_t unused:8;
	        uint64_t bufs:8;
#endif
	} snoip;
} cvmx_pip_wqe_word2;
union cvmx_pip_wqe_word0 {
	struct {
#ifdef __BIG_ENDIAN_BITFIELD
		uint16_t hw_chksum;
		uint8_t unused;
		uint64_t next_ptr:40;
#else
		uint64_t next_ptr:40;
		uint8_t unused;
		uint16_t hw_chksum;
#endif
	} cn38xx;
	struct {
#ifdef __BIG_ENDIAN_BITFIELD
		uint64_t l4ptr:8;        
		uint64_t unused0:8;      
		uint64_t l3ptr:8;        
		uint64_t l2ptr:8;        
		uint64_t unused1:18;     
		uint64_t bpid:6;         
		uint64_t unused2:2;      
		uint64_t pknd:6;         
#else
		uint64_t pknd:6;         
		uint64_t unused2:2;      
		uint64_t bpid:6;         
		uint64_t unused1:18;     
		uint64_t l2ptr:8;        
		uint64_t l3ptr:8;        
		uint64_t unused0:8;      
		uint64_t l4ptr:8;        
#endif
	} cn68xx;
};
union cvmx_wqe_word0 {
	uint64_t u64;
	union cvmx_pip_wqe_word0 pip;
};
union cvmx_wqe_word1 {
	uint64_t u64;
	struct {
#ifdef __BIG_ENDIAN_BITFIELD
		uint64_t len:16;
		uint64_t varies:14;
		uint64_t tag_type:2;
		uint64_t tag:32;
#else
		uint64_t tag:32;
		uint64_t tag_type:2;
		uint64_t varies:14;
		uint64_t len:16;
#endif
	};
	struct {
#ifdef __BIG_ENDIAN_BITFIELD
		uint64_t len:16;
		uint64_t zero_0:1;
		uint64_t qos:3;
		uint64_t zero_1:1;
		uint64_t grp:6;
		uint64_t zero_2:3;
		uint64_t tag_type:2;
		uint64_t tag:32;
#else
		uint64_t tag:32;
		uint64_t tag_type:2;
		uint64_t zero_2:3;
		uint64_t grp:6;
		uint64_t zero_1:1;
		uint64_t qos:3;
		uint64_t zero_0:1;
		uint64_t len:16;
#endif
	} cn68xx;
	struct {
#ifdef __BIG_ENDIAN_BITFIELD
		uint64_t len:16;
		uint64_t ipprt:6;
		uint64_t qos:3;
		uint64_t grp:4;
		uint64_t tag_type:3;
		uint64_t tag:32;
#else
		uint64_t tag:32;
		uint64_t tag_type:2;
		uint64_t zero_2:1;
		uint64_t grp:4;
		uint64_t qos:3;
		uint64_t ipprt:6;
		uint64_t len:16;
#endif
	} cn38xx;
};
struct cvmx_wqe {
	union cvmx_wqe_word0 word0;
	union cvmx_wqe_word1 word1;
	cvmx_pip_wqe_word2 word2;
	union cvmx_buf_ptr packet_ptr;
	uint8_t packet_data[96];
} CVMX_CACHE_LINE_ALIGNED;
static inline int cvmx_wqe_get_port(struct cvmx_wqe *work)
{
	int port;
	if (octeon_has_feature(OCTEON_FEATURE_CN68XX_WQE))
		port = work->word2.s_cn68xx.port;
	else
		port = work->word1.cn38xx.ipprt;
	return port;
}
static inline void cvmx_wqe_set_port(struct cvmx_wqe *work, int port)
{
	if (octeon_has_feature(OCTEON_FEATURE_CN68XX_WQE))
		work->word2.s_cn68xx.port = port;
	else
		work->word1.cn38xx.ipprt = port;
}
static inline int cvmx_wqe_get_grp(struct cvmx_wqe *work)
{
	int grp;
	if (octeon_has_feature(OCTEON_FEATURE_CN68XX_WQE))
		grp = work->word1.cn68xx.grp;
	else
		grp = work->word1.cn38xx.grp;
	return grp;
}
static inline void cvmx_wqe_set_grp(struct cvmx_wqe *work, int grp)
{
	if (octeon_has_feature(OCTEON_FEATURE_CN68XX_WQE))
		work->word1.cn68xx.grp = grp;
	else
		work->word1.cn38xx.grp = grp;
}
static inline int cvmx_wqe_get_qos(struct cvmx_wqe *work)
{
	int qos;
	if (octeon_has_feature(OCTEON_FEATURE_CN68XX_WQE))
		qos = work->word1.cn68xx.qos;
	else
		qos = work->word1.cn38xx.qos;
	return qos;
}
static inline void cvmx_wqe_set_qos(struct cvmx_wqe *work, int qos)
{
	if (octeon_has_feature(OCTEON_FEATURE_CN68XX_WQE))
		work->word1.cn68xx.qos = qos;
	else
		work->word1.cn38xx.qos = qos;
}
#endif  
