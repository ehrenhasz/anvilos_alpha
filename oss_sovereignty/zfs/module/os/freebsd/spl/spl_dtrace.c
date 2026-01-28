#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/types.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/sdt.h>
SDT_PROBE_DEFINE1(sdt, , , set__error, "int");
