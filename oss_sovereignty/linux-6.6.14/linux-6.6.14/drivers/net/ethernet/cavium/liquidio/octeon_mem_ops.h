#ifndef __OCTEON_MEM_OPS_H__
#define __OCTEON_MEM_OPS_H__
u64 octeon_read_device_mem64(struct octeon_device *oct, u64 core_addr);
u32 octeon_read_device_mem32(struct octeon_device *oct, u64 core_addr);
void
octeon_write_device_mem32(struct octeon_device *oct,
			  u64 core_addr,
			  u32 val);
void
octeon_pci_read_core_mem(struct octeon_device *oct,
			 u64 coreaddr,
			 u8 *buf,
			 u32 len);
void
octeon_pci_write_core_mem(struct octeon_device *oct,
			  u64 coreaddr,
			  const u8 *buf,
			  u32 len);
#endif
