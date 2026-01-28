#ifndef _NETCONFIG_H_
#define _NETCONFIG_H_

#ifdef HAVE_FEATURES_H
#include <features.h>
#endif

#define NETCONFIG "/etc/netconfig"
#define NETPATH	  "NETPATH"

struct netconfig {
  char *nc_netid;		
  unsigned long nc_semantics;	
  unsigned long nc_flag;	
  char *nc_protofmly;		
  char *nc_proto;		
  char *nc_device;		
  unsigned long nc_nlookups;	
  char **nc_lookups;		
  unsigned long nc_unused[9];	
};

typedef struct {
  struct netconfig **nc_head;
  struct netconfig **nc_curr;
} NCONF_HANDLE;


#define NC_TPI_CLTS	1
#define NC_TPI_COTS	2
#define NC_TPI_COTS_ORD	3
#define NC_TPI_RAW	4


#define NC_NOFLAG	0x00
#define NC_VISIBLE	0x01
#define NC_BROADCAST	0x02


#define NC_NOPROTOFMLY	"-"
#define NC_LOOPBACK	"loopback"
#define NC_INET		"inet"
#define NC_INET6	"inet6"
#define NC_IMPLINK	"implink"
#define NC_PUP		"pup"
#define NC_CHAOS	"chaos"
#define NC_NS		"ns"
#define NC_NBS		"nbs"
#define NC_ECMA		"ecma"
#define NC_DATAKIT	"datakit"
#define NC_CCITT	"ccitt"
#define NC_SNA		"sna"
#define NC_DECNET	"decnet"
#define NC_DLI		"dli"
#define NC_LAT		"lat"
#define NC_HYLINK	"hylink"
#define NC_APPLETALK	"appletalk"
#define NC_NIT		"nit"
#define NC_IEEE802	"ieee802"
#define NC_OSI		"osi"
#define NC_X25		"x25"
#define NC_OSINET	"osinet"
#define NC_GOSIP	"gosip"


#define NC_NOPROTO	"-"
#define NC_TCP		"tcp"
#define NC_UDP		"udp"
#define NC_ICMP		"icmp"

#ifdef __cplusplus
extern "C" {
#endif

extern void *setnetconfig (void);
extern struct netconfig *getnetconfig (void *);
extern struct netconfig *getnetconfigent (const char *);
extern void freenetconfigent (struct netconfig *);
extern int endnetconfig (void *);

extern void *setnetpath (void);
extern struct netconfig *getnetpath (void *);
extern int endnetpath (void *);

extern void nc_perror (const char *);
extern char *nc_sperror (void);

#ifdef __cplusplus
}
#endif

#endif 
