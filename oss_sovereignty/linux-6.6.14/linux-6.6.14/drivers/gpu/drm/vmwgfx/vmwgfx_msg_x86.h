#ifndef _VMWGFX_MSG_X86_H
#define _VMWGFX_MSG_X86_H
#if defined(__i386__) || defined(__x86_64__)
#include <asm/vmware.h>
#define VMW_PORT(cmd, in_ebx, in_si, in_di,	\
                 flags, magic,		\
                 eax, ebx, ecx, edx, si, di)	\
({						\
        asm volatile (VMWARE_HYPERCALL :	\
                "=a"(eax),			\
                "=b"(ebx),			\
                "=c"(ecx),			\
                "=d"(edx),			\
                "=S"(si),			\
                "=D"(di) :			\
                "a"(magic),			\
                "b"(in_ebx),			\
                "c"(cmd),			\
                "d"(flags),			\
                "S"(in_si),			\
                "D"(in_di) :			\
                "memory");			\
})
#ifdef __x86_64__
#define VMW_PORT_HB_OUT(cmd, in_ecx, in_si, in_di,	\
                        flags, magic, bp,		\
                        eax, ebx, ecx, edx, si, di)	\
({							\
        asm volatile (					\
		UNWIND_HINT_SAVE			\
		"push %%rbp;"				\
		UNWIND_HINT_UNDEFINED			\
                "mov %12, %%rbp;"			\
                VMWARE_HYPERCALL_HB_OUT			\
                "pop %%rbp;"				\
		UNWIND_HINT_RESTORE :			\
                "=a"(eax),				\
                "=b"(ebx),				\
                "=c"(ecx),				\
                "=d"(edx),				\
                "=S"(si),				\
                "=D"(di) :				\
                "a"(magic),				\
                "b"(cmd),				\
                "c"(in_ecx),				\
                "d"(flags),				\
                "S"(in_si),				\
                "D"(in_di),				\
                "r"(bp) :				\
                "memory", "cc");			\
})
#define VMW_PORT_HB_IN(cmd, in_ecx, in_si, in_di,	\
                       flags, magic, bp,		\
                       eax, ebx, ecx, edx, si, di)	\
({							\
        asm volatile (					\
		UNWIND_HINT_SAVE			\
		"push %%rbp;"				\
		UNWIND_HINT_UNDEFINED			\
                "mov %12, %%rbp;"			\
                VMWARE_HYPERCALL_HB_IN			\
                "pop %%rbp;"				\
		UNWIND_HINT_RESTORE :			\
                "=a"(eax),				\
                "=b"(ebx),				\
                "=c"(ecx),				\
                "=d"(edx),				\
                "=S"(si),				\
                "=D"(di) :				\
                "a"(magic),				\
                "b"(cmd),				\
                "c"(in_ecx),				\
                "d"(flags),				\
                "S"(in_si),				\
                "D"(in_di),				\
                "r"(bp) :				\
                "memory", "cc");			\
})
#elif defined(__i386__)
#define VMW_PORT_HB_OUT(cmd, in_ecx, in_si, in_di,	\
                        flags, magic, bp,		\
                        eax, ebx, ecx, edx, si, di)	\
({							\
        asm volatile ("push %12;"			\
                "push %%ebp;"				\
                "mov 0x04(%%esp), %%ebp;"		\
                VMWARE_HYPERCALL_HB_OUT			\
                "pop %%ebp;"				\
                "add $0x04, %%esp;" :			\
                "=a"(eax),				\
                "=b"(ebx),				\
                "=c"(ecx),				\
                "=d"(edx),				\
                "=S"(si),				\
                "=D"(di) :				\
                "a"(magic),				\
                "b"(cmd),				\
                "c"(in_ecx),				\
                "d"(flags),				\
                "S"(in_si),				\
                "D"(in_di),				\
                "m"(bp) :				\
                "memory", "cc");			\
})
#define VMW_PORT_HB_IN(cmd, in_ecx, in_si, in_di,	\
                       flags, magic, bp,		\
                       eax, ebx, ecx, edx, si, di)	\
({							\
        asm volatile ("push %12;"			\
                "push %%ebp;"				\
                "mov 0x04(%%esp), %%ebp;"		\
                VMWARE_HYPERCALL_HB_IN			\
                "pop %%ebp;"				\
                "add $0x04, %%esp;" :			\
                "=a"(eax),				\
                "=b"(ebx),				\
                "=c"(ecx),				\
                "=d"(edx),				\
                "=S"(si),				\
                "=D"(di) :				\
                "a"(magic),				\
                "b"(cmd),				\
                "c"(in_ecx),				\
                "d"(flags),				\
                "S"(in_si),				\
                "D"(in_di),				\
                "m"(bp) :				\
                "memory", "cc");			\
})
#endif  
#endif  
#endif  
