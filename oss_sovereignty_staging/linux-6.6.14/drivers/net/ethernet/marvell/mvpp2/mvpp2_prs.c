
 

#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/platform_device.h>
#include <uapi/linux/ppp_defs.h>
#include <net/ip.h>
#include <net/ipv6.h>

#include "mvpp2.h"
#include "mvpp2_prs.h"

 
static int mvpp2_prs_hw_write(struct mvpp2 *priv, struct mvpp2_prs_entry *pe)
{
	int i;

	if (pe->index > MVPP2_PRS_TCAM_SRAM_SIZE - 1)
		return -EINVAL;

	 
	pe->tcam[MVPP2_PRS_TCAM_INV_WORD] &= ~MVPP2_PRS_TCAM_INV_MASK;

	 
	mvpp2_write(priv, MVPP2_PRS_SRAM_IDX_REG, pe->index);
	for (i = 0; i < MVPP2_PRS_SRAM_WORDS; i++)
		mvpp2_write(priv, MVPP2_PRS_SRAM_DATA_REG(i), pe->sram[i]);

	 
	mvpp2_write(priv, MVPP2_PRS_TCAM_IDX_REG, pe->index);
	for (i = 0; i < MVPP2_PRS_TCAM_WORDS; i++)
		mvpp2_write(priv, MVPP2_PRS_TCAM_DATA_REG(i), pe->tcam[i]);

	return 0;
}

 
int mvpp2_prs_init_from_hw(struct mvpp2 *priv, struct mvpp2_prs_entry *pe,
			   int tid)
{
	int i;

	if (tid > MVPP2_PRS_TCAM_SRAM_SIZE - 1)
		return -EINVAL;

	memset(pe, 0, sizeof(*pe));
	pe->index = tid;

	 
	mvpp2_write(priv, MVPP2_PRS_TCAM_IDX_REG, pe->index);

	pe->tcam[MVPP2_PRS_TCAM_INV_WORD] = mvpp2_read(priv,
			      MVPP2_PRS_TCAM_DATA_REG(MVPP2_PRS_TCAM_INV_WORD));
	if (pe->tcam[MVPP2_PRS_TCAM_INV_WORD] & MVPP2_PRS_TCAM_INV_MASK)
		return MVPP2_PRS_TCAM_ENTRY_INVALID;

	for (i = 0; i < MVPP2_PRS_TCAM_WORDS; i++)
		pe->tcam[i] = mvpp2_read(priv, MVPP2_PRS_TCAM_DATA_REG(i));

	 
	mvpp2_write(priv, MVPP2_PRS_SRAM_IDX_REG, pe->index);
	for (i = 0; i < MVPP2_PRS_SRAM_WORDS; i++)
		pe->sram[i] = mvpp2_read(priv, MVPP2_PRS_SRAM_DATA_REG(i));

	return 0;
}

 
static void mvpp2_prs_hw_inv(struct mvpp2 *priv, int index)
{
	 
	mvpp2_write(priv, MVPP2_PRS_TCAM_IDX_REG, index);
	mvpp2_write(priv, MVPP2_PRS_TCAM_DATA_REG(MVPP2_PRS_TCAM_INV_WORD),
		    MVPP2_PRS_TCAM_INV_MASK);
}

 
static void mvpp2_prs_shadow_set(struct mvpp2 *priv, int index, int lu)
{
	priv->prs_shadow[index].valid = true;
	priv->prs_shadow[index].lu = lu;
}

 
static void mvpp2_prs_shadow_ri_set(struct mvpp2 *priv, int index,
				    unsigned int ri, unsigned int ri_mask)
{
	priv->prs_shadow[index].ri_mask = ri_mask;
	priv->prs_shadow[index].ri = ri;
}

 
static void mvpp2_prs_tcam_lu_set(struct mvpp2_prs_entry *pe, unsigned int lu)
{
	pe->tcam[MVPP2_PRS_TCAM_LU_WORD] &= ~MVPP2_PRS_TCAM_LU(MVPP2_PRS_LU_MASK);
	pe->tcam[MVPP2_PRS_TCAM_LU_WORD] &= ~MVPP2_PRS_TCAM_LU_EN(MVPP2_PRS_LU_MASK);
	pe->tcam[MVPP2_PRS_TCAM_LU_WORD] |= MVPP2_PRS_TCAM_LU(lu & MVPP2_PRS_LU_MASK);
	pe->tcam[MVPP2_PRS_TCAM_LU_WORD] |= MVPP2_PRS_TCAM_LU_EN(MVPP2_PRS_LU_MASK);
}

 
static void mvpp2_prs_tcam_port_set(struct mvpp2_prs_entry *pe,
				    unsigned int port, bool add)
{
	if (add)
		pe->tcam[MVPP2_PRS_TCAM_PORT_WORD] &= ~MVPP2_PRS_TCAM_PORT_EN(BIT(port));
	else
		pe->tcam[MVPP2_PRS_TCAM_PORT_WORD] |= MVPP2_PRS_TCAM_PORT_EN(BIT(port));
}

 
static void mvpp2_prs_tcam_port_map_set(struct mvpp2_prs_entry *pe,
					unsigned int ports)
{
	pe->tcam[MVPP2_PRS_TCAM_PORT_WORD] &= ~MVPP2_PRS_TCAM_PORT(MVPP2_PRS_PORT_MASK);
	pe->tcam[MVPP2_PRS_TCAM_PORT_WORD] &= ~MVPP2_PRS_TCAM_PORT_EN(MVPP2_PRS_PORT_MASK);
	pe->tcam[MVPP2_PRS_TCAM_PORT_WORD] |= MVPP2_PRS_TCAM_PORT_EN(~ports & MVPP2_PRS_PORT_MASK);
}

 
unsigned int mvpp2_prs_tcam_port_map_get(struct mvpp2_prs_entry *pe)
{
	return (~pe->tcam[MVPP2_PRS_TCAM_PORT_WORD] >> 24) & MVPP2_PRS_PORT_MASK;
}

 
static void mvpp2_prs_tcam_data_byte_set(struct mvpp2_prs_entry *pe,
					 unsigned int offs, unsigned char byte,
					 unsigned char enable)
{
	int pos = MVPP2_PRS_BYTE_IN_WORD(offs) * BITS_PER_BYTE;

	pe->tcam[MVPP2_PRS_BYTE_TO_WORD(offs)] &= ~(0xff << pos);
	pe->tcam[MVPP2_PRS_BYTE_TO_WORD(offs)] &= ~(MVPP2_PRS_TCAM_EN(0xff) << pos);
	pe->tcam[MVPP2_PRS_BYTE_TO_WORD(offs)] |= byte << pos;
	pe->tcam[MVPP2_PRS_BYTE_TO_WORD(offs)] |= MVPP2_PRS_TCAM_EN(enable << pos);
}

 
void mvpp2_prs_tcam_data_byte_get(struct mvpp2_prs_entry *pe,
				  unsigned int offs, unsigned char *byte,
				  unsigned char *enable)
{
	int pos = MVPP2_PRS_BYTE_IN_WORD(offs) * BITS_PER_BYTE;

	*byte = (pe->tcam[MVPP2_PRS_BYTE_TO_WORD(offs)] >> pos) & 0xff;
	*enable = (pe->tcam[MVPP2_PRS_BYTE_TO_WORD(offs)] >> (pos + 16)) & 0xff;
}

 
static bool mvpp2_prs_tcam_data_cmp(struct mvpp2_prs_entry *pe, int offs,
				    u16 data)
{
	u16 tcam_data;

	tcam_data = pe->tcam[MVPP2_PRS_BYTE_TO_WORD(offs)] & 0xffff;
	return tcam_data == data;
}

 
static void mvpp2_prs_tcam_ai_update(struct mvpp2_prs_entry *pe,
				     unsigned int bits, unsigned int enable)
{
	int i;

	for (i = 0; i < MVPP2_PRS_AI_BITS; i++) {
		if (!(enable & BIT(i)))
			continue;

		if (bits & BIT(i))
			pe->tcam[MVPP2_PRS_TCAM_AI_WORD] |= BIT(i);
		else
			pe->tcam[MVPP2_PRS_TCAM_AI_WORD] &= ~BIT(i);
	}

	pe->tcam[MVPP2_PRS_TCAM_AI_WORD] |= MVPP2_PRS_TCAM_AI_EN(enable);
}

 
static int mvpp2_prs_tcam_ai_get(struct mvpp2_prs_entry *pe)
{
	return pe->tcam[MVPP2_PRS_TCAM_AI_WORD] & MVPP2_PRS_AI_MASK;
}

 
static void mvpp2_prs_match_etype(struct mvpp2_prs_entry *pe, int offset,
				  unsigned short ethertype)
{
	mvpp2_prs_tcam_data_byte_set(pe, offset + 0, ethertype >> 8, 0xff);
	mvpp2_prs_tcam_data_byte_set(pe, offset + 1, ethertype & 0xff, 0xff);
}

 
static void mvpp2_prs_match_vid(struct mvpp2_prs_entry *pe, int offset,
				unsigned short vid)
{
	mvpp2_prs_tcam_data_byte_set(pe, offset + 0, (vid & 0xf00) >> 8, 0xf);
	mvpp2_prs_tcam_data_byte_set(pe, offset + 1, vid & 0xff, 0xff);
}

 
static void mvpp2_prs_sram_bits_set(struct mvpp2_prs_entry *pe, int bit_num,
				    u32 val)
{
	pe->sram[MVPP2_BIT_TO_WORD(bit_num)] |= (val << (MVPP2_BIT_IN_WORD(bit_num)));
}

 
static void mvpp2_prs_sram_bits_clear(struct mvpp2_prs_entry *pe, int bit_num,
				      u32 val)
{
	pe->sram[MVPP2_BIT_TO_WORD(bit_num)] &= ~(val << (MVPP2_BIT_IN_WORD(bit_num)));
}

 
static void mvpp2_prs_sram_ri_update(struct mvpp2_prs_entry *pe,
				     unsigned int bits, unsigned int mask)
{
	unsigned int i;

	for (i = 0; i < MVPP2_PRS_SRAM_RI_CTRL_BITS; i++) {
		if (!(mask & BIT(i)))
			continue;

		if (bits & BIT(i))
			mvpp2_prs_sram_bits_set(pe, MVPP2_PRS_SRAM_RI_OFFS + i,
						1);
		else
			mvpp2_prs_sram_bits_clear(pe,
						  MVPP2_PRS_SRAM_RI_OFFS + i,
						  1);

		mvpp2_prs_sram_bits_set(pe, MVPP2_PRS_SRAM_RI_CTRL_OFFS + i, 1);
	}
}

 
static int mvpp2_prs_sram_ri_get(struct mvpp2_prs_entry *pe)
{
	return pe->sram[MVPP2_PRS_SRAM_RI_WORD];
}

 
static void mvpp2_prs_sram_ai_update(struct mvpp2_prs_entry *pe,
				     unsigned int bits, unsigned int mask)
{
	unsigned int i;

	for (i = 0; i < MVPP2_PRS_SRAM_AI_CTRL_BITS; i++) {
		if (!(mask & BIT(i)))
			continue;

		if (bits & BIT(i))
			mvpp2_prs_sram_bits_set(pe, MVPP2_PRS_SRAM_AI_OFFS + i,
						1);
		else
			mvpp2_prs_sram_bits_clear(pe,
						  MVPP2_PRS_SRAM_AI_OFFS + i,
						  1);

		mvpp2_prs_sram_bits_set(pe, MVPP2_PRS_SRAM_AI_CTRL_OFFS + i, 1);
	}
}

 
static int mvpp2_prs_sram_ai_get(struct mvpp2_prs_entry *pe)
{
	u8 bits;
	 
	int ai_off = MVPP2_BIT_TO_WORD(MVPP2_PRS_SRAM_AI_OFFS);
	int ai_shift = MVPP2_BIT_IN_WORD(MVPP2_PRS_SRAM_AI_OFFS);

	bits = (pe->sram[ai_off] >> ai_shift) |
	       (pe->sram[ai_off + 1] << (32 - ai_shift));

	return bits;
}

 
static void mvpp2_prs_sram_next_lu_set(struct mvpp2_prs_entry *pe,
				       unsigned int lu)
{
	int sram_next_off = MVPP2_PRS_SRAM_NEXT_LU_OFFS;

	mvpp2_prs_sram_bits_clear(pe, sram_next_off,
				  MVPP2_PRS_SRAM_NEXT_LU_MASK);
	mvpp2_prs_sram_bits_set(pe, sram_next_off, lu);
}

 
static void mvpp2_prs_sram_shift_set(struct mvpp2_prs_entry *pe, int shift,
				     unsigned int op)
{
	 
	if (shift < 0) {
		mvpp2_prs_sram_bits_set(pe, MVPP2_PRS_SRAM_SHIFT_SIGN_BIT, 1);
		shift = 0 - shift;
	} else {
		mvpp2_prs_sram_bits_clear(pe, MVPP2_PRS_SRAM_SHIFT_SIGN_BIT, 1);
	}

	 
	pe->sram[MVPP2_BIT_TO_WORD(MVPP2_PRS_SRAM_SHIFT_OFFS)] |=
		shift & MVPP2_PRS_SRAM_SHIFT_MASK;

	 
	mvpp2_prs_sram_bits_clear(pe, MVPP2_PRS_SRAM_OP_SEL_SHIFT_OFFS,
				  MVPP2_PRS_SRAM_OP_SEL_SHIFT_MASK);
	mvpp2_prs_sram_bits_set(pe, MVPP2_PRS_SRAM_OP_SEL_SHIFT_OFFS, op);

	 
	mvpp2_prs_sram_bits_clear(pe, MVPP2_PRS_SRAM_OP_SEL_BASE_OFFS, 1);
}

 
static void mvpp2_prs_sram_offset_set(struct mvpp2_prs_entry *pe,
				      unsigned int type, int offset,
				      unsigned int op)
{
	 
	if (offset < 0) {
		mvpp2_prs_sram_bits_set(pe, MVPP2_PRS_SRAM_UDF_SIGN_BIT, 1);
		offset = 0 - offset;
	} else {
		mvpp2_prs_sram_bits_clear(pe, MVPP2_PRS_SRAM_UDF_SIGN_BIT, 1);
	}

	 
	mvpp2_prs_sram_bits_clear(pe, MVPP2_PRS_SRAM_UDF_OFFS,
				  MVPP2_PRS_SRAM_UDF_MASK);
	mvpp2_prs_sram_bits_set(pe, MVPP2_PRS_SRAM_UDF_OFFS,
				offset & MVPP2_PRS_SRAM_UDF_MASK);

	 
	mvpp2_prs_sram_bits_clear(pe, MVPP2_PRS_SRAM_UDF_TYPE_OFFS,
				  MVPP2_PRS_SRAM_UDF_TYPE_MASK);
	mvpp2_prs_sram_bits_set(pe, MVPP2_PRS_SRAM_UDF_TYPE_OFFS, type);

	 
	mvpp2_prs_sram_bits_clear(pe, MVPP2_PRS_SRAM_OP_SEL_UDF_OFFS,
				  MVPP2_PRS_SRAM_OP_SEL_UDF_MASK);
	mvpp2_prs_sram_bits_set(pe, MVPP2_PRS_SRAM_OP_SEL_UDF_OFFS,
				op & MVPP2_PRS_SRAM_OP_SEL_UDF_MASK);

	 
	mvpp2_prs_sram_bits_clear(pe, MVPP2_PRS_SRAM_OP_SEL_BASE_OFFS, 1);
}

 
static int mvpp2_prs_flow_find(struct mvpp2 *priv, int flow)
{
	struct mvpp2_prs_entry pe;
	int tid;

	 
	for (tid = MVPP2_PRS_TCAM_SRAM_SIZE - 1; tid >= 0; tid--) {
		u8 bits;

		if (!priv->prs_shadow[tid].valid ||
		    priv->prs_shadow[tid].lu != MVPP2_PRS_LU_FLOWS)
			continue;

		mvpp2_prs_init_from_hw(priv, &pe, tid);
		bits = mvpp2_prs_sram_ai_get(&pe);

		 
		if ((bits & MVPP2_PRS_FLOW_ID_MASK) == flow)
			return tid;
	}

	return -ENOENT;
}

 
static int mvpp2_prs_tcam_first_free(struct mvpp2 *priv, unsigned char start,
				     unsigned char end)
{
	int tid;

	if (start > end)
		swap(start, end);

	for (tid = start; tid <= end; tid++) {
		if (!priv->prs_shadow[tid].valid)
			return tid;
	}

	return -EINVAL;
}

 
static void mvpp2_prs_drop_fc(struct mvpp2 *priv)
{
	unsigned char da[ETH_ALEN] = { 0x01, 0x80, 0xC2, 0x00, 0x00, 0x01 };
	struct mvpp2_prs_entry pe;
	unsigned int len;

	memset(&pe, 0, sizeof(pe));

	 
	pe.index = MVPP2_PE_FC_DROP;
	mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_MAC);

	 
	len = ETH_ALEN;
	while (len--)
		mvpp2_prs_tcam_data_byte_set(&pe, len, da[len], 0xff);

	mvpp2_prs_sram_ri_update(&pe, MVPP2_PRS_RI_DROP_MASK,
				 MVPP2_PRS_RI_DROP_MASK);

	mvpp2_prs_sram_bits_set(&pe, MVPP2_PRS_SRAM_LU_GEN_BIT, 1);
	mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_FLOWS);

	 
	mvpp2_prs_tcam_port_map_set(&pe, MVPP2_PRS_PORT_MASK);

	 
	mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_MAC);
	mvpp2_prs_hw_write(priv, &pe);
}

 
static void mvpp2_prs_mac_drop_all_set(struct mvpp2 *priv, int port, bool add)
{
	struct mvpp2_prs_entry pe;

	if (priv->prs_shadow[MVPP2_PE_DROP_ALL].valid) {
		 
		mvpp2_prs_init_from_hw(priv, &pe, MVPP2_PE_DROP_ALL);
	} else {
		 
		memset(&pe, 0, sizeof(pe));
		mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_MAC);
		pe.index = MVPP2_PE_DROP_ALL;

		 
		mvpp2_prs_sram_ri_update(&pe, MVPP2_PRS_RI_DROP_MASK,
					 MVPP2_PRS_RI_DROP_MASK);

		mvpp2_prs_sram_bits_set(&pe, MVPP2_PRS_SRAM_LU_GEN_BIT, 1);
		mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_FLOWS);

		 
		mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_MAC);

		 
		mvpp2_prs_tcam_port_map_set(&pe, 0);
	}

	 
	mvpp2_prs_tcam_port_set(&pe, port, add);

	mvpp2_prs_hw_write(priv, &pe);
}

 
void mvpp2_prs_mac_promisc_set(struct mvpp2 *priv, int port,
			       enum mvpp2_prs_l2_cast l2_cast, bool add)
{
	struct mvpp2_prs_entry pe;
	unsigned char cast_match;
	unsigned int ri;
	int tid;

