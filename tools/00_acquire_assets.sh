#!/bin/bash
#
# Objective 0: Re-acquire Core OSS Assets
# This script downloads and extracts all necessary source code for Phase 1.
#

set -e

# --- CONFIGURATION ---
TARGET_DIR="oss_sovereignty"
DOWNLOAD_DIR="$TARGET_DIR/downloads"

# --- COLORS ---
CYAN="\033[38;5;117m"
PURPLE="\033[38;5;141m"
GREEN="\033[38;5;84m"
GREY="\033[38;5;103m"
RESET="\033[0m"

# --- DATA (URLS AND FILENAMES) ---
ASSETS=(
    "https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.6.14.tar.xz"
    "https://ftp.gnu.org/gnu/coreutils/coreutils-9.4.tar.xz"
    "https://ftp.gnu.org/gnu/bash/bash-5.2.21.tar.gz"
    "https://ftp.gnu.org/gnu/ncurses/ncurses-6.4.tar.gz"
    "https://ftp.gnu.org/gnu/readline/readline-8.2.tar.gz"
    "https://cdn.openbsd.org/pub/OpenBSD/OpenSSH/portable/openssh-9.6p1.tar.gz"
    "https://github.com/openzfs/zfs/releases/download/zfs-2.2.2/zfs-2.2.2.tar.gz"
)

# --- SCRIPT LOGIC ---
echo -e "${PURPLE}==========================================${RESET}"
echo -e "${CYAN}   AnvilOS Asset Acquisition Subsystem${RESET}"
echo -e "${PURPLE}==========================================${RESET}"

# 1. Create directories
mkdir -p "$TARGET_DIR"
mkdir -p "$DOWNLOAD_DIR"
echo -e "${GREEN}[OK] Created directory structure in '$TARGET_DIR'${RESET}"

# 2. Download, Extract, and Clean
for url in "${ASSETS[@]}"; do
    filename=$(basename "$url")
    # dirname is filename without extension e.g. linux-6.6.14
    dirname=$(basename "$filename" .tar.xz)
    dirname=$(basename "$dirname" .tar.gz)
    
    echo -e "${PURPLE}>>> Processing: ${CYAN}$dirname${RESET}"

    # Download
    if [ ! -f "$DOWNLOAD_DIR/$filename" ]; then
        echo -e "${GREY}  -> Downloading...${RESET}"
        wget -q -P "$DOWNLOAD_DIR" "$url"
    else
        echo -e "${GREY}  -> Already downloaded.${RESET}"
    fi

    # Extract
    if [ ! -d "$TARGET_DIR/$dirname" ]; then
        echo -e "${GREY}  -> Extracting...${RESET}"
        tar -xf "$DOWNLOAD_DIR/$filename" -C "$TARGET_DIR"
        # Special case for openssh which extracts to openssh-9.6p1
        if [ "$dirname" == "openssh-9.6p1" ] && [ ! -d "$TARGET_DIR/openssh-9.6p1" ] && [ -d "$TARGET_DIR/openssh-9.6p1" ]; then
            mv "$TARGET_DIR/openssh-9.6p1" "$TARGET_DIR/openssh-9.6p1"
        fi
    else
        echo -e "${GREY}  -> Already extracted.${RESET}"
    fi

    # Decouple (Remove .git if it's a git clone in a tarball)
    if [ -d "$TARGET_DIR/$dirname/.git" ]; then
        echo -e "${GREY}  -> Decoupling from git history...${RESET}"
        rm -rf "$TARGET_DIR/$dirname/.git"
    fi

    echo -e "${GREEN}  -> Done.${RESET}"
done

echo -e "${PURPLE}==========================================${RESET}"
echo -e "${CYAN}   Asset Acquisition Complete.${RESET}"
echo -e "${PURPLE}==========================================${RESET}"
