
 

#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_net.h>
#include <linux/of_mdio.h>

#include "stmmac.h"
#include "stmmac_platform.h"

#ifdef CONFIG_OF

 
static int dwmac1000_validate_mcast_bins(struct device *dev, int mcast_bins)
{
	int x = mcast_bins;

	switch (x) {
	case HASH_TABLE_SIZE:
	case 128:
	case 256:
		break;
	default:
		x = 0;
		dev_info(dev, "Hash table entries set to unexpected value %d\n",
			 mcast_bins);
		break;
	}
	return x;
}

 
static int dwmac1000_validate_ucast_entries(struct device *dev,
					    int ucast_entries)
{
	int x = ucast_entries;

	switch (x) {
	case 1 ... 32:
	case 64:
	case 128:
		break;
	default:
		x = 1;
		dev_info(dev, "Unicast table entries set to unexpected value %d\n",
			 ucast_entries);
		break;
	}
	return x;
}

 
static struct stmmac_axi *stmmac_axi_setup(struct platform_device *pdev)
{
	struct device_node *np;
	struct stmmac_axi *axi;

	np = of_parse_phandle(pdev->dev.of_node, "snps,axi-config", 0);
	if (!np)
		return NULL;

	axi = devm_kzalloc(&pdev->dev, sizeof(*axi), GFP_KERNEL);
	if (!axi) {
		of_node_put(np);
		return ERR_PTR(-ENOMEM);
	}

	axi->axi_lpi_en = of_property_read_bool(np, "snps,lpi_en");
	axi->axi_xit_frm = of_property_read_bool(np, "snps,xit_frm");
	axi->axi_kbbe = of_property_read_bool(np, "snps,kbbe");
	axi->axi_fb = of_property_read_bool(np, "snps,fb");
	axi->axi_mb = of_property_read_bool(np, "snps,mb");
	axi->axi_rb =  of_property_read_bool(np, "snps,rb");

	if (of_property_read_u32(np, "snps,wr_osr_lmt", &axi->axi_wr_osr_lmt))
		axi->axi_wr_osr_lmt = 1;
	if (of_property_read_u32(np, "snps,rd_osr_lmt", &axi->axi_rd_osr_lmt))
		axi->axi_rd_osr_lmt = 1;
	of_property_read_u32_array(np, "snps,blen", axi->axi_blen, AXI_BLEN);
	of_node_put(np);

	return axi;
}

 
static int stmmac_mtl_setup(struct platform_device *pdev,
			    struct plat_stmmacenet_data *plat)
{
	struct device_node *q_node;
	struct device_node *rx_node;
	struct device_node *tx_node;
	u8 queue = 0;
	int ret = 0;

	 
	plat->rx_queues_to_use = 1;
	plat->tx_queues_to_use = 1;

	 
	plat->rx_queues_cfg[0].mode_to_use = MTL_QUEUE_DCB;
	plat->tx_queues_cfg[0].mode_to_use = MTL_QUEUE_DCB;

	rx_node = of_parse_phandle(pdev->dev.of_node, "snps,mtl-rx-config", 0);
	if (!rx_node)
		return ret;

	tx_node = of_parse_phandle(pdev->dev.of_node, "snps,mtl-tx-config", 0);
	if (!tx_node) {
		of_node_put(rx_node);
		return ret;
	}

	 
	if (of_property_read_u32(rx_node, "snps,rx-queues-to-use",
				 &plat->rx_queues_to_use))
		plat->rx_queues_to_use = 1;

