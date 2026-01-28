#ifndef __RADEON_EVERGREEN_H__
#define __RADEON_EVERGREEN_H__
struct evergreen_mc_save;
struct evergreen_power_info;
struct radeon_device;
bool evergreen_is_display_hung(struct radeon_device *rdev);
void evergreen_print_gpu_status_regs(struct radeon_device *rdev);
void evergreen_mc_stop(struct radeon_device *rdev, struct evergreen_mc_save *save);
void evergreen_mc_resume(struct radeon_device *rdev, struct evergreen_mc_save *save);
int evergreen_mc_wait_for_idle(struct radeon_device *rdev);
void evergreen_mc_program(struct radeon_device *rdev);
void evergreen_irq_suspend(struct radeon_device *rdev);
int evergreen_mc_init(struct radeon_device *rdev);
void evergreen_fix_pci_max_read_req_size(struct radeon_device *rdev);
void evergreen_pcie_gen2_enable(struct radeon_device *rdev);
void evergreen_program_aspm(struct radeon_device *rdev);
void sumo_rlc_fini(struct radeon_device *rdev);
int sumo_rlc_init(struct radeon_device *rdev);
void evergreen_gpu_pci_config_reset(struct radeon_device *rdev);
u32 evergreen_get_number_of_dram_channels(struct radeon_device *rdev);
u32 evergreen_gpu_check_soft_reset(struct radeon_device *rdev);
int evergreen_rlc_resume(struct radeon_device *rdev);
struct evergreen_power_info *evergreen_get_pi(struct radeon_device *rdev);
#endif				 
