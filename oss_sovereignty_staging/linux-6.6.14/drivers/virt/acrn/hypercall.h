 
 
#ifndef __ACRN_HSM_HYPERCALL_H
#define __ACRN_HSM_HYPERCALL_H
#include <asm/acrn.h>

 
#define _HC_ID(x, y) (((x) << 24) | (y))

#define HC_ID 0x80UL

#define HC_ID_GEN_BASE			0x0UL
#define HC_SOS_REMOVE_CPU		_HC_ID(HC_ID, HC_ID_GEN_BASE + 0x01)

#define HC_ID_VM_BASE			0x10UL
#define HC_CREATE_VM			_HC_ID(HC_ID, HC_ID_VM_BASE + 0x00)
#define HC_DESTROY_VM			_HC_ID(HC_ID, HC_ID_VM_BASE + 0x01)
#define HC_START_VM			_HC_ID(HC_ID, HC_ID_VM_BASE + 0x02)
#define HC_PAUSE_VM			_HC_ID(HC_ID, HC_ID_VM_BASE + 0x03)
#define HC_RESET_VM			_HC_ID(HC_ID, HC_ID_VM_BASE + 0x05)
#define HC_SET_VCPU_REGS		_HC_ID(HC_ID, HC_ID_VM_BASE + 0x06)

#define HC_ID_IRQ_BASE			0x20UL
#define HC_INJECT_MSI			_HC_ID(HC_ID, HC_ID_IRQ_BASE + 0x03)
#define HC_VM_INTR_MONITOR		_HC_ID(HC_ID, HC_ID_IRQ_BASE + 0x04)
#define HC_SET_IRQLINE			_HC_ID(HC_ID, HC_ID_IRQ_BASE + 0x05)

#define HC_ID_IOREQ_BASE		0x30UL
#define HC_SET_IOREQ_BUFFER		_HC_ID(HC_ID, HC_ID_IOREQ_BASE + 0x00)
#define HC_NOTIFY_REQUEST_FINISH	_HC_ID(HC_ID, HC_ID_IOREQ_BASE + 0x01)

#define HC_ID_MEM_BASE			0x40UL
#define HC_VM_SET_MEMORY_REGIONS	_HC_ID(HC_ID, HC_ID_MEM_BASE + 0x02)

#define HC_ID_PCI_BASE			0x50UL
#define HC_SET_PTDEV_INTR		_HC_ID(HC_ID, HC_ID_PCI_BASE + 0x03)
#define HC_RESET_PTDEV_INTR		_HC_ID(HC_ID, HC_ID_PCI_BASE + 0x04)
#define HC_ASSIGN_PCIDEV		_HC_ID(HC_ID, HC_ID_PCI_BASE + 0x05)
#define HC_DEASSIGN_PCIDEV		_HC_ID(HC_ID, HC_ID_PCI_BASE + 0x06)
#define HC_ASSIGN_MMIODEV		_HC_ID(HC_ID, HC_ID_PCI_BASE + 0x07)
#define HC_DEASSIGN_MMIODEV		_HC_ID(HC_ID, HC_ID_PCI_BASE + 0x08)
#define HC_CREATE_VDEV			_HC_ID(HC_ID, HC_ID_PCI_BASE + 0x09)
#define HC_DESTROY_VDEV			_HC_ID(HC_ID, HC_ID_PCI_BASE + 0x0A)

#define HC_ID_PM_BASE			0x80UL
#define HC_PM_GET_CPU_STATE		_HC_ID(HC_ID, HC_ID_PM_BASE + 0x00)

 
static inline long hcall_sos_remove_cpu(u64 cpu)
{
	return acrn_hypercall1(HC_SOS_REMOVE_CPU, cpu);
}

 
static inline long hcall_create_vm(u64 vminfo)
{
	return acrn_hypercall1(HC_CREATE_VM, vminfo);
}

 
static inline long hcall_start_vm(u64 vmid)
{
	return acrn_hypercall1(HC_START_VM, vmid);
}

 
static inline long hcall_pause_vm(u64 vmid)
{
	return acrn_hypercall1(HC_PAUSE_VM, vmid);
}

 
static inline long hcall_destroy_vm(u64 vmid)
{
	return acrn_hypercall1(HC_DESTROY_VM, vmid);
}

 
static inline long hcall_reset_vm(u64 vmid)
{
	return acrn_hypercall1(HC_RESET_VM, vmid);
}

 
static inline long hcall_set_vcpu_regs(u64 vmid, u64 regs_state)
{
	return acrn_hypercall2(HC_SET_VCPU_REGS, vmid, regs_state);
}

 
static inline long hcall_inject_msi(u64 vmid, u64 msi)
{
	return acrn_hypercall2(HC_INJECT_MSI, vmid, msi);
}

 
static inline long hcall_vm_intr_monitor(u64 vmid, u64 addr)
{
	return acrn_hypercall2(HC_VM_INTR_MONITOR, vmid, addr);
}

 
static inline long hcall_set_irqline(u64 vmid, u64 op)
{
	return acrn_hypercall2(HC_SET_IRQLINE, vmid, op);
}

 
static inline long hcall_set_ioreq_buffer(u64 vmid, u64 buffer)
{
	return acrn_hypercall2(HC_SET_IOREQ_BUFFER, vmid, buffer);
}

 
static inline long hcall_notify_req_finish(u64 vmid, u64 vcpu)
{
	return acrn_hypercall2(HC_NOTIFY_REQUEST_FINISH, vmid, vcpu);
}

 
static inline long hcall_set_memory_regions(u64 regions_pa)
{
	return acrn_hypercall1(HC_VM_SET_MEMORY_REGIONS, regions_pa);
}

 
static inline long hcall_create_vdev(u64 vmid, u64 addr)
{
	return acrn_hypercall2(HC_CREATE_VDEV, vmid, addr);
}

 
static inline long hcall_destroy_vdev(u64 vmid, u64 addr)
{
	return acrn_hypercall2(HC_DESTROY_VDEV, vmid, addr);
}

 
static inline long hcall_assign_mmiodev(u64 vmid, u64 addr)
{
	return acrn_hypercall2(HC_ASSIGN_MMIODEV, vmid, addr);
}

 
static inline long hcall_deassign_mmiodev(u64 vmid, u64 addr)
{
	return acrn_hypercall2(HC_DEASSIGN_MMIODEV, vmid, addr);
}

 
static inline long hcall_assign_pcidev(u64 vmid, u64 addr)
{
	return acrn_hypercall2(HC_ASSIGN_PCIDEV, vmid, addr);
}

 
static inline long hcall_deassign_pcidev(u64 vmid, u64 addr)
{
	return acrn_hypercall2(HC_DEASSIGN_PCIDEV, vmid, addr);
}

 
static inline long hcall_set_ptdev_intr(u64 vmid, u64 irq)
{
	return acrn_hypercall2(HC_SET_PTDEV_INTR, vmid, irq);
}

 
static inline long hcall_reset_ptdev_intr(u64 vmid, u64 irq)
{
	return acrn_hypercall2(HC_RESET_PTDEV_INTR, vmid, irq);
}

 
static inline long hcall_get_cpu_state(u64 cmd, u64 state)
{
	return acrn_hypercall2(HC_PM_GET_CPU_STATE, cmd, state);
}

#endif  
