 
#ifndef MICROPY_INCLUDED_LIB_NETUTILS_NETUTILS_H
#define MICROPY_INCLUDED_LIB_NETUTILS_NETUTILS_H

#include "py/obj.h"

#define NETUTILS_IPV4ADDR_BUFSIZE    4

#define NETUTILS_TRACE_IS_TX        (0x0001)
#define NETUTILS_TRACE_PAYLOAD      (0x0002)
#define NETUTILS_TRACE_NEWLINE      (0x0004)

typedef enum _netutils_endian_t {
    NETUTILS_LITTLE,
    NETUTILS_BIG,
} netutils_endian_t;


mp_obj_t netutils_format_ipv4_addr(uint8_t *ip, netutils_endian_t endian);



mp_obj_t netutils_format_inet_addr(uint8_t *ip, mp_uint_t port, netutils_endian_t endian);

void netutils_parse_ipv4_addr(mp_obj_t addr_in, uint8_t *out_ip, netutils_endian_t endian);



mp_uint_t netutils_parse_inet_addr(mp_obj_t addr_in, uint8_t *out_ip, netutils_endian_t endian);

void netutils_ethernet_trace(const mp_print_t *print, size_t len, const uint8_t *buf, unsigned int flags);

#endif 
