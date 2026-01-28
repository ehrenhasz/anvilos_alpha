#ifndef _SYS_BITMAP_H
#define	_SYS_BITMAP_H
#ifdef	__cplusplus
extern "C" {
#endif
#ifdef _LP64
#define	BT_ULSHIFT	6  
#define	BT_ULSHIFT32	5  
#else
#define	BT_ULSHIFT	5  
#endif
#define	BT_NBIPUL	(1 << BT_ULSHIFT)	 
#define	BT_ULMASK	(BT_NBIPUL - 1)		 
#define	BT_WIM(bitmap, bitindex) \
	((bitmap)[(bitindex) >> BT_ULSHIFT])
#define	BT_BIW(bitindex) \
	(1UL << ((bitindex) & BT_ULMASK))
#define	BT_BITOUL(nbits) \
	(((nbits) + BT_NBIPUL - 1l) / BT_NBIPUL)
#define	BT_SIZEOFMAP(nbits) \
	(BT_BITOUL(nbits) * sizeof (ulong_t))
#define	BT_TEST(bitmap, bitindex) \
	((BT_WIM((bitmap), (bitindex)) & BT_BIW(bitindex)) ? 1 : 0)
#define	BT_SET(bitmap, bitindex) \
	{ BT_WIM((bitmap), (bitindex)) |= BT_BIW(bitindex); }
#define	BT_CLEAR(bitmap, bitindex) \
	{ BT_WIM((bitmap), (bitindex)) &= ~BT_BIW(bitindex); }
#ifdef	__cplusplus
}
#endif
#endif	 
