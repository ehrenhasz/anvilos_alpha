
 
#include <linux/string.h>
#include <linux/vfio.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/ctype.h>
#include <linux/bitops.h>
#include <linux/kvm_host.h>
#include <linux/module.h>
#include <linux/uuid.h>
#include <asm/kvm.h>
#include <asm/zcrypt.h>

#include "vfio_ap_private.h"
#include "vfio_ap_debug.h"

#define VFIO_AP_MDEV_TYPE_HWVIRT "passthrough"
#define VFIO_AP_MDEV_NAME_HWVIRT "VFIO AP Passthrough Device"

#define AP_QUEUE_ASSIGNED "assigned"
#define AP_QUEUE_UNASSIGNED "unassigned"
#define AP_QUEUE_IN_USE "in use"

#define AP_RESET_INTERVAL		20	 

static int vfio_ap_mdev_reset_queues(struct ap_queue_table *qtable);
static struct vfio_ap_queue *vfio_ap_find_queue(int apqn);
static const struct vfio_device_ops vfio_ap_matrix_dev_ops;
static void vfio_ap_mdev_reset_queue(struct vfio_ap_queue *q);

 
static inline void get_update_locks_for_kvm(struct kvm *kvm)
{
	mutex_lock(&matrix_dev->guests_lock);
	if (kvm)
		mutex_lock(&kvm->lock);
	mutex_lock(&matrix_dev->mdevs_lock);
}

 
static inline void release_update_locks_for_kvm(struct kvm *kvm)
{
	mutex_unlock(&matrix_dev->mdevs_lock);
	if (kvm)
		mutex_unlock(&kvm->lock);
	mutex_unlock(&matrix_dev->guests_lock);
}

 
static inline void get_update_locks_for_mdev(struct ap_matrix_mdev *matrix_mdev)
{
	mutex_lock(&matrix_dev->guests_lock);
	if (matrix_mdev && matrix_mdev->kvm)
		mutex_lock(&matrix_mdev->kvm->lock);
	mutex_lock(&matrix_dev->mdevs_lock);
}

 
static inline void release_update_locks_for_mdev(struct ap_matrix_mdev *matrix_mdev)
{
	mutex_unlock(&matrix_dev->mdevs_lock);
	if (matrix_mdev && matrix_mdev->kvm)
		mutex_unlock(&matrix_mdev->kvm->lock);
	mutex_unlock(&matrix_dev->guests_lock);
}

 
static struct ap_matrix_mdev *get_update_locks_by_apqn(int apqn)
{
	struct ap_matrix_mdev *matrix_mdev;

	mutex_lock(&matrix_dev->guests_lock);

	list_for_each_entry(matrix_mdev, &matrix_dev->mdev_list, node) {
		if (test_bit_inv(AP_QID_CARD(apqn), matrix_mdev->matrix.apm) &&
		    test_bit_inv(AP_QID_QUEUE(apqn), matrix_mdev->matrix.aqm)) {
			if (matrix_mdev->kvm)
				mutex_lock(&matrix_mdev->kvm->lock);

			mutex_lock(&matrix_dev->mdevs_lock);

			return matrix_mdev;
		}
	}

	mutex_lock(&matrix_dev->mdevs_lock);

	return NULL;
}

 
static inline void get_update_locks_for_queue(struct vfio_ap_queue *q)
{
	mutex_lock(&matrix_dev->guests_lock);
	if (q->matrix_mdev && q->matrix_mdev->kvm)
		mutex_lock(&q->matrix_mdev->kvm->lock);
	mutex_lock(&matrix_dev->mdevs_lock);
}

 
static struct vfio_ap_queue *vfio_ap_mdev_get_queue(
					struct ap_matrix_mdev *matrix_mdev,
					int apqn)
{
	struct vfio_ap_queue *q;

	hash_for_each_possible(matrix_mdev->qtable.queues, q, mdev_qnode,
			       apqn) {
		if (q && q->apqn == apqn)
			return q;
	}

	return NULL;
}

 
static void vfio_ap_wait_for_irqclear(int apqn)
{
	struct ap_queue_status status;
	int retry = 5;

	do {
		status = ap_tapq(apqn, NULL);
		switch (status.response_code) {
		case AP_RESPONSE_NORMAL:
		case AP_RESPONSE_RESET_IN_PROGRESS:
			if (!status.irq_enabled)
				return;
			fallthrough;
		case AP_RESPONSE_BUSY:
			msleep(20);
			break;
		case AP_RESPONSE_Q_NOT_AVAIL:
		case AP_RESPONSE_DECONFIGURED:
		case AP_RESPONSE_CHECKSTOPPED:
		default:
			WARN_ONCE(1, "%s: tapq rc %02x: %04x\n", __func__,
				  status.response_code, apqn);
			return;
		}
	} while (--retry);

	WARN_ONCE(1, "%s: tapq rc %02x: %04x could not clear IR bit\n",
		  __func__, status.response_code, apqn);
}

 
static void vfio_ap_free_aqic_resources(struct vfio_ap_queue *q)
{
	if (!q)
		return;
	if (q->saved_isc != VFIO_AP_ISC_INVALID &&
	    !WARN_ON(!(q->matrix_mdev && q->matrix_mdev->kvm))) {
		kvm_s390_gisc_unregister(q->matrix_mdev->kvm, q->saved_isc);
		q->saved_isc = VFIO_AP_ISC_INVALID;
	}
	if (q->saved_iova && !WARN_ON(!q->matrix_mdev)) {
		vfio_unpin_pages(&q->matrix_mdev->vdev, q->saved_iova, 1);
		q->saved_iova = 0;
	}
}

 
static struct ap_queue_status vfio_ap_irq_disable(struct vfio_ap_queue *q)
{
	union ap_qirq_ctrl aqic_gisa = { .value = 0 };
	struct ap_queue_status status;
	int retries = 5;

	do {
		status = ap_aqic(q->apqn, aqic_gisa, 0);
		switch (status.response_code) {
		case AP_RESPONSE_OTHERWISE_CHANGED:
		case AP_RESPONSE_NORMAL:
			vfio_ap_wait_for_irqclear(q->apqn);
			goto end_free;
		case AP_RESPONSE_RESET_IN_PROGRESS:
		case AP_RESPONSE_BUSY:
			msleep(20);
			break;
		case AP_RESPONSE_Q_NOT_AVAIL:
		case AP_RESPONSE_DECONFIGURED:
		case AP_RESPONSE_CHECKSTOPPED:
		case AP_RESPONSE_INVALID_ADDRESS:
		default:
			 
			WARN_ONCE(1, "%s: ap_aqic status %d\n", __func__,
				  status.response_code);
			goto end_free;
		}
	} while (retries--);

	WARN_ONCE(1, "%s: ap_aqic status %d\n", __func__,
		  status.response_code);
end_free:
	vfio_ap_free_aqic_resources(q);
	return status;
}

 
static int vfio_ap_validate_nib(struct kvm_vcpu *vcpu, dma_addr_t *nib)
{
	*nib = vcpu->run->s.regs.gprs[2];

	if (!*nib)
		return -EINVAL;
	if (kvm_is_error_hva(gfn_to_hva(vcpu->kvm, *nib >> PAGE_SHIFT)))
		return -EINVAL;

	return 0;
}

static int ensure_nib_shared(unsigned long addr, struct gmap *gmap)
{
	int ret;

	 
	ret = uv_pin_shared(addr);
	if (ret) {
		 
		gmap_convert_to_secure(gmap, addr);
	}
	return ret;
}

 
static struct ap_queue_status vfio_ap_irq_enable(struct vfio_ap_queue *q,
						 int isc,
						 struct kvm_vcpu *vcpu)
{
	union ap_qirq_ctrl aqic_gisa = { .value = 0 };
	struct ap_queue_status status = {};
	struct kvm_s390_gisa *gisa;
	struct page *h_page;
	int nisc;
	struct kvm *kvm;
	phys_addr_t h_nib;
	dma_addr_t nib;
	int ret;

	 
	if (vfio_ap_validate_nib(vcpu, &nib)) {
		VFIO_AP_DBF_WARN("%s: invalid NIB address: nib=%pad, apqn=%#04x\n",
				 __func__, &nib, q->apqn);

		status.response_code = AP_RESPONSE_INVALID_ADDRESS;
		return status;
	}

	ret = vfio_pin_pages(&q->matrix_mdev->vdev, nib, 1,
			     IOMMU_READ | IOMMU_WRITE, &h_page);
	switch (ret) {
	case 1:
		break;
	default:
		VFIO_AP_DBF_WARN("%s: vfio_pin_pages failed: rc=%d,"
				 "nib=%pad, apqn=%#04x\n",
				 __func__, ret, &nib, q->apqn);

		status.response_code = AP_RESPONSE_INVALID_ADDRESS;
		return status;
	}

