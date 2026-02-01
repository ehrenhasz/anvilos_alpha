 

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "py/runtime.h"
#include "shared/netutils/netutils.h"


mp_obj_t netutils_format_ipv4_addr(uint8_t *ip, netutils_endian_t endian) {
    char ip_str[16];
    mp_uint_t ip_len;
    if (endian == NETUTILS_LITTLE) {
        ip_len = snprintf(ip_str, 16, "%u.%u.%u.%u", ip[3], ip[2], ip[1], ip[0]);
    } else {
        ip_len = snprintf(ip_str, 16, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    }
    return mp_obj_new_str(ip_str, ip_len);
}



mp_obj_t netutils_format_inet_addr(uint8_t *ip, mp_uint_t port, netutils_endian_t endian) {
    mp_obj_t tuple[2] = {
        tuple[0] = netutils_format_ipv4_addr(ip, endian),
        tuple[1] = mp_obj_new_int(port),
    };
    return mp_obj_new_tuple(2, tuple);
}

void netutils_parse_ipv4_addr(mp_obj_t addr_in, uint8_t *out_ip, netutils_endian_t endian) {
    size_t addr_len;
    const char *addr_str = mp_obj_str_get_data(addr_in, &addr_len);
    if (addr_len == 0) {
        
        memset(out_ip, 0, NETUTILS_IPV4ADDR_BUFSIZE);
        return;
    }
    const char *s = addr_str;
    const char *s_top = addr_str + addr_len;
    for (mp_uint_t i = 3; ; i--) {
        mp_uint_t val = 0;
        for (; s < s_top && *s != '.'; s++) {
            val = val * 10 + *s - '0';
        }
        if (endian == NETUTILS_LITTLE) {
            out_ip[i] = val;
        } else {
            out_ip[NETUTILS_IPV4ADDR_BUFSIZE - 1 - i] = val;
        }
        if (i == 0 && s == s_top) {
            return;
        } else if (i > 0 && s < s_top && *s == '.') {
            s++;
        } else {
            mp_raise_ValueError(MP_ERROR_TEXT("invalid arguments"));
        }
    }
}



mp_uint_t netutils_parse_inet_addr(mp_obj_t addr_in, uint8_t *out_ip, netutils_endian_t endian) {
    mp_obj_t *addr_items;
    mp_obj_get_array_fixed_n(addr_in, 2, &addr_items);
    netutils_parse_ipv4_addr(addr_items[0], out_ip, endian);
    return mp_obj_get_int(addr_items[1]);
}
