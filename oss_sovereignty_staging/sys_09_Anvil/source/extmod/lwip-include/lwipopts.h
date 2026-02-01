#ifndef MICROPY_INCLUDED_EXTMOD_LWIP_INCLUDE_LWIPOPTS_H
#define MICROPY_INCLUDED_EXTMOD_LWIP_INCLUDE_LWIPOPTS_H

#include <py/mpconfig.h>
#include <py/misc.h>
#include <py/mphal.h>


#define NO_SYS 1

#define SYS_LIGHTWEIGHT_PROT 1
#include <stdint.h>
typedef uint32_t sys_prot_t;

#define TCP_LISTEN_BACKLOG 1


#define LWIP_ARP 0
#define LWIP_ETHERNET 0

#define LWIP_DNS 1

#define LWIP_NETCONN 0
#define LWIP_SOCKET 0

#ifdef MICROPY_PY_LWIP_SLIP
#define LWIP_HAVE_SLIPIF 1
#endif




#define sys_now mp_hal_ticks_ms

#endif 
