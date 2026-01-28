rm -rf oss_sovereignty/os_01_Linux_Kernel/*
rm -rf oss_sovereignty/os_01_Linux_Kernel/.*
LINUX_KERNEL_REPO="https://github.com/torvalds/linux.git"
TARGET_DIR="oss_sovereignty/os_01_Linux_Kernel"
git clone "" ""
rm -rf "/.git"
echo "Linux Kernel cloned and .git removed successfully."
