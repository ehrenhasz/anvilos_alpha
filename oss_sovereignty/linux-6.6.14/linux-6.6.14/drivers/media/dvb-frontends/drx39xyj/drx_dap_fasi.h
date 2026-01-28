#ifndef __DRX_DAP_FASI_H__
#define __DRX_DAP_FASI_H__
#include "drx_driver.h"
#if !defined(DRXDAPFASI_LONG_ADDR_ALLOWED)
#define  DRXDAPFASI_LONG_ADDR_ALLOWED 1
#endif
#if !defined(DRXDAPFASI_SHORT_ADDR_ALLOWED)
#define  DRXDAPFASI_SHORT_ADDR_ALLOWED 1
#endif
#if ((DRXDAPFASI_LONG_ADDR_ALLOWED == 0) && \
      (DRXDAPFASI_SHORT_ADDR_ALLOWED == 0))
#error  At least one of short- or long-addressing format must be allowed.
*;				 
#endif
#ifndef DRXDAP_SINGLE_MASTER
#define DRXDAP_SINGLE_MASTER 0
#endif
#if !defined(DRXDAP_MAX_WCHUNKSIZE)
#define  DRXDAP_MAX_WCHUNKSIZE 254
#endif
#if ((DRXDAPFASI_LONG_ADDR_ALLOWED == 0) && (DRXDAPFASI_SHORT_ADDR_ALLOWED == 1))
#if DRXDAP_SINGLE_MASTER
#define  DRXDAP_MAX_WCHUNKSIZE_MIN 3
#else
#define  DRXDAP_MAX_WCHUNKSIZE_MIN 5
#endif
#else
#if DRXDAP_SINGLE_MASTER
#define  DRXDAP_MAX_WCHUNKSIZE_MIN 5
#else
#define  DRXDAP_MAX_WCHUNKSIZE_MIN 7
#endif
#endif
#if  DRXDAP_MAX_WCHUNKSIZE <  DRXDAP_MAX_WCHUNKSIZE_MIN
#if ((DRXDAPFASI_LONG_ADDR_ALLOWED == 0) && (DRXDAPFASI_SHORT_ADDR_ALLOWED == 1))
#if DRXDAP_SINGLE_MASTER
#error  DRXDAP_MAX_WCHUNKSIZE must be at least 3 in single master mode
*;				 
#else
#error  DRXDAP_MAX_WCHUNKSIZE must be at least 5 in multi master mode
*;				 
#endif
#else
#if DRXDAP_SINGLE_MASTER
#error  DRXDAP_MAX_WCHUNKSIZE must be at least 5 in single master mode
*;				 
#else
#error  DRXDAP_MAX_WCHUNKSIZE must be at least 7 in multi master mode
*;				 
#endif
#endif
#endif
#if !defined(DRXDAP_MAX_RCHUNKSIZE)
#define  DRXDAP_MAX_RCHUNKSIZE 254
#endif
#if  DRXDAP_MAX_RCHUNKSIZE < 2
#error  DRXDAP_MAX_RCHUNKSIZE must be at least 2
*;				 
#endif
#if  DRXDAP_MAX_RCHUNKSIZE & 1
#error  DRXDAP_MAX_RCHUNKSIZE must be even
*;				 
#endif
#define DRXDAP_FASI_RMW           0x10000000
#define DRXDAP_FASI_BROADCAST     0x20000000
#define DRXDAP_FASI_CLEARCRC      0x80000000
#define DRXDAP_FASI_SINGLE_MASTER 0xC0000000
#define DRXDAP_FASI_MULTI_MASTER  0x40000000
#define DRXDAP_FASI_SMM_SWITCH    0x40000000	 
#define DRXDAP_FASI_MODEFLAGS     0xC0000000
#define DRXDAP_FASI_FLAGS         0xF0000000
#define DRXDAP_FASI_ADDR2BLOCK(addr)  (((addr)>>22)&0x3F)
#define DRXDAP_FASI_ADDR2BANK(addr)   (((addr)>>16)&0x3F)
#define DRXDAP_FASI_ADDR2OFFSET(addr) ((addr)&0x7FFF)
#define DRXDAP_FASI_SHORT_FORMAT(addr)     (((addr) & 0xFC30FF80) == 0)
#define DRXDAP_FASI_LONG_FORMAT(addr)      (((addr) & 0xFC30FF80) != 0)
#define DRXDAP_FASI_OFFSET_TOO_LARGE(addr) (((addr) & 0x00008000) != 0)
#endif				 
