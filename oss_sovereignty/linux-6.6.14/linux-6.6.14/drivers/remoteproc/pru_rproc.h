#ifndef _PRU_RPROC_H_
#define _PRU_RPROC_H_
struct pruss_int_map {
	u8 event;
	u8 chnl;
	u8 host;
};
struct pru_irq_rsc {
	u8 type;
	u8 num_evts;
	struct pruss_int_map pru_intc_map[];
} __packed;
#endif	 
