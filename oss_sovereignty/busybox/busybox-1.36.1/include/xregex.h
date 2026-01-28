#ifndef BB_REGEX_H
#define BB_REGEX_H 1
#include <regex.h>
PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN
char* regcomp_or_errmsg(regex_t *preg, const char *regex, int cflags) FAST_FUNC;
void xregcomp(regex_t *preg, const char *regex, int cflags) FAST_FUNC;
POP_SAVED_FUNCTION_VISIBILITY
#endif