	if (of_property_read_bool(rx_node, "snps,rx-sched-sp"))
		plat->rx_sched_algorithm = MTL_RX_ALGORITHM_SP;
	else if (of_property_read_bool(rx_node, "snps,rx-sched-wsp"))
		plat->rx_sched_algorithm = MTL_RX_ALGORITHM_WSP;
	else
		plat->rx_sched_algorithm = MTL_RX_ALGORITHM_SP;

	 
	for_each_child_of_node(rx_node, q_node) {
		if (queue >= plat->rx_queues_to_use)
			break;

		if (of_property_read_bool(q_node, "snps,dcb-algorithm"))
			plat->rx_queues_cfg[queue].mode_to_use = MTL_QUEUE_DCB;
		else if (of_property_read_bool(q_node, "snps,avb-algorithm"))
			plat->rx_queues_cfg[queue].mode_to_use = MTL_QUEUE_AVB;
		else
			plat->rx_queues_cfg[queue].mode_to_use = MTL_QUEUE_DCB;

		if (of_property_read_u32(q_node, "snps,map-to-dma-channel",
					 &plat->rx_queues_cfg[queue].chan))
			plat->rx_queues_cfg[queue].chan = queue;
		 

		if (of_property_read_u32(q_node, "snps,priority",
					&plat->rx_queues_cfg[queue].prio)) {
			plat->rx_queues_cfg[queue].prio = 0;
			plat->rx_queues_cfg[queue].use_prio = false;
		} else {
			plat->rx_queues_cfg[queue].use_prio = true;
		}

		 
		if (of_property_read_bool(q_node, "snps,route-avcp"))
			plat->rx_queues_cfg[queue].pkt_route = PACKET_AVCPQ;
		else if (of_property_read_bool(q_node, "snps,route-ptp"))
			plat->rx_queues_cfg[queue].pkt_route = PACKET_PTPQ;
		else if (of_property_read_bool(q_node, "snps,route-dcbcp"))
			plat->rx_queues_cfg[queue].pkt_route = PACKET_DCBCPQ;
		else if (of_property_read_bool(q_node, "snps,route-up"))
			plat->rx_queues_cfg[queue].pkt_route = PACKET_UPQ;
		else if (of_property_read_bool(q_node, "snps,route-multi-broad"))
			plat->rx_queues_cfg[queue].pkt_route = PACKET_MCBCQ;
		else
			plat->rx_queues_cfg[queue].pkt_route = 0x0;

		queue++;
	}
	if (queue != plat->rx_queues_to_use) {
		ret = -EINVAL;
		dev_err(&pdev->dev, "Not all RX queues were configured\n");
		goto out;
	}

	 
	if (of_property_read_u32(tx_node, "snps,tx-queues-to-use",
				 &plat->tx_queues_to_use))
		plat->tx_queues_to_use = 1;

	if (of_property_read_bool(tx_node, "snps,tx-sched-wrr"))
		plat->tx_sched_algorithm = MTL_TX_ALGORITHM_WRR;
	else if (of_property_read_bool(tx_node, "snps,tx-sched-wfq"))
		plat->tx_sched_algorithm = MTL_TX_ALGORITHM_WFQ;
	else if (of_property_read_bool(tx_node, "snps,tx-sched-dwrr"))
		plat->tx_sched_algorithm = MTL_TX_ALGORITHM_DWRR;
	else
		plat->tx_sched_algorithm = MTL_TX_ALGORITHM_SP;

