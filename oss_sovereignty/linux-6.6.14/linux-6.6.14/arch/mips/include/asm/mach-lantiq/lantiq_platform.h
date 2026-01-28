#ifndef _LANTIQ_PLATFORM_H__
#define _LANTIQ_PLATFORM_H__
#include <linux/socket.h>
struct ltq_eth_data {
	struct sockaddr mac;
	int mii_mode;
};
#endif
