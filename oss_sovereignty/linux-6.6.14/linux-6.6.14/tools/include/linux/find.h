#ifndef _TOOLS_LINUX_FIND_H_
#define _TOOLS_LINUX_FIND_H_
#ifndef _TOOLS_LINUX_BITMAP_H
#error tools: only <linux/bitmap.h> can be included directly
#endif
#include <linux/bitops.h>
unsigned long _find_next_bit(const unsigned long *addr1, unsigned long nbits,
				unsigned long start);
unsigned long _find_next_and_bit(const unsigned long *addr1, const unsigned long *addr2,
					unsigned long nbits, unsigned long start);
unsigned long _find_next_zero_bit(const unsigned long *addr, unsigned long nbits,
					 unsigned long start);
extern unsigned long _find_first_bit(const unsigned long *addr, unsigned long size);
extern unsigned long _find_first_and_bit(const unsigned long *addr1,
					 const unsigned long *addr2, unsigned long size);
extern unsigned long _find_first_zero_bit(const unsigned long *addr, unsigned long size);
#ifndef find_next_bit
static inline
unsigned long find_next_bit(const unsigned long *addr, unsigned long size,
			    unsigned long offset)
{
	if (small_const_nbits(size)) {
		unsigned long val;
		if (unlikely(offset >= size))
			return size;
		val = *addr & GENMASK(size - 1, offset);
		return val ? __ffs(val) : size;
	}
	return _find_next_bit(addr, size, offset);
}
#endif
#ifndef find_next_and_bit
static inline
unsigned long find_next_and_bit(const unsigned long *addr1,
		const unsigned long *addr2, unsigned long size,
		unsigned long offset)
{
	if (small_const_nbits(size)) {
		unsigned long val;
		if (unlikely(offset >= size))
			return size;
		val = *addr1 & *addr2 & GENMASK(size - 1, offset);
		return val ? __ffs(val) : size;
	}
	return _find_next_and_bit(addr1, addr2, size, offset);
}
#endif
#ifndef find_next_zero_bit
static inline
unsigned long find_next_zero_bit(const unsigned long *addr, unsigned long size,
				 unsigned long offset)
{
	if (small_const_nbits(size)) {
		unsigned long val;
		if (unlikely(offset >= size))
			return size;
		val = *addr | ~GENMASK(size - 1, offset);
		return val == ~0UL ? size : ffz(val);
	}
	return _find_next_zero_bit(addr, size, offset);
}
#endif
#ifndef find_first_bit
static inline
unsigned long find_first_bit(const unsigned long *addr, unsigned long size)
{
	if (small_const_nbits(size)) {
		unsigned long val = *addr & GENMASK(size - 1, 0);
		return val ? __ffs(val) : size;
	}
	return _find_first_bit(addr, size);
}
#endif
#ifndef find_first_and_bit
static inline
unsigned long find_first_and_bit(const unsigned long *addr1,
				 const unsigned long *addr2,
				 unsigned long size)
{
	if (small_const_nbits(size)) {
		unsigned long val = *addr1 & *addr2 & GENMASK(size - 1, 0);
		return val ? __ffs(val) : size;
	}
	return _find_first_and_bit(addr1, addr2, size);
}
#endif
#ifndef find_first_zero_bit
static inline
unsigned long find_first_zero_bit(const unsigned long *addr, unsigned long size)
{
	if (small_const_nbits(size)) {
		unsigned long val = *addr | ~GENMASK(size - 1, 0);
		return val == ~0UL ? size : ffz(val);
	}
	return _find_first_zero_bit(addr, size);
}
#endif
#endif  
