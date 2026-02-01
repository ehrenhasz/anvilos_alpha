
 

#include "coresight-etm4x.h"
#include "coresight-etm4x-cfg.h"
#include "coresight-priv.h"
#include "coresight-syscfg.h"

 
#define CHECKREG(cval, elem) \
	{ \
		if (offset == cval) { \
			reg_csdev->driver_regval = &drvcfg->elem; \
			err = 0; \
			break; \
		} \
	}

#define CHECKREGIDX(cval, elem, off_idx, mask)	\
	{ \
		if (mask == cval) { \
			reg_csdev->driver_regval = &drvcfg->elem[off_idx]; \
			err = 0; \
			break; \
		} \
	}

 
static int etm4_cfg_map_reg_offset(struct etmv4_drvdata *drvdata,
				   struct cscfg_regval_csdev *reg_csdev, u32 offset)
{
	int err = -EINVAL, idx;
	struct etmv4_config *drvcfg = &drvdata->config;
	u32 off_mask;

	if (((offset >= TRCEVENTCTL0R) && (offset <= TRCVIPCSSCTLR)) ||
	    ((offset >= TRCSEQRSTEVR) && (offset <= TRCEXTINSELR)) ||
	    ((offset >= TRCCIDCCTLR0) && (offset <= TRCVMIDCCTLR1))) {
		do {
			CHECKREG(TRCEVENTCTL0R, eventctrl0);
			CHECKREG(TRCEVENTCTL1R, eventctrl1);
			CHECKREG(TRCSTALLCTLR, stall_ctrl);
			CHECKREG(TRCTSCTLR, ts_ctrl);
			CHECKREG(TRCSYNCPR, syncfreq);
			CHECKREG(TRCCCCTLR, ccctlr);
			CHECKREG(TRCBBCTLR, bb_ctrl);
			CHECKREG(TRCVICTLR, vinst_ctrl);
			CHECKREG(TRCVIIECTLR, viiectlr);
			CHECKREG(TRCVISSCTLR, vissctlr);
			CHECKREG(TRCVIPCSSCTLR, vipcssctlr);
			CHECKREG(TRCSEQRSTEVR, seq_rst);
			CHECKREG(TRCSEQSTR, seq_state);
			CHECKREG(TRCEXTINSELR, ext_inp);
			CHECKREG(TRCCIDCCTLR0, ctxid_mask0);
			CHECKREG(TRCCIDCCTLR1, ctxid_mask1);
			CHECKREG(TRCVMIDCCTLR0, vmid_mask0);
			CHECKREG(TRCVMIDCCTLR1, vmid_mask1);
		} while (0);
	} else if ((offset & GENMASK(11, 4)) == TRCSEQEVRn(0)) {
		 
		idx = (offset & GENMASK(3, 0)) / 4;
		if (idx < ETM_MAX_SEQ_STATES) {
			reg_csdev->driver_regval = &drvcfg->seq_ctrl[idx];
			err = 0;
		}
	} else if ((offset >= TRCSSCCRn(0)) && (offset <= TRCSSPCICRn(7))) {
		 
		idx = (offset & GENMASK(4, 0)) / 4;
		off_mask =  (offset & GENMASK(11, 5));
		do {
			CHECKREGIDX(TRCSSCCRn(0), ss_ctrl, idx, off_mask);
			CHECKREGIDX(TRCSSCSRn(0), ss_status, idx, off_mask);
			CHECKREGIDX(TRCSSPCICRn(0), ss_pe_cmp, idx, off_mask);
		} while (0);
	} else if ((offset >= TRCCIDCVRn(0)) && (offset <= TRCVMIDCVRn(7))) {
		 
		idx = (offset & GENMASK(5, 0)) / 8;
		off_mask = (offset & GENMASK(11, 6));
		do {
			CHECKREGIDX(TRCCIDCVRn(0), ctxid_pid, idx, off_mask);
			CHECKREGIDX(TRCVMIDCVRn(0), vmid_val, idx, off_mask);
		} while (0);
	} else if ((offset >= TRCRSCTLRn(2)) &&
		   (offset <= TRCRSCTLRn((ETM_MAX_RES_SEL - 1)))) {
		 
		idx = (offset & GENMASK(6, 0)) / 4;
		if (idx < ETM_MAX_RES_SEL) {
			reg_csdev->driver_regval = &drvcfg->res_ctrl[idx];
			err = 0;
		}
	} else if ((offset >= TRCACVRn(0)) &&
		   (offset <= TRCACATRn((ETM_MAX_SINGLE_ADDR_CMP - 1)))) {
		 
		idx = (offset & GENMASK(6, 0)) / 8;
		off_mask = offset & GENMASK(11, 7);
		do {
			CHECKREGIDX(TRCACVRn(0), addr_val, idx, off_mask);
			CHECKREGIDX(TRCACATRn(0), addr_acc, idx, off_mask);
		} while (0);
	} else if ((offset >= TRCCNTRLDVRn(0)) &&
		   (offset <= TRCCNTVRn((ETMv4_MAX_CNTR - 1)))) {
		 
		idx = (offset &  GENMASK(3, 0)) / 4;
		off_mask = offset &  GENMASK(11, 4);
		do {
			CHECKREGIDX(TRCCNTRLDVRn(0), cntrldvr, idx, off_mask);
			CHECKREGIDX(TRCCNTCTLRn(0), cntr_ctrl, idx, off_mask);
			CHECKREGIDX(TRCCNTVRn(0), cntr_val, idx, off_mask);
		} while (0);
	}
	return err;
}

 
static int etm4_cfg_load_feature(struct coresight_device *csdev,
				 struct cscfg_feature_csdev *feat_csdev)
{
	struct device *dev = csdev->dev.parent;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev);
	const struct cscfg_feature_desc *feat_desc = feat_csdev->feat_desc;
	u32 offset;
	int i = 0, err = 0;

	 
	feat_csdev->drv_spinlock = &drvdata->spinlock;

	 
	for (i = 0; i < feat_csdev->nr_regs && !err; i++) {
		offset = feat_desc->regs_desc[i].offset;
		err = etm4_cfg_map_reg_offset(drvdata, &feat_csdev->regs_csdev[i], offset);
	}
	return err;
}

 
#define CS_CFG_ETM4_MATCH_FLAGS	(CS_CFG_MATCH_CLASS_SRC_ALL | \
				 CS_CFG_MATCH_CLASS_SRC_ETM4)

int etm4_cscfg_register(struct coresight_device *csdev)
{
	struct cscfg_csdev_feat_ops ops;

	ops.load_feat = &etm4_cfg_load_feature;

	return cscfg_register_csdev(csdev, CS_CFG_ETM4_MATCH_FLAGS, &ops);
}