	kvm = q->matrix_mdev->kvm;
	gisa = kvm->arch.gisa_int.origin;

	h_nib = page_to_phys(h_page) | (nib & ~PAGE_MASK);
	aqic_gisa.gisc = isc;

	 
	if (kvm_s390_pv_cpu_is_protected(vcpu) &&
	    ensure_nib_shared(h_nib & PAGE_MASK, kvm->arch.gmap)) {
		vfio_unpin_pages(&q->matrix_mdev->vdev, nib, 1);
		status.response_code = AP_RESPONSE_INVALID_ADDRESS;
		return status;
	}

	nisc = kvm_s390_gisc_register(kvm, isc);
	if (nisc < 0) {
		VFIO_AP_DBF_WARN("%s: gisc registration failed: nisc=%d, isc=%d, apqn=%#04x\n",
				 __func__, nisc, isc, q->apqn);

		status.response_code = AP_RESPONSE_INVALID_GISA;
		return status;
	}

	aqic_gisa.isc = nisc;
	aqic_gisa.ir = 1;
	aqic_gisa.gisa = virt_to_phys(gisa) >> 4;

	status = ap_aqic(q->apqn, aqic_gisa, h_nib);
	switch (status.response_code) {
	case AP_RESPONSE_NORMAL:
		 
		vfio_ap_free_aqic_resources(q);
		q->saved_iova = nib;
		q->saved_isc = isc;
		break;
	case AP_RESPONSE_OTHERWISE_CHANGED:
		 
		vfio_unpin_pages(&q->matrix_mdev->vdev, nib, 1);
		kvm_s390_gisc_unregister(kvm, isc);
		break;
	default:
		pr_warn("%s: apqn %04x: response: %02x\n", __func__, q->apqn,
			status.response_code);
		vfio_ap_irq_disable(q);
		break;
	}

	if (status.response_code != AP_RESPONSE_NORMAL) {
		VFIO_AP_DBF_WARN("%s: PQAP(AQIC) failed with status=%#02x: "
				 "zone=%#x, ir=%#x, gisc=%#x, f=%#x,"
				 "gisa=%#x, isc=%#x, apqn=%#04x\n",
				 __func__, status.response_code,
				 aqic_gisa.zone, aqic_gisa.ir, aqic_gisa.gisc,
				 aqic_gisa.gf, aqic_gisa.gisa, aqic_gisa.isc,
				 q->apqn);
	}

	return status;
}

 
static void vfio_ap_le_guid_to_be_uuid(guid_t *guid, unsigned long *uuid)
{
	 
	uuid[0] = le32_to_cpup((__le32 *)guid);
	uuid[1] = le16_to_cpup((__le16 *)&guid->b[4]);
	uuid[2] = le16_to_cpup((__le16 *)&guid->b[6]);
	uuid[3] = *((__u16 *)&guid->b[8]);
	uuid[4] = *((__u16 *)&guid->b[10]);
	uuid[5] = *((__u32 *)&guid->b[12]);
}

 
static int handle_pqap(struct kvm_vcpu *vcpu)
{
	uint64_t status;
	uint16_t apqn;
	unsigned long uuid[6];
	struct vfio_ap_queue *q;
	struct ap_queue_status qstatus = {
			       .response_code = AP_RESPONSE_Q_NOT_AVAIL, };
	struct ap_matrix_mdev *matrix_mdev;

	apqn = vcpu->run->s.regs.gprs[0] & 0xffff;

	 
	if (!(vcpu->arch.sie_block->eca & ECA_AIV)) {
		VFIO_AP_DBF_WARN("%s: AIV facility not installed: apqn=0x%04x, eca=0x%04x\n",
				 __func__, apqn, vcpu->arch.sie_block->eca);

		return -EOPNOTSUPP;
	}

	mutex_lock(&matrix_dev->mdevs_lock);

	if (!vcpu->kvm->arch.crypto.pqap_hook) {
		VFIO_AP_DBF_WARN("%s: PQAP(AQIC) hook not registered with the vfio_ap driver: apqn=0x%04x\n",
				 __func__, apqn);

		goto out_unlock;
	}

	matrix_mdev = container_of(vcpu->kvm->arch.crypto.pqap_hook,
				   struct ap_matrix_mdev, pqap_hook);

	 
	if (!matrix_mdev->kvm) {
		vfio_ap_le_guid_to_be_uuid(&matrix_mdev->mdev->uuid, uuid);
		VFIO_AP_DBF_WARN("%s: mdev %08lx-%04lx-%04lx-%04lx-%04lx%08lx not in use: apqn=0x%04x\n",
				 __func__, uuid[0],  uuid[1], uuid[2],
				 uuid[3], uuid[4], uuid[5], apqn);
		goto out_unlock;
	}

	q = vfio_ap_mdev_get_queue(matrix_mdev, apqn);
	if (!q) {
		VFIO_AP_DBF_WARN("%s: Queue %02x.%04x not bound to the vfio_ap driver\n",
				 __func__, AP_QID_CARD(apqn),
				 AP_QID_QUEUE(apqn));
		goto out_unlock;
	}

	status = vcpu->run->s.regs.gprs[1];

	 
	if ((status >> (63 - 16)) & 0x01)
		qstatus = vfio_ap_irq_enable(q, status & 0x07, vcpu);
	else
		qstatus = vfio_ap_irq_disable(q);

out_unlock:
	memcpy(&vcpu->run->s.regs.gprs[1], &qstatus, sizeof(qstatus));
	vcpu->run->s.regs.gprs[1] >>= 32;
	mutex_unlock(&matrix_dev->mdevs_lock);
	return 0;
}

static void vfio_ap_matrix_init(struct ap_config_info *info,
				struct ap_matrix *matrix)
{
	matrix->apm_max = info->apxa ? info->na : 63;
	matrix->aqm_max = info->apxa ? info->nd : 15;
	matrix->adm_max = info->apxa ? info->nd : 15;
}

static void vfio_ap_mdev_update_guest_apcb(struct ap_matrix_mdev *matrix_mdev)
{
	if (matrix_mdev->kvm)
		kvm_arch_crypto_set_masks(matrix_mdev->kvm,
					  matrix_mdev->shadow_apcb.apm,
					  matrix_mdev->shadow_apcb.aqm,
					  matrix_mdev->shadow_apcb.adm);
}

static bool vfio_ap_mdev_filter_cdoms(struct ap_matrix_mdev *matrix_mdev)
{
	DECLARE_BITMAP(prev_shadow_adm, AP_DOMAINS);

	bitmap_copy(prev_shadow_adm, matrix_mdev->shadow_apcb.adm, AP_DOMAINS);
	bitmap_and(matrix_mdev->shadow_apcb.adm, matrix_mdev->matrix.adm,
		   (unsigned long *)matrix_dev->info.adm, AP_DOMAINS);

	return !bitmap_equal(prev_shadow_adm, matrix_mdev->shadow_apcb.adm,
			     AP_DOMAINS);
}

 
static bool vfio_ap_mdev_filter_matrix(unsigned long *apm, unsigned long *aqm,
				       struct ap_matrix_mdev *matrix_mdev)
{
	unsigned long apid, apqi, apqn;
	DECLARE_BITMAP(prev_shadow_apm, AP_DEVICES);
	DECLARE_BITMAP(prev_shadow_aqm, AP_DOMAINS);
	struct vfio_ap_queue *q;

	bitmap_copy(prev_shadow_apm, matrix_mdev->shadow_apcb.apm, AP_DEVICES);
	bitmap_copy(prev_shadow_aqm, matrix_mdev->shadow_apcb.aqm, AP_DOMAINS);
	vfio_ap_matrix_init(&matrix_dev->info, &matrix_mdev->shadow_apcb);

	 
	bitmap_and(matrix_mdev->shadow_apcb.apm, matrix_mdev->matrix.apm,
		   (unsigned long *)matrix_dev->info.apm, AP_DEVICES);
	bitmap_and(matrix_mdev->shadow_apcb.aqm, matrix_mdev->matrix.aqm,
		   (unsigned long *)matrix_dev->info.aqm, AP_DOMAINS);

	for_each_set_bit_inv(apid, apm, AP_DEVICES) {
		for_each_set_bit_inv(apqi, aqm, AP_DOMAINS) {
			 
			apqn = AP_MKQID(apid, apqi);
			q = vfio_ap_mdev_get_queue(matrix_mdev, apqn);
			if (!q || q->reset_status.response_code) {
				clear_bit_inv(apid,
					      matrix_mdev->shadow_apcb.apm);
				break;
			}
		}
	}

