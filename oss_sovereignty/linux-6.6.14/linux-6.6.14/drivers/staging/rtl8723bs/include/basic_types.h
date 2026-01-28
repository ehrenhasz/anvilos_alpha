#ifndef __BASIC_TYPES_H__
#define __BASIC_TYPES_H__
#define SUCCESS	0
#define FAIL	(-1)
#include <linux/types.h>
#define FIELD_OFFSET(s, field)	((__kernel_ssize_t)&((s *)(0))->field)
#define SIZE_PTR __kernel_size_t
#define SSIZE_PTR __kernel_ssize_t
#define EF1Byte	(u8)
#define EF2Byte		le16_to_cpu
#define EF4Byte	le32_to_cpu
#define EF1BYTE(_val)		\
	((u8)(_val))
#define EF2BYTE(_val)		\
	(le16_to_cpu(_val))
#define EF4BYTE(_val)		\
	(le32_to_cpu(_val))
#define READEF1BYTE(_ptr)	\
	EF1BYTE(*((u8 *)(_ptr)))
#define READEF2BYTE(_ptr)	\
	EF2BYTE(*(_ptr))
#define READEF4BYTE(_ptr)	\
	EF4BYTE(*(_ptr))
#define WRITEEF1BYTE(_ptr, _val)			\
	do {						\
		(*((u8 *)(_ptr))) = EF1BYTE(_val);	\
	} while (0)
#define WRITEEF2BYTE(_ptr, _val)			\
	do {						\
		(*((u16 *)(_ptr))) = EF2BYTE(_val);	\
	} while (0)
#define WRITEEF4BYTE(_ptr, _val)			\
	do {						\
		(*((u32 *)(_ptr))) = EF2BYTE(_val);	\
	} while (0)
#define BIT_LEN_MASK_32(__bitlen)	 \
	(0xFFFFFFFF >> (32 - (__bitlen)))
#define BIT_LEN_MASK_16(__bitlen)	 \
	(0xFFFF >> (16 - (__bitlen)))
#define BIT_LEN_MASK_8(__bitlen) \
	(0xFF >> (8 - (__bitlen)))
#define BIT_OFFSET_LEN_MASK_32(__bitoffset, __bitlen) \
	(BIT_LEN_MASK_32(__bitlen) << (__bitoffset))
#define BIT_OFFSET_LEN_MASK_16(__bitoffset, __bitlen) \
	(BIT_LEN_MASK_16(__bitlen) << (__bitoffset))
#define BIT_OFFSET_LEN_MASK_8(__bitoffset, __bitlen) \
	(BIT_LEN_MASK_8(__bitlen) << (__bitoffset))
#define LE_P4BYTE_TO_HOST_4BYTE(__pstart) \
	(EF4BYTE(*((__le32 *)(__pstart))))
#define LE_P2BYTE_TO_HOST_2BYTE(__pstart) \
	(EF2BYTE(*((__le16 *)(__pstart))))
#define LE_P1BYTE_TO_HOST_1BYTE(__pstart) \
	(EF1BYTE(*((u8 *)(__pstart))))
#define LE_BITS_TO_4BYTE(__pstart, __bitoffset, __bitlen) \
	(\
		(LE_P4BYTE_TO_HOST_4BYTE(__pstart) >> (__bitoffset))  & \
		BIT_LEN_MASK_32(__bitlen) \
	)
#define LE_BITS_TO_2BYTE(__pstart, __bitoffset, __bitlen) \
	(\
		(LE_P2BYTE_TO_HOST_2BYTE(__pstart) >> (__bitoffset)) & \
		BIT_LEN_MASK_16(__bitlen) \
	)
#define LE_BITS_TO_1BYTE(__pstart, __bitoffset, __bitlen) \
	(\
		(LE_P1BYTE_TO_HOST_1BYTE(__pstart) >> (__bitoffset)) & \
		BIT_LEN_MASK_8(__bitlen) \
	)
#define LE_BITS_CLEARED_TO_4BYTE(__pstart, __bitoffset, __bitlen) \
	(\
		LE_P4BYTE_TO_HOST_4BYTE(__pstart)  & \
		(~BIT_OFFSET_LEN_MASK_32(__bitoffset, __bitlen)) \
	)
#define LE_BITS_CLEARED_TO_2BYTE(__pstart, __bitoffset, __bitlen) \
	(\
		LE_P2BYTE_TO_HOST_2BYTE(__pstart) & \
		(~BIT_OFFSET_LEN_MASK_16(__bitoffset, __bitlen)) \
	)
#define LE_BITS_CLEARED_TO_1BYTE(__pstart, __bitoffset, __bitlen) \
	(\
		LE_P1BYTE_TO_HOST_1BYTE(__pstart) & \
		(~BIT_OFFSET_LEN_MASK_8(__bitoffset, __bitlen)) \
	)
#define SET_BITS_TO_LE_4BYTE(__pstart, __bitoffset, __bitlen, __val) \
		*((u32 *)(__pstart)) =				\
		(						\
		LE_BITS_CLEARED_TO_4BYTE(__pstart, __bitoffset, __bitlen) | \
		((((u32)__val) & BIT_LEN_MASK_32(__bitlen)) << (__bitoffset)) \
		)
#define SET_BITS_TO_LE_2BYTE(__pstart, __bitoffset, __bitlen, __val) \
		*((u16 *)(__pstart)) =				\
		(					\
		LE_BITS_CLEARED_TO_2BYTE(__pstart, __bitoffset, __bitlen) | \
		((((u16)__val) & BIT_LEN_MASK_16(__bitlen)) << (__bitoffset)) \
		);
#define SET_BITS_TO_LE_1BYTE(__pstart, __bitoffset, __bitlen, __val) \
		*((u8 *)(__pstart)) = EF1BYTE			\
		(					\
		LE_BITS_CLEARED_TO_1BYTE(__pstart, __bitoffset, __bitlen) | \
		((((u8)__val) & BIT_LEN_MASK_8(__bitlen)) << (__bitoffset)) \
		)
#define LE_BITS_CLEARED_TO_1BYTE_8BIT(__pStart, __BitOffset, __BitLen) \
	(\
		LE_P1BYTE_TO_HOST_1BYTE(__pStart) \
	)
#define SET_BITS_TO_LE_1BYTE_8BIT(__pStart, __BitOffset, __BitLen, __Value) \
{ \
	*((u8 *)(__pStart)) = \
		EF1Byte(\
			LE_BITS_CLEARED_TO_1BYTE_8BIT(__pStart, __BitOffset, __BitLen) \
			| \
			((u8)__Value) \
		); \
}
#define N_BYTE_ALIGMENT(__Value, __Aligment) ((__Aligment == 1) ? (__Value) : (((__Value + __Aligment - 1) / __Aligment) * __Aligment))
#define TEST_FLAG(__Flag, __testFlag)		(((__Flag) & (__testFlag)) != 0)
#define SET_FLAG(__Flag, __setFlag)			((__Flag) |= __setFlag)
#define CLEAR_FLAG(__Flag, __clearFlag)		((__Flag) &= ~(__clearFlag))
#define CLEAR_FLAGS(__Flag)					((__Flag) = 0)
#define TEST_FLAGS(__Flag, __testFlags)		(((__Flag) & (__testFlags)) == (__testFlags))
#endif  
