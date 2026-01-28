

#ifndef UDHCP_DHCPC_H
#define UDHCP_DHCPC_H 1

PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN

struct client_data_t {
	uint8_t client_mac[6];          
	IF_FEATURE_UDHCP_PORT(uint16_t port;)
	int ifindex;                    
	uint32_t xid;
	uint8_t opt_mask[256 / 8];      

	const char *interface;          
	char *pidfile;                  
	const char *script;             
	struct option_set *options;     
	llist_t *envp;                  

	unsigned first_secs;
	unsigned last_secs;

	int sockfd;
	smallint listen_mode;
	smallint state;
} FIX_ALIASING;


#define client_data (*(struct client_data_t*)(&bb_common_bufsiz1[COMMON_BUFSIZE / 2]))

#if ENABLE_FEATURE_UDHCP_PORT
#define CLIENT_PORT  (client_data.port)
#define CLIENT_PORT6 (client_data.port)
#else
#define CLIENT_PORT  68
#define CLIENT_PORT6 546
#endif

POP_SAVED_FUNCTION_VISIBILITY

#endif
