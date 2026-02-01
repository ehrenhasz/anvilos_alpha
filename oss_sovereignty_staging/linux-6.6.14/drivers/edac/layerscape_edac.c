 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "edac_module.h"
#include "fsl_ddr_edac.h"

static const struct of_device_id fsl_ddr_mc_err_of_match[] = {
	{ .compatible = "fsl,qoriq-memory-controller", },
	{},
};
MODULE_DEVICE_TABLE(of, fsl_ddr_mc_err_of_match);

static struct platform_driver fsl_ddr_mc_err_driver = {
	.probe = fsl_mc_err_probe,
	.remove = fsl_mc_err_remove,
	.driver = {
		.name = "fsl_ddr_mc_err",
		.of_match_table = fsl_ddr_mc_err_of_match,
	},
};

static int __init fsl_ddr_mc_init(void)
{
	int res;

	if (ghes_get_devices())
		return -EBUSY;

	 
	switch (edac_op_state) {
	case EDAC_OPSTATE_POLL:
	case EDAC_OPSTATE_INT:
		break;
	default:
		edac_op_state = EDAC_OPSTATE_INT;
		break;
	}

	res = platform_driver_register(&fsl_ddr_mc_err_driver);
	if (res) {
		pr_err("MC fails to register\n");
		return res;
	}

	return 0;
}

module_init(fsl_ddr_mc_init);

static void __exit fsl_ddr_mc_exit(void)
{
	platform_driver_unregister(&fsl_ddr_mc_err_driver);
}

module_exit(fsl_ddr_mc_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("NXP Semiconductor");
module_param(edac_op_state, int, 0444);
MODULE_PARM_DESC(edac_op_state, "EDAC Error Reporting state: 0=Poll, 2=Interrupt");
