#!/bin/bash

# Task: Clone git://gcc.gnu.org/git/gcc.git into oss_sovereignty/bld_01_GCC, then sever git ties (rm -rf .git).

# Set the target directory
TARGET_DIR="oss_sovereignty/bld_01_GCC"

# Create the target directory if it doesn't exist
mkdir -p "$TARGET_DIR"

# Clone the GCC repository
git clone git://gcc.gnu.org/git/gcc.git "$TARGET_DIR"

# Remove the .git directory to sever the git ties
rm -rf "$TARGET_DIR/.git"

echo ">> GCC repository cloned and git history removed from $TARGET_DIR"
