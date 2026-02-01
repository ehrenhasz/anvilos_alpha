 

 

 
#include <asm/byteorder.h>

#ifdef __BIG_ENDIAN

static inline void
lpfc_memcpy_to_slim(void __iomem *dest, void *src, unsigned int bytes)
{
	uint32_t __iomem *dest32;
	uint32_t *src32;
	unsigned int four_bytes;


	dest32  = (uint32_t __iomem *) dest;
	src32  = (uint32_t *) src;

	 
	for (four_bytes = bytes /4; four_bytes > 0; four_bytes--) {
		writel( *src32, dest32);
		readl(dest32);  
		dest32++;
		src32++;
	}

	return;
}

static inline void
lpfc_memcpy_from_slim( void *dest, void __iomem *src, unsigned int bytes)
{
	uint32_t *dest32;
	uint32_t __iomem *src32;
	unsigned int four_bytes;


	dest32  = (uint32_t *) dest;
	src32  = (uint32_t __iomem *) src;

	 
	for (four_bytes = bytes /4; four_bytes > 0; four_bytes--) {
		*dest32 = readl( src32);
		dest32++;
		src32++;
	}

	return;
}

#else

static inline void
lpfc_memcpy_to_slim( void __iomem *dest, void *src, unsigned int bytes)
{
	 
	__iowrite32_copy(dest, src, bytes / sizeof(uint32_t));
}

static inline void
lpfc_memcpy_from_slim( void *dest, void __iomem *src, unsigned int bytes)
{
	 
	memcpy_fromio( dest, src, bytes);
}

#endif	 
