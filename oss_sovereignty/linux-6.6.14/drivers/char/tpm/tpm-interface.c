
 

#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/suspend.h>
#include <linux/freezer.h>
#include <linux/tpm_eventlog.h>

#include "tpm.h"

 
static u32 tpm_suspend_pcr;
module_param_named(suspend_pcr, tpm_suspend_pcr, uint, 0644);
MODULE_PARM_DESC(suspend_pcr,
		 "PCR to use for dummy writes to facilitate flush on suspend.");

 
unsigned long tpm_calc_ordinal_duration(struct tpm_chip *chip, u32 ordinal)
{
	if (chip->flags & TPM_CHIP_FLAG_TPM2)
		return tpm2_calc_ordinal_duration(chip, ordinal);
	else
		return tpm1_calc_ordinal_duration(chip, ordinal);
}
EXPORT_SYMBOL_GPL(tpm_calc_ordinal_duration);

static ssize_t tpm_try_transmit(struct tpm_chip *chip, void *buf, size_t bufsiz)
{
	struct tpm_header *header = buf;
	int rc;
	ssize_t len = 0;
	u32 count, ordinal;
	unsigned long stop;

	if (bufsiz < TPM_HEADER_SIZE)
		return -EINVAL;

	if (bufsiz > TPM_BUFSIZE)
		bufsiz = TPM_BUFSIZE;

	count = be32_to_cpu(header->length);
	ordinal = be32_to_cpu(header->ordinal);
	if (count == 0)
		return -ENODATA;
	if (count > bufsiz) {
		dev_err(&chip->dev,
			"invalid count value %x %zx\n", count, bufsiz);
		return -E2BIG;
	}

	rc = chip->ops->send(chip, buf, count);
	if (rc < 0) {
		if (rc != -EPIPE)
			dev_err(&chip->dev,
				"%s: send(): error %d\n", __func__, rc);
		return rc;
	}

	 
	if (rc > 0) {
		dev_warn(&chip->dev,
			 "%s: send(): invalid value %d\n", __func__, rc);
		rc = 0;
	}

	if (chip->flags & TPM_CHIP_FLAG_IRQ)
		goto out_recv;

	stop = jiffies + tpm_calc_ordinal_duration(chip, ordinal);
	do {
		u8 status = chip->ops->status(chip);
		if ((status & chip->ops->req_complete_mask) ==
		    chip->ops->req_complete_val)
			goto out_recv;

		if (chip->ops->req_canceled(chip, status)) {
			dev_err(&chip->dev, "Operation Canceled\n");
			return -ECANCELED;
		}

		tpm_msleep(TPM_TIMEOUT_POLL);
		rmb();
	} while (time_before(jiffies, stop));

	chip->ops->cancel(chip);
	dev_err(&chip->dev, "Operation Timed out\n");
	return -ETIME;

out_recv:
	len = chip->ops->recv(chip, buf, bufsiz);
	if (len < 0) {
		rc = len;
		dev_err(&chip->dev, "tpm_transmit: tpm_recv: error %d\n", rc);
	} else if (len < TPM_HEADER_SIZE || len != be32_to_cpu(header->length))
		rc = -EFAULT;

	return rc ? rc : len;
}

 
ssize_t tpm_transmit(struct tpm_chip *chip, u8 *buf, size_t bufsiz)
{
	struct tpm_header *header = (struct tpm_header *)buf;
	 
	u8 save[TPM_HEADER_SIZE + 3*sizeof(u32)];
	unsigned int delay_msec = TPM2_DURATION_SHORT;
	u32 rc = 0;
	ssize_t ret;
	const size_t save_size = min(sizeof(save), bufsiz);
	 
	u32 cc = be32_to_cpu(header->return_code);

	 
	memcpy(save, buf, save_size);

	for (;;) {
		ret = tpm_try_transmit(chip, buf, bufsiz);
		if (ret < 0)
			break;
		rc = be32_to_cpu(header->return_code);
		if (rc != TPM2_RC_RETRY && rc != TPM2_RC_TESTING)
			break;
		 
		if (rc == TPM2_RC_TESTING && cc == TPM2_CC_SELF_TEST)
			break;

		if (delay_msec > TPM2_DURATION_LONG) {
			if (rc == TPM2_RC_RETRY)
				dev_err(&chip->dev, "in retry loop\n");
			else
				dev_err(&chip->dev,
					"self test is still running\n");
			break;
		}
		tpm_msleep(delay_msec);
		delay_msec *= 2;
		memcpy(buf, save, save_size);
	}
	return ret;
}

 
ssize_t tpm_transmit_cmd(struct tpm_chip *chip, struct tpm_buf *buf,
			 size_t min_rsp_body_length, const char *desc)
{
	const struct tpm_header *header = (struct tpm_header *)buf->data;
	int err;
	ssize_t len;

	len = tpm_transmit(chip, buf->data, PAGE_SIZE);
	if (len <  0)
		return len;

	err = be32_to_cpu(header->return_code);
	if (err != 0 && err != TPM_ERR_DISABLED && err != TPM_ERR_DEACTIVATED
	    && err != TPM2_RC_TESTING && desc)
		dev_err(&chip->dev, "A TPM error (%d) occurred %s\n", err,
			desc);
	if (err)
		return err;

	if (len < min_rsp_body_length + TPM_HEADER_SIZE)
		return -EFAULT;

	return 0;
}
EXPORT_SYMBOL_GPL(tpm_transmit_cmd);

