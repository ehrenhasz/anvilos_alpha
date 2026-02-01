 

 

 

#include "fdlibm.h"

 

#ifdef __STDC__
static const int init_jk[] = {4,7,9};  
#else
static int init_jk[] = {4,7,9}; 
#endif

#ifdef __STDC__
static const float PIo2[] = {
#else
static float PIo2[] = {
#endif
  1.5703125000e+00f,  
  4.5776367188e-04f,  
  2.5987625122e-05f,  
  7.5437128544e-08f,  
  6.0026650317e-11f,  
  7.3896444519e-13f,  
  5.3845816694e-15f,  
  5.6378512969e-18f,  
  8.3009228831e-20f,  
  3.2756352257e-22f,  
  6.3331015649e-25f,  
};

#ifdef __STDC__
static const float			
#else
static float			
#endif
zero   = 0.0f,
one    = 1.0f,
two8   =  2.5600000000e+02f,  
twon8  =  3.9062500000e-03f;  

#ifdef __STDC__
	int __kernel_rem_pio2f(float *x, float *y, int e0, int nx, int prec, const __uint8_t *ipio2) 
#else
	int __kernel_rem_pio2f(x,y,e0,nx,prec,ipio2) 	
	float x[], y[]; int e0,nx,prec; __uint8_t ipio2[];
#endif
{
	__int32_t jz,jx,jv,jp,jk,carry,n,iq[20],i,j,k,m,q0,ih;
	float z,fw,f[20],fq[20],q[20];

     
	jk = init_jk[prec];
	jp = jk;

     
	jx =  nx-1;
	jv = (e0-3)/8; if(jv<0) jv=0;
	q0 =  e0-8*(jv+1);

     
	j = jv-jx; m = jx+jk;
	for(i=0;i<=m;i++,j++) f[i] = (j<0)? zero : (float) ipio2[j];

     
	for (i=0;i<=jk;i++) {
	    for(j=0,fw=0.0;j<=jx;j++) fw += x[j]*f[jx+i-j];
	    q[i] = fw;
	}

	jz = jk;
recompute:
     
	for(i=0,j=jz,z=q[jz];j>0;i++,j--) {
	    fw    =  (float)((__int32_t)(twon8* z));
	    iq[i] =  (__int32_t)(z-two8*fw);
	    z     =  q[j-1]+fw;
	}

     
	z  = scalbnf(z,(int)q0);	 
	z -= (float)8.0*floorf(z*(float)0.125);	 
	n  = (__int32_t) z;
	z -= (float)n;
	ih = 0;
	if(q0>0) {	 
	    i  = (iq[jz-1]>>(8-q0)); n += i;
	    iq[jz-1] -= i<<(8-q0);
	    ih = iq[jz-1]>>(7-q0);
	} 
	else if(q0==0) ih = iq[jz-1]>>8;
	else if(z>=(float)0.5) ih=2;

	if(ih>0) {	 
	    n += 1; carry = 0;
	    for(i=0;i<jz ;i++) {	 
		j = iq[i];
		if(carry==0) {
		    if(j!=0) {
			carry = 1; iq[i] = 0x100- j;
		    }
		} else  iq[i] = 0xff - j;
	    }
	    if(q0>0) {		 
	        switch(q0) {
	        case 1:
	    	   iq[jz-1] &= 0x7f; break;
	    	case 2:
	    	   iq[jz-1] &= 0x3f; break;
	        }
	    }
	    if(ih==2) {
		z = one - z;
		if(carry!=0) z -= scalbnf(one,(int)q0);
	    }
	}

     
	if(z==zero) {
	    j = 0;
	    for (i=jz-1;i>=jk;i--) j |= iq[i];
	    if(j==0) {  
		for(k=1;iq[jk-k]==0;k++);    

		for(i=jz+1;i<=jz+k;i++) {    
		    f[jx+i] = (float) ipio2[jv+i];
		    for(j=0,fw=0.0;j<=jx;j++) fw += x[j]*f[jx+i-j];
		    q[i] = fw;
		}
		jz += k;
		goto recompute;
	    }
	}

     
	if(z==(float)0.0) {
	    jz -= 1; q0 -= 8;
	    while(iq[jz]==0) { jz--; q0-=8;}
	} else {  
	    z = scalbnf(z,-(int)q0);
	    if(z>=two8) { 
		fw = (float)((__int32_t)(twon8*z));
		iq[jz] = (__int32_t)(z-two8*fw);
		jz += 1; q0 += 8;
		iq[jz] = (__int32_t) fw;
	    } else iq[jz] = (__int32_t) z ;
	}

     
	fw = scalbnf(one,(int)q0);
	for(i=jz;i>=0;i--) {
	    q[i] = fw*(float)iq[i]; fw*=twon8;
	}

     
	for(i=jz;i>=0;i--) {
	    for(fw=0.0,k=0;k<=jp&&k<=jz-i;k++) fw += PIo2[k]*q[i+k];
	    fq[jz-i] = fw;
	}

     
	switch(prec) {
	    case 0:
		fw = 0.0;
		for (i=jz;i>=0;i--) fw += fq[i];
		y[0] = (ih==0)? fw: -fw; 
		break;
	    case 1:
	    case 2:
		fw = 0.0;
		for (i=jz;i>=0;i--) fw += fq[i]; 
		y[0] = (ih==0)? fw: -fw; 
		fw = fq[0]-fw;
		for (i=1;i<=jz;i++) fw += fq[i];
		y[1] = (ih==0)? fw: -fw; 
		break;
	    case 3:	 
		for (i=jz;i>0;i--) {
		    fw      = fq[i-1]+fq[i]; 
		    fq[i]  += fq[i-1]-fw;
		    fq[i-1] = fw;
		}
		for (i=jz;i>1;i--) {
		    fw      = fq[i-1]+fq[i]; 
		    fq[i]  += fq[i-1]-fw;
		    fq[i-1] = fw;
		}
		for (fw=0.0,i=jz;i>=2;i--) fw += fq[i]; 
		if(ih==0) {
		    y[0] =  fq[0]; y[1] =  fq[1]; y[2] =  fw;
		} else {
		    y[0] = -fq[0]; y[1] = -fq[1]; y[2] = -fw;
		}
	}
	return n&7;
}
