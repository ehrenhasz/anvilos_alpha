#ifndef __ASM_MACH_GENERIC_MANGLE_PORT_H
#define __ASM_MACH_GENERIC_MANGLE_PORT_H
#define __swizzle_addr_b(port)	(port)
#define __swizzle_addr_w(port)	(port)
#define __swizzle_addr_l(port)	(port)
#define __swizzle_addr_q(port)	(port)
#if defined(CONFIG_SWAP_IO_SPACE)
# define ioswabb(a, x)		(x)
# define __mem_ioswabb(a, x)	(x)
# define ioswabw(a, x)		le16_to_cpu((__force __le16)(x))
# define __mem_ioswabw(a, x)	(x)
# define ioswabl(a, x)		le32_to_cpu((__force __le32)(x))
# define __mem_ioswabl(a, x)	(x)
# define ioswabq(a, x)		le64_to_cpu((__force __le64)(x))
# define __mem_ioswabq(a, x)	(x)
#else
# define ioswabb(a, x)		(x)
# define __mem_ioswabb(a, x)	(x)
# define ioswabw(a, x)		(x)
# define __mem_ioswabw(a, x)	((__force u16)cpu_to_le16(x))
# define ioswabl(a, x)		(x)
# define __mem_ioswabl(a, x)	((__force u32)cpu_to_le32(x))
# define ioswabq(a, x)		(x)
# define __mem_ioswabq(a, x)	((__force u64)cpu_to_le64(x))
#endif
#endif  