	if (l2_cast == MVPP2_PRS_L2_UNI_CAST) {
		cast_match = MVPP2_PRS_UCAST_VAL;
		tid = MVPP2_PE_MAC_UC_PROMISCUOUS;
		ri = MVPP2_PRS_RI_L2_UCAST;
	} else {
		cast_match = MVPP2_PRS_MCAST_VAL;
		tid = MVPP2_PE_MAC_MC_PROMISCUOUS;
		ri = MVPP2_PRS_RI_L2_MCAST;
	}

	 
	if (priv->prs_shadow[tid].valid) {
		mvpp2_prs_init_from_hw(priv, &pe, tid);
	} else {
		memset(&pe, 0, sizeof(pe));
		mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_MAC);
		pe.index = tid;

		 
		mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_DSA);

		 
		mvpp2_prs_sram_ri_update(&pe, ri, MVPP2_PRS_RI_L2_CAST_MASK);

		 
		mvpp2_prs_tcam_data_byte_set(&pe, 0, cast_match,
					     MVPP2_PRS_CAST_MASK);

		 
		mvpp2_prs_sram_shift_set(&pe, 2 * ETH_ALEN,
					 MVPP2_PRS_SRAM_OP_SEL_SHIFT_ADD);

		 
		mvpp2_prs_tcam_port_map_set(&pe, 0);

		 
		mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_MAC);
	}

	 
	mvpp2_prs_tcam_port_set(&pe, port, add);

	mvpp2_prs_hw_write(priv, &pe);
}

 
static void mvpp2_prs_dsa_tag_set(struct mvpp2 *priv, int port, bool add,
				  bool tagged, bool extend)
{
	struct mvpp2_prs_entry pe;
	int tid, shift;

	if (extend) {
		tid = tagged ? MVPP2_PE_EDSA_TAGGED : MVPP2_PE_EDSA_UNTAGGED;
		shift = 8;
	} else {
		tid = tagged ? MVPP2_PE_DSA_TAGGED : MVPP2_PE_DSA_UNTAGGED;
		shift = 4;
	}

	if (priv->prs_shadow[tid].valid) {
		 
		mvpp2_prs_init_from_hw(priv, &pe, tid);
	} else {
		 
		memset(&pe, 0, sizeof(pe));
		mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_DSA);
		pe.index = tid;

		 
		mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_DSA);

		if (tagged) {
			 
			mvpp2_prs_tcam_data_byte_set(&pe, 0,
					     MVPP2_PRS_TCAM_DSA_TAGGED_BIT,
					     MVPP2_PRS_TCAM_DSA_TAGGED_BIT);

			 
			if (extend)
				mvpp2_prs_sram_ai_update(&pe, 1,
							MVPP2_PRS_SRAM_AI_MASK);
			else
				mvpp2_prs_sram_ai_update(&pe, 0,
							MVPP2_PRS_SRAM_AI_MASK);

			 
			mvpp2_prs_sram_ri_update(&pe, MVPP2_PRS_RI_VLAN_SINGLE,
						 MVPP2_PRS_RI_VLAN_MASK);
			 
			mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_VID);
		} else {
			 
			mvpp2_prs_sram_shift_set(&pe, shift,
					MVPP2_PRS_SRAM_OP_SEL_SHIFT_ADD);

			 
			mvpp2_prs_sram_ri_update(&pe, MVPP2_PRS_RI_VLAN_NONE,
						 MVPP2_PRS_RI_VLAN_MASK);
			mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_L2);
		}

		 
		mvpp2_prs_tcam_port_map_set(&pe, 0);
	}

	 
	mvpp2_prs_tcam_port_set(&pe, port, add);

	mvpp2_prs_hw_write(priv, &pe);
}

 
static void mvpp2_prs_dsa_tag_ethertype_set(struct mvpp2 *priv, int port,
					    bool add, bool tagged, bool extend)
{
	struct mvpp2_prs_entry pe;
	int tid, shift, port_mask;

	if (extend) {
		tid = tagged ? MVPP2_PE_ETYPE_EDSA_TAGGED :
		      MVPP2_PE_ETYPE_EDSA_UNTAGGED;
		port_mask = 0;
		shift = 8;
	} else {
		tid = tagged ? MVPP2_PE_ETYPE_DSA_TAGGED :
		      MVPP2_PE_ETYPE_DSA_UNTAGGED;
		port_mask = MVPP2_PRS_PORT_MASK;
		shift = 4;
	}

	if (priv->prs_shadow[tid].valid) {
		 
		mvpp2_prs_init_from_hw(priv, &pe, tid);
	} else {
		 
		memset(&pe, 0, sizeof(pe));
		mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_DSA);
		pe.index = tid;

		 
		mvpp2_prs_match_etype(&pe, 0, ETH_P_EDSA);
		mvpp2_prs_match_etype(&pe, 2, 0);

		mvpp2_prs_sram_ri_update(&pe, MVPP2_PRS_RI_DSA_MASK,
					 MVPP2_PRS_RI_DSA_MASK);
		 
		mvpp2_prs_sram_shift_set(&pe, 2 + MVPP2_ETH_TYPE_LEN + shift,
					 MVPP2_PRS_SRAM_OP_SEL_SHIFT_ADD);

		 
		mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_DSA);

		if (tagged) {
			 
			mvpp2_prs_tcam_data_byte_set(&pe,
						     MVPP2_ETH_TYPE_LEN + 2 + 3,
						 MVPP2_PRS_TCAM_DSA_TAGGED_BIT,
						 MVPP2_PRS_TCAM_DSA_TAGGED_BIT);
			 
			mvpp2_prs_sram_ai_update(&pe, 0,
						 MVPP2_PRS_SRAM_AI_MASK);
			 
			mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_VLAN);
		} else {
			 
			mvpp2_prs_sram_ri_update(&pe, MVPP2_PRS_RI_VLAN_NONE,
						 MVPP2_PRS_RI_VLAN_MASK);
			mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_L2);
		}
		 
		mvpp2_prs_tcam_port_map_set(&pe, port_mask);
	}

	 
	mvpp2_prs_tcam_port_set(&pe, port, add);

	mvpp2_prs_hw_write(priv, &pe);
}

 
static int mvpp2_prs_vlan_find(struct mvpp2 *priv, unsigned short tpid, int ai)
{
	struct mvpp2_prs_entry pe;
	int tid;

	 
	for (tid = MVPP2_PE_FIRST_FREE_TID;
	     tid <= MVPP2_PE_LAST_FREE_TID; tid++) {
		unsigned int ri_bits, ai_bits;
		bool match;

		if (!priv->prs_shadow[tid].valid ||
		    priv->prs_shadow[tid].lu != MVPP2_PRS_LU_VLAN)
			continue;

		mvpp2_prs_init_from_hw(priv, &pe, tid);
		match = mvpp2_prs_tcam_data_cmp(&pe, 0, tpid);
		if (!match)
			continue;

		 
		ri_bits = mvpp2_prs_sram_ri_get(&pe);
		ri_bits &= MVPP2_PRS_RI_VLAN_MASK;

		 
		ai_bits = mvpp2_prs_tcam_ai_get(&pe);
		 
		ai_bits &= ~MVPP2_PRS_DBL_VLAN_AI_BIT;

		if (ai != ai_bits)
			continue;

		if (ri_bits == MVPP2_PRS_RI_VLAN_SINGLE ||
		    ri_bits == MVPP2_PRS_RI_VLAN_TRIPLE)
			return tid;
	}

	return -ENOENT;
}

 
static int mvpp2_prs_vlan_add(struct mvpp2 *priv, unsigned short tpid, int ai,
			      unsigned int port_map)
{
	struct mvpp2_prs_entry pe;
	int tid_aux, tid;
	int ret = 0;

	memset(&pe, 0, sizeof(pe));

	tid = mvpp2_prs_vlan_find(priv, tpid, ai);

	if (tid < 0) {
		 
		tid = mvpp2_prs_tcam_first_free(priv, MVPP2_PE_LAST_FREE_TID,
						MVPP2_PE_FIRST_FREE_TID);
		if (tid < 0)
			return tid;

		 
		for (tid_aux = MVPP2_PE_LAST_FREE_TID;
		     tid_aux >= MVPP2_PE_FIRST_FREE_TID; tid_aux--) {
			unsigned int ri_bits;

			if (!priv->prs_shadow[tid_aux].valid ||
			    priv->prs_shadow[tid_aux].lu != MVPP2_PRS_LU_VLAN)
				continue;

			mvpp2_prs_init_from_hw(priv, &pe, tid_aux);
			ri_bits = mvpp2_prs_sram_ri_get(&pe);
			if ((ri_bits & MVPP2_PRS_RI_VLAN_MASK) ==
			    MVPP2_PRS_RI_VLAN_DOUBLE)
				break;
		}

		if (tid <= tid_aux)
			return -EINVAL;

		memset(&pe, 0, sizeof(pe));
		pe.index = tid;
		mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_VLAN);

		mvpp2_prs_match_etype(&pe, 0, tpid);

		 
		mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_VID);

		 
		mvpp2_prs_sram_ai_update(&pe, 0, MVPP2_PRS_SRAM_AI_MASK);

		if (ai == MVPP2_PRS_SINGLE_VLAN_AI) {
			mvpp2_prs_sram_ri_update(&pe, MVPP2_PRS_RI_VLAN_SINGLE,
						 MVPP2_PRS_RI_VLAN_MASK);
		} else {
			ai |= MVPP2_PRS_DBL_VLAN_AI_BIT;
			mvpp2_prs_sram_ri_update(&pe, MVPP2_PRS_RI_VLAN_TRIPLE,
						 MVPP2_PRS_RI_VLAN_MASK);
		}
		mvpp2_prs_tcam_ai_update(&pe, ai, MVPP2_PRS_SRAM_AI_MASK);

		mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_VLAN);
	} else {
		mvpp2_prs_init_from_hw(priv, &pe, tid);
	}
	 
	mvpp2_prs_tcam_port_map_set(&pe, port_map);

	mvpp2_prs_hw_write(priv, &pe);

	return ret;
}

 
static int mvpp2_prs_double_vlan_ai_free_get(struct mvpp2 *priv)
{
	int i;

	for (i = 1; i < MVPP2_PRS_DBL_VLANS_MAX; i++) {
		if (!priv->prs_double_vlans[i])
			return i;
	}

	return -EINVAL;
}

 
static int mvpp2_prs_double_vlan_find(struct mvpp2 *priv, unsigned short tpid1,
				      unsigned short tpid2)
{
	struct mvpp2_prs_entry pe;
	int tid;

	 
	for (tid = MVPP2_PE_FIRST_FREE_TID;
	     tid <= MVPP2_PE_LAST_FREE_TID; tid++) {
		unsigned int ri_mask;
		bool match;

		if (!priv->prs_shadow[tid].valid ||
		    priv->prs_shadow[tid].lu != MVPP2_PRS_LU_VLAN)
			continue;

		mvpp2_prs_init_from_hw(priv, &pe, tid);

		match = mvpp2_prs_tcam_data_cmp(&pe, 0, tpid1) &&
			mvpp2_prs_tcam_data_cmp(&pe, 4, tpid2);

		if (!match)
			continue;

		ri_mask = mvpp2_prs_sram_ri_get(&pe) & MVPP2_PRS_RI_VLAN_MASK;
		if (ri_mask == MVPP2_PRS_RI_VLAN_DOUBLE)
			return tid;
	}

	return -ENOENT;
}

 
static int mvpp2_prs_double_vlan_add(struct mvpp2 *priv, unsigned short tpid1,
				     unsigned short tpid2,
				     unsigned int port_map)
{
	int tid_aux, tid, ai, ret = 0;
	struct mvpp2_prs_entry pe;

	memset(&pe, 0, sizeof(pe));

	tid = mvpp2_prs_double_vlan_find(priv, tpid1, tpid2);

	if (tid < 0) {
		 
		tid = mvpp2_prs_tcam_first_free(priv, MVPP2_PE_FIRST_FREE_TID,
				MVPP2_PE_LAST_FREE_TID);
		if (tid < 0)
			return tid;

		 
		ai = mvpp2_prs_double_vlan_ai_free_get(priv);
		if (ai < 0)
			return ai;

		 
		for (tid_aux = MVPP2_PE_FIRST_FREE_TID;
		     tid_aux <= MVPP2_PE_LAST_FREE_TID; tid_aux++) {
			unsigned int ri_bits;

			if (!priv->prs_shadow[tid_aux].valid ||
			    priv->prs_shadow[tid_aux].lu != MVPP2_PRS_LU_VLAN)
				continue;

			mvpp2_prs_init_from_hw(priv, &pe, tid_aux);
			ri_bits = mvpp2_prs_sram_ri_get(&pe);
			ri_bits &= MVPP2_PRS_RI_VLAN_MASK;
			if (ri_bits == MVPP2_PRS_RI_VLAN_SINGLE ||
			    ri_bits == MVPP2_PRS_RI_VLAN_TRIPLE)
				break;
		}

		if (tid >= tid_aux)
			return -ERANGE;

		memset(&pe, 0, sizeof(pe));
		mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_VLAN);
		pe.index = tid;

		priv->prs_double_vlans[ai] = true;

		mvpp2_prs_match_etype(&pe, 0, tpid1);
		mvpp2_prs_match_etype(&pe, 4, tpid2);

		mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_VLAN);
		 
		mvpp2_prs_sram_shift_set(&pe, MVPP2_VLAN_TAG_LEN,
					 MVPP2_PRS_SRAM_OP_SEL_SHIFT_ADD);
		mvpp2_prs_sram_ri_update(&pe, MVPP2_PRS_RI_VLAN_DOUBLE,
					 MVPP2_PRS_RI_VLAN_MASK);
		mvpp2_prs_sram_ai_update(&pe, ai | MVPP2_PRS_DBL_VLAN_AI_BIT,
					 MVPP2_PRS_SRAM_AI_MASK);

		mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_VLAN);
	} else {
		mvpp2_prs_init_from_hw(priv, &pe, tid);
	}

	 
	mvpp2_prs_tcam_port_map_set(&pe, port_map);
	mvpp2_prs_hw_write(priv, &pe);

	return ret;
}

 
static int mvpp2_prs_ip4_proto(struct mvpp2 *priv, unsigned short proto,
			       unsigned int ri, unsigned int ri_mask)
{
	struct mvpp2_prs_entry pe;
	int tid;

	if ((proto != IPPROTO_TCP) && (proto != IPPROTO_UDP) &&
	    (proto != IPPROTO_IGMP))
		return -EINVAL;

	 
	tid = mvpp2_prs_tcam_first_free(priv, MVPP2_PE_FIRST_FREE_TID,
					MVPP2_PE_LAST_FREE_TID);
	if (tid < 0)
		return tid;

	memset(&pe, 0, sizeof(pe));
	mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_IP4);
	pe.index = tid;

	 
	mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_FLOWS);
	mvpp2_prs_sram_bits_set(&pe, MVPP2_PRS_SRAM_LU_GEN_BIT, 1);

	 
	mvpp2_prs_sram_offset_set(&pe, MVPP2_PRS_SRAM_UDF_TYPE_L3, -4,
				  MVPP2_PRS_SRAM_OP_SEL_UDF_ADD);
	mvpp2_prs_sram_ai_update(&pe, 0, MVPP2_PRS_IPV4_DIP_AI_BIT);
	mvpp2_prs_sram_ri_update(&pe, ri, ri_mask | MVPP2_PRS_RI_IP_FRAG_MASK);

	mvpp2_prs_tcam_data_byte_set(&pe, 2, 0x00,
				     MVPP2_PRS_TCAM_PROTO_MASK_L);
	mvpp2_prs_tcam_data_byte_set(&pe, 3, 0x00,
				     MVPP2_PRS_TCAM_PROTO_MASK);

	mvpp2_prs_tcam_data_byte_set(&pe, 5, proto, MVPP2_PRS_TCAM_PROTO_MASK);
	mvpp2_prs_tcam_ai_update(&pe, MVPP2_PRS_IPV4_DIP_AI_BIT,
				 MVPP2_PRS_IPV4_DIP_AI_BIT);
	 
	mvpp2_prs_tcam_port_map_set(&pe, MVPP2_PRS_PORT_MASK);

	 
	mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_IP4);
	mvpp2_prs_hw_write(priv, &pe);

	 
	tid = mvpp2_prs_tcam_first_free(priv, MVPP2_PE_FIRST_FREE_TID,
					MVPP2_PE_LAST_FREE_TID);
	if (tid < 0)
		return tid;

	pe.index = tid;
	 
	pe.sram[MVPP2_PRS_SRAM_RI_WORD] = 0x0;
	pe.sram[MVPP2_PRS_SRAM_RI_CTRL_WORD] = 0x0;
	mvpp2_prs_sram_ri_update(&pe, ri, ri_mask);

	mvpp2_prs_sram_ri_update(&pe, ri | MVPP2_PRS_RI_IP_FRAG_TRUE,
				 ri_mask | MVPP2_PRS_RI_IP_FRAG_MASK);

	mvpp2_prs_tcam_data_byte_set(&pe, 2, 0x00, 0x0);
	mvpp2_prs_tcam_data_byte_set(&pe, 3, 0x00, 0x0);

	 
	mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_IP4);
	mvpp2_prs_hw_write(priv, &pe);

	return 0;
}

 
static int mvpp2_prs_ip4_cast(struct mvpp2 *priv, unsigned short l3_cast)
{
	struct mvpp2_prs_entry pe;
	int mask, tid;

	tid = mvpp2_prs_tcam_first_free(priv, MVPP2_PE_FIRST_FREE_TID,
					MVPP2_PE_LAST_FREE_TID);
	if (tid < 0)
		return tid;

	memset(&pe, 0, sizeof(pe));
	mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_IP4);
	pe.index = tid;

	switch (l3_cast) {
	case MVPP2_PRS_L3_MULTI_CAST:
		mvpp2_prs_tcam_data_byte_set(&pe, 0, MVPP2_PRS_IPV4_MC,
					     MVPP2_PRS_IPV4_MC_MASK);
		mvpp2_prs_sram_ri_update(&pe, MVPP2_PRS_RI_L3_MCAST,
					 MVPP2_PRS_RI_L3_ADDR_MASK);
		break;
	case  MVPP2_PRS_L3_BROAD_CAST:
		mask = MVPP2_PRS_IPV4_BC_MASK;
		mvpp2_prs_tcam_data_byte_set(&pe, 0, mask, mask);
		mvpp2_prs_tcam_data_byte_set(&pe, 1, mask, mask);
		mvpp2_prs_tcam_data_byte_set(&pe, 2, mask, mask);
		mvpp2_prs_tcam_data_byte_set(&pe, 3, mask, mask);
		mvpp2_prs_sram_ri_update(&pe, MVPP2_PRS_RI_L3_BCAST,
					 MVPP2_PRS_RI_L3_ADDR_MASK);
		break;
	default:
		return -EINVAL;
	}

	 
	mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_IP4);

	mvpp2_prs_sram_ai_update(&pe, MVPP2_PRS_IPV4_DIP_AI_BIT,
				 MVPP2_PRS_IPV4_DIP_AI_BIT);

	 
	mvpp2_prs_sram_shift_set(&pe, -12, MVPP2_PRS_SRAM_OP_SEL_SHIFT_ADD);

	mvpp2_prs_tcam_ai_update(&pe, 0, MVPP2_PRS_IPV4_DIP_AI_BIT);

	 
	mvpp2_prs_tcam_port_map_set(&pe, MVPP2_PRS_PORT_MASK);

	 
	mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_IP4);
	mvpp2_prs_hw_write(priv, &pe);

	return 0;
}

 
static int mvpp2_prs_ip6_proto(struct mvpp2 *priv, unsigned short proto,
			       unsigned int ri, unsigned int ri_mask)
{
	struct mvpp2_prs_entry pe;
	int tid;

	if ((proto != IPPROTO_TCP) && (proto != IPPROTO_UDP) &&
	    (proto != IPPROTO_ICMPV6) && (proto != IPPROTO_IPIP))
		return -EINVAL;

	tid = mvpp2_prs_tcam_first_free(priv, MVPP2_PE_FIRST_FREE_TID,
					MVPP2_PE_LAST_FREE_TID);
	if (tid < 0)
		return tid;

	memset(&pe, 0, sizeof(pe));
	mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_IP6);
	pe.index = tid;

	 
	mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_FLOWS);
	mvpp2_prs_sram_bits_set(&pe, MVPP2_PRS_SRAM_LU_GEN_BIT, 1);
	mvpp2_prs_sram_ri_update(&pe, ri, ri_mask);
	mvpp2_prs_sram_offset_set(&pe, MVPP2_PRS_SRAM_UDF_TYPE_L4,
				  sizeof(struct ipv6hdr) - 6,
				  MVPP2_PRS_SRAM_OP_SEL_UDF_ADD);

	mvpp2_prs_tcam_data_byte_set(&pe, 0, proto, MVPP2_PRS_TCAM_PROTO_MASK);
	mvpp2_prs_tcam_ai_update(&pe, MVPP2_PRS_IPV6_NO_EXT_AI_BIT,
				 MVPP2_PRS_IPV6_NO_EXT_AI_BIT);
	 
	mvpp2_prs_tcam_port_map_set(&pe, MVPP2_PRS_PORT_MASK);

	 
	mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_IP6);
	mvpp2_prs_hw_write(priv, &pe);

	return 0;
}

 
static int mvpp2_prs_ip6_cast(struct mvpp2 *priv, unsigned short l3_cast)
{
	struct mvpp2_prs_entry pe;
	int tid;

	if (l3_cast != MVPP2_PRS_L3_MULTI_CAST)
		return -EINVAL;

	tid = mvpp2_prs_tcam_first_free(priv, MVPP2_PE_FIRST_FREE_TID,
					MVPP2_PE_LAST_FREE_TID);
	if (tid < 0)
		return tid;

	memset(&pe, 0, sizeof(pe));
	mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_IP6);
	pe.index = tid;

	 
	mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_IP6);
	mvpp2_prs_sram_ri_update(&pe, MVPP2_PRS_RI_L3_MCAST,
				 MVPP2_PRS_RI_L3_ADDR_MASK);
	mvpp2_prs_sram_ai_update(&pe, MVPP2_PRS_IPV6_NO_EXT_AI_BIT,
				 MVPP2_PRS_IPV6_NO_EXT_AI_BIT);
	 
	mvpp2_prs_sram_shift_set(&pe, -18, MVPP2_PRS_SRAM_OP_SEL_SHIFT_ADD);

	mvpp2_prs_tcam_data_byte_set(&pe, 0, MVPP2_PRS_IPV6_MC,
				     MVPP2_PRS_IPV6_MC_MASK);
	mvpp2_prs_tcam_ai_update(&pe, 0, MVPP2_PRS_IPV6_NO_EXT_AI_BIT);
	 
	mvpp2_prs_tcam_port_map_set(&pe, MVPP2_PRS_PORT_MASK);

	 
	mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_IP6);
	mvpp2_prs_hw_write(priv, &pe);

	return 0;
}

 
static void mvpp2_prs_hw_port_init(struct mvpp2 *priv, int port, int lu_first,
				   int lu_max, int offset)
{
	u32 val;

	 
	val = mvpp2_read(priv, MVPP2_PRS_INIT_LOOKUP_REG);
	val &= ~MVPP2_PRS_PORT_LU_MASK(port);
	val |=  MVPP2_PRS_PORT_LU_VAL(port, lu_first);
	mvpp2_write(priv, MVPP2_PRS_INIT_LOOKUP_REG, val);

	 
	val = mvpp2_read(priv, MVPP2_PRS_MAX_LOOP_REG(port));
	val &= ~MVPP2_PRS_MAX_LOOP_MASK(port);
	val |= MVPP2_PRS_MAX_LOOP_VAL(port, lu_max);
	mvpp2_write(priv, MVPP2_PRS_MAX_LOOP_REG(port), val);

	 
	val = mvpp2_read(priv, MVPP2_PRS_INIT_OFFS_REG(port));
	val &= ~MVPP2_PRS_INIT_OFF_MASK(port);
	val |= MVPP2_PRS_INIT_OFF_VAL(port, offset);
	mvpp2_write(priv, MVPP2_PRS_INIT_OFFS_REG(port), val);
}

 
static void mvpp2_prs_def_flow_init(struct mvpp2 *priv)
{
	struct mvpp2_prs_entry pe;
	int port;

	for (port = 0; port < MVPP2_MAX_PORTS; port++) {
		memset(&pe, 0, sizeof(pe));
		mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_FLOWS);
		pe.index = MVPP2_PE_FIRST_DEFAULT_FLOW - port;

		 
		mvpp2_prs_tcam_port_map_set(&pe, 0);

		 
		mvpp2_prs_sram_ai_update(&pe, port, MVPP2_PRS_FLOW_ID_MASK);
		mvpp2_prs_sram_bits_set(&pe, MVPP2_PRS_SRAM_LU_DONE_BIT, 1);

		 
		mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_FLOWS);
		mvpp2_prs_hw_write(priv, &pe);
	}
}

 
static void mvpp2_prs_mh_init(struct mvpp2 *priv)
{
	struct mvpp2_prs_entry pe;

	memset(&pe, 0, sizeof(pe));

	pe.index = MVPP2_PE_MH_DEFAULT;
	mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_MH);
	mvpp2_prs_sram_shift_set(&pe, MVPP2_MH_SIZE,
				 MVPP2_PRS_SRAM_OP_SEL_SHIFT_ADD);
	mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_MAC);

	 
	mvpp2_prs_tcam_port_map_set(&pe, MVPP2_PRS_PORT_MASK);

	 
	mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_MH);
	mvpp2_prs_hw_write(priv, &pe);

	 
	pe.index = MVPP2_PE_MH_SKIP_PRS;
	mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_MH);
	mvpp2_prs_sram_shift_set(&pe, MVPP2_MH_SIZE,
				 MVPP2_PRS_SRAM_OP_SEL_SHIFT_ADD);
	mvpp2_prs_sram_bits_set(&pe, MVPP2_PRS_SRAM_LU_GEN_BIT, 1);
	mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_FLOWS);

	 
	mvpp2_prs_tcam_port_map_set(&pe, 0);

	 
	mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_MH);
	mvpp2_prs_hw_write(priv, &pe);
}

 
static void mvpp2_prs_mac_init(struct mvpp2 *priv)
{
	struct mvpp2_prs_entry pe;

	memset(&pe, 0, sizeof(pe));

	 
	pe.index = MVPP2_PE_MAC_NON_PROMISCUOUS;
	mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_MAC);

	mvpp2_prs_sram_ri_update(&pe, MVPP2_PRS_RI_DROP_MASK,
				 MVPP2_PRS_RI_DROP_MASK);
	mvpp2_prs_sram_bits_set(&pe, MVPP2_PRS_SRAM_LU_GEN_BIT, 1);
	mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_FLOWS);

	 
	mvpp2_prs_tcam_port_map_set(&pe, MVPP2_PRS_PORT_MASK);

	 
	mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_MAC);
	mvpp2_prs_hw_write(priv, &pe);

	 
	mvpp2_prs_drop_fc(priv);
	mvpp2_prs_mac_drop_all_set(priv, 0, false);
	mvpp2_prs_mac_promisc_set(priv, 0, MVPP2_PRS_L2_UNI_CAST, false);
	mvpp2_prs_mac_promisc_set(priv, 0, MVPP2_PRS_L2_MULTI_CAST, false);
}

 
static void mvpp2_prs_dsa_init(struct mvpp2 *priv)
{
	struct mvpp2_prs_entry pe;

	 
	mvpp2_prs_dsa_tag_set(priv, 0, false, MVPP2_PRS_UNTAGGED,
			      MVPP2_PRS_EDSA);

	 
	mvpp2_prs_dsa_tag_set(priv, 0, false, MVPP2_PRS_TAGGED, MVPP2_PRS_EDSA);

	 
	mvpp2_prs_dsa_tag_set(priv, 0, false, MVPP2_PRS_UNTAGGED,
			      MVPP2_PRS_DSA);

	 
	mvpp2_prs_dsa_tag_set(priv, 0, false, MVPP2_PRS_TAGGED, MVPP2_PRS_DSA);

	 
	mvpp2_prs_dsa_tag_ethertype_set(priv, 0, false,
					MVPP2_PRS_UNTAGGED, MVPP2_PRS_EDSA);

	 
	mvpp2_prs_dsa_tag_ethertype_set(priv, 0, false,
					MVPP2_PRS_TAGGED, MVPP2_PRS_EDSA);

	 
	mvpp2_prs_dsa_tag_ethertype_set(priv, 0, true,
					MVPP2_PRS_UNTAGGED, MVPP2_PRS_DSA);

	 
	mvpp2_prs_dsa_tag_ethertype_set(priv, 0, true,
					MVPP2_PRS_TAGGED, MVPP2_PRS_DSA);

	 
	memset(&pe, 0, sizeof(pe));
	mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_DSA);
	pe.index = MVPP2_PE_DSA_DEFAULT;
	mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_VLAN);

	 
	mvpp2_prs_sram_shift_set(&pe, 0, MVPP2_PRS_SRAM_OP_SEL_SHIFT_ADD);
	mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_MAC);

	 
	mvpp2_prs_sram_ai_update(&pe, 0, MVPP2_PRS_SRAM_AI_MASK);

	 
	mvpp2_prs_tcam_port_map_set(&pe, MVPP2_PRS_PORT_MASK);

	mvpp2_prs_hw_write(priv, &pe);
}

 
static void mvpp2_prs_vid_init(struct mvpp2 *priv)
{
	struct mvpp2_prs_entry pe;

	memset(&pe, 0, sizeof(pe));

	 
	pe.index = MVPP2_PE_VID_FLTR_DEFAULT;
	mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_VID);

	mvpp2_prs_tcam_ai_update(&pe, 0, MVPP2_PRS_EDSA_VID_AI_BIT);

	 
	mvpp2_prs_sram_shift_set(&pe, MVPP2_VLAN_TAG_LEN,
				 MVPP2_PRS_SRAM_OP_SEL_SHIFT_ADD);

	 
	mvpp2_prs_sram_ai_update(&pe, 0, MVPP2_PRS_SRAM_AI_MASK);

	mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_L2);

	 
	mvpp2_prs_tcam_port_map_set(&pe, MVPP2_PRS_PORT_MASK);

	 
	mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_VID);
	mvpp2_prs_hw_write(priv, &pe);

	 
	memset(&pe, 0, sizeof(pe));

	 
	pe.index = MVPP2_PE_VID_EDSA_FLTR_DEFAULT;
	mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_VID);

	mvpp2_prs_tcam_ai_update(&pe, MVPP2_PRS_EDSA_VID_AI_BIT,
				 MVPP2_PRS_EDSA_VID_AI_BIT);

	 
	mvpp2_prs_sram_shift_set(&pe, MVPP2_VLAN_TAG_EDSA_LEN,
				 MVPP2_PRS_SRAM_OP_SEL_SHIFT_ADD);

	 
	mvpp2_prs_sram_ai_update(&pe, 0, MVPP2_PRS_SRAM_AI_MASK);

	mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_L2);

	 
	mvpp2_prs_tcam_port_map_set(&pe, MVPP2_PRS_PORT_MASK);

	 
	mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_VID);
	mvpp2_prs_hw_write(priv, &pe);
}

 
static int mvpp2_prs_etype_init(struct mvpp2 *priv)
{
	struct mvpp2_prs_entry pe;
	int tid, ihl;

	 
	tid = mvpp2_prs_tcam_first_free(priv, MVPP2_PE_FIRST_FREE_TID,
					MVPP2_PE_LAST_FREE_TID);
	if (tid < 0)
		return tid;

	memset(&pe, 0, sizeof(pe));
	mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_L2);
	pe.index = tid;

	mvpp2_prs_match_etype(&pe, 0, ETH_P_PPP_SES);

	mvpp2_prs_sram_shift_set(&pe, MVPP2_PPPOE_HDR_SIZE,
				 MVPP2_PRS_SRAM_OP_SEL_SHIFT_ADD);
	mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_PPPOE);
	mvpp2_prs_sram_ri_update(&pe, MVPP2_PRS_RI_PPPOE_MASK,
				 MVPP2_PRS_RI_PPPOE_MASK);

	 
	mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_L2);
	priv->prs_shadow[pe.index].udf = MVPP2_PRS_UDF_L2_DEF;
	priv->prs_shadow[pe.index].finish = false;
	mvpp2_prs_shadow_ri_set(priv, pe.index, MVPP2_PRS_RI_PPPOE_MASK,
				MVPP2_PRS_RI_PPPOE_MASK);
	mvpp2_prs_hw_write(priv, &pe);

	 
	tid = mvpp2_prs_tcam_first_free(priv, MVPP2_PE_FIRST_FREE_TID,
					MVPP2_PE_LAST_FREE_TID);
	if (tid < 0)
		return tid;

	memset(&pe, 0, sizeof(pe));
	mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_L2);
	pe.index = tid;

	mvpp2_prs_match_etype(&pe, 0, ETH_P_ARP);

	 
	mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_FLOWS);
	mvpp2_prs_sram_bits_set(&pe, MVPP2_PRS_SRAM_LU_GEN_BIT, 1);
	mvpp2_prs_sram_ri_update(&pe, MVPP2_PRS_RI_L3_ARP,
				 MVPP2_PRS_RI_L3_PROTO_MASK);
	 
	mvpp2_prs_sram_offset_set(&pe, MVPP2_PRS_SRAM_UDF_TYPE_L3,
				  MVPP2_ETH_TYPE_LEN,
				  MVPP2_PRS_SRAM_OP_SEL_UDF_ADD);

	 
	mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_L2);
	priv->prs_shadow[pe.index].udf = MVPP2_PRS_UDF_L2_DEF;
	priv->prs_shadow[pe.index].finish = true;
	mvpp2_prs_shadow_ri_set(priv, pe.index, MVPP2_PRS_RI_L3_ARP,
				MVPP2_PRS_RI_L3_PROTO_MASK);
	mvpp2_prs_hw_write(priv, &pe);

	 
	tid = mvpp2_prs_tcam_first_free(priv, MVPP2_PE_FIRST_FREE_TID,
					MVPP2_PE_LAST_FREE_TID);
	if (tid < 0)
		return tid;

	memset(&pe, 0, sizeof(pe));
	mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_L2);
	pe.index = tid;

	mvpp2_prs_match_etype(&pe, 0, MVPP2_IP_LBDT_TYPE);

	 
	mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_FLOWS);
	mvpp2_prs_sram_bits_set(&pe, MVPP2_PRS_SRAM_LU_GEN_BIT, 1);
	mvpp2_prs_sram_ri_update(&pe, MVPP2_PRS_RI_CPU_CODE_RX_SPEC |
				 MVPP2_PRS_RI_UDF3_RX_SPECIAL,
				 MVPP2_PRS_RI_CPU_CODE_MASK |
				 MVPP2_PRS_RI_UDF3_MASK);
	 
	mvpp2_prs_sram_offset_set(&pe, MVPP2_PRS_SRAM_UDF_TYPE_L3,
				  MVPP2_ETH_TYPE_LEN,
				  MVPP2_PRS_SRAM_OP_SEL_UDF_ADD);

	 
	mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_L2);
	priv->prs_shadow[pe.index].udf = MVPP2_PRS_UDF_L2_DEF;
	priv->prs_shadow[pe.index].finish = true;
	mvpp2_prs_shadow_ri_set(priv, pe.index, MVPP2_PRS_RI_CPU_CODE_RX_SPEC |
				MVPP2_PRS_RI_UDF3_RX_SPECIAL,
				MVPP2_PRS_RI_CPU_CODE_MASK |
				MVPP2_PRS_RI_UDF3_MASK);
	mvpp2_prs_hw_write(priv, &pe);

	 
	for (ihl = MVPP2_PRS_IPV4_IHL_MIN; ihl <= MVPP2_PRS_IPV4_IHL_MAX; ihl++) {
		tid = mvpp2_prs_tcam_first_free(priv, MVPP2_PE_FIRST_FREE_TID,
						MVPP2_PE_LAST_FREE_TID);
		if (tid < 0)
			return tid;

		memset(&pe, 0, sizeof(pe));
		mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_L2);
		pe.index = tid;

		mvpp2_prs_match_etype(&pe, 0, ETH_P_IP);
		mvpp2_prs_tcam_data_byte_set(&pe, MVPP2_ETH_TYPE_LEN,
					     MVPP2_PRS_IPV4_HEAD | ihl,
					     MVPP2_PRS_IPV4_HEAD_MASK |
					     MVPP2_PRS_IPV4_IHL_MASK);

		mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_IP4);
		mvpp2_prs_sram_ri_update(&pe, MVPP2_PRS_RI_L3_IP4,
					 MVPP2_PRS_RI_L3_PROTO_MASK);
		 
		mvpp2_prs_sram_shift_set(&pe, MVPP2_ETH_TYPE_LEN +
					 sizeof(struct iphdr) - 4,
					 MVPP2_PRS_SRAM_OP_SEL_SHIFT_ADD);
		 
		mvpp2_prs_sram_offset_set(&pe, MVPP2_PRS_SRAM_UDF_TYPE_L4,
					  MVPP2_ETH_TYPE_LEN + (ihl * 4),
					  MVPP2_PRS_SRAM_OP_SEL_UDF_ADD);

		 
		mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_L2);
		priv->prs_shadow[pe.index].udf = MVPP2_PRS_UDF_L2_DEF;
		priv->prs_shadow[pe.index].finish = false;
		mvpp2_prs_shadow_ri_set(priv, pe.index, MVPP2_PRS_RI_L3_IP4,
					MVPP2_PRS_RI_L3_PROTO_MASK);
		mvpp2_prs_hw_write(priv, &pe);
	}

	 
	tid = mvpp2_prs_tcam_first_free(priv, MVPP2_PE_FIRST_FREE_TID,
					MVPP2_PE_LAST_FREE_TID);
	if (tid < 0)
		return tid;

	memset(&pe, 0, sizeof(pe));
	mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_L2);
	pe.index = tid;

	mvpp2_prs_match_etype(&pe, 0, ETH_P_IPV6);

	 
	mvpp2_prs_sram_shift_set(&pe, MVPP2_ETH_TYPE_LEN + 8 +
				 MVPP2_MAX_L3_ADDR_SIZE,
				 MVPP2_PRS_SRAM_OP_SEL_SHIFT_ADD);
	mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_IP6);
	mvpp2_prs_sram_ri_update(&pe, MVPP2_PRS_RI_L3_IP6,
				 MVPP2_PRS_RI_L3_PROTO_MASK);
	 
	mvpp2_prs_sram_offset_set(&pe, MVPP2_PRS_SRAM_UDF_TYPE_L3,
				  MVPP2_ETH_TYPE_LEN,
				  MVPP2_PRS_SRAM_OP_SEL_UDF_ADD);

	mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_L2);
	priv->prs_shadow[pe.index].udf = MVPP2_PRS_UDF_L2_DEF;
	priv->prs_shadow[pe.index].finish = false;
	mvpp2_prs_shadow_ri_set(priv, pe.index, MVPP2_PRS_RI_L3_IP6,
				MVPP2_PRS_RI_L3_PROTO_MASK);
	mvpp2_prs_hw_write(priv, &pe);

	 
	memset(&pe, 0, sizeof(struct mvpp2_prs_entry));
	mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_L2);
	pe.index = MVPP2_PE_ETH_TYPE_UN;

	 
	mvpp2_prs_tcam_port_map_set(&pe, MVPP2_PRS_PORT_MASK);

	 
	mvpp2_prs_sram_bits_set(&pe, MVPP2_PRS_SRAM_LU_GEN_BIT, 1);
	mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_FLOWS);
	mvpp2_prs_sram_ri_update(&pe, MVPP2_PRS_RI_L3_UN,
				 MVPP2_PRS_RI_L3_PROTO_MASK);
	 
	mvpp2_prs_sram_offset_set(&pe, MVPP2_PRS_SRAM_UDF_TYPE_L3,
				  MVPP2_ETH_TYPE_LEN,
				  MVPP2_PRS_SRAM_OP_SEL_UDF_ADD);

	 
	mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_L2);
	priv->prs_shadow[pe.index].udf = MVPP2_PRS_UDF_L2_DEF;
	priv->prs_shadow[pe.index].finish = true;
	mvpp2_prs_shadow_ri_set(priv, pe.index, MVPP2_PRS_RI_L3_UN,
				MVPP2_PRS_RI_L3_PROTO_MASK);
	mvpp2_prs_hw_write(priv, &pe);

	return 0;
}

 
static int mvpp2_prs_vlan_init(struct platform_device *pdev, struct mvpp2 *priv)
{
	struct mvpp2_prs_entry pe;
	int err;

	priv->prs_double_vlans = devm_kcalloc(&pdev->dev, sizeof(bool),
					      MVPP2_PRS_DBL_VLANS_MAX,
					      GFP_KERNEL);
	if (!priv->prs_double_vlans)
		return -ENOMEM;

	 
	err = mvpp2_prs_double_vlan_add(priv, ETH_P_8021AD, ETH_P_8021Q,
					MVPP2_PRS_PORT_MASK);
	if (err)
		return err;

	 
	err = mvpp2_prs_double_vlan_add(priv, ETH_P_8021Q, ETH_P_8021Q,
					MVPP2_PRS_PORT_MASK);
	if (err)
		return err;

	 
	err = mvpp2_prs_vlan_add(priv, ETH_P_8021AD, MVPP2_PRS_SINGLE_VLAN_AI,
				 MVPP2_PRS_PORT_MASK);
	if (err)
		return err;

	 
	err = mvpp2_prs_vlan_add(priv, ETH_P_8021Q, MVPP2_PRS_SINGLE_VLAN_AI,
				 MVPP2_PRS_PORT_MASK);
	if (err)
		return err;

	 
	memset(&pe, 0, sizeof(pe));
	mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_VLAN);
	pe.index = MVPP2_PE_VLAN_DBL;

	mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_VID);

	 
	mvpp2_prs_sram_ai_update(&pe, 0, MVPP2_PRS_SRAM_AI_MASK);
	mvpp2_prs_sram_ri_update(&pe, MVPP2_PRS_RI_VLAN_DOUBLE,
				 MVPP2_PRS_RI_VLAN_MASK);

	mvpp2_prs_tcam_ai_update(&pe, MVPP2_PRS_DBL_VLAN_AI_BIT,
				 MVPP2_PRS_DBL_VLAN_AI_BIT);
	 
	mvpp2_prs_tcam_port_map_set(&pe, MVPP2_PRS_PORT_MASK);

	 
	mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_VLAN);
	mvpp2_prs_hw_write(priv, &pe);

	 
	memset(&pe, 0, sizeof(pe));
	mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_VLAN);
	pe.index = MVPP2_PE_VLAN_NONE;

	mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_L2);
	mvpp2_prs_sram_ri_update(&pe, MVPP2_PRS_RI_VLAN_NONE,
				 MVPP2_PRS_RI_VLAN_MASK);

	 
	mvpp2_prs_tcam_port_map_set(&pe, MVPP2_PRS_PORT_MASK);

	 
	mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_VLAN);
	mvpp2_prs_hw_write(priv, &pe);

	return 0;
}

 
static int mvpp2_prs_pppoe_init(struct mvpp2 *priv)
{
	struct mvpp2_prs_entry pe;
	int tid, ihl;

	 
	for (ihl = MVPP2_PRS_IPV4_IHL_MIN; ihl <= MVPP2_PRS_IPV4_IHL_MAX; ihl++) {
		tid = mvpp2_prs_tcam_first_free(priv, MVPP2_PE_FIRST_FREE_TID,
						MVPP2_PE_LAST_FREE_TID);
		if (tid < 0)
			return tid;

		memset(&pe, 0, sizeof(pe));
		mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_PPPOE);
		pe.index = tid;

		mvpp2_prs_match_etype(&pe, 0, PPP_IP);
		mvpp2_prs_tcam_data_byte_set(&pe, MVPP2_ETH_TYPE_LEN,
					     MVPP2_PRS_IPV4_HEAD | ihl,
					     MVPP2_PRS_IPV4_HEAD_MASK |
					     MVPP2_PRS_IPV4_IHL_MASK);

		mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_IP4);
		mvpp2_prs_sram_ri_update(&pe, MVPP2_PRS_RI_L3_IP4,
					 MVPP2_PRS_RI_L3_PROTO_MASK);
		 
		mvpp2_prs_sram_shift_set(&pe, MVPP2_ETH_TYPE_LEN +
					 sizeof(struct iphdr) - 4,
					 MVPP2_PRS_SRAM_OP_SEL_SHIFT_ADD);
		 
		mvpp2_prs_sram_offset_set(&pe, MVPP2_PRS_SRAM_UDF_TYPE_L3,
					  MVPP2_ETH_TYPE_LEN,
					  MVPP2_PRS_SRAM_OP_SEL_UDF_ADD);
		 
		mvpp2_prs_sram_offset_set(&pe, MVPP2_PRS_SRAM_UDF_TYPE_L4,
					  MVPP2_ETH_TYPE_LEN + (ihl * 4),
					  MVPP2_PRS_SRAM_OP_SEL_UDF_ADD);

		 
		mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_PPPOE);
		mvpp2_prs_hw_write(priv, &pe);
	}

	 
	tid = mvpp2_prs_tcam_first_free(priv, MVPP2_PE_FIRST_FREE_TID,
					MVPP2_PE_LAST_FREE_TID);
	if (tid < 0)
		return tid;

	memset(&pe, 0, sizeof(pe));
	mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_PPPOE);
	pe.index = tid;

	mvpp2_prs_match_etype(&pe, 0, PPP_IPV6);

	mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_IP6);
	mvpp2_prs_sram_ri_update(&pe, MVPP2_PRS_RI_L3_IP6,
				 MVPP2_PRS_RI_L3_PROTO_MASK);
	 
	mvpp2_prs_sram_shift_set(&pe, MVPP2_ETH_TYPE_LEN + 8 +
				 MVPP2_MAX_L3_ADDR_SIZE,
				 MVPP2_PRS_SRAM_OP_SEL_SHIFT_ADD);
	 
	mvpp2_prs_sram_offset_set(&pe, MVPP2_PRS_SRAM_UDF_TYPE_L3,
				  MVPP2_ETH_TYPE_LEN,
				  MVPP2_PRS_SRAM_OP_SEL_UDF_ADD);

	 
	mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_PPPOE);
	mvpp2_prs_hw_write(priv, &pe);

	 
	tid = mvpp2_prs_tcam_first_free(priv, MVPP2_PE_FIRST_FREE_TID,
					MVPP2_PE_LAST_FREE_TID);
	if (tid < 0)
		return tid;

	memset(&pe, 0, sizeof(pe));
	mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_PPPOE);
	pe.index = tid;

	mvpp2_prs_sram_ri_update(&pe, MVPP2_PRS_RI_L3_UN,
				 MVPP2_PRS_RI_L3_PROTO_MASK);

	 
	mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_FLOWS);
	mvpp2_prs_sram_bits_set(&pe, MVPP2_PRS_SRAM_LU_GEN_BIT, 1);
	 
	mvpp2_prs_sram_offset_set(&pe, MVPP2_PRS_SRAM_UDF_TYPE_L3,
				  MVPP2_ETH_TYPE_LEN,
				  MVPP2_PRS_SRAM_OP_SEL_UDF_ADD);

	 
	mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_PPPOE);
	mvpp2_prs_hw_write(priv, &pe);

	return 0;
}

 
static int mvpp2_prs_ip4_init(struct mvpp2 *priv)
{
	struct mvpp2_prs_entry pe;
	int err;

	 
	err = mvpp2_prs_ip4_proto(priv, IPPROTO_TCP, MVPP2_PRS_RI_L4_TCP,
				  MVPP2_PRS_RI_L4_PROTO_MASK);
	if (err)
		return err;

	err = mvpp2_prs_ip4_proto(priv, IPPROTO_UDP, MVPP2_PRS_RI_L4_UDP,
				  MVPP2_PRS_RI_L4_PROTO_MASK);
	if (err)
		return err;

	err = mvpp2_prs_ip4_proto(priv, IPPROTO_IGMP,
				  MVPP2_PRS_RI_CPU_CODE_RX_SPEC |
				  MVPP2_PRS_RI_UDF3_RX_SPECIAL,
				  MVPP2_PRS_RI_CPU_CODE_MASK |
				  MVPP2_PRS_RI_UDF3_MASK);
	if (err)
		return err;

	 
	err = mvpp2_prs_ip4_cast(priv, MVPP2_PRS_L3_BROAD_CAST);
	if (err)
		return err;

	 
	err = mvpp2_prs_ip4_cast(priv, MVPP2_PRS_L3_MULTI_CAST);
	if (err)
		return err;

	 
	memset(&pe, 0, sizeof(pe));
	mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_IP4);
	pe.index = MVPP2_PE_IP4_PROTO_UN;

	 
	mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_FLOWS);
	mvpp2_prs_sram_bits_set(&pe, MVPP2_PRS_SRAM_LU_GEN_BIT, 1);

	 
	mvpp2_prs_sram_offset_set(&pe, MVPP2_PRS_SRAM_UDF_TYPE_L3, -4,
				  MVPP2_PRS_SRAM_OP_SEL_UDF_ADD);
	mvpp2_prs_sram_ai_update(&pe, 0, MVPP2_PRS_IPV4_DIP_AI_BIT);
	mvpp2_prs_sram_ri_update(&pe, MVPP2_PRS_RI_L4_OTHER,
				 MVPP2_PRS_RI_L4_PROTO_MASK);

	mvpp2_prs_tcam_ai_update(&pe, MVPP2_PRS_IPV4_DIP_AI_BIT,
				 MVPP2_PRS_IPV4_DIP_AI_BIT);
	 
	mvpp2_prs_tcam_port_map_set(&pe, MVPP2_PRS_PORT_MASK);

	 
	mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_IP4);
	mvpp2_prs_hw_write(priv, &pe);

	 
	memset(&pe, 0, sizeof(pe));
	mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_IP4);
	pe.index = MVPP2_PE_IP4_ADDR_UN;

	 
	mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_IP4);

	mvpp2_prs_sram_ai_update(&pe, MVPP2_PRS_IPV4_DIP_AI_BIT,
				 MVPP2_PRS_IPV4_DIP_AI_BIT);

	 
	mvpp2_prs_sram_shift_set(&pe, -12, MVPP2_PRS_SRAM_OP_SEL_SHIFT_ADD);

	mvpp2_prs_sram_ri_update(&pe, MVPP2_PRS_RI_L3_UCAST,
				 MVPP2_PRS_RI_L3_ADDR_MASK);
	mvpp2_prs_tcam_ai_update(&pe, 0, MVPP2_PRS_IPV4_DIP_AI_BIT);

	 
	mvpp2_prs_tcam_port_map_set(&pe, MVPP2_PRS_PORT_MASK);

	 
	mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_IP4);
	mvpp2_prs_hw_write(priv, &pe);

	return 0;
}

 
static int mvpp2_prs_ip6_init(struct mvpp2 *priv)
{
	struct mvpp2_prs_entry pe;
	int tid, err;

	 
	err = mvpp2_prs_ip6_proto(priv, IPPROTO_TCP,
				  MVPP2_PRS_RI_L4_TCP,
				  MVPP2_PRS_RI_L4_PROTO_MASK);
	if (err)
		return err;

	err = mvpp2_prs_ip6_proto(priv, IPPROTO_UDP,
				  MVPP2_PRS_RI_L4_UDP,
				  MVPP2_PRS_RI_L4_PROTO_MASK);
	if (err)
		return err;

	err = mvpp2_prs_ip6_proto(priv, IPPROTO_ICMPV6,
				  MVPP2_PRS_RI_CPU_CODE_RX_SPEC |
				  MVPP2_PRS_RI_UDF3_RX_SPECIAL,
				  MVPP2_PRS_RI_CPU_CODE_MASK |
				  MVPP2_PRS_RI_UDF3_MASK);
	if (err)
		return err;

	 
	 
	err = mvpp2_prs_ip6_proto(priv, IPPROTO_IPIP,
				  MVPP2_PRS_RI_UDF7_IP6_LITE,
				  MVPP2_PRS_RI_UDF7_MASK);
	if (err)
		return err;

	 
	err = mvpp2_prs_ip6_cast(priv, MVPP2_PRS_L3_MULTI_CAST);
	if (err)
		return err;

	 
	tid = mvpp2_prs_tcam_first_free(priv, MVPP2_PE_FIRST_FREE_TID,
					MVPP2_PE_LAST_FREE_TID);
	if (tid < 0)
		return tid;

	memset(&pe, 0, sizeof(pe));
	mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_IP6);
	pe.index = tid;

	 
	mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_FLOWS);
	mvpp2_prs_sram_bits_set(&pe, MVPP2_PRS_SRAM_LU_GEN_BIT, 1);
	mvpp2_prs_sram_ri_update(&pe, MVPP2_PRS_RI_L3_UN |
				 MVPP2_PRS_RI_DROP_MASK,
				 MVPP2_PRS_RI_L3_PROTO_MASK |
				 MVPP2_PRS_RI_DROP_MASK);

	mvpp2_prs_tcam_data_byte_set(&pe, 1, 0x00, MVPP2_PRS_IPV6_HOP_MASK);
	mvpp2_prs_tcam_ai_update(&pe, MVPP2_PRS_IPV6_NO_EXT_AI_BIT,
				 MVPP2_PRS_IPV6_NO_EXT_AI_BIT);

	 
	mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_IP4);
	mvpp2_prs_hw_write(priv, &pe);

	 
	memset(&pe, 0, sizeof(pe));
	mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_IP6);
	pe.index = MVPP2_PE_IP6_PROTO_UN;

	 
	mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_FLOWS);
	mvpp2_prs_sram_bits_set(&pe, MVPP2_PRS_SRAM_LU_GEN_BIT, 1);
	mvpp2_prs_sram_ri_update(&pe, MVPP2_PRS_RI_L4_OTHER,
				 MVPP2_PRS_RI_L4_PROTO_MASK);
	 
	mvpp2_prs_sram_offset_set(&pe, MVPP2_PRS_SRAM_UDF_TYPE_L4,
				  sizeof(struct ipv6hdr) - 4,
				  MVPP2_PRS_SRAM_OP_SEL_UDF_ADD);

	mvpp2_prs_tcam_ai_update(&pe, MVPP2_PRS_IPV6_NO_EXT_AI_BIT,
				 MVPP2_PRS_IPV6_NO_EXT_AI_BIT);
	 
	mvpp2_prs_tcam_port_map_set(&pe, MVPP2_PRS_PORT_MASK);

	 
	mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_IP4);
	mvpp2_prs_hw_write(priv, &pe);

	 
	memset(&pe, 0, sizeof(struct mvpp2_prs_entry));
	mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_IP6);
	pe.index = MVPP2_PE_IP6_EXT_PROTO_UN;

	 
	mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_FLOWS);
	mvpp2_prs_sram_bits_set(&pe, MVPP2_PRS_SRAM_LU_GEN_BIT, 1);
	mvpp2_prs_sram_ri_update(&pe, MVPP2_PRS_RI_L4_OTHER,
				 MVPP2_PRS_RI_L4_PROTO_MASK);

	mvpp2_prs_tcam_ai_update(&pe, MVPP2_PRS_IPV6_EXT_AI_BIT,
				 MVPP2_PRS_IPV6_EXT_AI_BIT);
	 
	mvpp2_prs_tcam_port_map_set(&pe, MVPP2_PRS_PORT_MASK);

	 
	mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_IP4);
	mvpp2_prs_hw_write(priv, &pe);

	 
	memset(&pe, 0, sizeof(struct mvpp2_prs_entry));
	mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_IP6);
	pe.index = MVPP2_PE_IP6_ADDR_UN;

	 
	mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_IP6);
	mvpp2_prs_sram_ri_update(&pe, MVPP2_PRS_RI_L3_UCAST,
				 MVPP2_PRS_RI_L3_ADDR_MASK);
	mvpp2_prs_sram_ai_update(&pe, MVPP2_PRS_IPV6_NO_EXT_AI_BIT,
				 MVPP2_PRS_IPV6_NO_EXT_AI_BIT);
	 
	mvpp2_prs_sram_shift_set(&pe, -18, MVPP2_PRS_SRAM_OP_SEL_SHIFT_ADD);

	mvpp2_prs_tcam_ai_update(&pe, 0, MVPP2_PRS_IPV6_NO_EXT_AI_BIT);
	 
	mvpp2_prs_tcam_port_map_set(&pe, MVPP2_PRS_PORT_MASK);

	 
	mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_IP6);
	mvpp2_prs_hw_write(priv, &pe);

	return 0;
}

 
static int mvpp2_prs_vid_range_find(struct mvpp2_port *port, u16 vid, u16 mask)
{
	unsigned char byte[2], enable[2];
	struct mvpp2_prs_entry pe;
	u16 rvid, rmask;
	int tid;

	 
	for (tid = MVPP2_PRS_VID_PORT_FIRST(port->id);
	     tid <= MVPP2_PRS_VID_PORT_LAST(port->id); tid++) {
		if (!port->priv->prs_shadow[tid].valid ||
		    port->priv->prs_shadow[tid].lu != MVPP2_PRS_LU_VID)
			continue;

		mvpp2_prs_init_from_hw(port->priv, &pe, tid);

		mvpp2_prs_tcam_data_byte_get(&pe, 2, &byte[0], &enable[0]);
		mvpp2_prs_tcam_data_byte_get(&pe, 3, &byte[1], &enable[1]);

		rvid = ((byte[0] & 0xf) << 8) + byte[1];
		rmask = ((enable[0] & 0xf) << 8) + enable[1];

		if (rvid != vid || rmask != mask)
			continue;

		return tid;
	}

	return -ENOENT;
}

 
int mvpp2_prs_vid_entry_add(struct mvpp2_port *port, u16 vid)
{
	unsigned int vid_start = MVPP2_PE_VID_FILT_RANGE_START +
				 port->id * MVPP2_PRS_VLAN_FILT_MAX;
	unsigned int mask = 0xfff, reg_val, shift;
	struct mvpp2 *priv = port->priv;
	struct mvpp2_prs_entry pe;
	int tid;

	memset(&pe, 0, sizeof(pe));

	 
	tid = mvpp2_prs_vid_range_find(port, vid, mask);

	reg_val = mvpp2_read(priv, MVPP2_MH_REG(port->id));
	if (reg_val & MVPP2_DSA_EXTENDED)
		shift = MVPP2_VLAN_TAG_EDSA_LEN;
	else
		shift = MVPP2_VLAN_TAG_LEN;

	 
	if (tid < 0) {

		 
		tid = mvpp2_prs_tcam_first_free(priv, vid_start,
						vid_start +
						MVPP2_PRS_VLAN_FILT_MAX_ENTRY);

		 
		if (tid < 0)
			return tid;

		mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_VID);
		pe.index = tid;

		 
		mvpp2_prs_tcam_port_map_set(&pe, 0);
	} else {
		mvpp2_prs_init_from_hw(priv, &pe, tid);
	}

	 
	mvpp2_prs_tcam_port_set(&pe, port->id, true);

	 
	mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_L2);

	 
	mvpp2_prs_sram_shift_set(&pe, shift, MVPP2_PRS_SRAM_OP_SEL_SHIFT_ADD);

	 
	mvpp2_prs_match_vid(&pe, MVPP2_PRS_VID_TCAM_BYTE, vid);

	 
	mvpp2_prs_sram_ai_update(&pe, 0, MVPP2_PRS_SRAM_AI_MASK);

	 
	mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_VID);
	mvpp2_prs_hw_write(priv, &pe);

	return 0;
}

 
void mvpp2_prs_vid_entry_remove(struct mvpp2_port *port, u16 vid)
{
	struct mvpp2 *priv = port->priv;
	int tid;

	 
	tid = mvpp2_prs_vid_range_find(port, vid, 0xfff);

	 
	if (tid < 0)
		return;

	mvpp2_prs_hw_inv(priv, tid);
	priv->prs_shadow[tid].valid = false;
}

 
void mvpp2_prs_vid_remove_all(struct mvpp2_port *port)
{
	struct mvpp2 *priv = port->priv;
	int tid;

	for (tid = MVPP2_PRS_VID_PORT_FIRST(port->id);
	     tid <= MVPP2_PRS_VID_PORT_LAST(port->id); tid++) {
		if (priv->prs_shadow[tid].valid) {
			mvpp2_prs_hw_inv(priv, tid);
			priv->prs_shadow[tid].valid = false;
		}
	}
}

 
void mvpp2_prs_vid_disable_filtering(struct mvpp2_port *port)
{
	unsigned int tid = MVPP2_PRS_VID_PORT_DFLT(port->id);
	struct mvpp2 *priv = port->priv;

	 
	mvpp2_prs_hw_inv(priv, tid);

	priv->prs_shadow[tid].valid = false;
}

 
void mvpp2_prs_vid_enable_filtering(struct mvpp2_port *port)
{
	unsigned int tid = MVPP2_PRS_VID_PORT_DFLT(port->id);
	struct mvpp2 *priv = port->priv;
	unsigned int reg_val, shift;
	struct mvpp2_prs_entry pe;

	if (priv->prs_shadow[tid].valid)
		return;

	memset(&pe, 0, sizeof(pe));

	pe.index = tid;

	reg_val = mvpp2_read(priv, MVPP2_MH_REG(port->id));
	if (reg_val & MVPP2_DSA_EXTENDED)
		shift = MVPP2_VLAN_TAG_EDSA_LEN;
	else
		shift = MVPP2_VLAN_TAG_LEN;

	mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_VID);

	 
	mvpp2_prs_tcam_port_map_set(&pe, 0);

	 
	mvpp2_prs_tcam_port_set(&pe, port->id, true);

	 
	mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_L2);

	 
	mvpp2_prs_sram_shift_set(&pe, shift, MVPP2_PRS_SRAM_OP_SEL_SHIFT_ADD);

	 
	mvpp2_prs_sram_ri_update(&pe, MVPP2_PRS_RI_DROP_MASK,
				 MVPP2_PRS_RI_DROP_MASK);

	 
	mvpp2_prs_sram_ai_update(&pe, 0, MVPP2_PRS_SRAM_AI_MASK);

	 
	mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_VID);
	mvpp2_prs_hw_write(priv, &pe);
}

 
int mvpp2_prs_default_init(struct platform_device *pdev, struct mvpp2 *priv)
{
	int err, index, i;

	 
	mvpp2_write(priv, MVPP2_PRS_TCAM_CTRL_REG, MVPP2_PRS_TCAM_EN_MASK);

	 
	for (index = 0; index < MVPP2_PRS_TCAM_SRAM_SIZE; index++) {
		mvpp2_write(priv, MVPP2_PRS_TCAM_IDX_REG, index);
		for (i = 0; i < MVPP2_PRS_TCAM_WORDS; i++)
			mvpp2_write(priv, MVPP2_PRS_TCAM_DATA_REG(i), 0);

		mvpp2_write(priv, MVPP2_PRS_SRAM_IDX_REG, index);
		for (i = 0; i < MVPP2_PRS_SRAM_WORDS; i++)
			mvpp2_write(priv, MVPP2_PRS_SRAM_DATA_REG(i), 0);
	}

	 
	for (index = 0; index < MVPP2_PRS_TCAM_SRAM_SIZE; index++)
		mvpp2_prs_hw_inv(priv, index);

	priv->prs_shadow = devm_kcalloc(&pdev->dev, MVPP2_PRS_TCAM_SRAM_SIZE,
					sizeof(*priv->prs_shadow),
					GFP_KERNEL);
	if (!priv->prs_shadow)
		return -ENOMEM;

	 
	for (index = 0; index < MVPP2_MAX_PORTS; index++)
		mvpp2_prs_hw_port_init(priv, index, MVPP2_PRS_LU_MH,
				       MVPP2_PRS_PORT_LU_MAX, 0);

	mvpp2_prs_def_flow_init(priv);

	mvpp2_prs_mh_init(priv);

	mvpp2_prs_mac_init(priv);

	mvpp2_prs_dsa_init(priv);

	mvpp2_prs_vid_init(priv);

	err = mvpp2_prs_etype_init(priv);
	if (err)
		return err;

	err = mvpp2_prs_vlan_init(pdev, priv);
	if (err)
		return err;

	err = mvpp2_prs_pppoe_init(priv);
	if (err)
		return err;

	err = mvpp2_prs_ip6_init(priv);
	if (err)
		return err;

	err = mvpp2_prs_ip4_init(priv);
	if (err)
		return err;

	return 0;
}

 
static bool mvpp2_prs_mac_range_equals(struct mvpp2_prs_entry *pe,
				       const u8 *da, unsigned char *mask)
{
	unsigned char tcam_byte, tcam_mask;
	int index;

	for (index = 0; index < ETH_ALEN; index++) {
		mvpp2_prs_tcam_data_byte_get(pe, index, &tcam_byte, &tcam_mask);
		if (tcam_mask != mask[index])
			return false;

		if ((tcam_mask & tcam_byte) != (da[index] & mask[index]))
			return false;
	}

	return true;
}

 
static int
mvpp2_prs_mac_da_range_find(struct mvpp2 *priv, int pmap, const u8 *da,
			    unsigned char *mask, int udf_type)
{
	struct mvpp2_prs_entry pe;
	int tid;

	 
	for (tid = MVPP2_PE_MAC_RANGE_START;
	     tid <= MVPP2_PE_MAC_RANGE_END; tid++) {
		unsigned int entry_pmap;

		if (!priv->prs_shadow[tid].valid ||
		    (priv->prs_shadow[tid].lu != MVPP2_PRS_LU_MAC) ||
		    (priv->prs_shadow[tid].udf != udf_type))
			continue;

		mvpp2_prs_init_from_hw(priv, &pe, tid);
		entry_pmap = mvpp2_prs_tcam_port_map_get(&pe);

		if (mvpp2_prs_mac_range_equals(&pe, da, mask) &&
		    entry_pmap == pmap)
			return tid;
	}

	return -ENOENT;
}

 
int mvpp2_prs_mac_da_accept(struct mvpp2_port *port, const u8 *da, bool add)
{
	unsigned char mask[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	struct mvpp2 *priv = port->priv;
	unsigned int pmap, len, ri;
	struct mvpp2_prs_entry pe;
	int tid;

	memset(&pe, 0, sizeof(pe));

	 
	tid = mvpp2_prs_mac_da_range_find(priv, BIT(port->id), da, mask,
					  MVPP2_PRS_UDF_MAC_DEF);

	 
	if (tid < 0) {
		if (!add)
			return 0;

		 
		 
		tid = mvpp2_prs_tcam_first_free(priv,
						MVPP2_PE_MAC_RANGE_START,
						MVPP2_PE_MAC_RANGE_END);
		if (tid < 0)
			return tid;

		pe.index = tid;

		 
		mvpp2_prs_tcam_port_map_set(&pe, 0);
	} else {
		mvpp2_prs_init_from_hw(priv, &pe, tid);
	}

	mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_MAC);

	 
	mvpp2_prs_tcam_port_set(&pe, port->id, add);

	 
	pmap = mvpp2_prs_tcam_port_map_get(&pe);
	if (pmap == 0) {
		if (add)
			return -EINVAL;

		mvpp2_prs_hw_inv(priv, pe.index);
		priv->prs_shadow[pe.index].valid = false;
		return 0;
	}

	 
	mvpp2_prs_sram_next_lu_set(&pe, MVPP2_PRS_LU_DSA);

	 
	len = ETH_ALEN;
	while (len--)
		mvpp2_prs_tcam_data_byte_set(&pe, len, da[len], 0xff);

	 
	if (is_broadcast_ether_addr(da)) {
		ri = MVPP2_PRS_RI_L2_BCAST;
	} else if (is_multicast_ether_addr(da)) {
		ri = MVPP2_PRS_RI_L2_MCAST;
	} else {
		ri = MVPP2_PRS_RI_L2_UCAST;

		if (ether_addr_equal(da, port->dev->dev_addr))
			ri |= MVPP2_PRS_RI_MAC_ME_MASK;
	}

	mvpp2_prs_sram_ri_update(&pe, ri, MVPP2_PRS_RI_L2_CAST_MASK |
				 MVPP2_PRS_RI_MAC_ME_MASK);
	mvpp2_prs_shadow_ri_set(priv, pe.index, ri, MVPP2_PRS_RI_L2_CAST_MASK |
				MVPP2_PRS_RI_MAC_ME_MASK);

	 
	mvpp2_prs_sram_shift_set(&pe, 2 * ETH_ALEN,
				 MVPP2_PRS_SRAM_OP_SEL_SHIFT_ADD);

	 
	priv->prs_shadow[pe.index].udf = MVPP2_PRS_UDF_MAC_DEF;
	mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_MAC);
	mvpp2_prs_hw_write(priv, &pe);

	return 0;
}