	return !bitmap_equal(prev_shadow_apm, matrix_mdev->shadow_apcb.apm,
			     AP_DEVICES) ||
	       !bitmap_equal(prev_shadow_aqm, matrix_mdev->shadow_apcb.aqm,
			     AP_DOMAINS);
}

static int vfio_ap_mdev_init_dev(struct vfio_device *vdev)
{
	struct ap_matrix_mdev *matrix_mdev =
		container_of(vdev, struct ap_matrix_mdev, vdev);

	matrix_mdev->mdev = to_mdev_device(vdev->dev);
	vfio_ap_matrix_init(&matrix_dev->info, &matrix_mdev->matrix);
	matrix_mdev->pqap_hook = handle_pqap;
	vfio_ap_matrix_init(&matrix_dev->info, &matrix_mdev->shadow_apcb);
	hash_init(matrix_mdev->qtable.queues);

	return 0;
}

static int vfio_ap_mdev_probe(struct mdev_device *mdev)
{
	struct ap_matrix_mdev *matrix_mdev;
	int ret;

	matrix_mdev = vfio_alloc_device(ap_matrix_mdev, vdev, &mdev->dev,
					&vfio_ap_matrix_dev_ops);
	if (IS_ERR(matrix_mdev))
		return PTR_ERR(matrix_mdev);

	ret = vfio_register_emulated_iommu_dev(&matrix_mdev->vdev);
	if (ret)
		goto err_put_vdev;
	matrix_mdev->req_trigger = NULL;
	dev_set_drvdata(&mdev->dev, matrix_mdev);
	mutex_lock(&matrix_dev->mdevs_lock);
	list_add(&matrix_mdev->node, &matrix_dev->mdev_list);
	mutex_unlock(&matrix_dev->mdevs_lock);
	return 0;

err_put_vdev:
	vfio_put_device(&matrix_mdev->vdev);
	return ret;
}

static void vfio_ap_mdev_link_queue(struct ap_matrix_mdev *matrix_mdev,
				    struct vfio_ap_queue *q)
{
	if (q) {
		q->matrix_mdev = matrix_mdev;
		hash_add(matrix_mdev->qtable.queues, &q->mdev_qnode, q->apqn);
	}
}

static void vfio_ap_mdev_link_apqn(struct ap_matrix_mdev *matrix_mdev, int apqn)
{
	struct vfio_ap_queue *q;

	q = vfio_ap_find_queue(apqn);
	vfio_ap_mdev_link_queue(matrix_mdev, q);
}

static void vfio_ap_unlink_queue_fr_mdev(struct vfio_ap_queue *q)
{
	hash_del(&q->mdev_qnode);
}

static void vfio_ap_unlink_mdev_fr_queue(struct vfio_ap_queue *q)
{
	q->matrix_mdev = NULL;
}

static void vfio_ap_mdev_unlink_fr_queues(struct ap_matrix_mdev *matrix_mdev)
{
	struct vfio_ap_queue *q;
	unsigned long apid, apqi;

	for_each_set_bit_inv(apid, matrix_mdev->matrix.apm, AP_DEVICES) {
		for_each_set_bit_inv(apqi, matrix_mdev->matrix.aqm,
				     AP_DOMAINS) {
			q = vfio_ap_mdev_get_queue(matrix_mdev,
						   AP_MKQID(apid, apqi));
			if (q)
				q->matrix_mdev = NULL;
		}
	}
}

static void vfio_ap_mdev_remove(struct mdev_device *mdev)
{
	struct ap_matrix_mdev *matrix_mdev = dev_get_drvdata(&mdev->dev);

	vfio_unregister_group_dev(&matrix_mdev->vdev);

	mutex_lock(&matrix_dev->guests_lock);
	mutex_lock(&matrix_dev->mdevs_lock);
	vfio_ap_mdev_reset_queues(&matrix_mdev->qtable);
	vfio_ap_mdev_unlink_fr_queues(matrix_mdev);
	list_del(&matrix_mdev->node);
	mutex_unlock(&matrix_dev->mdevs_lock);
	mutex_unlock(&matrix_dev->guests_lock);
	vfio_put_device(&matrix_mdev->vdev);
}

#define MDEV_SHARING_ERR "Userspace may not re-assign queue %02lx.%04lx " \
			 "already assigned to %s"

static void vfio_ap_mdev_log_sharing_err(struct ap_matrix_mdev *matrix_mdev,
					 unsigned long *apm,
					 unsigned long *aqm)
{
	unsigned long apid, apqi;
	const struct device *dev = mdev_dev(matrix_mdev->mdev);
	const char *mdev_name = dev_name(dev);

	for_each_set_bit_inv(apid, apm, AP_DEVICES)
		for_each_set_bit_inv(apqi, aqm, AP_DOMAINS)
			dev_warn(dev, MDEV_SHARING_ERR, apid, apqi, mdev_name);
}

 
static int vfio_ap_mdev_verify_no_sharing(unsigned long *mdev_apm,
					  unsigned long *mdev_aqm)
{
	struct ap_matrix_mdev *matrix_mdev;
	DECLARE_BITMAP(apm, AP_DEVICES);
	DECLARE_BITMAP(aqm, AP_DOMAINS);

	list_for_each_entry(matrix_mdev, &matrix_dev->mdev_list, node) {
		 
		if (mdev_apm == matrix_mdev->matrix.apm &&
		    mdev_aqm == matrix_mdev->matrix.aqm)
			continue;

		memset(apm, 0, sizeof(apm));
		memset(aqm, 0, sizeof(aqm));

		 
		if (!bitmap_and(apm, mdev_apm, matrix_mdev->matrix.apm,
				AP_DEVICES))
			continue;

		if (!bitmap_and(aqm, mdev_aqm, matrix_mdev->matrix.aqm,
				AP_DOMAINS))
			continue;

		vfio_ap_mdev_log_sharing_err(matrix_mdev, apm, aqm);

		return -EADDRINUSE;
	}

	return 0;
}

 
static int vfio_ap_mdev_validate_masks(struct ap_matrix_mdev *matrix_mdev)
{
	if (ap_apqn_in_matrix_owned_by_def_drv(matrix_mdev->matrix.apm,
					       matrix_mdev->matrix.aqm))
		return -EADDRNOTAVAIL;

	return vfio_ap_mdev_verify_no_sharing(matrix_mdev->matrix.apm,
					      matrix_mdev->matrix.aqm);
}

static void vfio_ap_mdev_link_adapter(struct ap_matrix_mdev *matrix_mdev,
				      unsigned long apid)
{
	unsigned long apqi;

	for_each_set_bit_inv(apqi, matrix_mdev->matrix.aqm, AP_DOMAINS)
		vfio_ap_mdev_link_apqn(matrix_mdev,
				       AP_MKQID(apid, apqi));
}

 
static ssize_t assign_adapter_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	int ret;
	unsigned long apid;
	DECLARE_BITMAP(apm_delta, AP_DEVICES);
	struct ap_matrix_mdev *matrix_mdev = dev_get_drvdata(dev);

	mutex_lock(&ap_perms_mutex);
	get_update_locks_for_mdev(matrix_mdev);

	ret = kstrtoul(buf, 0, &apid);
	if (ret)
		goto done;

	if (apid > matrix_mdev->matrix.apm_max) {
		ret = -ENODEV;
		goto done;
	}

	if (test_bit_inv(apid, matrix_mdev->matrix.apm)) {
		ret = count;
		goto done;
	}

	set_bit_inv(apid, matrix_mdev->matrix.apm);

	ret = vfio_ap_mdev_validate_masks(matrix_mdev);
	if (ret) {
		clear_bit_inv(apid, matrix_mdev->matrix.apm);
		goto done;
	}

	vfio_ap_mdev_link_adapter(matrix_mdev, apid);
	memset(apm_delta, 0, sizeof(apm_delta));
	set_bit_inv(apid, apm_delta);

	if (vfio_ap_mdev_filter_matrix(apm_delta,
				       matrix_mdev->matrix.aqm, matrix_mdev))
		vfio_ap_mdev_update_guest_apcb(matrix_mdev);

	ret = count;
done:
	release_update_locks_for_mdev(matrix_mdev);
	mutex_unlock(&ap_perms_mutex);

	return ret;
}
static DEVICE_ATTR_WO(assign_adapter);

static struct vfio_ap_queue
*vfio_ap_unlink_apqn_fr_mdev(struct ap_matrix_mdev *matrix_mdev,
			     unsigned long apid, unsigned long apqi)
{
	struct vfio_ap_queue *q = NULL;

	q = vfio_ap_mdev_get_queue(matrix_mdev, AP_MKQID(apid, apqi));
	 
	if (q)
		vfio_ap_unlink_queue_fr_mdev(q);

	return q;
}

 
static void vfio_ap_mdev_unlink_adapter(struct ap_matrix_mdev *matrix_mdev,
					unsigned long apid,
					struct ap_queue_table *qtable)
{
	unsigned long apqi;
	struct vfio_ap_queue *q;

