
#ifndef MICROPY_INCLUDED_STM32_MBOOT_VFS_H
#define MICROPY_INCLUDED_STM32_MBOOT_VFS_H

#include "gzstream.h"
#include "mboot.h"

#if MBOOT_VFS_FAT

#include "lib/oofatfs/ff.h"

typedef struct _vfs_fat_context_t {
    mboot_addr_t bdev_base_addr;
    uint32_t bdev_num_blocks;
    FATFS fatfs;
    FIL fp;
} vfs_fat_context_t;

extern const stream_methods_t vfs_fat_stream_methods;

int vfs_fat_mount(vfs_fat_context_t *ctx, mboot_addr_t base_addr, mboot_addr_t byte_len);

#endif

#if MBOOT_VFS_LFS1

#include "lib/littlefs/lfs1.h"

#define LFS_READ_SIZE (32)
#define LFS_PROG_SIZE (32)
#define LFS_LOOKAHEAD_SIZE (32)

typedef struct _vfs_lfs1_context_t {
    mboot_addr_t bdev_base_addr;
    struct lfs1_config config;
    lfs1_t lfs;
    struct lfs1_file_config filecfg;
    uint8_t filebuf[LFS_PROG_SIZE];
    lfs1_file_t file;
} vfs_lfs1_context_t;

extern const stream_methods_t vfs_lfs1_stream_methods;

int vfs_lfs1_mount(vfs_lfs1_context_t *ctx, mboot_addr_t base_addr, mboot_addr_t byte_len, uint32_t block_size);

#endif

#if MBOOT_VFS_LFS2

#include "lib/littlefs/lfs2.h"

#define LFS_READ_SIZE (32)
#define LFS_PROG_SIZE (32)
#define LFS_CACHE_SIZE (4 * LFS_READ_SIZE)
#define LFS_LOOKAHEAD_SIZE (32)

typedef struct _vfs_lfs2_context_t {
    mboot_addr_t bdev_base_addr;
    struct lfs2_config config;
    lfs2_t lfs;
    struct lfs2_file_config filecfg;
    uint8_t filebuf[LFS_CACHE_SIZE]; 
    lfs2_file_t file;
} vfs_lfs2_context_t;

extern const stream_methods_t vfs_lfs2_stream_methods;

int vfs_lfs2_mount(vfs_lfs2_context_t *ctx, mboot_addr_t base_addr, mboot_addr_t byte_len, uint32_t block_size);

#endif

#if MBOOT_VFS_RAW



typedef struct _vfs_raw_context_t {
    mboot_addr_t seg0_base_addr;
    mboot_addr_t seg0_byte_len;
    mboot_addr_t seg1_base_addr;
    mboot_addr_t seg1_byte_len;
    mboot_addr_t file_pos;
} vfs_raw_context_t;

extern const stream_methods_t vfs_raw_stream_methods;

int vfs_raw_mount(vfs_raw_context_t *ctx, mboot_addr_t seg0_base_addr, mboot_addr_t seg0_byte_len, mboot_addr_t seg1_base_addr, mboot_addr_t seg1_byte_len);

#endif

#endif 
