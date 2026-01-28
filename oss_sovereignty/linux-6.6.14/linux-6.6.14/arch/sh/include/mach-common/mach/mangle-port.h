#ifndef __MACH_COMMON_MANGLE_PORT_H
#define __MACH_COMMON_MANGLE_PORT_H
#if defined(CONFIG_SWAP_IO_SPACE)
# define ioswabb(x)		(x)
# define __mem_ioswabb(x)	(x)
# define ioswabw(x)		le16_to_cpu(x)
# define __mem_ioswabw(x)	(x)
# define ioswabl(x)		le32_to_cpu(x)
# define __mem_ioswabl(x)	(x)
# define ioswabq(x)		le64_to_cpu(x)
# define __mem_ioswabq(x)	(x)
#else
# define ioswabb(x)		(x)
# define __mem_ioswabb(x)	(x)
# define ioswabw(x)		(x)
# define __mem_ioswabw(x)	cpu_to_le16(x)
# define ioswabl(x)		(x)
# define __mem_ioswabl(x)	cpu_to_le32(x)
# define ioswabq(x)		(x)
# define __mem_ioswabq(x)	cpu_to_le32(x)
#endif
#endif  