	queue = 0;

	 
	for_each_child_of_node(tx_node, q_node) {
		if (queue >= plat->tx_queues_to_use)
			break;

		if (of_property_read_u32(q_node, "snps,weight",
					 &plat->tx_queues_cfg[queue].weight))
			plat->tx_queues_cfg[queue].weight = 0x10 + queue;

		if (of_property_read_bool(q_node, "snps,dcb-algorithm")) {
			plat->tx_queues_cfg[queue].mode_to_use = MTL_QUEUE_DCB;
		} else if (of_property_read_bool(q_node,
						 "snps,avb-algorithm")) {
			plat->tx_queues_cfg[queue].mode_to_use = MTL_QUEUE_AVB;

			 
			if (of_property_read_u32(q_node, "snps,send_slope",
				&plat->tx_queues_cfg[queue].send_slope))
				plat->tx_queues_cfg[queue].send_slope = 0x0;
			if (of_property_read_u32(q_node, "snps,idle_slope",
				&plat->tx_queues_cfg[queue].idle_slope))
				plat->tx_queues_cfg[queue].idle_slope = 0x0;
			if (of_property_read_u32(q_node, "snps,high_credit",
				&plat->tx_queues_cfg[queue].high_credit))
				plat->tx_queues_cfg[queue].high_credit = 0x0;
			if (of_property_read_u32(q_node, "snps,low_credit",
				&plat->tx_queues_cfg[queue].low_credit))
				plat->tx_queues_cfg[queue].low_credit = 0x0;
		} else {
			plat->tx_queues_cfg[queue].mode_to_use = MTL_QUEUE_DCB;
		}

		if (of_property_read_u32(q_node, "snps,priority",
					&plat->tx_queues_cfg[queue].prio)) {
			plat->tx_queues_cfg[queue].prio = 0;
			plat->tx_queues_cfg[queue].use_prio = false;
		} else {
			plat->tx_queues_cfg[queue].use_prio = true;
		}

		queue++;
	}
	if (queue != plat->tx_queues_to_use) {
		ret = -EINVAL;
		dev_err(&pdev->dev, "Not all TX queues were configured\n");
		goto out;
	}

out:
	of_node_put(rx_node);
	of_node_put(tx_node);
	of_node_put(q_node);

	return ret;
}

 
static int stmmac_dt_phy(struct plat_stmmacenet_data *plat,
			 struct device_node *np, struct device *dev)
{
	bool mdio = !of_phy_is_fixed_link(np);
	static const struct of_device_id need_mdio_ids[] = {
		{ .compatible = "snps,dwc-qos-ethernet-4.10" },
		{},
	};

	if (of_match_node(need_mdio_ids, np)) {
		plat->mdio_node = of_get_child_by_name(np, "mdio");
	} else {
		 
		for_each_child_of_node(np, plat->mdio_node) {
			if (of_device_is_compatible(plat->mdio_node,
						    "snps,dwmac-mdio"))
				break;
		}
	}

	if (plat->mdio_node) {
		dev_dbg(dev, "Found MDIO subnode\n");
		mdio = true;
	}

	if (mdio) {
		plat->mdio_bus_data =
			devm_kzalloc(dev, sizeof(struct stmmac_mdio_bus_data),
				     GFP_KERNEL);
		if (!plat->mdio_bus_data)
			return -ENOMEM;

		plat->mdio_bus_data->needs_reset = true;
	}

