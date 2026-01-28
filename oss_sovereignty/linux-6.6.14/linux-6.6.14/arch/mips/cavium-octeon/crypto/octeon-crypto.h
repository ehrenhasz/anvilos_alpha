#ifndef __LINUX_OCTEON_CRYPTO_H
#define __LINUX_OCTEON_CRYPTO_H
#include <linux/sched.h>
#include <asm/mipsregs.h>
#define OCTEON_CR_OPCODE_PRIORITY 300
extern unsigned long octeon_crypto_enable(struct octeon_cop2_state *state);
extern void octeon_crypto_disable(struct octeon_cop2_state *state,
				  unsigned long flags);
#define write_octeon_64bit_hash_dword(value, index)	\
do {							\
	__asm__ __volatile__ (				\
	"dmtc2 %[rt],0x0048+" STR(index)		\
	:						\
	: [rt] "d" (cpu_to_be64(value)));		\
} while (0)
#define read_octeon_64bit_hash_dword(index)		\
({							\
	__be64 __value;					\
							\
	__asm__ __volatile__ (				\
	"dmfc2 %[rt],0x0048+" STR(index)		\
	: [rt] "=d" (__value)				\
	: );						\
							\
	be64_to_cpu(__value);				\
})
#define write_octeon_64bit_block_dword(value, index)	\
do {							\
	__asm__ __volatile__ (				\
	"dmtc2 %[rt],0x0040+" STR(index)		\
	:						\
	: [rt] "d" (cpu_to_be64(value)));		\
} while (0)
#define octeon_md5_start(value)				\
do {							\
	__asm__ __volatile__ (				\
	"dmtc2 %[rt],0x4047"				\
	:						\
	: [rt] "d" (cpu_to_be64(value)));		\
} while (0)
#define octeon_sha1_start(value)			\
do {							\
	__asm__ __volatile__ (				\
	"dmtc2 %[rt],0x4057"				\
	:						\
	: [rt] "d" (value));				\
} while (0)
#define octeon_sha256_start(value)			\
do {							\
	__asm__ __volatile__ (				\
	"dmtc2 %[rt],0x404f"				\
	:						\
	: [rt] "d" (value));				\
} while (0)
#define write_octeon_64bit_hash_sha512(value, index)	\
do {							\
	__asm__ __volatile__ (				\
	"dmtc2 %[rt],0x0250+" STR(index)		\
	:						\
	: [rt] "d" (value));				\
} while (0)
#define read_octeon_64bit_hash_sha512(index)		\
({							\
	u64 __value;					\
							\
	__asm__ __volatile__ (				\
	"dmfc2 %[rt],0x0250+" STR(index)		\
	: [rt] "=d" (__value)				\
	: );						\
							\
	__value;					\
})
#define write_octeon_64bit_block_sha512(value, index)	\
do {							\
	__asm__ __volatile__ (				\
	"dmtc2 %[rt],0x0240+" STR(index)		\
	:						\
	: [rt] "d" (value));				\
} while (0)
#define octeon_sha512_start(value)			\
do {							\
	__asm__ __volatile__ (				\
	"dmtc2 %[rt],0x424f"				\
	:						\
	: [rt] "d" (value));				\
} while (0)
#define octeon_sha1_start(value)			\
do {							\
	__asm__ __volatile__ (				\
	"dmtc2 %[rt],0x4057"				\
	:						\
	: [rt] "d" (value));				\
} while (0)
#define octeon_sha256_start(value)			\
do {							\
	__asm__ __volatile__ (				\
	"dmtc2 %[rt],0x404f"				\
	:						\
	: [rt] "d" (value));				\
} while (0)
#define write_octeon_64bit_hash_sha512(value, index)	\
do {							\
	__asm__ __volatile__ (				\
	"dmtc2 %[rt],0x0250+" STR(index)		\
	:						\
	: [rt] "d" (value));				\
} while (0)
#define read_octeon_64bit_hash_sha512(index)		\
({							\
	u64 __value;					\
							\
	__asm__ __volatile__ (				\
	"dmfc2 %[rt],0x0250+" STR(index)		\
	: [rt] "=d" (__value)				\
	: );						\
							\
	__value;					\
})
#define write_octeon_64bit_block_sha512(value, index)	\
do {							\
	__asm__ __volatile__ (				\
	"dmtc2 %[rt],0x0240+" STR(index)		\
	:						\
	: [rt] "d" (value));				\
} while (0)
#define octeon_sha512_start(value)			\
do {							\
	__asm__ __volatile__ (				\
	"dmtc2 %[rt],0x424f"				\
	:						\
	: [rt] "d" (value));				\
} while (0)
#endif  
