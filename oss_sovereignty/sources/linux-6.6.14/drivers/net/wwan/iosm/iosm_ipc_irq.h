

#ifndef IOSM_IPC_IRQ_H
#define IOSM_IPC_IRQ_H

struct iosm_pcie;


void ipc_doorbell_fire(struct iosm_pcie *ipc_pcie, int irq_n, u32 data);


void ipc_release_irq(struct iosm_pcie *ipc_pcie);


int ipc_acquire_irq(struct iosm_pcie *ipc_pcie);

#endif
