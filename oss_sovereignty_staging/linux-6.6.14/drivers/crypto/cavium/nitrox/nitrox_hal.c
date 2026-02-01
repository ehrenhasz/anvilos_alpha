
#include <linux/delay.h>

#include "nitrox_dev.h"
#include "nitrox_csr.h"
#include "nitrox_hal.h"

#define PLL_REF_CLK 50
#define MAX_CSR_RETRIES 10

 
static void emu_enable_cores(struct nitrox_device *ndev)
{
	union emu_se_enable emu_se;
	union emu_ae_enable emu_ae;
	int i;

	 
	emu_ae.value = 0;
	emu_ae.s.enable = 0xfffff;

	 
	emu_se.value = 0;
	emu_se.s.enable = 0xffff;

	 
	for (i = 0; i < NR_CLUSTERS; i++) {
		nitrox_write_csr(ndev, EMU_AE_ENABLEX(i), emu_ae.value);
		nitrox_write_csr(ndev, EMU_SE_ENABLEX(i), emu_se.value);
	}
}

 
void nitrox_config_emu_unit(struct nitrox_device *ndev)
{
	union emu_wd_int_ena_w1s emu_wd_int;
	union emu_ge_int_ena_w1s emu_ge_int;
	u64 offset;
	int i;

	 
	emu_enable_cores(ndev);

	 
	emu_ge_int.value = 0;
	emu_ge_int.s.se_ge = 0xffff;
	emu_ge_int.s.ae_ge = 0xfffff;
	emu_wd_int.value = 0;
	emu_wd_int.s.se_wd = 1;

	for (i = 0; i < NR_CLUSTERS; i++) {
		offset = EMU_WD_INT_ENA_W1SX(i);
		nitrox_write_csr(ndev, offset, emu_wd_int.value);
		offset = EMU_GE_INT_ENA_W1SX(i);
		nitrox_write_csr(ndev, offset, emu_ge_int.value);
	}
}

static void reset_pkt_input_ring(struct nitrox_device *ndev, int ring)
{
	union nps_pkt_in_instr_ctl pkt_in_ctl;
	union nps_pkt_in_done_cnts pkt_in_cnts;
	int max_retries = MAX_CSR_RETRIES;
	u64 offset;

	 
	offset = NPS_PKT_IN_INSTR_CTLX(ring);
	pkt_in_ctl.value = nitrox_read_csr(ndev, offset);
	pkt_in_ctl.s.enb = 0;
	nitrox_write_csr(ndev, offset, pkt_in_ctl.value);

	 
	usleep_range(100, 150);
	do {
		pkt_in_ctl.value = nitrox_read_csr(ndev, offset);
		if (!pkt_in_ctl.s.enb)
			break;
		udelay(50);
	} while (max_retries--);

	 
	offset = NPS_PKT_IN_DONE_CNTSX(ring);
	pkt_in_cnts.value = nitrox_read_csr(ndev, offset);
	nitrox_write_csr(ndev, offset, pkt_in_cnts.value);
	usleep_range(50, 100);
}