	return 0;
}

 
static int stmmac_of_get_mac_mode(struct device_node *np)
{
	const char *pm;
	int err, i;

	err = of_property_read_string(np, "mac-mode", &pm);
	if (err < 0)
		return err;

	for (i = 0; i < PHY_INTERFACE_MODE_MAX; i++) {
		if (!strcasecmp(pm, phy_modes(i)))
			return i;
	}

	return -ENODEV;
}

 
struct plat_stmmacenet_data *
stmmac_probe_config_dt(struct platform_device *pdev, u8 *mac)
{
	struct device_node *np = pdev->dev.of_node;
	struct plat_stmmacenet_data *plat;
	struct stmmac_dma_cfg *dma_cfg;
	int phy_mode;
	void *ret;
	int rc;

	plat = devm_kzalloc(&pdev->dev, sizeof(*plat), GFP_KERNEL);
	if (!plat)
		return ERR_PTR(-ENOMEM);

	rc = of_get_mac_address(np, mac);
	if (rc) {
		if (rc == -EPROBE_DEFER)
			return ERR_PTR(rc);

		eth_zero_addr(mac);
	}

	phy_mode = device_get_phy_mode(&pdev->dev);
	if (phy_mode < 0)
		return ERR_PTR(phy_mode);

	plat->phy_interface = phy_mode;
	rc = stmmac_of_get_mac_mode(np);
	plat->mac_interface = rc < 0 ? plat->phy_interface : rc;

	 
	plat->phy_node = of_parse_phandle(np, "phy-handle", 0);

	 
	plat->port_node = of_fwnode_handle(np);

	 
	of_property_read_u32(np, "max-speed", &plat->max_speed);

	plat->bus_id = of_alias_get_id(np, "ethernet");
	if (plat->bus_id < 0)
		plat->bus_id = 0;

	 
	plat->phy_addr = -1;

	 
	plat->clk_csr = -1;
	if (of_property_read_u32(np, "snps,clk-csr", &plat->clk_csr))
		of_property_read_u32(np, "clk_csr", &plat->clk_csr);

	 
	if (of_property_read_u32(np, "snps,phy-addr", &plat->phy_addr) == 0)
		dev_warn(&pdev->dev, "snps,phy-addr property is deprecated\n");

	 
	rc = stmmac_dt_phy(plat, np, &pdev->dev);
	if (rc)
		return ERR_PTR(rc);

	of_property_read_u32(np, "tx-fifo-depth", &plat->tx_fifo_size);

	of_property_read_u32(np, "rx-fifo-depth", &plat->rx_fifo_size);

	plat->force_sf_dma_mode =
		of_property_read_bool(np, "snps,force_sf_dma_mode");

	if (of_property_read_bool(np, "snps,en-tx-lpi-clockgating"))
		plat->flags |= STMMAC_FLAG_EN_TX_LPI_CLOCKGATING;

	 
	plat->maxmtu = JUMBO_LEN;

	 
	plat->multicast_filter_bins = HASH_TABLE_SIZE;

	 
	plat->unicast_filter_entries = 1;

	 
	if (of_device_is_compatible(np, "st,spear600-gmac") ||
		of_device_is_compatible(np, "snps,dwmac-3.50a") ||
		of_device_is_compatible(np, "snps,dwmac-3.70a") ||
		of_device_is_compatible(np, "snps,dwmac")) {
		 
		of_property_read_u32(np, "max-frame-size", &plat->maxmtu);
		of_property_read_u32(np, "snps,multicast-filter-bins",
				     &plat->multicast_filter_bins);
		of_property_read_u32(np, "snps,perfect-filter-entries",
				     &plat->unicast_filter_entries);
		plat->unicast_filter_entries = dwmac1000_validate_ucast_entries(
				&pdev->dev, plat->unicast_filter_entries);
		plat->multicast_filter_bins = dwmac1000_validate_mcast_bins(
				&pdev->dev, plat->multicast_filter_bins);
		plat->has_gmac = 1;
		plat->pmt = 1;
	}

	if (of_device_is_compatible(np, "snps,dwmac-3.40a")) {
		plat->has_gmac = 1;
		plat->enh_desc = 1;
		plat->tx_coe = 1;
		plat->bugged_jumbo = 1;
		plat->pmt = 1;
	}

	if (of_device_is_compatible(np, "snps,dwmac-4.00") ||
	    of_device_is_compatible(np, "snps,dwmac-4.10a") ||
	    of_device_is_compatible(np, "snps,dwmac-4.20a") ||
	    of_device_is_compatible(np, "snps,dwmac-5.10a") ||
	    of_device_is_compatible(np, "snps,dwmac-5.20")) {
		plat->has_gmac4 = 1;
		plat->has_gmac = 0;
		plat->pmt = 1;
		if (of_property_read_bool(np, "snps,tso"))
			plat->flags |= STMMAC_FLAG_TSO_EN;
	}

	if (of_device_is_compatible(np, "snps,dwmac-3.610") ||
		of_device_is_compatible(np, "snps,dwmac-3.710")) {
		plat->enh_desc = 1;
		plat->bugged_jumbo = 1;
		plat->force_sf_dma_mode = 1;
	}

	if (of_device_is_compatible(np, "snps,dwxgmac")) {
		plat->has_xgmac = 1;
		plat->pmt = 1;
		if (of_property_read_bool(np, "snps,tso"))
			plat->flags |= STMMAC_FLAG_TSO_EN;
	}

	dma_cfg = devm_kzalloc(&pdev->dev, sizeof(*dma_cfg),
			       GFP_KERNEL);
	if (!dma_cfg) {
		stmmac_remove_config_dt(pdev, plat);
		return ERR_PTR(-ENOMEM);
	}
	plat->dma_cfg = dma_cfg;

	of_property_read_u32(np, "snps,pbl", &dma_cfg->pbl);
	if (!dma_cfg->pbl)
		dma_cfg->pbl = DEFAULT_DMA_PBL;
	of_property_read_u32(np, "snps,txpbl", &dma_cfg->txpbl);
	of_property_read_u32(np, "snps,rxpbl", &dma_cfg->rxpbl);
	dma_cfg->pblx8 = !of_property_read_bool(np, "snps,no-pbl-x8");

	dma_cfg->aal = of_property_read_bool(np, "snps,aal");
	dma_cfg->fixed_burst = of_property_read_bool(np, "snps,fixed-burst");
	dma_cfg->mixed_burst = of_property_read_bool(np, "snps,mixed-burst");

	plat->force_thresh_dma_mode = of_property_read_bool(np, "snps,force_thresh_dma_mode");
	if (plat->force_thresh_dma_mode && plat->force_sf_dma_mode) {
		plat->force_sf_dma_mode = 0;
		dev_warn(&pdev->dev,
			 "force_sf_dma_mode is ignored if force_thresh_dma_mode is set.\n");
	}

	of_property_read_u32(np, "snps,ps-speed", &plat->mac_port_sel_speed);

	plat->axi = stmmac_axi_setup(pdev);

	rc = stmmac_mtl_setup(pdev, plat);
	if (rc) {
		stmmac_remove_config_dt(pdev, plat);
		return ERR_PTR(rc);
	}

	 
	if (!of_device_is_compatible(np, "snps,dwc-qos-ethernet-4.10")) {
		plat->stmmac_clk = devm_clk_get(&pdev->dev,
						STMMAC_RESOURCE_NAME);
		if (IS_ERR(plat->stmmac_clk)) {
			dev_warn(&pdev->dev, "Cannot get CSR clock\n");
			plat->stmmac_clk = NULL;
		}
		clk_prepare_enable(plat->stmmac_clk);
	}

	plat->pclk = devm_clk_get_optional(&pdev->dev, "pclk");
	if (IS_ERR(plat->pclk)) {
		ret = plat->pclk;
		goto error_pclk_get;
	}
	clk_prepare_enable(plat->pclk);

	 
	plat->clk_ptp_ref = devm_clk_get(&pdev->dev, "ptp_ref");
	if (IS_ERR(plat->clk_ptp_ref)) {
		plat->clk_ptp_rate = clk_get_rate(plat->stmmac_clk);
		plat->clk_ptp_ref = NULL;
		dev_info(&pdev->dev, "PTP uses main clock\n");
	} else {
		plat->clk_ptp_rate = clk_get_rate(plat->clk_ptp_ref);
		dev_dbg(&pdev->dev, "PTP rate %d\n", plat->clk_ptp_rate);
	}

	plat->stmmac_rst = devm_reset_control_get_optional(&pdev->dev,
							   STMMAC_RESOURCE_NAME);
	if (IS_ERR(plat->stmmac_rst)) {
		ret = plat->stmmac_rst;
		goto error_hw_init;
	}

	plat->stmmac_ahb_rst = devm_reset_control_get_optional_shared(
							&pdev->dev, "ahb");
	if (IS_ERR(plat->stmmac_ahb_rst)) {
		ret = plat->stmmac_ahb_rst;
		goto error_hw_init;
	}

	return plat;

error_hw_init:
	clk_disable_unprepare(plat->pclk);
error_pclk_get:
	clk_disable_unprepare(plat->stmmac_clk);

	return ret;
}