int tpm_get_timeouts(struct tpm_chip *chip)
{
	if (chip->flags & TPM_CHIP_FLAG_HAVE_TIMEOUTS)
		return 0;

	if (chip->flags & TPM_CHIP_FLAG_TPM2)
		return tpm2_get_timeouts(chip);
	else
		return tpm1_get_timeouts(chip);
}
EXPORT_SYMBOL_GPL(tpm_get_timeouts);

 
int tpm_is_tpm2(struct tpm_chip *chip)
{
	int rc;

	chip = tpm_find_get_ops(chip);
	if (!chip)
		return -ENODEV;

	rc = (chip->flags & TPM_CHIP_FLAG_TPM2) != 0;

	tpm_put_ops(chip);

	return rc;
}
EXPORT_SYMBOL_GPL(tpm_is_tpm2);

 
int tpm_pcr_read(struct tpm_chip *chip, u32 pcr_idx,
		 struct tpm_digest *digest)
{
	int rc;

	chip = tpm_find_get_ops(chip);
	if (!chip)
		return -ENODEV;

	if (chip->flags & TPM_CHIP_FLAG_TPM2)
		rc = tpm2_pcr_read(chip, pcr_idx, digest, NULL);
	else
		rc = tpm1_pcr_read(chip, pcr_idx, digest->digest);

	tpm_put_ops(chip);
	return rc;
}
EXPORT_SYMBOL_GPL(tpm_pcr_read);

 
int tpm_pcr_extend(struct tpm_chip *chip, u32 pcr_idx,
		   struct tpm_digest *digests)
{
	int rc;
	int i;

	chip = tpm_find_get_ops(chip);
	if (!chip)
		return -ENODEV;

	for (i = 0; i < chip->nr_allocated_banks; i++) {
		if (digests[i].alg_id != chip->allocated_banks[i].alg_id) {
			rc = -EINVAL;
			goto out;
		}
	}

	if (chip->flags & TPM_CHIP_FLAG_TPM2) {
		rc = tpm2_pcr_extend(chip, pcr_idx, digests);
		goto out;
	}

	rc = tpm1_pcr_extend(chip, pcr_idx, digests[0].digest,
			     "attempting extend a PCR value");

out:
	tpm_put_ops(chip);
	return rc;
}
EXPORT_SYMBOL_GPL(tpm_pcr_extend);

 
int tpm_send(struct tpm_chip *chip, void *cmd, size_t buflen)
{
	struct tpm_buf buf;
	int rc;

	chip = tpm_find_get_ops(chip);
	if (!chip)
		return -ENODEV;

	buf.data = cmd;
	rc = tpm_transmit_cmd(chip, &buf, 0, "attempting to a send a command");

	tpm_put_ops(chip);
	return rc;
}
EXPORT_SYMBOL_GPL(tpm_send);