	for_each_set_bit_inv(apqi, matrix_mdev->matrix.aqm, AP_DOMAINS) {
		q = vfio_ap_unlink_apqn_fr_mdev(matrix_mdev, apid, apqi);

		if (q && qtable) {
			if (test_bit_inv(apid, matrix_mdev->shadow_apcb.apm) &&
			    test_bit_inv(apqi, matrix_mdev->shadow_apcb.aqm))
				hash_add(qtable->queues, &q->mdev_qnode,
					 q->apqn);
		}
	}
}

static void vfio_ap_mdev_hot_unplug_adapter(struct ap_matrix_mdev *matrix_mdev,
					    unsigned long apid)
{
	int loop_cursor;
	struct vfio_ap_queue *q;
	struct ap_queue_table *qtable = kzalloc(sizeof(*qtable), GFP_KERNEL);

	hash_init(qtable->queues);
	vfio_ap_mdev_unlink_adapter(matrix_mdev, apid, qtable);

	if (test_bit_inv(apid, matrix_mdev->shadow_apcb.apm)) {
		clear_bit_inv(apid, matrix_mdev->shadow_apcb.apm);
		vfio_ap_mdev_update_guest_apcb(matrix_mdev);
	}

	vfio_ap_mdev_reset_queues(qtable);

	hash_for_each(qtable->queues, loop_cursor, q, mdev_qnode) {
		vfio_ap_unlink_mdev_fr_queue(q);
		hash_del(&q->mdev_qnode);
	}

	kfree(qtable);
}

 
static ssize_t unassign_adapter_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	int ret;
	unsigned long apid;
	struct ap_matrix_mdev *matrix_mdev = dev_get_drvdata(dev);

	get_update_locks_for_mdev(matrix_mdev);

	ret = kstrtoul(buf, 0, &apid);
	if (ret)
		goto done;

	if (apid > matrix_mdev->matrix.apm_max) {
		ret = -ENODEV;
		goto done;
	}

	if (!test_bit_inv(apid, matrix_mdev->matrix.apm)) {
		ret = count;
		goto done;
	}

	clear_bit_inv((unsigned long)apid, matrix_mdev->matrix.apm);
	vfio_ap_mdev_hot_unplug_adapter(matrix_mdev, apid);
	ret = count;
done:
	release_update_locks_for_mdev(matrix_mdev);
	return ret;
}
static DEVICE_ATTR_WO(unassign_adapter);

static void vfio_ap_mdev_link_domain(struct ap_matrix_mdev *matrix_mdev,
				     unsigned long apqi)
{
	unsigned long apid;

	for_each_set_bit_inv(apid, matrix_mdev->matrix.apm, AP_DEVICES)
		vfio_ap_mdev_link_apqn(matrix_mdev,
				       AP_MKQID(apid, apqi));
}

 
static ssize_t assign_domain_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int ret;
	unsigned long apqi;
	DECLARE_BITMAP(aqm_delta, AP_DOMAINS);
	struct ap_matrix_mdev *matrix_mdev = dev_get_drvdata(dev);

	mutex_lock(&ap_perms_mutex);
	get_update_locks_for_mdev(matrix_mdev);

	ret = kstrtoul(buf, 0, &apqi);
	if (ret)
		goto done;

	if (apqi > matrix_mdev->matrix.aqm_max) {
		ret = -ENODEV;
		goto done;
	}

	if (test_bit_inv(apqi, matrix_mdev->matrix.aqm)) {
		ret = count;
		goto done;
	}

	set_bit_inv(apqi, matrix_mdev->matrix.aqm);

	ret = vfio_ap_mdev_validate_masks(matrix_mdev);
	if (ret) {
		clear_bit_inv(apqi, matrix_mdev->matrix.aqm);
		goto done;
	}

	vfio_ap_mdev_link_domain(matrix_mdev, apqi);
	memset(aqm_delta, 0, sizeof(aqm_delta));
	set_bit_inv(apqi, aqm_delta);

	if (vfio_ap_mdev_filter_matrix(matrix_mdev->matrix.apm, aqm_delta,
				       matrix_mdev))
		vfio_ap_mdev_update_guest_apcb(matrix_mdev);

	ret = count;
done:
	release_update_locks_for_mdev(matrix_mdev);
	mutex_unlock(&ap_perms_mutex);

	return ret;
}
static DEVICE_ATTR_WO(assign_domain);

static void vfio_ap_mdev_unlink_domain(struct ap_matrix_mdev *matrix_mdev,
				       unsigned long apqi,
				       struct ap_queue_table *qtable)
{
	unsigned long apid;
	struct vfio_ap_queue *q;

	for_each_set_bit_inv(apid, matrix_mdev->matrix.apm, AP_DEVICES) {
		q = vfio_ap_unlink_apqn_fr_mdev(matrix_mdev, apid, apqi);

		if (q && qtable) {
			if (test_bit_inv(apid, matrix_mdev->shadow_apcb.apm) &&
			    test_bit_inv(apqi, matrix_mdev->shadow_apcb.aqm))
				hash_add(qtable->queues, &q->mdev_qnode,
					 q->apqn);
		}
	}
}

static void vfio_ap_mdev_hot_unplug_domain(struct ap_matrix_mdev *matrix_mdev,
					   unsigned long apqi)
{
	int loop_cursor;
	struct vfio_ap_queue *q;
	struct ap_queue_table *qtable = kzalloc(sizeof(*qtable), GFP_KERNEL);

	hash_init(qtable->queues);
	vfio_ap_mdev_unlink_domain(matrix_mdev, apqi, qtable);

	if (test_bit_inv(apqi, matrix_mdev->shadow_apcb.aqm)) {
		clear_bit_inv(apqi, matrix_mdev->shadow_apcb.aqm);
		vfio_ap_mdev_update_guest_apcb(matrix_mdev);
	}

	vfio_ap_mdev_reset_queues(qtable);

	hash_for_each(qtable->queues, loop_cursor, q, mdev_qnode) {
		vfio_ap_unlink_mdev_fr_queue(q);
		hash_del(&q->mdev_qnode);
	}

	kfree(qtable);
}

 
static ssize_t unassign_domain_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int ret;
	unsigned long apqi;
	struct ap_matrix_mdev *matrix_mdev = dev_get_drvdata(dev);

	get_update_locks_for_mdev(matrix_mdev);

	ret = kstrtoul(buf, 0, &apqi);
	if (ret)
		goto done;

	if (apqi > matrix_mdev->matrix.aqm_max) {
		ret = -ENODEV;
		goto done;
	}

	if (!test_bit_inv(apqi, matrix_mdev->matrix.aqm)) {
		ret = count;
		goto done;
	}

	clear_bit_inv((unsigned long)apqi, matrix_mdev->matrix.aqm);
	vfio_ap_mdev_hot_unplug_domain(matrix_mdev, apqi);
	ret = count;

done:
	release_update_locks_for_mdev(matrix_mdev);
	return ret;
}
static DEVICE_ATTR_WO(unassign_domain);

 
static ssize_t assign_control_domain_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	int ret;
	unsigned long id;
	struct ap_matrix_mdev *matrix_mdev = dev_get_drvdata(dev);

	get_update_locks_for_mdev(matrix_mdev);

	ret = kstrtoul(buf, 0, &id);
	if (ret)
		goto done;

	if (id > matrix_mdev->matrix.adm_max) {
		ret = -ENODEV;
		goto done;
	}

	if (test_bit_inv(id, matrix_mdev->matrix.adm)) {
		ret = count;
		goto done;
	}

	 
	set_bit_inv(id, matrix_mdev->matrix.adm);
	if (vfio_ap_mdev_filter_cdoms(matrix_mdev))
		vfio_ap_mdev_update_guest_apcb(matrix_mdev);

	ret = count;
done:
	release_update_locks_for_mdev(matrix_mdev);
	return ret;
}
static DEVICE_ATTR_WO(assign_control_domain);

 
static ssize_t unassign_control_domain_store(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t count)
{
	int ret;
	unsigned long domid;
	struct ap_matrix_mdev *matrix_mdev = dev_get_drvdata(dev);

	get_update_locks_for_mdev(matrix_mdev);

	ret = kstrtoul(buf, 0, &domid);
	if (ret)
		goto done;

	if (domid > matrix_mdev->matrix.adm_max) {
		ret = -ENODEV;
		goto done;
	}

	if (!test_bit_inv(domid, matrix_mdev->matrix.adm)) {
		ret = count;
		goto done;
	}

	clear_bit_inv(domid, matrix_mdev->matrix.adm);

	if (test_bit_inv(domid, matrix_mdev->shadow_apcb.adm)) {
		clear_bit_inv(domid, matrix_mdev->shadow_apcb.adm);
		vfio_ap_mdev_update_guest_apcb(matrix_mdev);
	}

	ret = count;
done:
	release_update_locks_for_mdev(matrix_mdev);
	return ret;
}
static DEVICE_ATTR_WO(unassign_control_domain);

