import os
def build_kernel():
    print(">> ANVIL_CHUNK: KERNEL_CONFIG")
    os.system("make -C oss_sovereignty/sys_01_Linux_Kernel/source x86_64_defconfig")
    print(">> ANVIL_CHUNK: DRIVER_INJECTION")
    drivers = ["CONFIG_IWLWIFI", "CONFIG_DRM_I915", "CONFIG_NVME_CORE", "CONFIG_VIRTIO_PCI"]
    for d in drivers:
        os.system(f"./oss_sovereignty/sys_01_Linux_Kernel/source/scripts/config --enable {d}")
if __name__ == "__main__":
    build_kernel()
