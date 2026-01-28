"""
Important `libzfs_core` constants.
"""
from __future__ import absolute_import, division, print_function
import errno
import sys
if sys.platform.startswith('freebsd'):
    ECHRNG = errno.ENXIO
    ECKSUM = 97  # EINTEGRITY
    ETIME = errno.ETIMEDOUT
else:
    ECHRNG = errno.ECHRNG
    ECKSUM = errno.EBADE
    ETIME = errno.ETIME
def enum_with_offset(offset, sequential, named):
    enums = dict(((b, a + offset) for a, b in enumerate(sequential)), **named)
    return type('Enum', (), enums)
def enum(*sequential, **named):
    return enum_with_offset(0, sequential, named)
MAXNAMELEN = 255
ZCP_DEFAULT_INSTRLIMIT = 10 * 1000 * 1000
ZCP_DEFAULT_MEMLIMIT = 10 * 1024 * 1024
WRAPPING_KEY_LEN = 32
zfs_key_location = enum(
    'ZFS_KEYLOCATION_NONE',
    'ZFS_KEYLOCATION_PROMPT',
    'ZFS_KEYLOCATION_URI'
)
zfs_keyformat = enum(
    'ZFS_KEYFORMAT_NONE',
    'ZFS_KEYFORMAT_RAW',
    'ZFS_KEYFORMAT_HEX',
    'ZFS_KEYFORMAT_PASSPHRASE'
)
zio_encrypt = enum(
    'ZIO_CRYPT_INHERIT',
    'ZIO_CRYPT_ON',
    'ZIO_CRYPT_OFF',
    'ZIO_CRYPT_AES_128_CCM',
    'ZIO_CRYPT_AES_192_CCM',
    'ZIO_CRYPT_AES_256_CCM',
    'ZIO_CRYPT_AES_128_GCM',
    'ZIO_CRYPT_AES_192_GCM',
    'ZIO_CRYPT_AES_256_GCM'
)
zfs_errno = enum_with_offset(1024, [
        'ZFS_ERR_CHECKPOINT_EXISTS',
        'ZFS_ERR_DISCARDING_CHECKPOINT',
        'ZFS_ERR_NO_CHECKPOINT',
        'ZFS_ERR_DEVRM_IN_PROGRESS',
        'ZFS_ERR_VDEV_TOO_BIG',
        'ZFS_ERR_IOC_CMD_UNAVAIL',
        'ZFS_ERR_IOC_ARG_UNAVAIL',
        'ZFS_ERR_IOC_ARG_REQUIRED',
        'ZFS_ERR_IOC_ARG_BADTYPE',
        'ZFS_ERR_WRONG_PARENT',
        'ZFS_ERR_FROM_IVSET_GUID_MISSING',
        'ZFS_ERR_FROM_IVSET_GUID_MISMATCH',
        'ZFS_ERR_SPILL_BLOCK_FLAG_MISSING',
        'ZFS_ERR_UNKNOWN_SEND_STREAM_FEATURE',
        'ZFS_ERR_EXPORT_IN_PROGRESS',
        'ZFS_ERR_BOOKMARK_SOURCE_NOT_ANCESTOR',
        'ZFS_ERR_STREAM_TRUNCATED',
        'ZFS_ERR_STREAM_LARGE_BLOCK_MISMATCH',
        'ZFS_ERR_RESILVER_IN_PROGRESS',
        'ZFS_ERR_REBUILD_IN_PROGRESS',
        'ZFS_ERR_BADPROP',
        'ZFS_ERR_VDEV_NOTSUP',
        'ZFS_ERR_NOT_USER_NAMESPACE',
        'ZFS_ERR_RESUME_EXISTS',
        'ZFS_ERR_CRYPTO_NOTSUP',
    ],
    {}
)
ZFS_ERR_CHECKPOINT_EXISTS = zfs_errno.ZFS_ERR_CHECKPOINT_EXISTS
assert (ZFS_ERR_CHECKPOINT_EXISTS == 1024)
ZFS_ERR_DISCARDING_CHECKPOINT = zfs_errno.ZFS_ERR_DISCARDING_CHECKPOINT
ZFS_ERR_NO_CHECKPOINT = zfs_errno.ZFS_ERR_NO_CHECKPOINT
ZFS_ERR_DEVRM_IN_PROGRESS = zfs_errno.ZFS_ERR_DEVRM_IN_PROGRESS
ZFS_ERR_VDEV_TOO_BIG = zfs_errno.ZFS_ERR_VDEV_TOO_BIG
ZFS_ERR_WRONG_PARENT = zfs_errno.ZFS_ERR_WRONG_PARENT
ZFS_ERR_VDEV_NOTSUP = zfs_errno.ZFS_ERR_VDEV_NOTSUP