static ssize_t control_domains_show(struct device *dev,
				    struct device_attribute *dev_attr,
				    char *buf)
{
	unsigned long id;
	int nchars = 0;
	int n;
	char *bufpos = buf;
	struct ap_matrix_mdev *matrix_mdev = dev_get_drvdata(dev);
	unsigned long max_domid = matrix_mdev->matrix.adm_max;

	mutex_lock(&matrix_dev->mdevs_lock);
	for_each_set_bit_inv(id, matrix_mdev->matrix.adm, max_domid + 1) {
		n = sprintf(bufpos, "%04lx\n", id);
		bufpos += n;
		nchars += n;
	}
	mutex_unlock(&matrix_dev->mdevs_lock);

	return nchars;
}
static DEVICE_ATTR_RO(control_domains);

static ssize_t vfio_ap_mdev_matrix_show(struct ap_matrix *matrix, char *buf)
{
	char *bufpos = buf;
	unsigned long apid;
	unsigned long apqi;
	unsigned long apid1;
	unsigned long apqi1;
	unsigned long napm_bits = matrix->apm_max + 1;
	unsigned long naqm_bits = matrix->aqm_max + 1;
	int nchars = 0;
	int n;

	apid1 = find_first_bit_inv(matrix->apm, napm_bits);
	apqi1 = find_first_bit_inv(matrix->aqm, naqm_bits);

	if ((apid1 < napm_bits) && (apqi1 < naqm_bits)) {
		for_each_set_bit_inv(apid, matrix->apm, napm_bits) {
			for_each_set_bit_inv(apqi, matrix->aqm,
					     naqm_bits) {
				n = sprintf(bufpos, "%02lx.%04lx\n", apid,
					    apqi);
				bufpos += n;
				nchars += n;
			}
		}
	} else if (apid1 < napm_bits) {
		for_each_set_bit_inv(apid, matrix->apm, napm_bits) {
			n = sprintf(bufpos, "%02lx.\n", apid);
			bufpos += n;
			nchars += n;
		}
	} else if (apqi1 < naqm_bits) {
		for_each_set_bit_inv(apqi, matrix->aqm, naqm_bits) {
			n = sprintf(bufpos, ".%04lx\n", apqi);
			bufpos += n;
			nchars += n;
		}
	}

	return nchars;
}

static ssize_t matrix_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	ssize_t nchars;
	struct ap_matrix_mdev *matrix_mdev = dev_get_drvdata(dev);

	mutex_lock(&matrix_dev->mdevs_lock);
	nchars = vfio_ap_mdev_matrix_show(&matrix_mdev->matrix, buf);
	mutex_unlock(&matrix_dev->mdevs_lock);

	return nchars;
}
static DEVICE_ATTR_RO(matrix);

static ssize_t guest_matrix_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	ssize_t nchars;
	struct ap_matrix_mdev *matrix_mdev = dev_get_drvdata(dev);

	mutex_lock(&matrix_dev->mdevs_lock);
	nchars = vfio_ap_mdev_matrix_show(&matrix_mdev->shadow_apcb, buf);
	mutex_unlock(&matrix_dev->mdevs_lock);

	return nchars;
}
static DEVICE_ATTR_RO(guest_matrix);

static struct attribute *vfio_ap_mdev_attrs[] = {
	&dev_attr_assign_adapter.attr,
	&dev_attr_unassign_adapter.attr,
	&dev_attr_assign_domain.attr,
	&dev_attr_unassign_domain.attr,
	&dev_attr_assign_control_domain.attr,
	&dev_attr_unassign_control_domain.attr,
	&dev_attr_control_domains.attr,
	&dev_attr_matrix.attr,
	&dev_attr_guest_matrix.attr,
	NULL,
};

static struct attribute_group vfio_ap_mdev_attr_group = {
	.attrs = vfio_ap_mdev_attrs
};

static const struct attribute_group *vfio_ap_mdev_attr_groups[] = {
	&vfio_ap_mdev_attr_group,
	NULL
};

 
static int vfio_ap_mdev_set_kvm(struct ap_matrix_mdev *matrix_mdev,
				struct kvm *kvm)
{
	struct ap_matrix_mdev *m;

	if (kvm->arch.crypto.crycbd) {
		down_write(&kvm->arch.crypto.pqap_hook_rwsem);
		kvm->arch.crypto.pqap_hook = &matrix_mdev->pqap_hook;
		up_write(&kvm->arch.crypto.pqap_hook_rwsem);

		get_update_locks_for_kvm(kvm);

		list_for_each_entry(m, &matrix_dev->mdev_list, node) {
			if (m != matrix_mdev && m->kvm == kvm) {
				release_update_locks_for_kvm(kvm);
				return -EPERM;
			}
		}

		kvm_get_kvm(kvm);
		matrix_mdev->kvm = kvm;
		vfio_ap_mdev_update_guest_apcb(matrix_mdev);

		release_update_locks_for_kvm(kvm);
	}

	return 0;
}

static void unmap_iova(struct ap_matrix_mdev *matrix_mdev, u64 iova, u64 length)
{
	struct ap_queue_table *qtable = &matrix_mdev->qtable;
	struct vfio_ap_queue *q;
	int loop_cursor;

	hash_for_each(qtable->queues, loop_cursor, q, mdev_qnode) {
		if (q->saved_iova >= iova && q->saved_iova < iova + length)
			vfio_ap_irq_disable(q);
	}
}

static void vfio_ap_mdev_dma_unmap(struct vfio_device *vdev, u64 iova,
				   u64 length)
{
	struct ap_matrix_mdev *matrix_mdev =
		container_of(vdev, struct ap_matrix_mdev, vdev);

	mutex_lock(&matrix_dev->mdevs_lock);

	unmap_iova(matrix_mdev, iova, length);

	mutex_unlock(&matrix_dev->mdevs_lock);
}

 
static void vfio_ap_mdev_unset_kvm(struct ap_matrix_mdev *matrix_mdev)
{
	struct kvm *kvm = matrix_mdev->kvm;

	if (kvm && kvm->arch.crypto.crycbd) {
		down_write(&kvm->arch.crypto.pqap_hook_rwsem);
		kvm->arch.crypto.pqap_hook = NULL;
		up_write(&kvm->arch.crypto.pqap_hook_rwsem);

		get_update_locks_for_kvm(kvm);

		kvm_arch_crypto_clear_masks(kvm);
		vfio_ap_mdev_reset_queues(&matrix_mdev->qtable);
		kvm_put_kvm(kvm);
		matrix_mdev->kvm = NULL;

		release_update_locks_for_kvm(kvm);
	}
}

static struct vfio_ap_queue *vfio_ap_find_queue(int apqn)
{
	struct ap_queue *queue;
	struct vfio_ap_queue *q = NULL;

	queue = ap_get_qdev(apqn);
	if (!queue)
		return NULL;

	if (queue->ap_dev.device.driver == &matrix_dev->vfio_ap_drv->driver)
		q = dev_get_drvdata(&queue->ap_dev.device);

	put_device(&queue->ap_dev.device);

	return q;
}

static int apq_status_check(int apqn, struct ap_queue_status *status)
{
	switch (status->response_code) {
	case AP_RESPONSE_NORMAL:
	case AP_RESPONSE_DECONFIGURED:
		return 0;
	case AP_RESPONSE_RESET_IN_PROGRESS:
	case AP_RESPONSE_BUSY:
		return -EBUSY;
	case AP_RESPONSE_ASSOC_SECRET_NOT_UNIQUE:
	case AP_RESPONSE_ASSOC_FAILED:
		 
		return -EAGAIN;
	default:
		WARN(true,
		     "failed to verify reset of queue %02x.%04x: TAPQ rc=%u\n",
		     AP_QID_CARD(apqn), AP_QID_QUEUE(apqn),
		     status->response_code);
		return -EIO;
	}
}

#define WAIT_MSG "Waited %dms for reset of queue %02x.%04x (%u, %u, %u)"

