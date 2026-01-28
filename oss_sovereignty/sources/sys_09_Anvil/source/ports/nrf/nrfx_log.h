

#ifndef NRFX_LOG_H
#define NRFX_LOG_H

#include <stdio.h>
#include "mphalport.h"
#include "nrfx_config.h"

#define LOG_TEST_UART 1

#define TEST_MODULE_IMPL(x, y) LOG_TEST_##x == LOG_TEST_##y
#define TEST_MODULE(x, y) TEST_MODULE_IMPL(x, y)

#if (!defined(NRFX_LOG_ENABLED) || (NRFX_LOG_ENABLED == 0)) || \
    (TEST_MODULE(NRFX_LOG_MODULE, UART) && defined(NRFX_LOG_UART_DISABLED) && NRFX_LOG_UART_DISABLED)

    #define NRFX_LOG_DEBUG(fmt, ...)
    #define NRFX_LOG_ERROR(fmt, ...)
    #define NRFX_LOG_WARNING(fmt, ...)
    #define NRFX_LOG_INFO(fmt, ...)

    #define NRFX_LOG_HEXDUMP_ERROR(p_memory, length)
    #define NRFX_LOG_HEXDUMP_WARNING(p_memory, length)
    #define NRFX_LOG_HEXDUMP_INFO(p_memory, length)
    #define NRFX_LOG_HEXDUMP_DEBUG(p_memory, length)
    #define NRFX_LOG_ERROR_STRING_GET(error_code) ""

#else

    #define VALUE_TO_STR(x) #x
    #define VALUE(x) VALUE_TO_STR(x)

    #define LOG_PRINTF(fmt, ...) \
    do { \
        printf("%s: ", VALUE(NRFX_LOG_MODULE)); \
        printf(fmt,##__VA_ARGS__); \
        printf("\n"); \
    } while (0)

    #define NRFX_LOG_DEBUG   LOG_PRINTF
    #define NRFX_LOG_ERROR   LOG_PRINTF
    #define NRFX_LOG_WARNING LOG_PRINTF
    #define NRFX_LOG_INFO    LOG_PRINTF


    #define NRFX_LOG_HEXDUMP_ERROR(p_memory, length)

    #define NRFX_LOG_HEXDUMP_WARNING(p_memory, length)

    #define NRFX_LOG_HEXDUMP_INFO(p_memory, length)

    #define NRFX_LOG_HEXDUMP_DEBUG(p_memory, length)

    #define NRFX_LOG_ERROR_STRING_GET(error_code) \
    nrfx_error_code_lookup(error_code)

#endif 

#endif 
