 

#ifndef HUMAN_H_
# define HUMAN_H_ 1

# include <limits.h>
# include <stdint.h>
# include <unistd.h>

# include <xstrtol.h>

 
# define LONGEST_HUMAN_READABLE \
  ((2 * sizeof (uintmax_t) * CHAR_BIT * 146 / 485 + 1) * (MB_LEN_MAX + 1) \
   - MB_LEN_MAX + 1 + 3)

 
enum
{
   

   
   
  human_ceiling = 0,
   
  human_round_to_nearest = 1,
   
  human_floor = 2,

   
  human_group_digits = 4,

   
  human_suppress_point_zero = 8,

   
  human_autoscale = 16,

   
  human_base_1024 = 32,

   
  human_space_before_unit = 64,

   
  human_SI = 128,

   
  human_B = 256
};

char *human_readable (uintmax_t, char *, int, uintmax_t, uintmax_t);

enum strtol_error human_options (char const *, int *, uintmax_t *);

#endif  
