#ifndef CW1200_HWIO_H_INCLUDED
#define CW1200_HWIO_H_INCLUDED
  struct cw1200_common;
#define CW1200_CUT_11_ID_STR		(0x302E3830)
#define CW1200_CUT_22_ID_STR1		(0x302e3132)
#define CW1200_CUT_22_ID_STR2		(0x32302e30)
#define CW1200_CUT_22_ID_STR3		(0x3335)
#define CW1200_CUT_ID_ADDR		(0xFFF17F90)
#define CW1200_CUT2_ID_ADDR		(0xFFF1FF90)
#define DOWNLOAD_BOOT_LOADER_OFFSET	(0x00000000)
#define DOWNLOAD_FIFO_OFFSET		(0x00004000)
#define DOWNLOAD_FIFO_SIZE		(0x00008000)
#define DOWNLOAD_CTRL_OFFSET		(0x0000FF80)
#define DOWNLOAD_CTRL_DATA_DWORDS	(32-6)
struct download_cntl_t {
	u32 image_size;
	u32 flags;
	u32 put;
	u32 trace_pc;
	u32 get;
	u32 status;
	u32 debug_data[DOWNLOAD_CTRL_DATA_DWORDS];
};
#define	DOWNLOAD_IMAGE_SIZE_REG		\
	(DOWNLOAD_CTRL_OFFSET + offsetof(struct download_cntl_t, image_size))
#define	DOWNLOAD_FLAGS_REG		\
	(DOWNLOAD_CTRL_OFFSET + offsetof(struct download_cntl_t, flags))
#define DOWNLOAD_PUT_REG		\
	(DOWNLOAD_CTRL_OFFSET + offsetof(struct download_cntl_t, put))
#define DOWNLOAD_TRACE_PC_REG		\
	(DOWNLOAD_CTRL_OFFSET + offsetof(struct download_cntl_t, trace_pc))
#define	DOWNLOAD_GET_REG		\
	(DOWNLOAD_CTRL_OFFSET + offsetof(struct download_cntl_t, get))
#define	DOWNLOAD_STATUS_REG		\
	(DOWNLOAD_CTRL_OFFSET + offsetof(struct download_cntl_t, status))
#define DOWNLOAD_DEBUG_DATA_REG		\
	(DOWNLOAD_CTRL_OFFSET + offsetof(struct download_cntl_t, debug_data))
#define DOWNLOAD_DEBUG_DATA_LEN		(108)
#define DOWNLOAD_BLOCK_SIZE		(1024)
#define DOWNLOAD_ARE_YOU_HERE		(0x87654321)
#define DOWNLOAD_I_AM_HERE		(0x12345678)
#define DOWNLOAD_PENDING		(0xFFFFFFFF)
#define DOWNLOAD_SUCCESS		(0)
#define DOWNLOAD_EXCEPTION		(1)
#define DOWNLOAD_ERR_MEM_1		(2)
#define DOWNLOAD_ERR_MEM_2		(3)
#define DOWNLOAD_ERR_SOFTWARE		(4)
#define DOWNLOAD_ERR_FILE_SIZE		(5)
#define DOWNLOAD_ERR_CHECKSUM		(6)
#define DOWNLOAD_ERR_OVERFLOW		(7)
#define DOWNLOAD_ERR_IMAGE		(8)
#define DOWNLOAD_ERR_HOST		(9)
#define DOWNLOAD_ERR_ABORT		(10)
#define SYS_BASE_ADDR_SILICON		(0)
#define PAC_BASE_ADDRESS_SILICON	(SYS_BASE_ADDR_SILICON + 0x09000000)
#define PAC_SHARED_MEMORY_SILICON	(PAC_BASE_ADDRESS_SILICON)
#define CW1200_APB(addr)		(PAC_SHARED_MEMORY_SILICON + (addr))
#define ST90TDS_ADDR_ID_BASE		(0x0000)
#define ST90TDS_CONFIG_REG_ID		(0x0000)
#define ST90TDS_CONTROL_REG_ID		(0x0001)
#define ST90TDS_IN_OUT_QUEUE_REG_ID	(0x0002)
#define ST90TDS_AHB_DPORT_REG_ID	(0x0003)
#define ST90TDS_SRAM_BASE_ADDR_REG_ID   (0x0004)
#define ST90TDS_SRAM_DPORT_REG_ID	(0x0005)
#define ST90TDS_TSET_GEN_R_W_REG_ID	(0x0006)
#define ST90TDS_FRAME_OUT_REG_ID	(0x0007)
#define ST90TDS_ADDR_ID_MAX		(ST90TDS_FRAME_OUT_REG_ID)
#define ST90TDS_CONT_NEXT_LEN_MASK	(0x0FFF)
#define ST90TDS_CONT_WUP_BIT		(BIT(12))
#define ST90TDS_CONT_RDY_BIT		(BIT(13))
#define ST90TDS_CONT_IRQ_ENABLE		(BIT(14))
#define ST90TDS_CONT_RDY_ENABLE		(BIT(15))
#define ST90TDS_CONT_IRQ_RDY_ENABLE	(BIT(14)|BIT(15))
#define ST90TDS_CONFIG_FRAME_BIT	(BIT(2))
#define ST90TDS_CONFIG_WORD_MODE_BITS	(BIT(3)|BIT(4))
#define ST90TDS_CONFIG_WORD_MODE_1	(BIT(3))
#define ST90TDS_CONFIG_WORD_MODE_2	(BIT(4))
#define ST90TDS_CONFIG_ERROR_0_BIT	(BIT(5))
#define ST90TDS_CONFIG_ERROR_1_BIT	(BIT(6))
#define ST90TDS_CONFIG_ERROR_2_BIT	(BIT(7))
#define ST90TDS_CONFIG_CSN_FRAME_BIT	(BIT(7))
#define ST90TDS_CONFIG_ERROR_3_BIT	(BIT(8))
#define ST90TDS_CONFIG_ERROR_4_BIT	(BIT(9))
#define ST90TDS_CONFIG_ACCESS_MODE_BIT	(BIT(10))
#define ST90TDS_CONFIG_AHB_PRFETCH_BIT	(BIT(11))
#define ST90TDS_CONFIG_CPU_CLK_DIS_BIT	(BIT(12))
#define ST90TDS_CONFIG_PRFETCH_BIT	(BIT(13))
#define ST90TDS_CONFIG_CPU_RESET_BIT	(BIT(14))
#define ST90TDS_CONFIG_CLEAR_INT_BIT	(BIT(15))
#define ST90TDS_CONF_IRQ_ENABLE		(BIT(16))
#define ST90TDS_CONF_RDY_ENABLE		(BIT(17))
#define ST90TDS_CONF_IRQ_RDY_ENABLE	(BIT(16)|BIT(17))
int cw1200_data_read(struct cw1200_common *priv,
		     void *buf, size_t buf_len);