static void apq_reset_check(struct work_struct *reset_work)
{
	int ret = -EBUSY, elapsed = 0;
	struct ap_queue_status status;
	struct vfio_ap_queue *q;

	q = container_of(reset_work, struct vfio_ap_queue, reset_work);
	memcpy(&status, &q->reset_status, sizeof(status));
	while (true) {
		msleep(AP_RESET_INTERVAL);
		elapsed += AP_RESET_INTERVAL;
		status = ap_tapq(q->apqn, NULL);
		ret = apq_status_check(q->apqn, &status);
		if (ret == -EIO)
			return;
		if (ret == -EBUSY) {
			pr_notice_ratelimited(WAIT_MSG, elapsed,
					      AP_QID_CARD(q->apqn),
					      AP_QID_QUEUE(q->apqn),
					      status.response_code,
					      status.queue_empty,
					      status.irq_enabled);
		} else {
			if (q->reset_status.response_code == AP_RESPONSE_RESET_IN_PROGRESS ||
			    q->reset_status.response_code == AP_RESPONSE_BUSY ||
			    q->reset_status.response_code == AP_RESPONSE_STATE_CHANGE_IN_PROGRESS ||
			    ret == -EAGAIN) {
				status = ap_zapq(q->apqn, 0);
				memcpy(&q->reset_status, &status, sizeof(status));
				continue;
			}
			 
			if (status.response_code == AP_RESPONSE_DECONFIGURED)
				q->reset_status.response_code = 0;
			if (q->saved_isc != VFIO_AP_ISC_INVALID)
				vfio_ap_free_aqic_resources(q);
			break;
		}
	}
}

static void vfio_ap_mdev_reset_queue(struct vfio_ap_queue *q)
{
	struct ap_queue_status status;

	if (!q)
		return;
	status = ap_zapq(q->apqn, 0);
	memcpy(&q->reset_status, &status, sizeof(status));
	switch (status.response_code) {
	case AP_RESPONSE_NORMAL:
	case AP_RESPONSE_RESET_IN_PROGRESS:
	case AP_RESPONSE_BUSY:
	case AP_RESPONSE_STATE_CHANGE_IN_PROGRESS:
		 
		queue_work(system_long_wq, &q->reset_work);
		break;
	case AP_RESPONSE_DECONFIGURED:
		 
		q->reset_status.response_code = 0;
		vfio_ap_free_aqic_resources(q);
		break;
	default:
		WARN(true,
		     "PQAP/ZAPQ for %02x.%04x failed with invalid rc=%u\n",
		     AP_QID_CARD(q->apqn), AP_QID_QUEUE(q->apqn),
		     status.response_code);
	}
}

static int vfio_ap_mdev_reset_queues(struct ap_queue_table *qtable)
{
	int ret = 0, loop_cursor;
	struct vfio_ap_queue *q;

	hash_for_each(qtable->queues, loop_cursor, q, mdev_qnode)
		vfio_ap_mdev_reset_queue(q);

	hash_for_each(qtable->queues, loop_cursor, q, mdev_qnode) {
		flush_work(&q->reset_work);

		if (q->reset_status.response_code)
			ret = -EIO;
	}

	return ret;
}

static int vfio_ap_mdev_open_device(struct vfio_device *vdev)
{
	struct ap_matrix_mdev *matrix_mdev =
		container_of(vdev, struct ap_matrix_mdev, vdev);

	if (!vdev->kvm)
		return -EINVAL;

	return vfio_ap_mdev_set_kvm(matrix_mdev, vdev->kvm);
}

static void vfio_ap_mdev_close_device(struct vfio_device *vdev)
{
	struct ap_matrix_mdev *matrix_mdev =
		container_of(vdev, struct ap_matrix_mdev, vdev);

	vfio_ap_mdev_unset_kvm(matrix_mdev);
}

static void vfio_ap_mdev_request(struct vfio_device *vdev, unsigned int count)
{
	struct device *dev = vdev->dev;
	struct ap_matrix_mdev *matrix_mdev;

	matrix_mdev = container_of(vdev, struct ap_matrix_mdev, vdev);

	if (matrix_mdev->req_trigger) {
		if (!(count % 10))
			dev_notice_ratelimited(dev,
					       "Relaying device request to user (#%u)\n",
					       count);

		eventfd_signal(matrix_mdev->req_trigger, 1);
	} else if (count == 0) {
		dev_notice(dev,
			   "No device request registered, blocked until released by user\n");
	}
}

static int vfio_ap_mdev_get_device_info(unsigned long arg)
{
	unsigned long minsz;
	struct vfio_device_info info;

	minsz = offsetofend(struct vfio_device_info, num_irqs);

	if (copy_from_user(&info, (void __user *)arg, minsz))
		return -EFAULT;

	if (info.argsz < minsz)
		return -EINVAL;

	info.flags = VFIO_DEVICE_FLAGS_AP | VFIO_DEVICE_FLAGS_RESET;
	info.num_regions = 0;
	info.num_irqs = VFIO_AP_NUM_IRQS;

	return copy_to_user((void __user *)arg, &info, minsz) ? -EFAULT : 0;
}

static ssize_t vfio_ap_get_irq_info(unsigned long arg)
{
	unsigned long minsz;
	struct vfio_irq_info info;

	minsz = offsetofend(struct vfio_irq_info, count);

	if (copy_from_user(&info, (void __user *)arg, minsz))
		return -EFAULT;

	if (info.argsz < minsz || info.index >= VFIO_AP_NUM_IRQS)
		return -EINVAL;

	switch (info.index) {
	case VFIO_AP_REQ_IRQ_INDEX:
		info.count = 1;
		info.flags = VFIO_IRQ_INFO_EVENTFD;
		break;
	default:
		return -EINVAL;
	}

	return copy_to_user((void __user *)arg, &info, minsz) ? -EFAULT : 0;
}

static int vfio_ap_irq_set_init(struct vfio_irq_set *irq_set, unsigned long arg)
{
	int ret;
	size_t data_size;
	unsigned long minsz;

	minsz = offsetofend(struct vfio_irq_set, count);

	if (copy_from_user(irq_set, (void __user *)arg, minsz))
		return -EFAULT;

	ret = vfio_set_irqs_validate_and_prepare(irq_set, 1, VFIO_AP_NUM_IRQS,
						 &data_size);
	if (ret)
		return ret;

	if (!(irq_set->flags & VFIO_IRQ_SET_ACTION_TRIGGER))
		return -EINVAL;

	return 0;
}

static int vfio_ap_set_request_irq(struct ap_matrix_mdev *matrix_mdev,
				   unsigned long arg)
{
	s32 fd;
	void __user *data;
	unsigned long minsz;
	struct eventfd_ctx *req_trigger;

	minsz = offsetofend(struct vfio_irq_set, count);
	data = (void __user *)(arg + minsz);

	if (get_user(fd, (s32 __user *)data))
		return -EFAULT;

	if (fd == -1) {
		if (matrix_mdev->req_trigger)
			eventfd_ctx_put(matrix_mdev->req_trigger);
		matrix_mdev->req_trigger = NULL;
	} else if (fd >= 0) {
		req_trigger = eventfd_ctx_fdget(fd);
		if (IS_ERR(req_trigger))
			return PTR_ERR(req_trigger);

		if (matrix_mdev->req_trigger)
			eventfd_ctx_put(matrix_mdev->req_trigger);

		matrix_mdev->req_trigger = req_trigger;
	} else {
		return -EINVAL;
	}

	return 0;
}

static int vfio_ap_set_irqs(struct ap_matrix_mdev *matrix_mdev,
			    unsigned long arg)
{
	int ret;
	struct vfio_irq_set irq_set;

	ret = vfio_ap_irq_set_init(&irq_set, arg);
	if (ret)
		return ret;

	switch (irq_set.flags & VFIO_IRQ_SET_DATA_TYPE_MASK) {
	case VFIO_IRQ_SET_DATA_EVENTFD:
		switch (irq_set.index) {
		case VFIO_AP_REQ_IRQ_INDEX:
			return vfio_ap_set_request_irq(matrix_mdev, arg);
		default:
			return -EINVAL;
		}
	default:
		return -EINVAL;
	}
}

static ssize_t vfio_ap_mdev_ioctl(struct vfio_device *vdev,
				    unsigned int cmd, unsigned long arg)
{
	struct ap_matrix_mdev *matrix_mdev =
		container_of(vdev, struct ap_matrix_mdev, vdev);
	int ret;

	mutex_lock(&matrix_dev->mdevs_lock);
	switch (cmd) {
	case VFIO_DEVICE_GET_INFO:
		ret = vfio_ap_mdev_get_device_info(arg);
		break;
	case VFIO_DEVICE_RESET:
		ret = vfio_ap_mdev_reset_queues(&matrix_mdev->qtable);
		break;
	case VFIO_DEVICE_GET_IRQ_INFO:
			ret = vfio_ap_get_irq_info(arg);
			break;
	case VFIO_DEVICE_SET_IRQS:
		ret = vfio_ap_set_irqs(matrix_mdev, arg);
		break;
	default:
		ret = -EOPNOTSUPP;
		break;
	}
	mutex_unlock(&matrix_dev->mdevs_lock);

	return ret;
}

