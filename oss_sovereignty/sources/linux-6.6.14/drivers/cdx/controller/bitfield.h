

#ifndef CDX_BITFIELD_H
#define CDX_BITFIELD_H

#include <linux/bitfield.h>


#define CDX_DWORD_LBN 0
#define CDX_DWORD_WIDTH 32


#define CDX_VAL(field, attribute) field ## _ ## attribute

#define CDX_LOW_BIT(field) CDX_VAL(field, LBN)

#define CDX_WIDTH(field) CDX_VAL(field, WIDTH)

#define CDX_HIGH_BIT(field) (CDX_LOW_BIT(field) + CDX_WIDTH(field) - 1)


struct cdx_dword {
	__le32 cdx_u32;
};


#define CDX_DWORD_VAL(dword)				\
	((unsigned int)le32_to_cpu((dword).cdx_u32))


#define CDX_DWORD_FIELD(dword, field)					\
	(FIELD_GET(GENMASK(CDX_HIGH_BIT(field), CDX_LOW_BIT(field)),	\
		   le32_to_cpu((dword).cdx_u32)))


#define CDX_INSERT_FIELD(field, value)				\
	(FIELD_PREP(GENMASK(CDX_HIGH_BIT(field),		\
			    CDX_LOW_BIT(field)), value))


#define CDX_INSERT_FIELDS(field1, value1,		\
			  field2, value2,		\
			  field3, value3,		\
			  field4, value4,		\
			  field5, value5,		\
			  field6, value6,		\
			  field7, value7)		\
	(CDX_INSERT_FIELD(field1, (value1)) |		\
	 CDX_INSERT_FIELD(field2, (value2)) |		\
	 CDX_INSERT_FIELD(field3, (value3)) |		\
	 CDX_INSERT_FIELD(field4, (value4)) |		\
	 CDX_INSERT_FIELD(field5, (value5)) |		\
	 CDX_INSERT_FIELD(field6, (value6)) |		\
	 CDX_INSERT_FIELD(field7, (value7)))

#define CDX_POPULATE_DWORD(dword, ...)					\
	(dword).cdx_u32 = cpu_to_le32(CDX_INSERT_FIELDS(__VA_ARGS__))


#define CDX_POPULATE_DWORD_7 CDX_POPULATE_DWORD
#define CDX_POPULATE_DWORD_6(dword, ...) \
	CDX_POPULATE_DWORD_7(dword, CDX_DWORD, 0, __VA_ARGS__)
#define CDX_POPULATE_DWORD_5(dword, ...) \
	CDX_POPULATE_DWORD_6(dword, CDX_DWORD, 0, __VA_ARGS__)
#define CDX_POPULATE_DWORD_4(dword, ...) \
	CDX_POPULATE_DWORD_5(dword, CDX_DWORD, 0, __VA_ARGS__)
#define CDX_POPULATE_DWORD_3(dword, ...) \
	CDX_POPULATE_DWORD_4(dword, CDX_DWORD, 0, __VA_ARGS__)
#define CDX_POPULATE_DWORD_2(dword, ...) \
	CDX_POPULATE_DWORD_3(dword, CDX_DWORD, 0, __VA_ARGS__)
#define CDX_POPULATE_DWORD_1(dword, ...) \
	CDX_POPULATE_DWORD_2(dword, CDX_DWORD, 0, __VA_ARGS__)
#define CDX_SET_DWORD(dword) \
	CDX_POPULATE_DWORD_1(dword, CDX_DWORD, 0xffffffff)

#endif 
