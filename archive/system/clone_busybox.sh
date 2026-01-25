#!/bin/bash

# Define the target directory
TARGET_DIR="oss_sovereignty/os_02_BusyBox"

# Create the target directory if it doesn't exist
mkdir -p ""

# Clone the BusyBox repository
git clone https://git.busybox.net/busybox ""

# Remove the .git directory to sever ties
rm -rf "/.git"

echo "BusyBox cloned and .git removed successfully!"
