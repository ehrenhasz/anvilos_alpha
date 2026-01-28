

#ifndef INET_COMMON_H
#define INET_COMMON_H 1

PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN


int INET_resolve(const char *name, struct sockaddr_in *s_in, int hostfirst) FAST_FUNC;



int INET6_resolve(const char *name, struct sockaddr_in6 *sin6) FAST_FUNC;


char *INET_rresolve(struct sockaddr_in *s_in, int numeric, uint32_t netmask) FAST_FUNC;
char *INET6_rresolve(struct sockaddr_in6 *sin6, int numeric) FAST_FUNC;

POP_SAVED_FUNCTION_VISIBILITY

#endif
