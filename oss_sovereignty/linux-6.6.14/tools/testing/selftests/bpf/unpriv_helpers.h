

#include <stdbool.h>

#define UNPRIV_SYSCTL "kernel/unprivileged_bpf_disabled"

bool get_unpriv_disabled(void);