void enable_pkt_input_ring(struct nitrox_device *ndev, int ring)
{
	union nps_pkt_in_instr_ctl pkt_in_ctl;
	int max_retries = MAX_CSR_RETRIES;
	u64 offset;

	 
	offset = NPS_PKT_IN_INSTR_CTLX(ring);
	pkt_in_ctl.value = nitrox_read_csr(ndev, offset);
	pkt_in_ctl.s.is64b = 1;
	pkt_in_ctl.s.enb = 1;
	nitrox_write_csr(ndev, offset, pkt_in_ctl.value);

	 
	do {
		pkt_in_ctl.value = nitrox_read_csr(ndev, offset);
		if (pkt_in_ctl.s.enb)
			break;
		udelay(50);
	} while (max_retries--);
}

 
void nitrox_config_pkt_input_rings(struct nitrox_device *ndev)
{
	int i;

	for (i = 0; i < ndev->nr_queues; i++) {
		struct nitrox_cmdq *cmdq = &ndev->pkt_inq[i];
		union nps_pkt_in_instr_rsize pkt_in_rsize;
		union nps_pkt_in_instr_baoff_dbell pkt_in_dbell;
		u64 offset;

		reset_pkt_input_ring(ndev, i);

		 
		offset = NPS_PKT_IN_INSTR_BADDRX(i);
		nitrox_write_csr(ndev, offset, cmdq->dma);

		 
		offset = NPS_PKT_IN_INSTR_RSIZEX(i);
		pkt_in_rsize.value = 0;
		pkt_in_rsize.s.rsize = ndev->qlen;
		nitrox_write_csr(ndev, offset, pkt_in_rsize.value);

		 
		offset = NPS_PKT_IN_INT_LEVELSX(i);
		nitrox_write_csr(ndev, offset, 0xffffffff);

		 
		offset = NPS_PKT_IN_INSTR_BAOFF_DBELLX(i);
		pkt_in_dbell.value = 0;
		pkt_in_dbell.s.dbell = 0xffffffff;
		nitrox_write_csr(ndev, offset, pkt_in_dbell.value);

		 
		enable_pkt_input_ring(ndev, i);
	}
}

static void reset_pkt_solicit_port(struct nitrox_device *ndev, int port)
{
	union nps_pkt_slc_ctl pkt_slc_ctl;
	union nps_pkt_slc_cnts pkt_slc_cnts;
	int max_retries = MAX_CSR_RETRIES;
	u64 offset;

	 
	offset = NPS_PKT_SLC_CTLX(port);
	pkt_slc_ctl.value = nitrox_read_csr(ndev, offset);
	pkt_slc_ctl.s.enb = 0;
	nitrox_write_csr(ndev, offset, pkt_slc_ctl.value);

	 
	usleep_range(100, 150);
	 
	do {
		pkt_slc_ctl.value = nitrox_read_csr(ndev, offset);
		if (!pkt_slc_ctl.s.enb)
			break;
		udelay(50);
	} while (max_retries--);

	 
	offset = NPS_PKT_SLC_CNTSX(port);
	pkt_slc_cnts.value = nitrox_read_csr(ndev, offset);
	nitrox_write_csr(ndev, offset, pkt_slc_cnts.value);
	usleep_range(50, 100);
}

void enable_pkt_solicit_port(struct nitrox_device *ndev, int port)
{
	union nps_pkt_slc_ctl pkt_slc_ctl;
	int max_retries = MAX_CSR_RETRIES;
	u64 offset;

	offset = NPS_PKT_SLC_CTLX(port);
	pkt_slc_ctl.value = 0;
	pkt_slc_ctl.s.enb = 1;
	 
	pkt_slc_ctl.s.z = 1;
	 
	pkt_slc_ctl.s.rh = 1;
	nitrox_write_csr(ndev, offset, pkt_slc_ctl.value);

	 
	do {
		pkt_slc_ctl.value = nitrox_read_csr(ndev, offset);
		if (pkt_slc_ctl.s.enb)
			break;
		udelay(50);
	} while (max_retries--);
}

static void config_pkt_solicit_port(struct nitrox_device *ndev, int port)
{
	union nps_pkt_slc_int_levels pkt_slc_int;
	u64 offset;

	reset_pkt_solicit_port(ndev, port);

	 
	offset = NPS_PKT_SLC_INT_LEVELSX(port);
	pkt_slc_int.value = 0;
	 
	pkt_slc_int.s.timet = 0x3fffff;
	nitrox_write_csr(ndev, offset, pkt_slc_int.value);

	 
	enable_pkt_solicit_port(ndev, port);
}

