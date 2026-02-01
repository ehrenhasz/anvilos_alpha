
 

#include <linux/sched.h>
#include <linux/device.h>
#include "kfd_priv.h"
#include "amdgpu_amdkfd.h"

static int kfd_init(void)
{
	int err;

	 
	if ((sched_policy < KFD_SCHED_POLICY_HWS) ||
		(sched_policy > KFD_SCHED_POLICY_NO_HWS)) {
		pr_err("sched_policy has invalid value\n");
		return -EINVAL;
	}

	 
	if ((max_num_of_queues_per_device < 1) ||
		(max_num_of_queues_per_device >
			KFD_MAX_NUM_OF_QUEUES_PER_DEVICE)) {
		pr_err("max_num_of_queues_per_device must be between 1 to KFD_MAX_NUM_OF_QUEUES_PER_DEVICE\n");
		return -EINVAL;
	}

	err = kfd_chardev_init();
	if (err < 0)
		goto err_ioctl;

	err = kfd_topology_init();
	if (err < 0)
		goto err_topology;

	err = kfd_process_create_wq();
	if (err < 0)
		goto err_create_wq;

	 
	kfd_procfs_init();

	kfd_debugfs_init();

	return 0;

err_create_wq:
	kfd_topology_shutdown();
err_topology:
	kfd_chardev_exit();
err_ioctl:
	pr_err("KFD is disabled due to module initialization failure\n");
	return err;
}

static void kfd_exit(void)
{
	kfd_cleanup_processes();
	kfd_debugfs_fini();
	kfd_process_destroy_wq();
	kfd_procfs_shutdown();
	kfd_topology_shutdown();
	kfd_chardev_exit();
}

int kgd2kfd_init(void)
{
	return kfd_init();
}

void kgd2kfd_exit(void)
{
	kfd_exit();
}
