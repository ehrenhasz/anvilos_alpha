 
#include <linux/hdmi.h>
#include <linux/gcd.h>

#include <drm/amdgpu_drm.h>
#include "amdgpu.h"

static const struct amdgpu_afmt_acr amdgpu_afmt_predefined_acr[] = {
     
     
    {  25175,  4096,  25175, 28224, 125875,  6144,  25175 },  
    {  25200,  4096,  25200,  6272,  28000,  6144,  25200 },  
    {  27000,  4096,  27000,  6272,  30000,  6144,  27000 },  
    {  27027,  4096,  27027,  6272,  30030,  6144,  27027 },  
    {  54000,  4096,  54000,  6272,  60000,  6144,  54000 },  
    {  54054,  4096,  54054,  6272,  60060,  6144,  54054 },  
    {  74176,  4096,  74176,  5733,  75335,  6144,  74176 },  
    {  74250,  4096,  74250,  6272,  82500,  6144,  74250 },  
    { 148352,  4096, 148352,  5733, 150670,  6144, 148352 },  
    { 148500,  4096, 148500,  6272, 165000,  6144, 148500 },  
};


 
static void amdgpu_afmt_calc_cts(uint32_t clock, int *CTS, int *N, int freq)
{
	int n, cts;
	unsigned long div, mul;

	 
	n = 128 * freq;
	cts = clock * 1000;

	 
	div = gcd(n, cts);

	n /= div;
	cts /= div;

	 
	mul = ((128*freq/1000) + (n-1))/n;

	n *= mul;
	cts *= mul;

	 
	if (n < (128*freq/1500))
		pr_warn("Calculated ACR N value is too small. You may experience audio problems.\n");
	if (n > (128*freq/300))
		pr_warn("Calculated ACR N value is too large. You may experience audio problems.\n");

	*N = n;
	*CTS = cts;

	DRM_DEBUG("Calculated ACR timing N=%d CTS=%d for frequency %d\n",
		  *N, *CTS, freq);
}

struct amdgpu_afmt_acr amdgpu_afmt_acr(uint32_t clock)
{
	struct amdgpu_afmt_acr res;
	u8 i;

	 
	for (i = 0; i < ARRAY_SIZE(amdgpu_afmt_predefined_acr); i++) {
		if (amdgpu_afmt_predefined_acr[i].clock == clock)
			return amdgpu_afmt_predefined_acr[i];
	}

	 
	amdgpu_afmt_calc_cts(clock, &res.cts_32khz, &res.n_32khz, 32000);
	amdgpu_afmt_calc_cts(clock, &res.cts_44_1khz, &res.n_44_1khz, 44100);
	amdgpu_afmt_calc_cts(clock, &res.cts_48khz, &res.n_48khz, 48000);

	return res;
}