static void devm_stmmac_remove_config_dt(void *data)
{
	struct plat_stmmacenet_data *plat = data;

	 
	stmmac_remove_config_dt(NULL, plat);
}

 
struct plat_stmmacenet_data *
devm_stmmac_probe_config_dt(struct platform_device *pdev, u8 *mac)
{
	struct plat_stmmacenet_data *plat;
	int ret;

	plat = stmmac_probe_config_dt(pdev, mac);
	if (IS_ERR(plat))
		return plat;

	ret = devm_add_action_or_reset(&pdev->dev,
				       devm_stmmac_remove_config_dt, plat);
	if (ret)
		return ERR_PTR(ret);

	return plat;
}

 
void stmmac_remove_config_dt(struct platform_device *pdev,
			     struct plat_stmmacenet_data *plat)
{
	clk_disable_unprepare(plat->stmmac_clk);
	clk_disable_unprepare(plat->pclk);
	of_node_put(plat->phy_node);
	of_node_put(plat->mdio_node);
}
#else
struct plat_stmmacenet_data *
stmmac_probe_config_dt(struct platform_device *pdev, u8 *mac)
{
	return ERR_PTR(-EINVAL);
}

struct plat_stmmacenet_data *
devm_stmmac_probe_config_dt(struct platform_device *pdev, u8 *mac)
{
	return ERR_PTR(-EINVAL);
}

