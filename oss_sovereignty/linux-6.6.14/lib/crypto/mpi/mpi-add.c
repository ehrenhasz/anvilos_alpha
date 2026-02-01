 

#include "mpi-internal.h"

 
void mpi_add_ui(MPI w, MPI u, unsigned long v)
{
	mpi_ptr_t wp, up;
	mpi_size_t usize, wsize;
	int usign, wsign;

	usize = u->nlimbs;
	usign = u->sign;
	wsign = 0;

	 
	wsize = usize + 1;
	if (w->alloced < wsize)
		mpi_resize(w, wsize);

	 
	up = u->d;
	wp = w->d;

	if (!usize) {   
		wp[0] = v;
		wsize = v ? 1:0;
	} else if (!usign) {   
		mpi_limb_t cy;
		cy = mpihelp_add_1(wp, up, usize, v);
		wp[usize] = cy;
		wsize = usize + cy;
	} else {
		 
		if (usize == 1 && up[0] < v) {
			wp[0] = v - up[0];
			wsize = 1;
		} else {
			mpihelp_sub_1(wp, up, usize, v);
			 
			wsize = usize - (wp[usize-1] == 0);
			wsign = 1;
		}
	}

	w->nlimbs = wsize;
	w->sign   = wsign;
}


void mpi_add(MPI w, MPI u, MPI v)
{
	mpi_ptr_t wp, up, vp;
	mpi_size_t usize, vsize, wsize;
	int usign, vsign, wsign;

	if (u->nlimbs < v->nlimbs) {  
		usize = v->nlimbs;
		usign = v->sign;
		vsize = u->nlimbs;
		vsign = u->sign;
		wsize = usize + 1;
		RESIZE_IF_NEEDED(w, wsize);
		 
		up = v->d;
		vp = u->d;
	} else {
		usize = u->nlimbs;
		usign = u->sign;
		vsize = v->nlimbs;
		vsign = v->sign;
		wsize = usize + 1;
		RESIZE_IF_NEEDED(w, wsize);
		 
		up = u->d;
		vp = v->d;
	}
	wp = w->d;
	wsign = 0;

	if (!vsize) {   
		MPN_COPY(wp, up, usize);
		wsize = usize;
		wsign = usign;
	} else if (usign != vsign) {  
		 
		if (usize != vsize) {
			mpihelp_sub(wp, up, usize, vp, vsize);
			wsize = usize;
			MPN_NORMALIZE(wp, wsize);
			wsign = usign;
		} else if (mpihelp_cmp(up, vp, usize) < 0) {
			mpihelp_sub_n(wp, vp, up, usize);
			wsize = usize;
			MPN_NORMALIZE(wp, wsize);
			if (!usign)
				wsign = 1;
		} else {
			mpihelp_sub_n(wp, up, vp, usize);
			wsize = usize;
			MPN_NORMALIZE(wp, wsize);
			if (usign)
				wsign = 1;
		}
	} else {  
		mpi_limb_t cy = mpihelp_add(wp, up, usize, vp, vsize);
		wp[usize] = cy;
		wsize = usize + cy;
		if (usign)
			wsign = 1;
	}

	w->nlimbs = wsize;
	w->sign = wsign;
}
EXPORT_SYMBOL_GPL(mpi_add);

void mpi_sub(MPI w, MPI u, MPI v)
{
	MPI vv = mpi_copy(v);
	vv->sign = !vv->sign;
	mpi_add(w, u, vv);
	mpi_free(vv);
}
EXPORT_SYMBOL_GPL(mpi_sub);

void mpi_addm(MPI w, MPI u, MPI v, MPI m)
{
	mpi_add(w, u, v);
	mpi_mod(w, w, m);
}
EXPORT_SYMBOL_GPL(mpi_addm);

void mpi_subm(MPI w, MPI u, MPI v, MPI m)
{
	mpi_sub(w, u, v);
	mpi_mod(w, w, m);
}
EXPORT_SYMBOL_GPL(mpi_subm);
