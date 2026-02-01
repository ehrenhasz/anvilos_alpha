#include "mbedtls/error.h"
#include <string.h>
#include <stdio.h>





int test_code(int code, char *str) {
    char buf[100];
    int ok = 1;
    int res;

    
    memset(buf, -3, 100);
    mbedtls_strerror(code, buf + 4, 0);
    for (int i = 0; i < 10; i++) {
        if (buf[i] != -3) {
            printf("Error: guard overwritten buflen=0 i=%d buf[i]=%d\n", i, buf[i]);
            ok = 0;
        }
    }

    
    for (size_t buflen = 1; buflen < 90; buflen++) {
        memset(buf, -3, 100);
        mbedtls_strerror(code, buf + 4, buflen);
        for (int i = 0; i < 4; i++) {
            if (buf[i] != -3) {
                printf("Error: pre-guard overwritten buflen=%d i=%d buf[i]=%d\n", buflen, i, buf[i]);
                ok = 0;
            }
        }
        for (int i = 4 + buflen; i < 100; i++) {
            if (buf[i] != -3) {
                printf("Error: post-guard overwritten buflen=%d i=%d buf[i]=%d\n", buflen, i, buf[i]);
                ok = 0;
            }
        }
        char exp[100];
        strncpy(exp, str, buflen);
        exp[buflen - 1] = 0;
        if (strcmp(buf + 4, exp) != 0) {
            printf("Error: expected %s, got %s\n", exp, buf);
            ok = 0;
        }
    }

    printf("Test %x -> %s is %s\n", code, str, ok?"OK":"*** BAD ***");
}

int main() {
    test_code(0x7200, "MBEDTLS_ERR_SSL_INVALID_RECORD");
    test_code(0x7780, "MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE");
    test_code(0x0074, "MBEDTLS_ERR_SHA256_BAD_INPUT_DATA");
    test_code(0x6600 | 0x0074, "MBEDTLS_ERR_SSL_INVALID_VERIFY_HASH+MBEDTLS_ERR_SHA256_BAD_INPUT_DATA");
    test_code(103, "MBEDTLS_ERR_UNKNOWN (0x0067)");
}