void stmmac_remove_config_dt(struct platform_device *pdev,
			     struct plat_stmmacenet_data *plat)
{
}
#endif  
EXPORT_SYMBOL_GPL(stmmac_probe_config_dt);
EXPORT_SYMBOL_GPL(devm_stmmac_probe_config_dt);
EXPORT_SYMBOL_GPL(stmmac_remove_config_dt);

int stmmac_get_platform_resources(struct platform_device *pdev,
				  struct stmmac_resources *stmmac_res)
{
	memset(stmmac_res, 0, sizeof(*stmmac_res));

	 
	stmmac_res->irq = platform_get_irq_byname(pdev, "macirq");
	if (stmmac_res->irq < 0)
		return stmmac_res->irq;

	 
	stmmac_res->wol_irq =
		platform_get_irq_byname_optional(pdev, "eth_wake_irq");
	if (stmmac_res->wol_irq < 0) {
		if (stmmac_res->wol_irq == -EPROBE_DEFER)
			return -EPROBE_DEFER;
		dev_info(&pdev->dev, "IRQ eth_wake_irq not found\n");
		stmmac_res->wol_irq = stmmac_res->irq;
	}

	stmmac_res->lpi_irq =
		platform_get_irq_byname_optional(pdev, "eth_lpi");
	if (stmmac_res->lpi_irq < 0) {
		if (stmmac_res->lpi_irq == -EPROBE_DEFER)
			return -EPROBE_DEFER;
		dev_info(&pdev->dev, "IRQ eth_lpi not found\n");
	}

	stmmac_res->addr = devm_platform_ioremap_resource(pdev, 0);

	return PTR_ERR_OR_ZERO(stmmac_res->addr);
}
EXPORT_SYMBOL_GPL(stmmac_get_platform_resources);

 
int stmmac_pltfr_init(struct platform_device *pdev,
		      struct plat_stmmacenet_data *plat)
{
	int ret = 0;

	if (plat->init)
		ret = plat->init(pdev, plat->bsp_priv);

	return ret;
}
EXPORT_SYMBOL_GPL(stmmac_pltfr_init);

 
void stmmac_pltfr_exit(struct platform_device *pdev,
		       struct plat_stmmacenet_data *plat)
{
	if (plat->exit)
		plat->exit(pdev, plat->bsp_priv);
}
EXPORT_SYMBOL_GPL(stmmac_pltfr_exit);

 
int stmmac_pltfr_probe(struct platform_device *pdev,
		       struct plat_stmmacenet_data *plat,
		       struct stmmac_resources *res)
{
	int ret;

	ret = stmmac_pltfr_init(pdev, plat);
	if (ret)
		return ret;