void nitrox_config_pkt_solicit_ports(struct nitrox_device *ndev)
{
	int i;

	for (i = 0; i < ndev->nr_queues; i++)
		config_pkt_solicit_port(ndev, i);
}

 
static void enable_nps_core_interrupts(struct nitrox_device *ndev)
{
	union nps_core_int_ena_w1s core_int;

	 
	core_int.value = 0;
	core_int.s.host_wr_err = 1;
	core_int.s.host_wr_timeout = 1;
	core_int.s.exec_wr_timeout = 1;
	core_int.s.npco_dma_malform = 1;
	core_int.s.host_nps_wr_err = 1;
	nitrox_write_csr(ndev, NPS_CORE_INT_ENA_W1S, core_int.value);
}

void nitrox_config_nps_core_unit(struct nitrox_device *ndev)
{
	union nps_core_gbl_vfcfg core_gbl_vfcfg;

	 
	nitrox_write_csr(ndev, NPS_CORE_CONTROL, 1ULL);

	 
	core_gbl_vfcfg.value = 0;
	core_gbl_vfcfg.s.ilk_disable = 1;
	core_gbl_vfcfg.s.cfg = __NDEV_MODE_PF;
	nitrox_write_csr(ndev, NPS_CORE_GBL_VFCFG, core_gbl_vfcfg.value);

	 
	enable_nps_core_interrupts(ndev);
}

 
static void enable_nps_pkt_interrupts(struct nitrox_device *ndev)
{
	 
	nitrox_write_csr(ndev, NPS_PKT_IN_RERR_LO_ENA_W1S, (~0ULL));
	nitrox_write_csr(ndev, NPS_PKT_IN_RERR_HI_ENA_W1S, (~0ULL));
	nitrox_write_csr(ndev, NPS_PKT_IN_ERR_TYPE_ENA_W1S, (~0ULL));
	 
	nitrox_write_csr(ndev, NPS_PKT_SLC_RERR_HI_ENA_W1S, (~0ULL));
	nitrox_write_csr(ndev, NPS_PKT_SLC_RERR_LO_ENA_W1S, (~0ULL));
	nitrox_write_csr(ndev, NPS_PKT_SLC_ERR_TYPE_ENA_W1S, (~0uLL));
}

void nitrox_config_nps_pkt_unit(struct nitrox_device *ndev)
{
	 
	nitrox_config_pkt_input_rings(ndev);
	nitrox_config_pkt_solicit_ports(ndev);

	 
	enable_nps_pkt_interrupts(ndev);
}

static void reset_aqm_ring(struct nitrox_device *ndev, int ring)
{
	union aqmq_en aqmq_en_reg;
	union aqmq_activity_stat activity_stat;
	union aqmq_cmp_cnt cmp_cnt;
	int max_retries = MAX_CSR_RETRIES;
	u64 offset;

	 
	offset = AQMQ_ENX(ring);
	aqmq_en_reg.value = 0;
	aqmq_en_reg.queue_enable = 0;
	nitrox_write_csr(ndev, offset, aqmq_en_reg.value);

	 
	usleep_range(100, 150);
	offset = AQMQ_ACTIVITY_STATX(ring);
	do {
		activity_stat.value = nitrox_read_csr(ndev, offset);
		if (!activity_stat.queue_active)
			break;
		udelay(50);
	} while (max_retries--);

	 
	offset = AQMQ_CMP_CNTX(ring);
	cmp_cnt.value = nitrox_read_csr(ndev, offset);
	nitrox_write_csr(ndev, offset, cmp_cnt.value);
	usleep_range(50, 100);
}

void enable_aqm_ring(struct nitrox_device *ndev, int ring)
{
	union aqmq_en aqmq_en_reg;
	u64 offset;

	offset = AQMQ_ENX(ring);
	aqmq_en_reg.value = 0;
	aqmq_en_reg.queue_enable = 1;
	nitrox_write_csr(ndev, offset, aqmq_en_reg.value);
	usleep_range(50, 100);
}

