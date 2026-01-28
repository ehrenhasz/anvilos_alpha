

#ifndef UDHCP_COMMON_H
#define UDHCP_COMMON_H 1

#include "libbb.h"
#include "common_bufsiz.h"
#include <netinet/udp.h>
#include <netinet/ip.h>

PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN

extern const uint8_t MAC_BCAST_ADDR[6] ALIGN2; 





#define DHCP_MAGIC              0x63825363
#define DHCP_OPTIONS_BUFSIZE    308
#define BOOTREQUEST             1
#define BOOTREPLY               2


struct dhcp_packet {
	uint8_t op;      
	uint8_t htype;   
	uint8_t hlen;    
	uint8_t hops;    
	uint32_t xid;    
	uint16_t secs;   
	uint16_t flags;  
#define BROADCAST_FLAG 0x8000 
	uint32_t ciaddr; 
	uint32_t yiaddr; 
	
	uint32_t siaddr_nip;
	
	uint32_t gateway_nip; 
	uint8_t chaddr[16];   
	uint8_t sname[64];    
	
	uint8_t file[128];    
	
	
	uint32_t cookie;      
	uint8_t options[DHCP_OPTIONS_BUFSIZE + CONFIG_UDHCPC_SLACK_FOR_BUGGY_SERVERS];
};
#define DHCP_PKT_SNAME_LEN      64
#define DHCP_PKT_FILE_LEN      128
#define DHCP_PKT_SNAME_LEN_STR "64"
#define DHCP_PKT_FILE_LEN_STR "128"

struct ip_udp_dhcp_packet {
	struct iphdr ip;
	struct udphdr udp;
	struct dhcp_packet data;
};

struct udp_dhcp_packet {
	struct udphdr udp;
	struct dhcp_packet data;
};

enum {
	IP_UDP_DHCP_SIZE = sizeof(struct ip_udp_dhcp_packet) - CONFIG_UDHCPC_SLACK_FOR_BUGGY_SERVERS,
	UDP_DHCP_SIZE    = sizeof(struct udp_dhcp_packet) - CONFIG_UDHCPC_SLACK_FOR_BUGGY_SERVERS,
	DHCP_SIZE        = sizeof(struct dhcp_packet) - CONFIG_UDHCPC_SLACK_FOR_BUGGY_SERVERS,
};


struct BUG_bad_sizeof_struct_ip_udp_dhcp_packet {
	char c[IP_UDP_DHCP_SIZE == 576 ? 1 : -1];
};




enum {
	OPTION_IP = 0,
	OPTION_IP_PAIR,
	OPTION_STRING,
	
	OPTION_STRING_HOST,

	OPTION_U8,
	OPTION_U16,

	OPTION_U32,
	OPTION_S32,
	OPTION_BIN,
	OPTION_STATIC_ROUTES,
	OPTION_6RD,
#if ENABLE_FEATURE_UDHCP_RFC3397 || ENABLE_FEATURE_UDHCPC6_RFC3646 || ENABLE_FEATURE_UDHCPC6_RFC4704
	OPTION_DNS_STRING,  
#endif
#if ENABLE_FEATURE_UDHCP_RFC3397
	OPTION_SIP_SERVERS,
#endif

	OPTION_TYPE_MASK = 0x0f,
	
	OPTION_REQ  = 0x10,
	
	OPTION_LIST = 0x20,
};

struct dhcp_scan_state {
	int overload;
	int rem;
	uint8_t *optionptr;
};


#define DHCP_PADDING            0x00
#define DHCP_SUBNET             0x01








#define DHCP_HOST_NAME          0x0c 












#define DHCP_REQUESTED_IP       0x32 
#define DHCP_LEASE_TIME         0x33 
#define DHCP_OPTION_OVERLOAD    0x34 
#define DHCP_MESSAGE_TYPE       0x35 
#define DHCP_SERVER_ID          0x36 
#define DHCP_PARAM_REQ          0x37 

#define DHCP_MAX_SIZE           0x39 


#define DHCP_VENDOR             0x3c 
#define DHCP_CLIENT_ID          0x3d 




#define DHCP_FQDN               0x51 












#define DHCP_END                0xff 


#define OPT_CODE                0
#define OPT_LEN                 1
#define OPT_DATA                2

#define D6_OPT_CODE             0
#define D6_OPT_LEN              2
#define D6_OPT_DATA             4

#define OPTION_FIELD            0
#define FILE_FIELD              1
#define SNAME_FIELD             2


#define DHCPDISCOVER            1 
#define DHCPOFFER               2 
#define DHCPREQUEST             3 
#define DHCPDECLINE             4 
#define DHCPACK                 5 
#define DHCPNAK                 6 
#define DHCPRELEASE             7 
#define DHCPINFORM              8 
#define DHCP_MINTYPE DHCPDISCOVER
#define DHCP_MAXTYPE DHCPINFORM

struct dhcp_optflag {
	uint8_t flags;
	uint8_t code;
};

struct option_set {
	uint8_t *data;
	struct option_set *next;
};

#if ENABLE_UDHCPC || ENABLE_UDHCPD
extern const struct dhcp_optflag dhcp_optflags[];
extern const char dhcp_option_strings[] ALIGN1;
#endif
extern const uint8_t dhcp_option_lengths[] ALIGN1;

unsigned FAST_FUNC udhcp_option_idx(const char *name, const char *option_strings);

