#ifndef __ASM_PVCLOCK_ABI_H
#define __ASM_PVCLOCK_ABI_H
struct pvclock_vcpu_stolen_time {
	__le32 revision;
	__le32 attributes;
	__le64 stolen_time;
	u8 padding[48];
} __packed;
#endif