void nitrox_config_aqm_rings(struct nitrox_device *ndev)
{
	int ring;

	for (ring = 0; ring < ndev->nr_queues; ring++) {
		struct nitrox_cmdq *cmdq = ndev->aqmq[ring];
		union aqmq_drbl drbl;
		union aqmq_qsz qsize;
		union aqmq_cmp_thr cmp_thr;
		u64 offset;

		 
		reset_aqm_ring(ndev, ring);

		 
		offset = AQMQ_DRBLX(ring);
		drbl.value = 0;
		drbl.dbell_count = 0xFFFFFFFF;
		nitrox_write_csr(ndev, offset, drbl.value);

		 

		 
		offset = AQMQ_NXT_CMDX(ring);
		nitrox_write_csr(ndev, offset, 0ULL);

		 
		offset = AQMQ_BADRX(ring);
		nitrox_write_csr(ndev, offset, cmdq->dma);

		 
		offset = AQMQ_QSZX(ring);
		qsize.value = 0;
		qsize.host_queue_size = ndev->qlen;
		nitrox_write_csr(ndev, offset, qsize.value);

		 
		offset = AQMQ_CMP_THRX(ring);
		cmp_thr.value = 0;
		cmp_thr.commands_completed_threshold = 1;
		nitrox_write_csr(ndev, offset, cmp_thr.value);

		 
		enable_aqm_ring(ndev, ring);
	}
}

static void enable_aqm_interrupts(struct nitrox_device *ndev)
{
	 
	nitrox_write_csr(ndev, AQM_DBELL_OVF_LO_ENA_W1S, (~0ULL));
	nitrox_write_csr(ndev, AQM_DBELL_OVF_HI_ENA_W1S, (~0ULL));
	nitrox_write_csr(ndev, AQM_DMA_RD_ERR_LO_ENA_W1S, (~0ULL));
	nitrox_write_csr(ndev, AQM_DMA_RD_ERR_HI_ENA_W1S, (~0ULL));
	nitrox_write_csr(ndev, AQM_EXEC_NA_LO_ENA_W1S, (~0ULL));
	nitrox_write_csr(ndev, AQM_EXEC_NA_HI_ENA_W1S, (~0ULL));
	nitrox_write_csr(ndev, AQM_EXEC_ERR_LO_ENA_W1S, (~0ULL));
	nitrox_write_csr(ndev, AQM_EXEC_ERR_HI_ENA_W1S, (~0ULL));
}

void nitrox_config_aqm_unit(struct nitrox_device *ndev)
{
	 
	nitrox_config_aqm_rings(ndev);

	 
	enable_aqm_interrupts(ndev);
}

void nitrox_config_pom_unit(struct nitrox_device *ndev)
{
	union pom_int_ena_w1s pom_int;
	int i;

	 
	pom_int.value = 0;
	pom_int.s.illegal_dport = 1;
	nitrox_write_csr(ndev, POM_INT_ENA_W1S, pom_int.value);

	 
	for (i = 0; i < ndev->hw.se_cores; i++)
		nitrox_write_csr(ndev, POM_PERF_CTL, BIT_ULL(i));
}

 
void nitrox_config_rand_unit(struct nitrox_device *ndev)
{
	union efl_rnm_ctl_status efl_rnm_ctl;
	u64 offset;

	offset = EFL_RNM_CTL_STATUS;
	efl_rnm_ctl.value = nitrox_read_csr(ndev, offset);
	efl_rnm_ctl.s.ent_en = 1;
	efl_rnm_ctl.s.rng_en = 1;
	nitrox_write_csr(ndev, offset, efl_rnm_ctl.value);
}

