# anvil_kernel_builder.py
# Compiled for the Anvil VM to orchestrate the kernel build.
import os

def build_kernel():
    # Micro-Chunk 1: Configuration
    print(">> ANVIL_CHUNK: KERNEL_CONFIG")
    # In Anvil, we avoid shell if possible, but for the legacy kernel build, 
    # we bridge using the Anvil-Shell provider.
    os.system("make -C oss_sovereignty/sys_01_Linux_Kernel/source x86_64_defconfig")
    
    # Micro-Chunk 2: Driver Injection
    print(">> ANVIL_CHUNK: DRIVER_INJECTION")
    drivers = ["CONFIG_IWLWIFI", "CONFIG_DRM_I915", "CONFIG_NVME_CORE", "CONFIG_VIRTIO_PCI"]
    for d in drivers:
        os.system(f"./oss_sovereignty/sys_01_Linux_Kernel/source/scripts/config --enable {d}")

if __name__ == "__main__":
    build_kernel()
