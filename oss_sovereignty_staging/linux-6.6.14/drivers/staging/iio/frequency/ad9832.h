 
 
#ifndef IIO_DDS_AD9832_H_
#define IIO_DDS_AD9832_H_

 

 

struct ad9832_platform_data {
	unsigned long		freq0;
	unsigned long		freq1;
	unsigned short		phase0;
	unsigned short		phase1;
	unsigned short		phase2;
	unsigned short		phase3;
};

#endif  
