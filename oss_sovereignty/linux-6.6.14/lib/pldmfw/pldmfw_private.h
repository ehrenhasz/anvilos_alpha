 
 

#ifndef _PLDMFW_PRIVATE_H_
#define _PLDMFW_PRIVATE_H_

 

 
static const uuid_t pldm_firmware_header_id =
	UUID_INIT(0xf018878c, 0xcb7d, 0x4943,
		  0x98, 0x00, 0xa0, 0x2f, 0x05, 0x9a, 0xca, 0x02);

 
#define PACKAGE_HEADER_FORMAT_REVISION 0x01

 
#define PLDM_TIMESTAMP_SIZE 13
struct __pldm_timestamp {
	u8 b[PLDM_TIMESTAMP_SIZE];
} __packed __aligned(1);

 
struct __pldm_header {
	uuid_t id;			     
	u8 revision;			     
	__le16 size;			     
	struct __pldm_timestamp release_date;  
	__le16 component_bitmap_len;	     
	u8 version_type;		     
	u8 version_len;			     

	 
	u8 version_string[];		 
} __packed __aligned(1);

 
struct __pldmfw_record_info {
	__le16 record_len;		 
	u8 descriptor_count;		 
	__le32 device_update_flags;	 
	u8 version_type;		 
	u8 version_len;			 
	__le16 package_data_len;	 

	 
	u8 variable_record_data[];
} __packed __aligned(1);

 
struct __pldmfw_desc_tlv {
	__le16 type;			 
	__le16 size;			 
	u8 data[];			 
} __aligned(1);

 
struct __pldmfw_record_area {
	u8 record_count;		 
	 
	u8 records[];
} __aligned(1);

 
struct __pldmfw_component_info {
	__le16 classification;		 
	__le16 identifier;		 
	__le32 comparison_stamp;	 
	__le16 options;			 
	__le16 activation_method;	 
	__le32 location_offset;		 
	__le32 size;			 
	u8 version_type;		 
	u8 version_len;		 

	 
	u8 version_string[];		 
} __packed __aligned(1);

 
struct __pldmfw_component_area {
	__le16 component_image_count;
	 
	u8 components[];
} __aligned(1);

 
#define pldm_first_desc_tlv(start)					\
	((const struct __pldmfw_desc_tlv *)(start))

 
#define pldm_next_desc_tlv(desc)						\
	((const struct __pldmfw_desc_tlv *)((desc)->data +			\
					     get_unaligned_le16(&(desc)->size)))

 
#define pldm_for_each_desc_tlv(i, desc, start, count)			\
	for ((i) = 0, (desc) = pldm_first_desc_tlv(start);		\
	     (i) < (count);						\
	     (i)++, (desc) = pldm_next_desc_tlv(desc))

 
#define pldm_first_record(start)					\
	((const struct __pldmfw_record_info *)(start))

 
#define pldm_next_record(record)					\
	((const struct __pldmfw_record_info *)				\
	 ((const u8 *)(record) + get_unaligned_le16(&(record)->record_len)))

 
#define pldm_for_each_record(i, record, start, count)			\
	for ((i) = 0, (record) = pldm_first_record(start);		\
	     (i) < (count);						\
	     (i)++, (record) = pldm_next_record(record))

 
#define pldm_first_component(start)					\
	((const struct __pldmfw_component_info *)(start))

 
#define pldm_next_component(component)						\
	((const struct __pldmfw_component_info *)((component)->version_string +	\
						  (component)->version_len))

 
#define pldm_for_each_component(i, component, start, count)		\
	for ((i) = 0, (component) = pldm_first_component(start);	\
	     (i) < (count);						\
	     (i)++, (component) = pldm_next_component(component))

#endif
