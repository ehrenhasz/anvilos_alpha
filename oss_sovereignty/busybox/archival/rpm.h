struct rpm_lead {
	uint32_t magic;
	uint8_t  major, minor;
	uint16_t type;
	uint16_t archnum;
	char     name[66];
	uint16_t osnum;
	uint16_t signature_type;
	char     reserved[16];
};
struct BUG_rpm_lead {
	char bug[sizeof(struct rpm_lead) == 96 ? 1 : -1];
};
#define RPM_LEAD_MAGIC      0xedabeedb
#define RPM_LEAD_MAGIC_STR  "\355\253\356\333"
struct rpm_header {
	uint32_t magic_and_ver;  
	uint32_t reserved;       
	uint32_t entries;        
	uint32_t size;           
};
struct BUG_rpm_header {
	char bug[sizeof(struct rpm_header) == 16 ? 1 : -1];
};
#define RPM_HEADER_MAGICnVER  0x8eade801
#define RPM_HEADER_MAGIC_STR  "\216\255\350"
