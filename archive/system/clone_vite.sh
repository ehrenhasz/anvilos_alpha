#!/bin/bash
# Script to clone the vite repository and sever git ties.
set -e

# Define the destination directory
DEST_DIR="oss_sovereignty/ctx_07_vite"

# Ensure the destination directory exists
mkdir -p oss_sovereignty

# Clone the repository
git clone https://github.com/vitejs/vite.git "$DEST_DIR"

# Remove the .git directory to sever ties
rm -rf "$DEST_DIR/.git"

echo "Vite repository cloned and git ties severed in $DEST_DIR"