static struct ap_matrix_mdev *vfio_ap_mdev_for_queue(struct vfio_ap_queue *q)
{
	struct ap_matrix_mdev *matrix_mdev;
	unsigned long apid = AP_QID_CARD(q->apqn);
	unsigned long apqi = AP_QID_QUEUE(q->apqn);

	list_for_each_entry(matrix_mdev, &matrix_dev->mdev_list, node) {
		if (test_bit_inv(apid, matrix_mdev->matrix.apm) &&
		    test_bit_inv(apqi, matrix_mdev->matrix.aqm))
			return matrix_mdev;
	}

	return NULL;
}

static ssize_t status_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	ssize_t nchars = 0;
	struct vfio_ap_queue *q;
	struct ap_matrix_mdev *matrix_mdev;
	struct ap_device *apdev = to_ap_dev(dev);

	mutex_lock(&matrix_dev->mdevs_lock);
	q = dev_get_drvdata(&apdev->device);
	matrix_mdev = vfio_ap_mdev_for_queue(q);

	if (matrix_mdev) {
		if (matrix_mdev->kvm)
			nchars = scnprintf(buf, PAGE_SIZE, "%s\n",
					   AP_QUEUE_IN_USE);
		else
			nchars = scnprintf(buf, PAGE_SIZE, "%s\n",
					   AP_QUEUE_ASSIGNED);
	} else {
		nchars = scnprintf(buf, PAGE_SIZE, "%s\n",
				   AP_QUEUE_UNASSIGNED);
	}

	mutex_unlock(&matrix_dev->mdevs_lock);

	return nchars;
}

static DEVICE_ATTR_RO(status);

static struct attribute *vfio_queue_attrs[] = {
	&dev_attr_status.attr,
	NULL,
};

static const struct attribute_group vfio_queue_attr_group = {
	.attrs = vfio_queue_attrs,
};

static const struct vfio_device_ops vfio_ap_matrix_dev_ops = {
	.init = vfio_ap_mdev_init_dev,
	.open_device = vfio_ap_mdev_open_device,
	.close_device = vfio_ap_mdev_close_device,
	.ioctl = vfio_ap_mdev_ioctl,
	.dma_unmap = vfio_ap_mdev_dma_unmap,
	.bind_iommufd = vfio_iommufd_emulated_bind,
	.unbind_iommufd = vfio_iommufd_emulated_unbind,
	.attach_ioas = vfio_iommufd_emulated_attach_ioas,
	.detach_ioas = vfio_iommufd_emulated_detach_ioas,
	.request = vfio_ap_mdev_request
};

static struct mdev_driver vfio_ap_matrix_driver = {
	.device_api = VFIO_DEVICE_API_AP_STRING,
	.max_instances = MAX_ZDEV_ENTRIES_EXT,
	.driver = {
		.name = "vfio_ap_mdev",
		.owner = THIS_MODULE,
		.mod_name = KBUILD_MODNAME,
		.dev_groups = vfio_ap_mdev_attr_groups,
	},
	.probe = vfio_ap_mdev_probe,
	.remove = vfio_ap_mdev_remove,
};

int vfio_ap_mdev_register(void)
{
	int ret;

	ret = mdev_register_driver(&vfio_ap_matrix_driver);
	if (ret)
		return ret;

	matrix_dev->mdev_type.sysfs_name = VFIO_AP_MDEV_TYPE_HWVIRT;
	matrix_dev->mdev_type.pretty_name = VFIO_AP_MDEV_NAME_HWVIRT;
	matrix_dev->mdev_types[0] = &matrix_dev->mdev_type;
	ret = mdev_register_parent(&matrix_dev->parent, &matrix_dev->device,
				   &vfio_ap_matrix_driver,
				   matrix_dev->mdev_types, 1);
	if (ret)
		goto err_driver;
	return 0;

err_driver:
	mdev_unregister_driver(&vfio_ap_matrix_driver);
	return ret;
}

void vfio_ap_mdev_unregister(void)
{
	mdev_unregister_parent(&matrix_dev->parent);
	mdev_unregister_driver(&vfio_ap_matrix_driver);
}

int vfio_ap_mdev_probe_queue(struct ap_device *apdev)
{
	int ret;
	struct vfio_ap_queue *q;
	struct ap_matrix_mdev *matrix_mdev;

	ret = sysfs_create_group(&apdev->device.kobj, &vfio_queue_attr_group);
	if (ret)
		return ret;

	q = kzalloc(sizeof(*q), GFP_KERNEL);
	if (!q) {
		ret = -ENOMEM;
		goto err_remove_group;
	}

	q->apqn = to_ap_queue(&apdev->device)->qid;
	q->saved_isc = VFIO_AP_ISC_INVALID;
	memset(&q->reset_status, 0, sizeof(q->reset_status));
	INIT_WORK(&q->reset_work, apq_reset_check);
	matrix_mdev = get_update_locks_by_apqn(q->apqn);

	if (matrix_mdev) {
		vfio_ap_mdev_link_queue(matrix_mdev, q);

		if (vfio_ap_mdev_filter_matrix(matrix_mdev->matrix.apm,
					       matrix_mdev->matrix.aqm,
					       matrix_mdev))
			vfio_ap_mdev_update_guest_apcb(matrix_mdev);
	}
	dev_set_drvdata(&apdev->device, q);
	release_update_locks_for_mdev(matrix_mdev);

	return 0;

err_remove_group:
	sysfs_remove_group(&apdev->device.kobj, &vfio_queue_attr_group);
	return ret;
}

void vfio_ap_mdev_remove_queue(struct ap_device *apdev)
{
	unsigned long apid, apqi;
	struct vfio_ap_queue *q;
	struct ap_matrix_mdev *matrix_mdev;

	sysfs_remove_group(&apdev->device.kobj, &vfio_queue_attr_group);
	q = dev_get_drvdata(&apdev->device);
	get_update_locks_for_queue(q);
	matrix_mdev = q->matrix_mdev;

	if (matrix_mdev) {
		vfio_ap_unlink_queue_fr_mdev(q);

		apid = AP_QID_CARD(q->apqn);
		apqi = AP_QID_QUEUE(q->apqn);

		 
		if (test_bit_inv(apid, matrix_mdev->shadow_apcb.apm) &&
		    test_bit_inv(apqi, matrix_mdev->shadow_apcb.aqm)) {
			clear_bit_inv(apid, matrix_mdev->shadow_apcb.apm);
			vfio_ap_mdev_update_guest_apcb(matrix_mdev);
		}
	}

	vfio_ap_mdev_reset_queue(q);
	flush_work(&q->reset_work);
	dev_set_drvdata(&apdev->device, NULL);
	kfree(q);
	release_update_locks_for_mdev(matrix_mdev);
}

 
int vfio_ap_mdev_resource_in_use(unsigned long *apm, unsigned long *aqm)
{
	int ret;

	mutex_lock(&matrix_dev->guests_lock);
	mutex_lock(&matrix_dev->mdevs_lock);
	ret = vfio_ap_mdev_verify_no_sharing(apm, aqm);
	mutex_unlock(&matrix_dev->mdevs_lock);
	mutex_unlock(&matrix_dev->guests_lock);

	return ret;
}

 
static void vfio_ap_mdev_hot_unplug_cfg(struct ap_matrix_mdev *matrix_mdev,
					unsigned long *aprem,
					unsigned long *aqrem,
					unsigned long *cdrem)
{
	int do_hotplug = 0;

	if (!bitmap_empty(aprem, AP_DEVICES)) {
		do_hotplug |= bitmap_andnot(matrix_mdev->shadow_apcb.apm,
					    matrix_mdev->shadow_apcb.apm,
					    aprem, AP_DEVICES);
	}

	if (!bitmap_empty(aqrem, AP_DOMAINS)) {
		do_hotplug |= bitmap_andnot(matrix_mdev->shadow_apcb.aqm,
					    matrix_mdev->shadow_apcb.aqm,
					    aqrem, AP_DEVICES);
	}

	if (!bitmap_empty(cdrem, AP_DOMAINS))
		do_hotplug |= bitmap_andnot(matrix_mdev->shadow_apcb.adm,
					    matrix_mdev->shadow_apcb.adm,
					    cdrem, AP_DOMAINS);

	if (do_hotplug)
		vfio_ap_mdev_update_guest_apcb(matrix_mdev);
}

 
static void vfio_ap_mdev_cfg_remove(unsigned long *ap_remove,
				    unsigned long *aq_remove,
				    unsigned long *cd_remove)
{
	struct ap_matrix_mdev *matrix_mdev;
	DECLARE_BITMAP(aprem, AP_DEVICES);
	DECLARE_BITMAP(aqrem, AP_DOMAINS);
	DECLARE_BITMAP(cdrem, AP_DOMAINS);
	int do_remove = 0;

	list_for_each_entry(matrix_mdev, &matrix_dev->mdev_list, node) {
		mutex_lock(&matrix_mdev->kvm->lock);
		mutex_lock(&matrix_dev->mdevs_lock);

		do_remove |= bitmap_and(aprem, ap_remove,
					  matrix_mdev->matrix.apm,
					  AP_DEVICES);
		do_remove |= bitmap_and(aqrem, aq_remove,
					  matrix_mdev->matrix.aqm,
					  AP_DOMAINS);
		do_remove |= bitmap_andnot(cdrem, cd_remove,
					     matrix_mdev->matrix.adm,
					     AP_DOMAINS);

		if (do_remove)
			vfio_ap_mdev_hot_unplug_cfg(matrix_mdev, aprem, aqrem,
						    cdrem);

		mutex_unlock(&matrix_dev->mdevs_lock);
		mutex_unlock(&matrix_mdev->kvm->lock);
	}
}

 
static void vfio_ap_mdev_on_cfg_remove(struct ap_config_info *cur_config_info,
				       struct ap_config_info *prev_config_info)
{
	int do_remove;
	DECLARE_BITMAP(aprem, AP_DEVICES);
	DECLARE_BITMAP(aqrem, AP_DOMAINS);
	DECLARE_BITMAP(cdrem, AP_DOMAINS);

