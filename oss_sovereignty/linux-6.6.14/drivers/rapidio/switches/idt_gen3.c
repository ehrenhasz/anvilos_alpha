
 

#include <linux/stat.h>
#include <linux/module.h>
#include <linux/rio.h>
#include <linux/rio_drv.h>
#include <linux/rio_ids.h>
#include <linux/delay.h>

#include <asm/page.h>
#include "../rio.h"

#define RIO_EM_PW_STAT		0x40020
#define RIO_PW_CTL		0x40204
#define RIO_PW_CTL_PW_TMR		0xffffff00
#define RIO_PW_ROUTE		0x40208

#define RIO_EM_DEV_INT_EN	0x40030

#define RIO_PLM_SPx_IMP_SPEC_CTL(x)	(0x10100 + (x)*0x100)
#define RIO_PLM_SPx_IMP_SPEC_CTL_SOFT_RST	0x02000000

#define RIO_PLM_SPx_PW_EN(x)	(0x10118 + (x)*0x100)
#define RIO_PLM_SPx_PW_EN_OK2U	0x40000000
#define RIO_PLM_SPx_PW_EN_LINIT 0x10000000

#define RIO_BC_L2_Gn_ENTRYx_CSR(n, x)	(0x31000 + (n)*0x400 + (x)*0x4)
#define RIO_SPx_L2_Gn_ENTRYy_CSR(x, n, y) \
				(0x51000 + (x)*0x2000 + (n)*0x400 + (y)*0x4)

static int
idtg3_route_add_entry(struct rio_mport *mport, u16 destid, u8 hopcount,
		       u16 table, u16 route_destid, u8 route_port)
{
	u32 rval;
	u32 entry = route_port;
	int err = 0;

	pr_debug("RIO: %s t=0x%x did_%x to p_%x\n",
		 __func__, table, route_destid, entry);

	if (route_destid > 0xFF)
		return -EINVAL;

	if (route_port == RIO_INVALID_ROUTE)
		entry = RIO_RT_ENTRY_DROP_PKT;

	if (table == RIO_GLOBAL_TABLE) {
		 
		err = rio_mport_write_config_32(mport, destid, hopcount,
				RIO_BC_L2_Gn_ENTRYx_CSR(0, route_destid),
				entry);
		return err;
	}

	 
	err = rio_mport_read_config_32(mport, destid, hopcount,
				       RIO_SWP_INFO_CAR, &rval);
	if (err)
		return err;

	if (table >= RIO_GET_TOTAL_PORTS(rval))
		return -EINVAL;

	err = rio_mport_write_config_32(mport, destid, hopcount,
			RIO_SPx_L2_Gn_ENTRYy_CSR(table, 0, route_destid),
			entry);
	return err;
}

static int
idtg3_route_get_entry(struct rio_mport *mport, u16 destid, u8 hopcount,
		       u16 table, u16 route_destid, u8 *route_port)
{
	u32 rval;
	int err;

	if (route_destid > 0xFF)
		return -EINVAL;

	err = rio_mport_read_config_32(mport, destid, hopcount,
				       RIO_SWP_INFO_CAR, &rval);
	if (err)
		return err;

	 
	if (table == RIO_GLOBAL_TABLE)
		table = RIO_GET_PORT_NUM(rval);
	else if (table >= RIO_GET_TOTAL_PORTS(rval))
		return -EINVAL;

	err = rio_mport_read_config_32(mport, destid, hopcount,
			RIO_SPx_L2_Gn_ENTRYy_CSR(table, 0, route_destid),
			&rval);
	if (err)
		return err;

	if (rval == RIO_RT_ENTRY_DROP_PKT)
		*route_port = RIO_INVALID_ROUTE;
	else
		*route_port = (u8)rval;

	return 0;
}

