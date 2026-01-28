#ifndef MMSS_CC_XML
#define MMSS_CC_XML
enum mmss_cc_clk {
	CLK = 0,
	PCLK = 1,
};
#define REG_MMSS_CC_AHB						0x00000008
static inline uint32_t __offset_CLK(enum mmss_cc_clk idx)
{
	switch (idx) {
		case CLK: return 0x0000004c;
		case PCLK: return 0x00000130;
		default: return INVALID_IDX(idx);
	}
}
static inline uint32_t REG_MMSS_CC_CLK(enum mmss_cc_clk i0) { return 0x00000000 + __offset_CLK(i0); }
static inline uint32_t REG_MMSS_CC_CLK_CC(enum mmss_cc_clk i0) { return 0x00000000 + __offset_CLK(i0); }
#define MMSS_CC_CLK_CC_CLK_EN					0x00000001
#define MMSS_CC_CLK_CC_ROOT_EN					0x00000004
#define MMSS_CC_CLK_CC_MND_EN					0x00000020
#define MMSS_CC_CLK_CC_MND_MODE__MASK				0x000000c0
#define MMSS_CC_CLK_CC_MND_MODE__SHIFT				6
static inline uint32_t MMSS_CC_CLK_CC_MND_MODE(uint32_t val)
{
	return ((val) << MMSS_CC_CLK_CC_MND_MODE__SHIFT) & MMSS_CC_CLK_CC_MND_MODE__MASK;
}
#define MMSS_CC_CLK_CC_PMXO_SEL__MASK				0x00000300
#define MMSS_CC_CLK_CC_PMXO_SEL__SHIFT				8
static inline uint32_t MMSS_CC_CLK_CC_PMXO_SEL(uint32_t val)
{
	return ((val) << MMSS_CC_CLK_CC_PMXO_SEL__SHIFT) & MMSS_CC_CLK_CC_PMXO_SEL__MASK;
}
static inline uint32_t REG_MMSS_CC_CLK_MD(enum mmss_cc_clk i0) { return 0x00000004 + __offset_CLK(i0); }
#define MMSS_CC_CLK_MD_D__MASK					0x000000ff
#define MMSS_CC_CLK_MD_D__SHIFT					0
static inline uint32_t MMSS_CC_CLK_MD_D(uint32_t val)
{
	return ((val) << MMSS_CC_CLK_MD_D__SHIFT) & MMSS_CC_CLK_MD_D__MASK;
}
#define MMSS_CC_CLK_MD_M__MASK					0x0000ff00
#define MMSS_CC_CLK_MD_M__SHIFT					8
static inline uint32_t MMSS_CC_CLK_MD_M(uint32_t val)
{
	return ((val) << MMSS_CC_CLK_MD_M__SHIFT) & MMSS_CC_CLK_MD_M__MASK;
}
static inline uint32_t REG_MMSS_CC_CLK_NS(enum mmss_cc_clk i0) { return 0x00000008 + __offset_CLK(i0); }
#define MMSS_CC_CLK_NS_SRC__MASK				0x0000000f
#define MMSS_CC_CLK_NS_SRC__SHIFT				0
static inline uint32_t MMSS_CC_CLK_NS_SRC(uint32_t val)
{
	return ((val) << MMSS_CC_CLK_NS_SRC__SHIFT) & MMSS_CC_CLK_NS_SRC__MASK;
}
#define MMSS_CC_CLK_NS_PRE_DIV_FUNC__MASK			0x00fff000
#define MMSS_CC_CLK_NS_PRE_DIV_FUNC__SHIFT			12
static inline uint32_t MMSS_CC_CLK_NS_PRE_DIV_FUNC(uint32_t val)
{
	return ((val) << MMSS_CC_CLK_NS_PRE_DIV_FUNC__SHIFT) & MMSS_CC_CLK_NS_PRE_DIV_FUNC__MASK;
}
#define MMSS_CC_CLK_NS_VAL__MASK				0xff000000
#define MMSS_CC_CLK_NS_VAL__SHIFT				24
static inline uint32_t MMSS_CC_CLK_NS_VAL(uint32_t val)
{
	return ((val) << MMSS_CC_CLK_NS_VAL__SHIFT) & MMSS_CC_CLK_NS_VAL__MASK;
}
#define REG_MMSS_CC_DSI2_PIXEL_CC				0x00000094
#define REG_MMSS_CC_DSI2_PIXEL_NS				0x000000e4
#define REG_MMSS_CC_DSI2_PIXEL_CC2				0x00000264
#endif  
