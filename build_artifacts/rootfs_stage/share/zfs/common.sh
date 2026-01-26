#!/bin/sh

# Directories
export BIN_DIR=//bin
export SBIN_DIR=/sbin
export LIBEXEC_DIR=//libexec/zfs
export ZTS_DIR=//share/zfs
export SCRIPT_DIR=//share/zfs

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