static int
idtg3_route_clr_table(struct rio_mport *mport, u16 destid, u8 hopcount,
		       u16 table)
{
	u32 i;
	u32 rval;
	int err;

	if (table == RIO_GLOBAL_TABLE) {
		for (i = 0; i <= 0xff; i++) {
			err = rio_mport_write_config_32(mport, destid, hopcount,
						RIO_BC_L2_Gn_ENTRYx_CSR(0, i),
						RIO_RT_ENTRY_DROP_PKT);
			if (err)
				break;
		}

		return err;
	}

	err = rio_mport_read_config_32(mport, destid, hopcount,
				       RIO_SWP_INFO_CAR, &rval);
	if (err)
		return err;

	if (table >= RIO_GET_TOTAL_PORTS(rval))
		return -EINVAL;

	for (i = 0; i <= 0xff; i++) {
		err = rio_mport_write_config_32(mport, destid, hopcount,
					RIO_SPx_L2_Gn_ENTRYy_CSR(table, 0, i),
					RIO_RT_ENTRY_DROP_PKT);
		if (err)
			break;
	}

	return err;
}

 
static int
idtg3_em_init(struct rio_dev *rdev)
{
	int i, tmp;
	u32 rval;

	pr_debug("RIO: %s [%d:%d]\n", __func__, rdev->destid, rdev->hopcount);

	 
	rio_write_config_32(rdev, RIO_EM_DEV_INT_EN, 0);

	 
	rio_write_config_32(rdev, rdev->em_efptr + RIO_EM_PW_TX_CTRL,
			    RIO_EM_PW_TX_CTRL_PW_DIS);

	 
	tmp = RIO_GET_TOTAL_PORTS(rdev->swpinfo);
	for (i = 0; i < tmp; i++) {

		rio_read_config_32(rdev,
			RIO_DEV_PORT_N_ERR_STS_CSR(rdev, i),
			&rval);
		if (rval & RIO_PORT_N_ERR_STS_PORT_UA)
			continue;

		 
		rio_write_config_32(rdev,
			rdev->em_efptr + RIO_EM_PN_ERR_DETECT(i), 0);

		 
		rio_write_config_32(rdev,
			rdev->em_efptr + RIO_EM_PN_ERRRATE_EN(i),
			RIO_EM_PN_ERRRATE_EN_OK2U | RIO_EM_PN_ERRRATE_EN_U2OK);
		 
		rio_write_config_32(rdev, RIO_PLM_SPx_PW_EN(i),
			RIO_PLM_SPx_PW_EN_OK2U | RIO_PLM_SPx_PW_EN_LINIT);

	}

	 
	tmp = RIO_GET_PORT_NUM(rdev->swpinfo);
	rio_write_config_32(rdev, RIO_PW_ROUTE, 1 << tmp);


	 
	rio_write_config_32(rdev, rdev->em_efptr + RIO_EM_PW_TX_CTRL, 0);

	 
	rio_write_config_32(rdev,
		rdev->phys_efptr + RIO_PORT_LINKTO_CTL_CSR, 0x8e << 8);
	return 0;
}


 
static int
idtg3_em_handler(struct rio_dev *rdev, u8 pnum)
{
	u32 err_status;
	u32 rval;

	rio_read_config_32(rdev,
			RIO_DEV_PORT_N_ERR_STS_CSR(rdev, pnum),
			&err_status);

	 
	if (err_status & RIO_PORT_N_ERR_STS_PORT_UNINIT)
		return 0;

	 
	if (err_status & (RIO_PORT_N_ERR_STS_OUT_ES |
				RIO_PORT_N_ERR_STS_INP_ES)) {
		rio_read_config_32(rdev, RIO_PLM_SPx_IMP_SPEC_CTL(pnum), &rval);
		rio_write_config_32(rdev, RIO_PLM_SPx_IMP_SPEC_CTL(pnum),
				    rval | RIO_PLM_SPx_IMP_SPEC_CTL_SOFT_RST);
		udelay(10);
		rio_write_config_32(rdev, RIO_PLM_SPx_IMP_SPEC_CTL(pnum), rval);
		msleep(500);
	}

	return 0;
}

static struct rio_switch_ops idtg3_switch_ops = {
	.owner = THIS_MODULE,
	.add_entry = idtg3_route_add_entry,
	.get_entry = idtg3_route_get_entry,
	.clr_table = idtg3_route_clr_table,
	.em_init   = idtg3_em_init,
	.em_handle = idtg3_em_handler,
};

static int idtg3_probe(struct rio_dev *rdev, const struct rio_device_id *id)
{
	pr_debug("RIO: %s for %s\n", __func__, rio_name(rdev));

	spin_lock(&rdev->rswitch->lock);

	if (rdev->rswitch->ops) {
		spin_unlock(&rdev->rswitch->lock);
		return -EINVAL;
	}

	rdev->rswitch->ops = &idtg3_switch_ops;

	if (rdev->do_enum) {
		 
		rio_write_config_32(rdev, 0x5000 + RIO_BC_RT_CTL_CSR, 0);
	}

	spin_unlock(&rdev->rswitch->lock);

	return 0;
}

static void idtg3_remove(struct rio_dev *rdev)
{
	pr_debug("RIO: %s for %s\n", __func__, rio_name(rdev));
	spin_lock(&rdev->rswitch->lock);
	if (rdev->rswitch->ops == &idtg3_switch_ops)
		rdev->rswitch->ops = NULL;
	spin_unlock(&rdev->rswitch->lock);
}

 
static void idtg3_shutdown(struct rio_dev *rdev)
{
	int i;
	u32 rval;
	u16 destid;

	 
	if (!rdev->do_enum)
		return;

	pr_debug("RIO: %s(%s)\n", __func__, rio_name(rdev));

	rio_read_config_32(rdev, RIO_PW_ROUTE, &rval);
	i = RIO_GET_PORT_NUM(rdev->swpinfo);

	 
	if (!((1 << i) & rval))
		return;

	 
	rio_read_config_32(rdev, rdev->em_efptr + RIO_EM_PW_TGT_DEVID, &rval);

	if (rval & RIO_EM_PW_TGT_DEVID_DEV16)
		destid = rval >> 16;
	else
		destid = ((rval & RIO_EM_PW_TGT_DEVID_D8) >> 16);

	if (rdev->net->hport->host_deviceid == destid) {
		rio_write_config_32(rdev,
				    rdev->em_efptr + RIO_EM_PW_TX_CTRL, 0);
		pr_debug("RIO: %s(%s) PW transmission disabled\n",
			 __func__, rio_name(rdev));
	}
}

static const struct rio_device_id idtg3_id_table[] = {
	{RIO_DEVICE(RIO_DID_IDTRXS1632, RIO_VID_IDT)},
	{RIO_DEVICE(RIO_DID_IDTRXS2448, RIO_VID_IDT)},
	{ 0, }	 
};

static struct rio_driver idtg3_driver = {
	.name = "idt_gen3",
	.id_table = idtg3_id_table,
	.probe = idtg3_probe,
	.remove = idtg3_remove,
	.shutdown = idtg3_shutdown,
};

static int __init idtg3_init(void)
{
	return rio_register_driver(&idtg3_driver);
}

static void __exit idtg3_exit(void)
{
	pr_debug("RIO: %s\n", __func__);
	rio_unregister_driver(&idtg3_driver);
	pr_debug("RIO: %s done\n", __func__);
}

device_initcall(idtg3_init);
module_exit(idtg3_exit);

MODULE_DESCRIPTION("IDT RXS Gen.3 Serial RapidIO switch family driver");
MODULE_AUTHOR("Integrated Device Technology, Inc.");
MODULE_LICENSE("GPL");
