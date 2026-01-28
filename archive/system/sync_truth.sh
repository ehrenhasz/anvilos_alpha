#!/bin/bash

# 1. Clear content of 'oss_sovereignty/os_01_Linux_Kernel'
rm -rf oss_sovereignty/os_01_Linux_Kernel/*
rm -rf oss_sovereignty/os_01_Linux_Kernel/.*

# 2. Find official git repo for 'os_01_Linux_Kernel'
# Assuming the official repo is the torvalds/linux repo.
# You may need to update this if there's a different official source.
LINUX_KERNEL_REPO="https://github.com/torvalds/linux.git"
TARGET_DIR="oss_sovereignty/os_01_Linux_Kernel"

# 3. Clone repo into 'oss_sovereignty/os_01_Linux_Kernel'
git clone "" ""

# 4. Remove '.git' folder to disconnect
rm -rf "/.git"

echo "Linux Kernel cloned and .git removed successfully."
