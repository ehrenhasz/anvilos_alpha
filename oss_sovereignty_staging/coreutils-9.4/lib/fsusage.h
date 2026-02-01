 

#if !defined FSUSAGE_H_
# define FSUSAGE_H_

# include <stdint.h>

struct fs_usage
{
  uintmax_t fsu_blocksize;       
  uintmax_t fsu_blocks;          
  uintmax_t fsu_bfree;           
  uintmax_t fsu_bavail;          
  bool fsu_bavail_top_bit_set;   
  uintmax_t fsu_files;           
  uintmax_t fsu_ffree;           
};

int get_fs_usage (char const *file, char const *disk, struct fs_usage *fsp);

#endif