int mvpp2_prs_update_mac_da(struct net_device *dev, const u8 *da)
{
	struct mvpp2_port *port = netdev_priv(dev);
	int err;

	 
	err = mvpp2_prs_mac_da_accept(port, dev->dev_addr, false);
	if (err)
		return err;

	 
	err = mvpp2_prs_mac_da_accept(port, da, true);
	if (err)
		return err;

	 
	eth_hw_addr_set(dev, da);

	return 0;
}

void mvpp2_prs_mac_del_all(struct mvpp2_port *port)
{
	struct mvpp2 *priv = port->priv;
	struct mvpp2_prs_entry pe;
	unsigned long pmap;
	int index, tid;

	for (tid = MVPP2_PE_MAC_RANGE_START;
	     tid <= MVPP2_PE_MAC_RANGE_END; tid++) {
		unsigned char da[ETH_ALEN], da_mask[ETH_ALEN];

		if (!priv->prs_shadow[tid].valid ||
		    (priv->prs_shadow[tid].lu != MVPP2_PRS_LU_MAC) ||
		    (priv->prs_shadow[tid].udf != MVPP2_PRS_UDF_MAC_DEF))
			continue;

		mvpp2_prs_init_from_hw(priv, &pe, tid);

		pmap = mvpp2_prs_tcam_port_map_get(&pe);

		 
		if (!test_bit(port->id, &pmap))
			continue;

		 
		for (index = 0; index < ETH_ALEN; index++)
			mvpp2_prs_tcam_data_byte_get(&pe, index, &da[index],
						     &da_mask[index]);

		 
		if (is_broadcast_ether_addr(da) ||
		    ether_addr_equal(da, port->dev->dev_addr))
			continue;

		 
		mvpp2_prs_mac_da_accept(port, da, false);
	}
}