int cw1200_data_write(struct cw1200_common *priv,
		      const void *buf, size_t buf_len);
int cw1200_reg_read(struct cw1200_common *priv, u16 addr,
		    void *buf, size_t buf_len);
int cw1200_reg_write(struct cw1200_common *priv, u16 addr,
		     const void *buf, size_t buf_len);
static inline int cw1200_reg_read_16(struct cw1200_common *priv,
				     u16 addr, u16 *val)
{
	__le32 tmp;
	int i;
	i = cw1200_reg_read(priv, addr, &tmp, sizeof(tmp));
	*val = le32_to_cpu(tmp) & 0xfffff;
	return i;
}
static inline int cw1200_reg_write_16(struct cw1200_common *priv,
				      u16 addr, u16 val)
{
	__le32 tmp = cpu_to_le32((u32)val);
	return cw1200_reg_write(priv, addr, &tmp, sizeof(tmp));
}
static inline int cw1200_reg_read_32(struct cw1200_common *priv,
				     u16 addr, u32 *val)
{
	__le32 tmp;
	int i = cw1200_reg_read(priv, addr, &tmp, sizeof(tmp));
	*val = le32_to_cpu(tmp);
	return i;
}
static inline int cw1200_reg_write_32(struct cw1200_common *priv,
				      u16 addr, u32 val)
{
	__le32 tmp = cpu_to_le32(val);
	return cw1200_reg_write(priv, addr, &tmp, sizeof(val));
}
int cw1200_indirect_read(struct cw1200_common *priv, u32 addr, void *buf,
			 size_t buf_len, u32 prefetch, u16 port_addr);
int cw1200_apb_write(struct cw1200_common *priv, u32 addr, const void *buf,
		     size_t buf_len);
static inline int cw1200_apb_read(struct cw1200_common *priv, u32 addr,
				  void *buf, size_t buf_len)
{
	return cw1200_indirect_read(priv, addr, buf, buf_len,
				    ST90TDS_CONFIG_PRFETCH_BIT,
				    ST90TDS_SRAM_DPORT_REG_ID);
}
static inline int cw1200_ahb_read(struct cw1200_common *priv, u32 addr,
				  void *buf, size_t buf_len)
{
	return cw1200_indirect_read(priv, addr, buf, buf_len,
				    ST90TDS_CONFIG_AHB_PRFETCH_BIT,
				    ST90TDS_AHB_DPORT_REG_ID);
}
static inline int cw1200_apb_read_32(struct cw1200_common *priv,
				     u32 addr, u32 *val)
{
	__le32 tmp;
	int i = cw1200_apb_read(priv, addr, &tmp, sizeof(tmp));
	*val = le32_to_cpu(tmp);
	return i;
}
static inline int cw1200_apb_write_32(struct cw1200_common *priv,
				      u32 addr, u32 val)
{
	__le32 tmp = cpu_to_le32(val);
	return cw1200_apb_write(priv, addr, &tmp, sizeof(val));
}
static inline int cw1200_ahb_read_32(struct cw1200_common *priv,
				     u32 addr, u32 *val)
{
	__le32 tmp;
	int i = cw1200_ahb_read(priv, addr, &tmp, sizeof(tmp));
	*val = le32_to_cpu(tmp);
	return i;
}
#endif  
