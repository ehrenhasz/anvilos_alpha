#!/bin/sh

# Directories
export BIN_DIR=/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/tests/zfs-tests/bin
export SBIN_DIR=/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2
export LIBEXEC_DIR=/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2
export ZTS_DIR=/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/tests
export SCRIPT_DIR=/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/scripts

# General commands
export ZDB="${ZDB:-$SBIN_DIR/zdb}"
export ZFS="${ZFS:-$SBIN_DIR/zfs}"
export ZPOOL="${ZPOOL:-$SBIN_DIR/zpool}"
export ZTEST="${ZTEST:-$SBIN_DIR/ztest}"
export ZFS_SH="${ZFS_SH:-$SCRIPT_DIR/zfs.sh}"

# Test Suite
export RUNFILE_DIR="${RUNFILE_DIR:-$ZTS_DIR/runfiles}"
export TEST_RUNNER="${TEST_RUNNER:-$ZTS_DIR/test-runner/bin/test-runner.py}"
export ZTS_REPORT="${ZTS_REPORT:-$ZTS_DIR/test-runner/bin/zts-report.py}"
export STF_TOOLS="${STF_TOOLS:-$ZTS_DIR/test-runner}"
export STF_SUITE="${STF_SUITE:-$ZTS_DIR/zfs-tests}"

# Only required for in-tree use
export INTREE="yes"
export GDB="libtool --mode=execute gdb"
export LDMOD=/sbin/insmod

export CMD_DIR=/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2
export UDEV_SCRIPT_DIR=/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/udev
export UDEV_CMD_DIR=/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/udev
export UDEV_RULE_DIR=/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/udev/rules.d
export ZEDLET_ETC_DIR=$CMD_DIR/cmd/zed/zed.d
export ZEDLET_LIBEXEC_DIR=$CMD_DIR/cmd/zed/zed.d
export ZPOOL_SCRIPT_DIR=$CMD_DIR/cmd/zpool/zpool.d
export ZPOOL_SCRIPTS_PATH=$CMD_DIR/cmd/zpool/zpool.d
export ZPOOL_COMPAT_DIR=$CMD_DIR/cmd/zpool/compatibility.d
export CONTRIB_DIR=/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/contrib
export LIB_DIR=/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/.libs
export SYSCONF_DIR=/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/etc

export INSTALL_UDEV_DIR=/lib/udev
export INSTALL_UDEV_RULE_DIR=/lib/udev/rules.d
export INSTALL_MOUNT_HELPER_DIR=/sbin
export INSTALL_SYSCONF_DIR=/etc
export INSTALL_PYTHON_DIR=

export KMOD_SPL=/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/spl.ko
export KMOD_ZFS=/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/zfs.ko
export KMOD_FREEBSD=/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/openzfs.ko
