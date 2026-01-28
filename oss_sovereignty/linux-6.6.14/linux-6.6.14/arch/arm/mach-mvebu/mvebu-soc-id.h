#ifndef __LINUX_MVEBU_SOC_ID_H
#define __LINUX_MVEBU_SOC_ID_H
#define MV78230_DEV_ID	    0x7823
#define MV78260_DEV_ID	    0x7826
#define MV78460_DEV_ID	    0x7846
#define MV78XX0_A0_REV	    0x1
#define MV78XX0_B0_REV	    0x2
#define ARMADA_370_DEV_ID   0x6710
#define ARMADA_370_A1_REV   0x1
#define ARMADA_375_DEV_ID   0x6720
#define ARMADA_375_Z1_REV   0x0
#define ARMADA_375_A0_REV   0x3
#define ARMADA_380_DEV_ID   0x6810
#define ARMADA_385_DEV_ID   0x6820
#define ARMADA_388_DEV_ID   0x6828
#define ARMADA_38x_Z1_REV   0x0
#define ARMADA_38x_A0_REV   0x4
#ifdef CONFIG_ARCH_MVEBU
int mvebu_get_soc_id(u32 *dev, u32 *rev);
#else
static inline int mvebu_get_soc_id(u32 *dev, u32 *rev)
{
	return -1;
}
#endif
#endif  