void nitrox_config_efl_unit(struct nitrox_device *ndev)
{
	int i;

	for (i = 0; i < NR_CLUSTERS; i++) {
		union efl_core_int_ena_w1s efl_core_int;
		u64 offset;

		 
		offset = EFL_CORE_INT_ENA_W1SX(i);
		efl_core_int.value = 0;
		efl_core_int.s.len_ovr = 1;
		efl_core_int.s.d_left = 1;
		efl_core_int.s.epci_decode_err = 1;
		nitrox_write_csr(ndev, offset, efl_core_int.value);

		offset = EFL_CORE_VF_ERR_INT0_ENA_W1SX(i);
		nitrox_write_csr(ndev, offset, (~0ULL));
		offset = EFL_CORE_VF_ERR_INT1_ENA_W1SX(i);
		nitrox_write_csr(ndev, offset, (~0ULL));
	}
}

void nitrox_config_bmi_unit(struct nitrox_device *ndev)
{
	union bmi_ctl bmi_ctl;
	union bmi_int_ena_w1s bmi_int_ena;
	u64 offset;

	 
	offset = BMI_CTL;
	bmi_ctl.value = nitrox_read_csr(ndev, offset);
	bmi_ctl.s.max_pkt_len = 0xff;
	bmi_ctl.s.nps_free_thrsh = 0xff;
	bmi_ctl.s.nps_hdrq_thrsh = 0x7a;
	nitrox_write_csr(ndev, offset, bmi_ctl.value);

	 
	offset = BMI_INT_ENA_W1S;
	bmi_int_ena.value = 0;
	bmi_int_ena.s.max_len_err_nps = 1;
	bmi_int_ena.s.pkt_rcv_err_nps = 1;
	bmi_int_ena.s.fpf_undrrn = 1;
	nitrox_write_csr(ndev, offset, bmi_int_ena.value);
}

void nitrox_config_bmo_unit(struct nitrox_device *ndev)
{
	union bmo_ctl2 bmo_ctl2;
	u64 offset;

	 
	offset = BMO_CTL2;
	bmo_ctl2.value = nitrox_read_csr(ndev, offset);
	bmo_ctl2.s.nps_slc_buf_thrsh = 0xff;
	nitrox_write_csr(ndev, offset, bmo_ctl2.value);
}

void invalidate_lbc(struct nitrox_device *ndev)
{
	union lbc_inval_ctl lbc_ctl;
	union lbc_inval_status lbc_stat;
	int max_retries = MAX_CSR_RETRIES;
	u64 offset;

	 
	offset = LBC_INVAL_CTL;
	lbc_ctl.value = nitrox_read_csr(ndev, offset);
	lbc_ctl.s.cam_inval_start = 1;
	nitrox_write_csr(ndev, offset, lbc_ctl.value);

	offset = LBC_INVAL_STATUS;
	do {
		lbc_stat.value = nitrox_read_csr(ndev, offset);
		if (lbc_stat.s.done)
			break;
		udelay(50);
	} while (max_retries--);
}

void nitrox_config_lbc_unit(struct nitrox_device *ndev)
{
	union lbc_int_ena_w1s lbc_int_ena;
	u64 offset;

	invalidate_lbc(ndev);

	 
	offset = LBC_INT_ENA_W1S;
	lbc_int_ena.value = 0;
	lbc_int_ena.s.dma_rd_err = 1;
	lbc_int_ena.s.over_fetch_err = 1;
	lbc_int_ena.s.cam_inval_abort = 1;
	lbc_int_ena.s.cam_hard_err = 1;
	nitrox_write_csr(ndev, offset, lbc_int_ena.value);

	offset = LBC_PLM_VF1_64_INT_ENA_W1S;
	nitrox_write_csr(ndev, offset, (~0ULL));
	offset = LBC_PLM_VF65_128_INT_ENA_W1S;
	nitrox_write_csr(ndev, offset, (~0ULL));

	offset = LBC_ELM_VF1_64_INT_ENA_W1S;
	nitrox_write_csr(ndev, offset, (~0ULL));
	offset = LBC_ELM_VF65_128_INT_ENA_W1S;
	nitrox_write_csr(ndev, offset, (~0ULL));
}

