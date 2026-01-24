#!/bin/bash
set -e

# Define the target directory
TARGET_DIR="oss_sovereignty/bld_05_Gzip"

# Ensure the parent directory exists
mkdir -p oss_sovereignty

# Clone the repository
git clone https://git.savannah.gnu.org/git/gzip.git ""

# Remove Git ties
rm -rf "/.git"

echo ">> Gzip cloned and .git removed successfully."