int tpm_auto_startup(struct tpm_chip *chip)
{
	int rc;

	if (!(chip->ops->flags & TPM_OPS_AUTO_STARTUP))
		return 0;

	if (chip->flags & TPM_CHIP_FLAG_TPM2)
		rc = tpm2_auto_startup(chip);
	else
		rc = tpm1_auto_startup(chip);

	return rc;
}

 
int tpm_pm_suspend(struct device *dev)
{
	struct tpm_chip *chip = dev_get_drvdata(dev);
	int rc = 0;

	if (!chip)
		return -ENODEV;

	if (chip->flags & TPM_CHIP_FLAG_ALWAYS_POWERED)
		goto suspended;

	if ((chip->flags & TPM_CHIP_FLAG_FIRMWARE_POWER_MANAGED) &&
	    !pm_suspend_via_firmware())
		goto suspended;

	rc = tpm_try_get_ops(chip);
	if (!rc) {
		if (chip->flags & TPM_CHIP_FLAG_TPM2)
			tpm2_shutdown(chip, TPM2_SU_STATE);
		else
			rc = tpm1_pm_suspend(chip, tpm_suspend_pcr);

		tpm_put_ops(chip);
	}

suspended:
	chip->flags |= TPM_CHIP_FLAG_SUSPENDED;

	if (rc)
		dev_err(dev, "Ignoring error %d while suspending\n", rc);
	return 0;
}
EXPORT_SYMBOL_GPL(tpm_pm_suspend);

 
int tpm_pm_resume(struct device *dev)
{
	struct tpm_chip *chip = dev_get_drvdata(dev);

	if (chip == NULL)
		return -ENODEV;

	chip->flags &= ~TPM_CHIP_FLAG_SUSPENDED;

	 
	wmb();

	return 0;
}
EXPORT_SYMBOL_GPL(tpm_pm_resume);

 
int tpm_get_random(struct tpm_chip *chip, u8 *out, size_t max)
{
	int rc;

	if (!out || max > TPM_MAX_RNG_DATA)
		return -EINVAL;

	chip = tpm_find_get_ops(chip);
	if (!chip)
		return -ENODEV;

	if (chip->flags & TPM_CHIP_FLAG_TPM2)
		rc = tpm2_get_random(chip, out, max);
	else
		rc = tpm1_get_random(chip, out, max);

	tpm_put_ops(chip);
	return rc;
}
EXPORT_SYMBOL_GPL(tpm_get_random);

static int __init tpm_init(void)
{
	int rc;

	rc = class_register(&tpm_class);
	if (rc) {
		pr_err("couldn't create tpm class\n");
		return rc;
	}

	rc = class_register(&tpmrm_class);
	if (rc) {
		pr_err("couldn't create tpmrm class\n");
		goto out_destroy_tpm_class;
	}

	rc = alloc_chrdev_region(&tpm_devt, 0, 2*TPM_NUM_DEVICES, "tpm");
	if (rc < 0) {
		pr_err("tpm: failed to allocate char dev region\n");
		goto out_destroy_tpmrm_class;
	}

	rc = tpm_dev_common_init();
	if (rc) {
		pr_err("tpm: failed to allocate char dev region\n");
		goto out_unreg_chrdev;
	}

	return 0;

out_unreg_chrdev:
	unregister_chrdev_region(tpm_devt, 2 * TPM_NUM_DEVICES);
out_destroy_tpmrm_class:
	class_unregister(&tpmrm_class);
out_destroy_tpm_class:
	class_unregister(&tpm_class);

	return rc;
}

static void __exit tpm_exit(void)
{
	idr_destroy(&dev_nums_idr);
	class_unregister(&tpm_class);
	class_unregister(&tpmrm_class);
	unregister_chrdev_region(tpm_devt, 2*TPM_NUM_DEVICES);
	tpm_dev_common_exit();
}

subsys_initcall(tpm_init);
module_exit(tpm_exit);

MODULE_AUTHOR("Leendert van Doorn (leendert@watson.ibm.com)");
MODULE_DESCRIPTION("TPM Driver");
MODULE_VERSION("2.0");
MODULE_LICENSE("GPL");
