#ifndef __OCTEON_NIC_H__
#define  __OCTEON_NIC_H__
#define  MAX_NCTRL_UDD  32
typedef void (*octnic_ctrl_pkt_cb_fn_t) (void *);
struct octnic_ctrl_pkt {
	union octnet_cmd ncmd;
	void *data;
	u64 dmadata;
	void *rdata;
	u64 dmardata;
	u64 udd[MAX_NCTRL_UDD];
	u64 iq_no;
	u64 netpndev;
	octnic_ctrl_pkt_cb_fn_t cb_fn;
	u32 sc_status;
};
#define MAX_UDD_SIZE(nctrl) (sizeof((nctrl)->udd))
struct octnic_data_pkt {
	void *buf;
	u32 reqtype;
	u32 datasize;
	union octeon_instr_64B cmd;
	u32 q_no;
};
union octnic_cmd_setup {
	struct {
		u32 iq_no:8;
		u32 gather:1;
		u32 timestamp:1;
		u32 ip_csum:1;
		u32 transport_csum:1;
		u32 tnl_csum:1;
		u32 rsvd:19;
		union {
			u32 datasize;
			u32 gatherptrs;
		} u;
	} s;
	u64 u64;
};
static inline int octnet_iq_is_full(struct octeon_device *oct, u32 q_no)
{
	return ((u32)atomic_read(&oct->instr_queue[q_no]->instr_pending)
		>= (oct->instr_queue[q_no]->max_count - 2));
}
static inline void
octnet_prepare_pci_cmd_o2(struct octeon_device *oct,
			  union octeon_instr_64B *cmd,
			  union octnic_cmd_setup *setup, u32 tag)
{
	struct octeon_instr_ih2 *ih2;
	struct octeon_instr_irh *irh;
	union octnic_packet_params packet_params;
	int port;
	memset(cmd, 0, sizeof(union octeon_instr_64B));
	ih2 = (struct octeon_instr_ih2 *)&cmd->cmd2.ih2;
	ih2->fsz = LIO_PCICMD_O2;
	ih2->tagtype = ORDERED_TAG;
	ih2->grp = DEFAULT_POW_GRP;
	port = (int)oct->instr_queue[setup->s.iq_no]->txpciq.s.port;
	if (tag)
		ih2->tag = tag;
	else
		ih2->tag = LIO_DATA(port);
	ih2->raw = 1;
	ih2->qos = (port & 3) + 4;	 
	if (!setup->s.gather) {
		ih2->dlengsz = setup->s.u.datasize;
	} else {
		ih2->gather = 1;
		ih2->dlengsz = setup->s.u.gatherptrs;
	}
	irh = (struct octeon_instr_irh *)&cmd->cmd2.irh;
	irh->opcode = OPCODE_NIC;
	irh->subcode = OPCODE_NIC_NW_DATA;
	packet_params.u32 = 0;
	packet_params.s.ip_csum = setup->s.ip_csum;
	packet_params.s.transport_csum = setup->s.transport_csum;
	packet_params.s.tnl_csum = setup->s.tnl_csum;
	packet_params.s.tsflag = setup->s.timestamp;
	irh->ossp = packet_params.u32;
}
static inline void
octnet_prepare_pci_cmd_o3(struct octeon_device *oct,
			  union octeon_instr_64B *cmd,
			  union octnic_cmd_setup *setup, u32 tag)
{
	struct octeon_instr_irh *irh;
	struct octeon_instr_ih3     *ih3;
	struct octeon_instr_pki_ih3 *pki_ih3;
	union octnic_packet_params packet_params;
	int port;
	memset(cmd, 0, sizeof(union octeon_instr_64B));
	ih3 = (struct octeon_instr_ih3 *)&cmd->cmd3.ih3;
	pki_ih3 = (struct octeon_instr_pki_ih3 *)&cmd->cmd3.pki_ih3;
	ih3->pkind       = oct->instr_queue[setup->s.iq_no]->txpciq.s.pkind;
	ih3->fsz = LIO_PCICMD_O3;
	if (!setup->s.gather) {
		ih3->dlengsz = setup->s.u.datasize;
	} else {
		ih3->gather = 1;
		ih3->dlengsz = setup->s.u.gatherptrs;
	}
	pki_ih3->w       = 1;
	pki_ih3->raw     = 1;
	pki_ih3->utag    = 1;
	pki_ih3->utt     = 1;
	pki_ih3->uqpg    = oct->instr_queue[setup->s.iq_no]->txpciq.s.use_qpg;
	port = (int)oct->instr_queue[setup->s.iq_no]->txpciq.s.port;
	if (tag)
		pki_ih3->tag = tag;
	else
		pki_ih3->tag     = LIO_DATA(port);
	pki_ih3->tagtype = ORDERED_TAG;
	pki_ih3->qpg     = oct->instr_queue[setup->s.iq_no]->txpciq.s.qpg;
	pki_ih3->pm      = 0x7;  
	pki_ih3->sl      = 8;    
	irh = (struct octeon_instr_irh *)&cmd->cmd3.irh;
	irh->opcode = OPCODE_NIC;
	irh->subcode = OPCODE_NIC_NW_DATA;
	packet_params.u32 = 0;
	packet_params.s.ip_csum = setup->s.ip_csum;
	packet_params.s.transport_csum = setup->s.transport_csum;
	packet_params.s.tnl_csum = setup->s.tnl_csum;
	packet_params.s.tsflag = setup->s.timestamp;
	irh->ossp = packet_params.u32;
}
static inline void
octnet_prepare_pci_cmd(struct octeon_device *oct, union octeon_instr_64B *cmd,
		       union octnic_cmd_setup *setup, u32 tag)
{
	if (OCTEON_CN6XXX(oct))
		octnet_prepare_pci_cmd_o2(oct, cmd, setup, tag);
	else
		octnet_prepare_pci_cmd_o3(oct, cmd, setup, tag);
}
void *
octeon_alloc_soft_command_resp(struct octeon_device    *oct,
			       union octeon_instr_64B *cmd,
			       u32		       rdatasize);
int octnet_send_nic_data_pkt(struct octeon_device *oct,
			     struct octnic_data_pkt *ndata,
			     int xmit_more);
int
octnet_send_nic_ctrl_pkt(struct octeon_device *oct,
			 struct octnic_ctrl_pkt *nctrl);
#endif
