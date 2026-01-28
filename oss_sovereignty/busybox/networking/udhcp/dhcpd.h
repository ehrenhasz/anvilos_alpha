#ifndef UDHCP_DHCPD_H
#define UDHCP_DHCPD_H 1
PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN
#define DEFAULT_LEASE_TIME      (60*60*24 * 10)
#define LEASES_FILE             CONFIG_DHCPD_LEASES_FILE
#define DHCPD_CONF_FILE         "/etc/udhcpd.conf"
struct static_lease;
struct server_data_t {
	char *interface;                 
	int ifindex;
	uint32_t server_nip;
#if ENABLE_FEATURE_UDHCP_PORT
	uint16_t port;
#endif
	uint8_t server_mac[6];           
	struct option_set *options;      
	uint32_t start_ip;               
	uint32_t end_ip;                 
	uint32_t max_lease_sec;          
	uint32_t min_lease_sec;          
	uint32_t max_leases;             
	uint32_t auto_time;              
	uint32_t decline_time;           
	uint32_t conflict_time;          
	uint32_t offer_time;             
	uint32_t siaddr_nip;             
	char *lease_file;
	char *pidfile;
	char *notify_file;               
	char *sname;                     
	char *boot_file;                 
	struct static_lease *static_leases;  
} FIX_ALIASING;
#define server_data (*(struct server_data_t*)bb_common_bufsiz1)
#if ENABLE_FEATURE_UDHCP_PORT
#define SERVER_PORT  (server_data.port)
#define SERVER_PORT6 (server_data.port)
#else
#define SERVER_PORT  67
#define SERVER_PORT6 547
#endif
typedef uint32_t leasetime_t;
typedef int32_t signed_leasetime_t;
struct dyn_lease {
	leasetime_t expires;
	uint32_t lease_nip;
	uint8_t lease_mac[6];
	char hostname[20];
	uint8_t pad[2];
} PACKED;
POP_SAVED_FUNCTION_VISIBILITY
#endif