int mvpp2_prs_tag_mode_set(struct mvpp2 *priv, int port, int type)
{
	switch (type) {
	case MVPP2_TAG_TYPE_EDSA:
		 
		mvpp2_prs_dsa_tag_set(priv, port, true,
				      MVPP2_PRS_TAGGED, MVPP2_PRS_EDSA);
		mvpp2_prs_dsa_tag_set(priv, port, true,
				      MVPP2_PRS_UNTAGGED, MVPP2_PRS_EDSA);
		 
		mvpp2_prs_dsa_tag_set(priv, port, false,
				      MVPP2_PRS_TAGGED, MVPP2_PRS_DSA);
		mvpp2_prs_dsa_tag_set(priv, port, false,
				      MVPP2_PRS_UNTAGGED, MVPP2_PRS_DSA);
		break;

	case MVPP2_TAG_TYPE_DSA:
		 
		mvpp2_prs_dsa_tag_set(priv, port, true,
				      MVPP2_PRS_TAGGED, MVPP2_PRS_DSA);
		mvpp2_prs_dsa_tag_set(priv, port, true,
				      MVPP2_PRS_UNTAGGED, MVPP2_PRS_DSA);
		 
		mvpp2_prs_dsa_tag_set(priv, port, false,
				      MVPP2_PRS_TAGGED, MVPP2_PRS_EDSA);
		mvpp2_prs_dsa_tag_set(priv, port, false,
				      MVPP2_PRS_UNTAGGED, MVPP2_PRS_EDSA);
		break;

	case MVPP2_TAG_TYPE_MH:
	case MVPP2_TAG_TYPE_NONE:
		 
		mvpp2_prs_dsa_tag_set(priv, port, false,
				      MVPP2_PRS_TAGGED, MVPP2_PRS_DSA);
		mvpp2_prs_dsa_tag_set(priv, port, false,
				      MVPP2_PRS_UNTAGGED, MVPP2_PRS_DSA);
		mvpp2_prs_dsa_tag_set(priv, port, false,
				      MVPP2_PRS_TAGGED, MVPP2_PRS_EDSA);
		mvpp2_prs_dsa_tag_set(priv, port, false,
				      MVPP2_PRS_UNTAGGED, MVPP2_PRS_EDSA);
		break;

	default:
		if ((type < 0) || (type > MVPP2_TAG_TYPE_EDSA))
			return -EINVAL;
	}

	return 0;
}

