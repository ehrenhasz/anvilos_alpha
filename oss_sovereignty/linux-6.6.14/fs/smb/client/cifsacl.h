#ifndef _CIFSACL_H
#define _CIFSACL_H
#define NUM_AUTHS (6)	 
#define SID_MAX_SUB_AUTHORITIES (15)  
#define READ_BIT        0x4
#define WRITE_BIT       0x2
#define EXEC_BIT        0x1
#define ACL_OWNER_MASK 0700
#define ACL_GROUP_MASK 0070
#define ACL_EVERYONE_MASK 0007
#define UBITSHIFT	6
#define GBITSHIFT	3
#define ACCESS_ALLOWED	0
#define ACCESS_DENIED	1
#define SIDOWNER 1
#define SIDGROUP 2
#define DEFAULT_SEC_DESC_LEN (sizeof(struct cifs_ntsd) + \
			      sizeof(struct cifs_acl) + \
			      (sizeof(struct cifs_ace) * 4))
#define SID_STRING_BASE_SIZE (2 + 3 + 15 + 1)
#define SID_STRING_SUBAUTH_SIZE (11)  
struct cifs_ntsd {
	__le16 revision;  
	__le16 type;
	__le32 osidoffset;
	__le32 gsidoffset;
	__le32 sacloffset;
	__le32 dacloffset;
} __attribute__((packed));
struct cifs_sid {
	__u8 revision;  
	__u8 num_subauth;
	__u8 authority[NUM_AUTHS];
	__le32 sub_auth[SID_MAX_SUB_AUTHORITIES];  
} __attribute__((packed));
#define CIFS_SID_BASE_SIZE (1 + 1 + NUM_AUTHS)
struct cifs_acl {
	__le16 revision;  
	__le16 size;
	__le32 num_aces;
} __attribute__((packed));
#define ACCESS_ALLOWED_ACE_TYPE	0x00
#define ACCESS_DENIED_ACE_TYPE	0x01
#define SYSTEM_AUDIT_ACE_TYPE	0x02
#define SYSTEM_ALARM_ACE_TYPE	0x03
#define ACCESS_ALLOWED_COMPOUND_ACE_TYPE 0x04
#define ACCESS_ALLOWED_OBJECT_ACE_TYPE	0x05
#define ACCESS_DENIED_OBJECT_ACE_TYPE	0x06
#define SYSTEM_AUDIT_OBJECT_ACE_TYPE	0x07
#define SYSTEM_ALARM_OBJECT_ACE_TYPE	0x08
#define ACCESS_ALLOWED_CALLBACK_ACE_TYPE 0x09
#define ACCESS_DENIED_CALLBACK_ACE_TYPE	0x0A
#define ACCESS_ALLOWED_CALLBACK_OBJECT_ACE_TYPE 0x0B
#define ACCESS_DENIED_CALLBACK_OBJECT_ACE_TYPE  0x0C
#define SYSTEM_AUDIT_CALLBACK_ACE_TYPE	0x0D
#define SYSTEM_ALARM_CALLBACK_ACE_TYPE	0x0E  
#define SYSTEM_AUDIT_CALLBACK_OBJECT_ACE_TYPE 0x0F
#define SYSTEM_ALARM_CALLBACK_OBJECT_ACE_TYPE 0x10  
#define SYSTEM_MANDATORY_LABEL_ACE_TYPE	0x11
#define SYSTEM_RESOURCE_ATTRIBUTE_ACE_TYPE 0x12
#define SYSTEM_SCOPED_POLICY_ID_ACE_TYPE 0x13
#define OBJECT_INHERIT_ACE	0x01
#define CONTAINER_INHERIT_ACE	0x02
#define NO_PROPAGATE_INHERIT_ACE 0x04
#define INHERIT_ONLY_ACE	0x08
#define INHERITED_ACE		0x10
#define SUCCESSFUL_ACCESS_ACE_FLAG 0x40
#define FAILED_ACCESS_ACE_FLAG	0x80
struct cifs_ace {
	__u8 type;  
	__u8 flags;
	__le16 size;
	__le32 access_req;
	struct cifs_sid sid;  
} __attribute__((packed));
struct smb3_sd {
	__u8 Revision;  
	__u8 Sbz1;  
	__le16 Control;
	__le32 OffsetOwner;
	__le32 OffsetGroup;
	__le32 OffsetSacl;
	__le32 OffsetDacl;
} __packed;
#define ACL_CONTROL_SR	0x8000	 
#define ACL_CONTROL_RM	0x4000	 
#define ACL_CONTROL_PS	0x2000	 
#define ACL_CONTROL_PD	0x1000	 
#define ACL_CONTROL_SI	0x0800	 
#define ACL_CONTROL_DI	0x0400	 
#define ACL_CONTROL_SC	0x0200	 
#define ACL_CONTROL_DC	0x0100	 
#define ACL_CONTROL_SS	0x0080	 
#define ACL_CONTROL_DT	0x0040	 
#define ACL_CONTROL_SD	0x0020	 
#define ACL_CONTROL_SP	0x0010	 
#define ACL_CONTROL_DD	0x0008	 
#define ACL_CONTROL_DP	0x0004	 
#define ACL_CONTROL_GD	0x0002	 
#define ACL_CONTROL_OD	0x0001	 
#define ACL_REVISION	0x02  
#define ACL_REVISION_DS	0x04  
struct smb3_acl {
	u8 AclRevision;  
	u8 Sbz1;  
	__le16 AclSize;
	__le16 AceCount;
	__le16 Sbz2;  
} __packed;
struct owner_sid {
	u8 Revision;
	u8 NumAuth;
	u8 Authority[6];
	__le32 SubAuthorities[3];
} __packed;
struct owner_group_sids {
	struct owner_sid owner;
	struct owner_sid group;
} __packed;
#define MIN_SID_LEN  (1 + 1 + 6 + 4)  
#define MIN_SEC_DESC_LEN  (sizeof(struct cifs_ntsd) + (2 * MIN_SID_LEN))
#endif  
