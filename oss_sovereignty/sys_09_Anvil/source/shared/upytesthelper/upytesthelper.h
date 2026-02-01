 

#include <stdbool.h>
#include <stdio.h>
#include "py/mpconfig.h"
#include "lib/tinytest/tinytest.h"
#include "lib/tinytest/tinytest_macros.h"

void upytest_set_heap(void *start, void *end);
void upytest_set_expected_output(const char *output, unsigned len);
void upytest_execute_test(const char *src);
void upytest_output(const char *str, mp_uint_t len);
bool upytest_is_failed(void);
