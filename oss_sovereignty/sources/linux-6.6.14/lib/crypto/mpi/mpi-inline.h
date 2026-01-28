


#ifndef G10_MPI_INLINE_H
#define G10_MPI_INLINE_H

#ifndef G10_MPI_INLINE_DECL
#define G10_MPI_INLINE_DECL  static inline
#endif

G10_MPI_INLINE_DECL mpi_limb_t
mpihelp_add_1(mpi_ptr_t res_ptr, mpi_ptr_t s1_ptr,
	      mpi_size_t s1_size, mpi_limb_t s2_limb)
{
	mpi_limb_t x;

	x = *s1_ptr++;
	s2_limb += x;
	*res_ptr++ = s2_limb;
	if (s2_limb < x) {	
		while (--s1_size) {
			x = *s1_ptr++ + 1;	
			*res_ptr++ = x;	
			if (x)	
				goto leave;
		}
		return 1;	
	}

leave:
	if (res_ptr != s1_ptr) {	
		mpi_size_t i;	
		for (i = 0; i < s1_size - 1; i++)
			res_ptr[i] = s1_ptr[i];
	}
	return 0;		
}

G10_MPI_INLINE_DECL mpi_limb_t
mpihelp_add(mpi_ptr_t res_ptr, mpi_ptr_t s1_ptr, mpi_size_t s1_size,
	    mpi_ptr_t s2_ptr, mpi_size_t s2_size)
{
	mpi_limb_t cy = 0;

	if (s2_size)
		cy = mpihelp_add_n(res_ptr, s1_ptr, s2_ptr, s2_size);

	if (s1_size - s2_size)
		cy = mpihelp_add_1(res_ptr + s2_size, s1_ptr + s2_size,
				   s1_size - s2_size, cy);
	return cy;
}

G10_MPI_INLINE_DECL mpi_limb_t
mpihelp_sub_1(mpi_ptr_t res_ptr, mpi_ptr_t s1_ptr,
	      mpi_size_t s1_size, mpi_limb_t s2_limb)
{
	mpi_limb_t x;

	x = *s1_ptr++;
	s2_limb = x - s2_limb;
	*res_ptr++ = s2_limb;
	if (s2_limb > x) {
		while (--s1_size) {
			x = *s1_ptr++;
			*res_ptr++ = x - 1;
			if (x)
				goto leave;
		}
		return 1;
	}

leave:
	if (res_ptr != s1_ptr) {
		mpi_size_t i;
		for (i = 0; i < s1_size - 1; i++)
			res_ptr[i] = s1_ptr[i];
	}
	return 0;
}

G10_MPI_INLINE_DECL mpi_limb_t
mpihelp_sub(mpi_ptr_t res_ptr, mpi_ptr_t s1_ptr, mpi_size_t s1_size,
	    mpi_ptr_t s2_ptr, mpi_size_t s2_size)
{
	mpi_limb_t cy = 0;

	if (s2_size)
		cy = mpihelp_sub_n(res_ptr, s1_ptr, s2_ptr, s2_size);

	if (s1_size - s2_size)
		cy = mpihelp_sub_1(res_ptr + s2_size, s1_ptr + s2_size,
				   s1_size - s2_size, cy);
	return cy;
}

#endif 
