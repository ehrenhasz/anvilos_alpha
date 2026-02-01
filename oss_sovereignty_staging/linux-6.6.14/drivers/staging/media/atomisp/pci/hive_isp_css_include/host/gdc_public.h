 
 

#ifndef __GDC_PUBLIC_H_INCLUDED__
#define __GDC_PUBLIC_H_INCLUDED__

 
void gdc_lut_store(
    const gdc_ID_t		ID,
    const int			data[4][HRT_GDC_N]);

 
void gdc_lut_convert_to_isp_format(
    const int in_lut[4][HRT_GDC_N],
    int out_lut[4][HRT_GDC_N]);

 
int gdc_get_unity(
    const gdc_ID_t		ID);

#endif  
