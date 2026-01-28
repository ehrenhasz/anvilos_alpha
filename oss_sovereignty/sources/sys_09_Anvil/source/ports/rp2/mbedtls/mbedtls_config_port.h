
#ifndef MICROPY_INCLUDED_MBEDTLS_CONFIG_H
#define MICROPY_INCLUDED_MBEDTLS_CONFIG_H


#define MBEDTLS_ECP_NIST_OPTIM
#define MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED


#define MBEDTLS_GCM_C


#include <time.h>
time_t rp2_rtctime_seconds(time_t *timer);
#define MBEDTLS_PLATFORM_TIME_MACRO rp2_rtctime_seconds
#define MBEDTLS_PLATFORM_MS_TIME_ALT mbedtls_ms_time


#define MICROPY_MBEDTLS_CONFIG_BARE_METAL (1)


#include "extmod/mbedtls/mbedtls_config_common.h"

#endif 