int mvpp2_prs_add_flow(struct mvpp2 *priv, int flow, u32 ri, u32 ri_mask)
{
	struct mvpp2_prs_entry pe;
	u8 *ri_byte, *ri_byte_mask;
	int tid, i;

	memset(&pe, 0, sizeof(pe));

	tid = mvpp2_prs_tcam_first_free(priv,
					MVPP2_PE_LAST_FREE_TID,
					MVPP2_PE_FIRST_FREE_TID);
	if (tid < 0)
		return tid;

	pe.index = tid;

	ri_byte = (u8 *)&ri;
	ri_byte_mask = (u8 *)&ri_mask;

	mvpp2_prs_sram_ai_update(&pe, flow, MVPP2_PRS_FLOW_ID_MASK);
	mvpp2_prs_sram_bits_set(&pe, MVPP2_PRS_SRAM_LU_DONE_BIT, 1);

	for (i = 0; i < 4; i++) {
		mvpp2_prs_tcam_data_byte_set(&pe, i, ri_byte[i],
					     ri_byte_mask[i]);
	}

	mvpp2_prs_shadow_set(priv, pe.index, MVPP2_PRS_LU_FLOWS);
	mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_FLOWS);
	mvpp2_prs_tcam_port_map_set(&pe, MVPP2_PRS_PORT_MASK);
	mvpp2_prs_hw_write(priv, &pe);

	return 0;
}

 
int mvpp2_prs_def_flow(struct mvpp2_port *port)
{
	struct mvpp2_prs_entry pe;
	int tid;

	memset(&pe, 0, sizeof(pe));

	tid = mvpp2_prs_flow_find(port->priv, port->id);

	 
	if (tid < 0) {
		 
		tid = mvpp2_prs_tcam_first_free(port->priv,
						MVPP2_PE_LAST_FREE_TID,
					       MVPP2_PE_FIRST_FREE_TID);
		if (tid < 0)
			return tid;

		pe.index = tid;

		 
		mvpp2_prs_sram_ai_update(&pe, port->id, MVPP2_PRS_FLOW_ID_MASK);
		mvpp2_prs_sram_bits_set(&pe, MVPP2_PRS_SRAM_LU_DONE_BIT, 1);

		 
		mvpp2_prs_shadow_set(port->priv, pe.index, MVPP2_PRS_LU_FLOWS);
	} else {
		mvpp2_prs_init_from_hw(port->priv, &pe, tid);
	}

	mvpp2_prs_tcam_lu_set(&pe, MVPP2_PRS_LU_FLOWS);
	mvpp2_prs_tcam_port_map_set(&pe, (1 << port->id));
	mvpp2_prs_hw_write(port->priv, &pe);

	return 0;
}

int mvpp2_prs_hits(struct mvpp2 *priv, int index)
{
	u32 val;

	if (index > MVPP2_PRS_TCAM_SRAM_SIZE)
		return -EINVAL;

	mvpp2_write(priv, MVPP2_PRS_TCAM_HIT_IDX_REG, index);

	val = mvpp2_read(priv, MVPP2_PRS_TCAM_HIT_CNT_REG);

	val &= MVPP2_PRS_TCAM_HIT_CNT_MASK;

	return val;
}
