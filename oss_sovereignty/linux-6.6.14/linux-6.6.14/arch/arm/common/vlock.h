#ifndef __VLOCK_H
#define __VLOCK_H
#include <asm/mcpm.h>
#define VLOCK_OWNER_OFFSET	0
#define VLOCK_VOTING_OFFSET	4
#define VLOCK_VOTING_SIZE	((MAX_CPUS_PER_CLUSTER + 3) / 4 * 4)
#define VLOCK_SIZE		(VLOCK_VOTING_OFFSET + VLOCK_VOTING_SIZE)
#define VLOCK_OWNER_NONE	0
#endif  
