 
 

#ifndef PWC_DEC23_H
#define PWC_DEC23_H

struct pwc_device;

struct pwc_dec23_private
{
	struct mutex lock;

	unsigned char last_cmd, last_cmd_valid;

  unsigned int scalebits;
  unsigned int nbitsmask, nbits;  

  unsigned int reservoir;
  unsigned int nbits_in_reservoir;

  const unsigned char *stream;
  int temp_colors[16];

  unsigned char table_0004_pass1[16][1024];
  unsigned char table_0004_pass2[16][1024];
  unsigned char table_8004_pass1[16][256];
  unsigned char table_8004_pass2[16][256];
  unsigned int  table_subblock[256][12];

  unsigned char table_bitpowermask[8][256];
  unsigned int  table_d800[256];
  unsigned int  table_dc00[256];

};

void pwc_dec23_init(struct pwc_device *pdev, const unsigned char *cmd);
void pwc_dec23_decompress(struct pwc_device *pdev,
			  const void *src,
			  void *dst);
#endif
