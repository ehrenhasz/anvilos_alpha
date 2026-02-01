 
 

#ifndef _IPWIRELESS_CS_H_
#define _IPWIRELESS_CS_H_

#include <linux/sched.h>
#include <linux/types.h>

#include <pcmcia/cistpl.h>
#include <pcmcia/ds.h>

#include "hardware.h"

#define IPWIRELESS_PCCARD_NAME		"ipwireless"
#define IPWIRELESS_PCMCIA_VERSION	"1.1"
#define IPWIRELESS_PCMCIA_AUTHOR        \
	"Stephen Blackheath, Ben Martel, Jiri Kosina and David Sterba"

#define IPWIRELESS_TX_QUEUE_SIZE  262144
#define IPWIRELESS_RX_QUEUE_SIZE  262144

#define IPWIRELESS_STATE_DEBUG

struct ipw_hardware;
struct ipw_network;
struct ipw_tty;

struct ipw_dev {
	struct pcmcia_device *link;
	int is_v2_card;

	void __iomem *attr_memory;

	void __iomem *common_memory;

	 
	void *attribute_memory;

	 
	struct ipw_hardware *hardware;
	 
	struct ipw_network *network;
	 
	struct ipw_tty *tty;
	struct work_struct work_reboot;
};

 
extern int ipwireless_debug;
extern int ipwireless_loopback;
extern int ipwireless_out_queue;

#endif
