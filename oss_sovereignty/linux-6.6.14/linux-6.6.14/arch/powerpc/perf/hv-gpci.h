#ifndef LINUX_POWERPC_PERF_HV_GPCI_H_
#define LINUX_POWERPC_PERF_HV_GPCI_H_
#define COUNTER_INFO_VERSION_CURRENT 0x8
enum {
	HV_GPCI_CM_GA = (1 << 7),
	HV_GPCI_CM_EXPANDED = (1 << 6),
	HV_GPCI_CM_LAB = (1 << 5)
};
#define REQUEST_FILE "../hv-gpci-requests.h"
#define NAME_LOWER hv_gpci
#define NAME_UPPER HV_GPCI
#define ENABLE_EVENTS_COUNTERINFO_V6
#include "req-gen/perf.h"
#undef REQUEST_FILE
#undef NAME_LOWER
#undef NAME_UPPER
#endif