	do_remove = bitmap_andnot(aprem,
				  (unsigned long *)prev_config_info->apm,
				  (unsigned long *)cur_config_info->apm,
				  AP_DEVICES);
	do_remove |= bitmap_andnot(aqrem,
				   (unsigned long *)prev_config_info->aqm,
				   (unsigned long *)cur_config_info->aqm,
				   AP_DEVICES);
	do_remove |= bitmap_andnot(cdrem,
				   (unsigned long *)prev_config_info->adm,
				   (unsigned long *)cur_config_info->adm,
				   AP_DEVICES);

	if (do_remove)
		vfio_ap_mdev_cfg_remove(aprem, aqrem, cdrem);
}

 
static void vfio_ap_filter_apid_by_qtype(unsigned long *apm, unsigned long *aqm)
{
	bool apid_cleared;
	struct ap_queue_status status;
	unsigned long apid, apqi;
	struct ap_tapq_gr2 info;

	for_each_set_bit_inv(apid, apm, AP_DEVICES) {
		apid_cleared = false;

		for_each_set_bit_inv(apqi, aqm, AP_DOMAINS) {
			status = ap_test_queue(AP_MKQID(apid, apqi), 1, &info);
			switch (status.response_code) {
			 
			case AP_RESPONSE_NORMAL:
			case AP_RESPONSE_RESET_IN_PROGRESS:
			case AP_RESPONSE_DECONFIGURED:
			case AP_RESPONSE_CHECKSTOPPED:
			case AP_RESPONSE_BUSY:
				 
				if (info.at < AP_DEVICE_TYPE_CEX4) {
					clear_bit_inv(apid, apm);
					apid_cleared = true;
				}

				break;

			default:
				 
				clear_bit_inv(apid, apm);
				apid_cleared = true;
				break;
			}

			 
			if (apid_cleared)
				continue;
		}
	}
}

 
static void vfio_ap_mdev_cfg_add(unsigned long *apm_add, unsigned long *aqm_add,
				 unsigned long *adm_add)
{
	struct ap_matrix_mdev *matrix_mdev;

	if (list_empty(&matrix_dev->mdev_list))
		return;

	vfio_ap_filter_apid_by_qtype(apm_add, aqm_add);

	list_for_each_entry(matrix_mdev, &matrix_dev->mdev_list, node) {
		bitmap_and(matrix_mdev->apm_add,
			   matrix_mdev->matrix.apm, apm_add, AP_DEVICES);
		bitmap_and(matrix_mdev->aqm_add,
			   matrix_mdev->matrix.aqm, aqm_add, AP_DOMAINS);
		bitmap_and(matrix_mdev->adm_add,
			   matrix_mdev->matrix.adm, adm_add, AP_DEVICES);
	}
}

 
static void vfio_ap_mdev_on_cfg_add(struct ap_config_info *cur_config_info,
				    struct ap_config_info *prev_config_info)
{
	bool do_add;
	DECLARE_BITMAP(apm_add, AP_DEVICES);
	DECLARE_BITMAP(aqm_add, AP_DOMAINS);
	DECLARE_BITMAP(adm_add, AP_DOMAINS);

	do_add = bitmap_andnot(apm_add,
			       (unsigned long *)cur_config_info->apm,
			       (unsigned long *)prev_config_info->apm,
			       AP_DEVICES);
	do_add |= bitmap_andnot(aqm_add,
				(unsigned long *)cur_config_info->aqm,
				(unsigned long *)prev_config_info->aqm,
				AP_DOMAINS);
	do_add |= bitmap_andnot(adm_add,
				(unsigned long *)cur_config_info->adm,
				(unsigned long *)prev_config_info->adm,
				AP_DOMAINS);

	if (do_add)
		vfio_ap_mdev_cfg_add(apm_add, aqm_add, adm_add);
}

 
void vfio_ap_on_cfg_changed(struct ap_config_info *cur_cfg_info,
			    struct ap_config_info *prev_cfg_info)
{
	if (!cur_cfg_info || !prev_cfg_info)
		return;

	mutex_lock(&matrix_dev->guests_lock);

	vfio_ap_mdev_on_cfg_remove(cur_cfg_info, prev_cfg_info);
	vfio_ap_mdev_on_cfg_add(cur_cfg_info, prev_cfg_info);
	memcpy(&matrix_dev->info, cur_cfg_info, sizeof(*cur_cfg_info));

	mutex_unlock(&matrix_dev->guests_lock);
}

static void vfio_ap_mdev_hot_plug_cfg(struct ap_matrix_mdev *matrix_mdev)
{
	bool do_hotplug = false;
	int filter_domains = 0;
	int filter_adapters = 0;
	DECLARE_BITMAP(apm, AP_DEVICES);
	DECLARE_BITMAP(aqm, AP_DOMAINS);

	mutex_lock(&matrix_mdev->kvm->lock);
	mutex_lock(&matrix_dev->mdevs_lock);

	filter_adapters = bitmap_and(apm, matrix_mdev->matrix.apm,
				     matrix_mdev->apm_add, AP_DEVICES);
	filter_domains = bitmap_and(aqm, matrix_mdev->matrix.aqm,
				    matrix_mdev->aqm_add, AP_DOMAINS);

	if (filter_adapters && filter_domains)
		do_hotplug |= vfio_ap_mdev_filter_matrix(apm, aqm, matrix_mdev);
	else if (filter_adapters)
		do_hotplug |=
			vfio_ap_mdev_filter_matrix(apm,
						   matrix_mdev->shadow_apcb.aqm,
						   matrix_mdev);
	else
		do_hotplug |=
			vfio_ap_mdev_filter_matrix(matrix_mdev->shadow_apcb.apm,
						   aqm, matrix_mdev);

	if (bitmap_intersects(matrix_mdev->matrix.adm, matrix_mdev->adm_add,
			      AP_DOMAINS))
		do_hotplug |= vfio_ap_mdev_filter_cdoms(matrix_mdev);

	if (do_hotplug)
		vfio_ap_mdev_update_guest_apcb(matrix_mdev);

	mutex_unlock(&matrix_dev->mdevs_lock);
	mutex_unlock(&matrix_mdev->kvm->lock);
}

void vfio_ap_on_scan_complete(struct ap_config_info *new_config_info,
			      struct ap_config_info *old_config_info)
{
	struct ap_matrix_mdev *matrix_mdev;

	mutex_lock(&matrix_dev->guests_lock);

	list_for_each_entry(matrix_mdev, &matrix_dev->mdev_list, node) {
		if (bitmap_empty(matrix_mdev->apm_add, AP_DEVICES) &&
		    bitmap_empty(matrix_mdev->aqm_add, AP_DOMAINS) &&
		    bitmap_empty(matrix_mdev->adm_add, AP_DOMAINS))
			continue;

		vfio_ap_mdev_hot_plug_cfg(matrix_mdev);
		bitmap_clear(matrix_mdev->apm_add, 0, AP_DEVICES);
		bitmap_clear(matrix_mdev->aqm_add, 0, AP_DOMAINS);
		bitmap_clear(matrix_mdev->adm_add, 0, AP_DOMAINS);
	}

	mutex_unlock(&matrix_dev->guests_lock);
}
