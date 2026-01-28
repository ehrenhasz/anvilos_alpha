#ifndef SFPB_XML
#define SFPB_XML
enum sfpb_ahb_arb_master_port_en {
	SFPB_MASTER_PORT_ENABLE = 3,
	SFPB_MASTER_PORT_DISABLE = 0,
};
#define REG_SFPB_GPREG						0x00000058
#define SFPB_GPREG_MASTER_PORT_EN__MASK				0x00001800
#define SFPB_GPREG_MASTER_PORT_EN__SHIFT			11
static inline uint32_t SFPB_GPREG_MASTER_PORT_EN(enum sfpb_ahb_arb_master_port_en val)
{
	return ((val) << SFPB_GPREG_MASTER_PORT_EN__SHIFT) & SFPB_GPREG_MASTER_PORT_EN__MASK;
}
#endif  