void config_nps_core_vfcfg_mode(struct nitrox_device *ndev, enum vf_mode mode)
{
	union nps_core_gbl_vfcfg vfcfg;

	vfcfg.value = nitrox_read_csr(ndev, NPS_CORE_GBL_VFCFG);
	vfcfg.s.cfg = mode & 0x7;

	nitrox_write_csr(ndev, NPS_CORE_GBL_VFCFG, vfcfg.value);
}

static const char *get_core_option(u8 se_cores, u8 ae_cores)
{
	const char *option = "";

	if (ae_cores == AE_MAX_CORES) {
		switch (se_cores) {
		case SE_MAX_CORES:
			option = "60";
			break;
		case 40:
			option = "60s";
			break;
		}
	} else if (ae_cores == (AE_MAX_CORES / 2)) {
		option = "30";
	} else {
		option = "60i";
	}

	return option;
}

static const char *get_feature_option(u8 zip_cores, int core_freq)
{
	if (zip_cores == 0)
		return "";
	else if (zip_cores < ZIP_MAX_CORES)
		return "-C15";

	if (core_freq >= 850)
		return "-C45";
	else if (core_freq >= 750)
		return "-C35";
	else if (core_freq >= 550)
		return "-C25";

	return "";
}

void nitrox_get_hwinfo(struct nitrox_device *ndev)
{
	union emu_fuse_map emu_fuse;
	union rst_boot rst_boot;
	union fus_dat1 fus_dat1;
	unsigned char name[IFNAMSIZ * 2] = {};
	int i, dead_cores;
	u64 offset;

	 
	offset = RST_BOOT;
	rst_boot.value = nitrox_read_csr(ndev, offset);
	ndev->hw.freq = (rst_boot.pnr_mul + 3) * PLL_REF_CLK;

	for (i = 0; i < NR_CLUSTERS; i++) {
		offset = EMU_FUSE_MAPX(i);
		emu_fuse.value = nitrox_read_csr(ndev, offset);
		if (emu_fuse.s.valid) {
			dead_cores = hweight32(emu_fuse.s.ae_fuse);
			ndev->hw.ae_cores += AE_CORES_PER_CLUSTER - dead_cores;
			dead_cores = hweight16(emu_fuse.s.se_fuse);
			ndev->hw.se_cores += SE_CORES_PER_CLUSTER - dead_cores;
		}
	}
	 
	offset = FUS_DAT1;
	fus_dat1.value = nitrox_read_csr(ndev, offset);
	if (!fus_dat1.nozip) {
		dead_cores = hweight8(fus_dat1.zip_info);
		ndev->hw.zip_cores = ZIP_MAX_CORES - dead_cores;
	}

	 
	snprintf(name, sizeof(name), "CNN55%s-%3dBG676%s-1.%u",
		 get_core_option(ndev->hw.se_cores, ndev->hw.ae_cores),
		 ndev->hw.freq,
		 get_feature_option(ndev->hw.zip_cores, ndev->hw.freq),
		 ndev->hw.revision_id);

	 
	strncpy(ndev->hw.partname, name, sizeof(ndev->hw.partname));
}

void enable_pf2vf_mbox_interrupts(struct nitrox_device *ndev)
{
	u64 value = ~0ULL;
	u64 reg_addr;

	 
	reg_addr = NPS_PKT_MBOX_INT_LO_ENA_W1S;
	nitrox_write_csr(ndev, reg_addr, value);

	 
	reg_addr = NPS_PKT_MBOX_INT_HI_ENA_W1S;
	nitrox_write_csr(ndev, reg_addr, value);
}

void disable_pf2vf_mbox_interrupts(struct nitrox_device *ndev)
{
	u64 value = ~0ULL;
	u64 reg_addr;

	 
	reg_addr = NPS_PKT_MBOX_INT_LO_ENA_W1C;
	nitrox_write_csr(ndev, reg_addr, value);

	 
	reg_addr = NPS_PKT_MBOX_INT_HI_ENA_W1C;
	nitrox_write_csr(ndev, reg_addr, value);
}