void init_scan_state(struct dhcp_packet *packet, struct dhcp_scan_state *scan_state) FAST_FUNC;
uint8_t *udhcp_scan_options(struct dhcp_packet *packet, struct dhcp_scan_state *scan_state) FAST_FUNC;
uint8_t *udhcp_get_option(struct dhcp_packet *packet, int code) FAST_FUNC;

uint8_t *udhcp_get_option32(struct dhcp_packet *packet, int code) FAST_FUNC;
int udhcp_end_option(uint8_t *optionptr) FAST_FUNC;
void udhcp_add_binary_option(struct dhcp_packet *packet, uint8_t *addopt) FAST_FUNC;
#if ENABLE_UDHCPC || ENABLE_UDHCPD
void udhcp_add_simple_option(struct dhcp_packet *packet, uint8_t code, uint32_t data) FAST_FUNC;
#endif
#if ENABLE_FEATURE_UDHCP_RFC3397 || ENABLE_FEATURE_UDHCPC6_RFC3646 || ENABLE_FEATURE_UDHCPC6_RFC4704
char *dname_dec(const uint8_t *cstr, int clen, const char *pre) FAST_FUNC;
uint8_t *dname_enc( const char *src, int *retlen) FAST_FUNC;
#endif
#if !ENABLE_UDHCPC6
#define udhcp_find_option(opt_list, code, dhcpv6) \
	udhcp_find_option(opt_list, code)
#endif
struct option_set *udhcp_find_option(struct option_set *opt_list, uint8_t code, bool dhcpv6) FAST_FUNC;










































#if defined CONFIG_UDHCP_DEBUG && CONFIG_UDHCP_DEBUG >= 1
# define IF_UDHCP_VERBOSE(...) __VA_ARGS__
extern unsigned dhcp_verbose;
# define log1(...) do { if (dhcp_verbose >= 1) bb_info_msg(__VA_ARGS__); } while (0)

void log1s(const char *msg) FAST_FUNC;
# if CONFIG_UDHCP_DEBUG >= 2
void udhcp_dump_packet(struct dhcp_packet *packet) FAST_FUNC;
#  define log2(...) do { if (dhcp_verbose >= 2) bb_info_msg(__VA_ARGS__); } while (0)
#  define log2s(msg) do { if (dhcp_verbose >= 2) bb_simple_info_msg(msg); } while (0)
# else
#  define udhcp_dump_packet(...) ((void)0)
#  define log2(...) ((void)0)
#  define log2s(msg) ((void)0)
# endif
# if CONFIG_UDHCP_DEBUG >= 3
#  define log3(...) do { if (dhcp_verbose >= 3) bb_info_msg(__VA_ARGS__); } while (0)
#  define log3s(msg) do { if (dhcp_verbose >= 3) bb_simple_info_msg(msg); } while (0)
# else
#  define log3(...) ((void)0)
#  define log3s(msg) ((void)0)
# endif
#else
# define IF_UDHCP_VERBOSE(...)
# define udhcp_dump_packet(...) ((void)0)
# define log1(...) ((void)0)
# define log1s(msg) ((void)0)
# define log2(...) ((void)0)
# define log2s(msg) ((void)0)
# define log3(...) ((void)0)
# define log3s(msg) ((void)0)
#endif




int FAST_FUNC udhcp_str2nip(const char *str, void *arg);

#if !ENABLE_UDHCPC6
#define udhcp_insert_new_option(opt_list, code, length, dhcpv6) \
	udhcp_insert_new_option(opt_list, code, length)
#endif
void* FAST_FUNC udhcp_insert_new_option(struct option_set **opt_list,
		unsigned code,
		unsigned length,
		bool dhcpv6);


#if !ENABLE_UDHCPC6
#define udhcp_str2optset(str, arg, optflags, option_strings, dhcpv6) \
	udhcp_str2optset(str, arg, optflags, option_strings)
#endif
int FAST_FUNC udhcp_str2optset(const char *str,
		void *arg,
		const struct dhcp_optflag *optflags,
		const char *option_strings,
		bool dhcpv6);

#if ENABLE_UDHCPC || ENABLE_UDHCPD
void udhcp_init_header(struct dhcp_packet *packet, char type) FAST_FUNC;
#endif

int udhcp_recv_kernel_packet(struct dhcp_packet *packet, int fd) FAST_FUNC;

int udhcp_send_raw_packet(struct dhcp_packet *dhcp_pkt,
		uint32_t source_nip, int source_port,
		uint32_t dest_nip, int dest_port, const uint8_t *dest_arp,
		int ifindex) FAST_FUNC;

int udhcp_send_kernel_packet(struct dhcp_packet *dhcp_pkt,
		uint32_t source_nip, int source_port,
		uint32_t dest_nip, int dest_port,
		const char *ifname) FAST_FUNC;

void udhcp_sp_setup(void) FAST_FUNC;
void udhcp_sp_fd_set(struct pollfd *pfds, int extra_fd) FAST_FUNC;
int udhcp_sp_read(void) FAST_FUNC;

int udhcp_read_interface(const char *interface,
		int *ifindex, uint32_t *nip, uint8_t *mac) FAST_FUNC;

int udhcp_listen_socket( int port, const char *inf) FAST_FUNC;


int arpping(uint32_t test_nip,
		const uint8_t *safe_mac,
		uint32_t from_ip,
		uint8_t *from_mac,
		const char *interface,
		unsigned timeo) FAST_FUNC;


int sprint_nip6(char *dest,  const uint8_t *ip) FAST_FUNC;

POP_SAVED_FUNCTION_VISIBILITY

#endif
