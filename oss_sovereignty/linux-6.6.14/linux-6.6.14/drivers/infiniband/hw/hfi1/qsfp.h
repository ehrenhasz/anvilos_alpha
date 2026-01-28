#define QSFP_DEV 0xA0
#define QSFP_PWR_LAG_MSEC 2000
#define QSFP_MODPRS_LAG_MSEC 20
#define QSFP_MAX_NUM_PAGES	5
#define QSFP_HFI0_I2CCLK    BIT(0)
#define QSFP_HFI0_I2CDAT    BIT(1)
#define QSFP_HFI0_RESET_N   BIT(2)
#define QSFP_HFI0_INT_N	    BIT(3)
#define QSFP_HFI0_MODPRST_N BIT(4)
#define QSFP_PAGESIZE 256
#define QSFP_RW_BOUNDARY 128
#define __QSFP_OFFSET_SIZE 1                            
#define QSFP_OFFSET_SIZE (__QSFP_OFFSET_SIZE << 8)      
#define QSFP_MONITOR_VAL_START 22
#define QSFP_MONITOR_VAL_END 81
#define QSFP_MONITOR_RANGE (QSFP_MONITOR_VAL_END - QSFP_MONITOR_VAL_START + 1)
#define QSFP_TX_CTRL_BYTE_OFFS 86
#define QSFP_PWR_CTRL_BYTE_OFFS 93
#define QSFP_CDR_CTRL_BYTE_OFFS 98
#define QSFP_PAGE_SELECT_BYTE_OFFS 127
#define QSFP_MOD_ID_OFFS 128
#define QSFP_MOD_PWR_OFFS 129
#define QSFP_NOM_BIT_RATE_100_OFFS 140
#define QSFP_MOD_LEN_OFFS 146
#define QSFP_MOD_TECH_OFFS 147
extern const char *const hfi1_qsfp_devtech[16];
#define QSFP_IS_ACTIVE(tech) ((0xA2FF >> ((tech) >> 4)) & 1)
#define QSFP_IS_ACTIVE_FAR(tech) ((0x32FF >> ((tech) >> 4)) & 1)
#define QSFP_HAS_ATTEN(tech) ((0x4D00 >> ((tech) >> 4)) & 1)
#define QSFP_IS_CU(tech) ((0xED00 >> ((tech) >> 4)) & 1)
#define QSFP_TECH_1490 9
#define QSFP_OUI(oui) (((unsigned)oui[0] << 16) | ((unsigned)oui[1] << 8) | \
			oui[2])
#define QSFP_OUI_AMPHENOL 0x415048
#define QSFP_OUI_FINISAR  0x009065
#define QSFP_OUI_GORE     0x002177
#define QSFP_VEND_OFFS 148
#define QSFP_VEND_LEN 16
#define QSFP_IBXCV_OFFS 164
#define QSFP_VOUI_OFFS 165
#define QSFP_VOUI_LEN 3
#define QSFP_PN_OFFS 168
#define QSFP_PN_LEN 16
#define QSFP_REV_OFFS 184
#define QSFP_REV_LEN 2
#define QSFP_ATTEN_OFFS 186
#define QSFP_ATTEN_LEN 2
#define QSFP_CU_ATTEN_7G_OFFS 188
#define QSFP_CU_ATTEN_12G_OFFS 189
#define QSFP_CC_OFFS 191
#define QSFP_EQ_INFO_OFFS 193
#define QSFP_CDR_INFO_OFFS 194
#define QSFP_SN_OFFS 196
#define QSFP_SN_LEN 16
#define QSFP_DATE_OFFS 212
#define QSFP_DATE_LEN 6
#define QSFP_LOT_OFFS 218
#define QSFP_LOT_LEN 2
#define QSFP_NOM_BIT_RATE_250_OFFS 222
#define QSFP_CC_EXT_OFFS 223
#define QSFP_DATA_NOT_READY		0x01
#define QSFP_HIGH_TEMP_ALARM		0x80
#define QSFP_LOW_TEMP_ALARM		0x40
#define QSFP_HIGH_TEMP_WARNING		0x20
#define QSFP_LOW_TEMP_WARNING		0x10
#define QSFP_HIGH_VCC_ALARM		0x80
#define QSFP_LOW_VCC_ALARM		0x40
#define QSFP_HIGH_VCC_WARNING		0x20
#define QSFP_LOW_VCC_WARNING		0x10
#define QSFP_HIGH_POWER_ALARM		0x88
#define QSFP_LOW_POWER_ALARM		0x44
#define QSFP_HIGH_POWER_WARNING		0x22
#define QSFP_LOW_POWER_WARNING		0x11
#define QSFP_HIGH_BIAS_ALARM		0x88
#define QSFP_LOW_BIAS_ALARM		0x44
#define QSFP_HIGH_BIAS_WARNING		0x22
#define QSFP_LOW_BIAS_WARNING		0x11
#define QSFP_ATTEN_SDR(attenarray) (attenarray[0])
#define QSFP_ATTEN_DDR(attenarray) (attenarray[1])
struct qsfp_data {
	struct hfi1_pportdata *ppd;
	struct work_struct qsfp_work;
	u8 cache[QSFP_MAX_NUM_PAGES * 128];
	spinlock_t qsfp_lock;
	u8 check_interrupt_flags;
	u8 reset_needed;
	u8 limiting_active;
	u8 cache_valid;
	u8 cache_refresh_required;
};
int refresh_qsfp_cache(struct hfi1_pportdata *ppd,
		       struct qsfp_data *cp);
int get_qsfp_power_class(u8 power_byte);
int qsfp_mod_present(struct hfi1_pportdata *ppd);
int get_cable_info(struct hfi1_devdata *dd, u32 port_num, u32 addr,
		   u32 len, u8 *data);
int i2c_write(struct hfi1_pportdata *ppd, u32 target, int i2c_addr,
	      int offset, void *bp, int len);
int i2c_read(struct hfi1_pportdata *ppd, u32 target, int i2c_addr,
	     int offset, void *bp, int len);
int qsfp_write(struct hfi1_pportdata *ppd, u32 target, int addr, void *bp,
	       int len);
int qsfp_read(struct hfi1_pportdata *ppd, u32 target, int addr, void *bp,
	      int len);
int one_qsfp_write(struct hfi1_pportdata *ppd, u32 target, int addr, void *bp,
		   int len);
int one_qsfp_read(struct hfi1_pportdata *ppd, u32 target, int addr, void *bp,
		  int len);
struct hfi1_asic_data;
int set_up_i2c(struct hfi1_devdata *dd, struct hfi1_asic_data *ad);
void clean_up_i2c(struct hfi1_devdata *dd, struct hfi1_asic_data *ad);