	ret = stmmac_dvr_probe(&pdev->dev, plat, res);
	if (ret) {
		stmmac_pltfr_exit(pdev, plat);
		return ret;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(stmmac_pltfr_probe);

static void devm_stmmac_pltfr_remove(void *data)
{
	struct platform_device *pdev = data;

	stmmac_pltfr_remove_no_dt(pdev);
}

 
int devm_stmmac_pltfr_probe(struct platform_device *pdev,
			    struct plat_stmmacenet_data *plat,
			    struct stmmac_resources *res)
{
	int ret;

	ret = stmmac_pltfr_probe(pdev, plat, res);
	if (ret)
		return ret;

	return devm_add_action_or_reset(&pdev->dev, devm_stmmac_pltfr_remove,
					pdev);
}
EXPORT_SYMBOL_GPL(devm_stmmac_pltfr_probe);

 
void stmmac_pltfr_remove_no_dt(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(ndev);
	struct plat_stmmacenet_data *plat = priv->plat;

	stmmac_dvr_remove(&pdev->dev);
	stmmac_pltfr_exit(pdev, plat);
}
EXPORT_SYMBOL_GPL(stmmac_pltfr_remove_no_dt);

 
void stmmac_pltfr_remove(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(ndev);
	struct plat_stmmacenet_data *plat = priv->plat;

	stmmac_pltfr_remove_no_dt(pdev);
	stmmac_remove_config_dt(pdev, plat);
}
EXPORT_SYMBOL_GPL(stmmac_pltfr_remove);

 
static int __maybe_unused stmmac_pltfr_suspend(struct device *dev)
{
	int ret;
	struct net_device *ndev = dev_get_drvdata(dev);
	struct stmmac_priv *priv = netdev_priv(ndev);
	struct platform_device *pdev = to_platform_device(dev);

	ret = stmmac_suspend(dev);
	stmmac_pltfr_exit(pdev, priv->plat);

	return ret;
}

 
static int __maybe_unused stmmac_pltfr_resume(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct stmmac_priv *priv = netdev_priv(ndev);
	struct platform_device *pdev = to_platform_device(dev);
	int ret;

	ret = stmmac_pltfr_init(pdev, priv->plat);
	if (ret)
		return ret;

	return stmmac_resume(dev);
}

static int __maybe_unused stmmac_runtime_suspend(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct stmmac_priv *priv = netdev_priv(ndev);

	stmmac_bus_clks_config(priv, false);

	return 0;
}

static int __maybe_unused stmmac_runtime_resume(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct stmmac_priv *priv = netdev_priv(ndev);

	return stmmac_bus_clks_config(priv, true);
}

static int __maybe_unused stmmac_pltfr_noirq_suspend(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct stmmac_priv *priv = netdev_priv(ndev);
	int ret;

	if (!netif_running(ndev))
		return 0;

	if (!device_may_wakeup(priv->device) || !priv->plat->pmt) {
		 
		clk_disable_unprepare(priv->plat->clk_ptp_ref);

		ret = pm_runtime_force_suspend(dev);
		if (ret)
			return ret;
	}

	return 0;
}

static int __maybe_unused stmmac_pltfr_noirq_resume(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct stmmac_priv *priv = netdev_priv(ndev);
	int ret;

	if (!netif_running(ndev))
		return 0;

	if (!device_may_wakeup(priv->device) || !priv->plat->pmt) {
		 
		ret = pm_runtime_force_resume(dev);
		if (ret)
			return ret;

		ret = clk_prepare_enable(priv->plat->clk_ptp_ref);
		if (ret < 0) {
			netdev_warn(priv->dev,
				    "failed to enable PTP reference clock: %pe\n",
				    ERR_PTR(ret));
			return ret;
		}
	}

	return 0;
}

const struct dev_pm_ops stmmac_pltfr_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(stmmac_pltfr_suspend, stmmac_pltfr_resume)
	SET_RUNTIME_PM_OPS(stmmac_runtime_suspend, stmmac_runtime_resume, NULL)
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(stmmac_pltfr_noirq_suspend, stmmac_pltfr_noirq_resume)
};
EXPORT_SYMBOL_GPL(stmmac_pltfr_pm_ops);

MODULE_DESCRIPTION("STMMAC 10/100/1000 Ethernet platform support");
MODULE_AUTHOR("Giuseppe Cavallaro <peppe.cavallaro@st.com>");
MODULE_LICENSE("GPL");
