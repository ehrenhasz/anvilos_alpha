#!/bin/bash

# Define the target directory
TARGET_DIR="oss_sovereignty/bld_01_GCC"

# Ensure the parent directory exists
mkdir -p oss_sovereignty

# Clone the GCC repository
git clone git://gcc.gnu.org/git/gcc.git "$TARGET_DIR"

# Remove the .git directory to sever the Git connection
rm -rf "$TARGET_DIR/.git"

echo ">> GCC cloned and Git history removed from $TARGET_DIR"
