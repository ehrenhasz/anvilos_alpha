echo "IDF_PATH=$IDF_PATH"
MBEDTLS=$IDF_PATH/components/mbedtls/mbedtls
patch -o esp32_generate_errors.pl $MBEDTLS/scripts/generate_errors.pl <generate_errors.diff
perl ./esp32_generate_errors.pl $MBEDTLS/include/mbedtls . esp32_mbedtls_errors.c
