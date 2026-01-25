#!/bin/bash
# ------------------------------------------------------------------
# [ANVIL] COLONIZATION SCRIPT v2 (SMART NVMe)
# ------------------------------------------------------------------
set -e
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

log() { echo -e "${GREEN}>> [INSTALL] $1${NC}"; }
err() { echo -e "${RED}>> [FATAL] $1${NC}"; }

if [ "$EUID" -ne 0 ]; then err "RUN AS SUDO."; exit 1; fi

log "SCANNING FOR LARGEST DRIVE..."
TARGET_DISK=$(lsblk -dn -o NAME,SIZE,TYPE | grep disk | sort -h -k2 | tail -1 | awk '{print "/dev/" $1}')

if [ -z "$TARGET_DISK" ]; then err "NO DISK FOUND."; exit 1; fi

if [[ "$TARGET_DISK" == *"nvme"* ]]; then PART_PREFIX="${TARGET_DISK}p"; else PART_PREFIX="${TARGET_DISK}"; fi

echo -e "${RED}------------------------------------------------"
echo -e "TARGET LOCK: $TARGET_DISK"
echo -e "WARNING: ALL DATA ON THIS DRIVE WILL BE INCINERATED."
echo -e "------------------------------------------------${NC}"
read -p "Type 'DESTROY' to execute: " CONFIRM
if [ "$CONFIRM" != "DESTROY" ]; then err "ABORTED."; exit 1; fi

log "PARTITIONING $TARGET_DISK..."
sgdisk -Z $TARGET_DISK || true
parted -s $TARGET_DISK mklabel gpt
parted -s $TARGET_DISK mkpart ESP fat32 1MiB 513MiB
parted -s $TARGET_DISK set 1 boot on
parted -s $TARGET_DISK mkpart primary ext4 513MiB 100%
partprobe $TARGET_DISK
sleep 2

EFI_PART="${PART_PREFIX}1"
ROOT_PART="${PART_PREFIX}2"

log "FORMATTING..."
mkfs.vfat -F32 $EFI_PART
mkfs.ext4 -F -L "ANVIL_ROOT" $ROOT_PART

log "MOUNTING..."
mkdir -p /mnt/target
mount $ROOT_PART /mnt/target
mkdir -p /mnt/target/boot/efi
mount $EFI_PART /mnt/target/boot/efi

log "CLONING SYSTEM..."
rsync -aAX --exclude={"/dev/*","/proc/*","/sys/*","/tmp/*","/run/*","/mnt/*","/media/*","/lost+found","/cdrom/*"} / /mnt/target

log "INSTALLING BOOTLOADER..."
ROOT_UUID=$(blkid -s UUID -o value $ROOT_PART)
EFI_UUID=$(blkid -s UUID -o value $EFI_PART)
cat << FSTAB > /mnt/target/etc/fstab
UUID=$ROOT_UUID  /          ext4    errors=remount-ro 0       1
UUID=$EFI_UUID   /boot/efi  vfat    umask=0077      0       1
FSTAB

mount --bind /dev /mnt/target/dev
mount --bind /proc /mnt/target/proc
mount --bind /sys /mnt/target/sys
mount --bind /sys/firmware/efi/efivars /mnt/target/sys/firmware/efi/efivars || true

chroot /mnt/target /bin/bash << EOCHROOT
    grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=ANVIL --recheck --removable
    update-grub
EOCHROOT

umount -R /mnt/target
log "INSTALLATION COMPLETE. REMOVE USB AND REBOOT."
EOF

chmod +x install_to_disk.sh
