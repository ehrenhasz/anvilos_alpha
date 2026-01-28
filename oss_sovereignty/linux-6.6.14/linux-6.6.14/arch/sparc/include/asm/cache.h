#ifndef _SPARC_CACHE_H
#define _SPARC_CACHE_H
#define ARCH_SLAB_MINALIGN	__alignof__(unsigned long long)
#define L1_CACHE_SHIFT 5
#define L1_CACHE_BYTES 32
#ifdef CONFIG_SPARC32
#define SMP_CACHE_BYTES_SHIFT 5
#else
#define SMP_CACHE_BYTES_SHIFT 6
#endif
#define SMP_CACHE_BYTES (1 << SMP_CACHE_BYTES_SHIFT)
#define __read_mostly __section(".data..read_mostly")
#endif  
