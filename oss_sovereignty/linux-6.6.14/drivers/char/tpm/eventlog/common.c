
 

#include <linux/seq_file.h>
#include <linux/fs.h>
#include <linux/security.h>
#include <linux/module.h>
#include <linux/tpm_eventlog.h>

#include "../tpm.h"
#include "common.h"

static int tpm_bios_measurements_open(struct inode *inode,
					    struct file *file)
{
	int err;
	struct seq_file *seq;
	struct tpm_chip_seqops *chip_seqops;
	const struct seq_operations *seqops;
	struct tpm_chip *chip;

	inode_lock(inode);
	if (!inode->i_private) {
		inode_unlock(inode);
		return -ENODEV;
	}
	chip_seqops = inode->i_private;
	seqops = chip_seqops->seqops;
	chip = chip_seqops->chip;
	get_device(&chip->dev);
	inode_unlock(inode);

	 
	err = seq_open(file, seqops);
	if (!err) {
		seq = file->private_data;
		seq->private = chip;
	}

	return err;
}

static int tpm_bios_measurements_release(struct inode *inode,
					 struct file *file)
{
	struct seq_file *seq = file->private_data;
	struct tpm_chip *chip = seq->private;

	put_device(&chip->dev);

	return seq_release(inode, file);
}

static const struct file_operations tpm_bios_measurements_ops = {
	.owner = THIS_MODULE,
	.open = tpm_bios_measurements_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = tpm_bios_measurements_release,
};

static int tpm_read_log(struct tpm_chip *chip)
{
	int rc;

	if (chip->log.bios_event_log != NULL) {
		dev_dbg(&chip->dev,
			"%s: ERROR - event log already initialized\n",
			__func__);
		return -EFAULT;
	}

	rc = tpm_read_log_acpi(chip);
	if (rc != -ENODEV)
		return rc;

	rc = tpm_read_log_efi(chip);
	if (rc != -ENODEV)
		return rc;

	return tpm_read_log_of(chip);
}

 
void tpm_bios_log_setup(struct tpm_chip *chip)
{
	const char *name = dev_name(&chip->dev);
	unsigned int cnt;
	int log_version;
	int rc = 0;

	if (chip->flags & TPM_CHIP_FLAG_VIRTUAL)
		return;

	rc = tpm_read_log(chip);
	if (rc < 0)
		return;
	log_version = rc;

	cnt = 0;
	chip->bios_dir[cnt] = securityfs_create_dir(name, NULL);
	 
	if (IS_ERR(chip->bios_dir[cnt]))
		goto err;
	cnt++;

	chip->bin_log_seqops.chip = chip;
	if (log_version == EFI_TCG2_EVENT_LOG_FORMAT_TCG_2)
		chip->bin_log_seqops.seqops =
			&tpm2_binary_b_measurements_seqops;
	else
		chip->bin_log_seqops.seqops =
			&tpm1_binary_b_measurements_seqops;


	chip->bios_dir[cnt] =
	    securityfs_create_file("binary_bios_measurements",
				   0440, chip->bios_dir[0],
				   (void *)&chip->bin_log_seqops,
				   &tpm_bios_measurements_ops);
	if (IS_ERR(chip->bios_dir[cnt]))
		goto err;
	cnt++;

	if (!(chip->flags & TPM_CHIP_FLAG_TPM2)) {

		chip->ascii_log_seqops.chip = chip;
		chip->ascii_log_seqops.seqops =
			&tpm1_ascii_b_measurements_seqops;

		chip->bios_dir[cnt] =
			securityfs_create_file("ascii_bios_measurements",
					       0440, chip->bios_dir[0],
					       (void *)&chip->ascii_log_seqops,
					       &tpm_bios_measurements_ops);
		if (IS_ERR(chip->bios_dir[cnt]))
			goto err;
		cnt++;
	}

	return;

err:
	chip->bios_dir[cnt] = NULL;
	tpm_bios_log_teardown(chip);
	return;
}

void tpm_bios_log_teardown(struct tpm_chip *chip)
{
	int i;
	struct inode *inode;

	 
	for (i = (TPM_NUM_EVENT_LOG_FILES - 1); i >= 0; i--) {
		if (chip->bios_dir[i]) {
			inode = d_inode(chip->bios_dir[i]);
			inode_lock(inode);
			inode->i_private = NULL;
			inode_unlock(inode);
			securityfs_remove(chip->bios_dir[i]);
		}
	}
}
