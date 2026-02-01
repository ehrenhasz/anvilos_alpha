
 

#include <linux/export.h>
#include <linux/irq.h>

#include "msi.h"

 
int pci_enable_msi(struct pci_dev *dev)
{
	int rc = __pci_enable_msi_range(dev, 1, 1, NULL);
	if (rc < 0)
		return rc;
	return 0;
}
EXPORT_SYMBOL(pci_enable_msi);

 
void pci_disable_msi(struct pci_dev *dev)
{
	if (!pci_msi_enabled() || !dev || !dev->msi_enabled)
		return;

	msi_lock_descs(&dev->dev);
	pci_msi_shutdown(dev);
	pci_free_msi_irqs(dev);
	msi_unlock_descs(&dev->dev);
}
EXPORT_SYMBOL(pci_disable_msi);

 
int pci_msix_vec_count(struct pci_dev *dev)
{
	u16 control;

	if (!dev->msix_cap)
		return -EINVAL;

	pci_read_config_word(dev, dev->msix_cap + PCI_MSIX_FLAGS, &control);
	return msix_table_size(control);
}
EXPORT_SYMBOL(pci_msix_vec_count);

 
int pci_enable_msix_range(struct pci_dev *dev, struct msix_entry *entries,
			  int minvec, int maxvec)
{
	return __pci_enable_msix_range(dev, entries, minvec, maxvec, NULL, 0);
}
EXPORT_SYMBOL(pci_enable_msix_range);

 
bool pci_msix_can_alloc_dyn(struct pci_dev *dev)
{
	if (!dev->msix_cap)
		return false;

	return pci_msi_domain_supports(dev, MSI_FLAG_PCI_MSIX_ALLOC_DYN, DENY_LEGACY);
}
EXPORT_SYMBOL_GPL(pci_msix_can_alloc_dyn);

 
struct msi_map pci_msix_alloc_irq_at(struct pci_dev *dev, unsigned int index,
				     const struct irq_affinity_desc *affdesc)
{
	struct msi_map map = { .index = -ENOTSUPP };

	if (!dev->msix_enabled)
		return map;

	if (!pci_msix_can_alloc_dyn(dev))
		return map;

	return msi_domain_alloc_irq_at(&dev->dev, MSI_DEFAULT_DOMAIN, index, affdesc, NULL);
}
EXPORT_SYMBOL_GPL(pci_msix_alloc_irq_at);

 
void pci_msix_free_irq(struct pci_dev *dev, struct msi_map map)
{
	if (WARN_ON_ONCE(map.index < 0 || map.virq <= 0))
		return;
	if (WARN_ON_ONCE(!pci_msix_can_alloc_dyn(dev)))
		return;
	msi_domain_free_irqs_range(&dev->dev, MSI_DEFAULT_DOMAIN, map.index, map.index);
}
EXPORT_SYMBOL_GPL(pci_msix_free_irq);

 
void pci_disable_msix(struct pci_dev *dev)
{
	if (!pci_msi_enabled() || !dev || !dev->msix_enabled)
		return;

	msi_lock_descs(&dev->dev);
	pci_msix_shutdown(dev);
	pci_free_msi_irqs(dev);
	msi_unlock_descs(&dev->dev);
}
EXPORT_SYMBOL(pci_disable_msix);

 
int pci_alloc_irq_vectors(struct pci_dev *dev, unsigned int min_vecs,
			  unsigned int max_vecs, unsigned int flags)
{
	return pci_alloc_irq_vectors_affinity(dev, min_vecs, max_vecs,
					      flags, NULL);
}
EXPORT_SYMBOL(pci_alloc_irq_vectors);

 
int pci_alloc_irq_vectors_affinity(struct pci_dev *dev, unsigned int min_vecs,
				   unsigned int max_vecs, unsigned int flags,
				   struct irq_affinity *affd)
{
	struct irq_affinity msi_default_affd = {0};
	int nvecs = -ENOSPC;

	if (flags & PCI_IRQ_AFFINITY) {
		if (!affd)
			affd = &msi_default_affd;
	} else {
		if (WARN_ON(affd))
			affd = NULL;
	}

	if (flags & PCI_IRQ_MSIX) {
		nvecs = __pci_enable_msix_range(dev, NULL, min_vecs, max_vecs,
						affd, flags);
		if (nvecs > 0)
			return nvecs;
	}

	if (flags & PCI_IRQ_MSI) {
		nvecs = __pci_enable_msi_range(dev, min_vecs, max_vecs, affd);
		if (nvecs > 0)
			return nvecs;
	}

	 
	if (flags & PCI_IRQ_LEGACY) {
		if (min_vecs == 1 && dev->irq) {
			 
			if (affd)
				irq_create_affinity_masks(1, affd);
			pci_intx(dev, 1);
			return 1;
		}
	}

	return nvecs;
}
EXPORT_SYMBOL(pci_alloc_irq_vectors_affinity);

 
int pci_irq_vector(struct pci_dev *dev, unsigned int nr)
{
	unsigned int irq;

	if (!dev->msi_enabled && !dev->msix_enabled)
		return !nr ? dev->irq : -EINVAL;

	irq = msi_get_virq(&dev->dev, nr);
	return irq ? irq : -EINVAL;
}
EXPORT_SYMBOL(pci_irq_vector);

 
const struct cpumask *pci_irq_get_affinity(struct pci_dev *dev, int nr)
{
	int idx, irq = pci_irq_vector(dev, nr);
	struct msi_desc *desc;

	if (WARN_ON_ONCE(irq <= 0))
		return NULL;

	desc = irq_get_msi_desc(irq);
	 
	if (!desc)
		return cpu_possible_mask;

	 
	if (!desc->affinity)
		return NULL;

	 
	idx = dev->msi_enabled ? nr : 0;
	return &desc->affinity[idx].mask;
}
EXPORT_SYMBOL(pci_irq_get_affinity);

 
struct msi_map pci_ims_alloc_irq(struct pci_dev *dev, union msi_instance_cookie *icookie,
				 const struct irq_affinity_desc *affdesc)
{
	return msi_domain_alloc_irq_at(&dev->dev, MSI_SECONDARY_DOMAIN, MSI_ANY_INDEX,
				       affdesc, icookie);
}
EXPORT_SYMBOL_GPL(pci_ims_alloc_irq);

 
void pci_ims_free_irq(struct pci_dev *dev, struct msi_map map)
{
	if (WARN_ON_ONCE(map.index < 0 || map.virq <= 0))
		return;
	msi_domain_free_irqs_range(&dev->dev, MSI_SECONDARY_DOMAIN, map.index, map.index);
}
EXPORT_SYMBOL_GPL(pci_ims_free_irq);

 
void pci_free_irq_vectors(struct pci_dev *dev)
{
	pci_disable_msix(dev);
	pci_disable_msi(dev);
}
EXPORT_SYMBOL(pci_free_irq_vectors);

 
void pci_restore_msi_state(struct pci_dev *dev)
{
	__pci_restore_msi_state(dev);
	__pci_restore_msix_state(dev);
}
EXPORT_SYMBOL_GPL(pci_restore_msi_state);

 
int pci_msi_enabled(void)
{
	return pci_msi_enable;
}
EXPORT_SYMBOL(pci_msi_enabled);
