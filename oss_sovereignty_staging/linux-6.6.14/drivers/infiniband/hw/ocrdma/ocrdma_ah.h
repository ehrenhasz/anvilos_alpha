 

#ifndef __OCRDMA_AH_H__
#define __OCRDMA_AH_H__

enum {
	OCRDMA_AH_ID_MASK		= 0x3FF,
	OCRDMA_AH_VLAN_VALID_MASK	= 0x01,
	OCRDMA_AH_VLAN_VALID_SHIFT	= 0x1F,
	OCRDMA_AH_L3_TYPE_MASK		= 0x03,
	OCRDMA_AH_L3_TYPE_SHIFT		= 0x1D  
};

int ocrdma_create_ah(struct ib_ah *ah, struct rdma_ah_init_attr *init_attr,
		     struct ib_udata *udata);
int ocrdma_destroy_ah(struct ib_ah *ah, u32 flags);
int ocrdma_query_ah(struct ib_ah *ah, struct rdma_ah_attr *ah_attr);

int ocrdma_process_mad(struct ib_device *dev, int process_mad_flags,
		       u32 port_num, const struct ib_wc *in_wc,
		       const struct ib_grh *in_grh, const struct ib_mad *in,
		       struct ib_mad *out, size_t *out_mad_size,
		       u16 *out_mad_pkey_index);
#endif				 
