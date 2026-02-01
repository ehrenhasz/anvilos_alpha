 

#include "mpi-internal.h"
#include "longlong.h"

void mpi_tdiv_qr(MPI quot, MPI rem, MPI num, MPI den);
void mpi_fdiv_qr(MPI quot, MPI rem, MPI dividend, MPI divisor);

void mpi_fdiv_r(MPI rem, MPI dividend, MPI divisor)
{
	int divisor_sign = divisor->sign;
	MPI temp_divisor = NULL;

	 
	if (rem == divisor) {
		temp_divisor = mpi_copy(divisor);
		divisor = temp_divisor;
	}

	mpi_tdiv_r(rem, dividend, divisor);

	if (((divisor_sign?1:0) ^ (dividend->sign?1:0)) && rem->nlimbs)
		mpi_add(rem, rem, divisor);

	if (temp_divisor)
		mpi_free(temp_divisor);
}

void mpi_fdiv_q(MPI quot, MPI dividend, MPI divisor)
{
	MPI tmp = mpi_alloc(mpi_get_nlimbs(quot));
	mpi_fdiv_qr(quot, tmp, dividend, divisor);
	mpi_free(tmp);
}

void mpi_fdiv_qr(MPI quot, MPI rem, MPI dividend, MPI divisor)
{
	int divisor_sign = divisor->sign;
	MPI temp_divisor = NULL;

	if (quot == divisor || rem == divisor) {
		temp_divisor = mpi_copy(divisor);
		divisor = temp_divisor;
	}

	mpi_tdiv_qr(quot, rem, dividend, divisor);

	if ((divisor_sign ^ dividend->sign) && rem->nlimbs) {
		mpi_sub_ui(quot, quot, 1);
		mpi_add(rem, rem, divisor);
	}

	if (temp_divisor)
		mpi_free(temp_divisor);
}

 

void mpi_tdiv_r(MPI rem, MPI num, MPI den)
{
	mpi_tdiv_qr(NULL, rem, num, den);
}

void mpi_tdiv_qr(MPI quot, MPI rem, MPI num, MPI den)
{
	mpi_ptr_t np, dp;
	mpi_ptr_t qp, rp;
	mpi_size_t nsize = num->nlimbs;
	mpi_size_t dsize = den->nlimbs;
	mpi_size_t qsize, rsize;
	mpi_size_t sign_remainder = num->sign;
	mpi_size_t sign_quotient = num->sign ^ den->sign;
	unsigned int normalization_steps;
	mpi_limb_t q_limb;
	mpi_ptr_t marker[5];
	int markidx = 0;

	 
	rsize = nsize + 1;
	mpi_resize(rem, rsize);

	qsize = rsize - dsize;	   
	if (qsize <= 0) {
		if (num != rem) {
			rem->nlimbs = num->nlimbs;
			rem->sign = num->sign;
			MPN_COPY(rem->d, num->d, nsize);
		}
		if (quot) {
			 
			quot->nlimbs = 0;
			quot->sign = 0;
		}
		return;
	}

	if (quot)
		mpi_resize(quot, qsize);

	 
	np = num->d;
	dp = den->d;
	rp = rem->d;

	 
	if (dsize == 1) {
		mpi_limb_t rlimb;
		if (quot) {
			qp = quot->d;
			rlimb = mpihelp_divmod_1(qp, np, nsize, dp[0]);
			qsize -= qp[qsize - 1] == 0;
			quot->nlimbs = qsize;
			quot->sign = sign_quotient;
		} else
			rlimb = mpihelp_mod_1(np, nsize, dp[0]);
		rp[0] = rlimb;
		rsize = rlimb != 0?1:0;
		rem->nlimbs = rsize;
		rem->sign = sign_remainder;
		return;
	}


	if (quot) {
		qp = quot->d;
		 
		if (qp == np) {  
			np = marker[markidx++] = mpi_alloc_limb_space(nsize);
			MPN_COPY(np, qp, nsize);
		}
	} else  
		qp = rp + dsize;

	normalization_steps = count_leading_zeros(dp[dsize - 1]);

	 
	if (normalization_steps) {
		mpi_ptr_t tp;
		mpi_limb_t nlimb;

		 
		tp = marker[markidx++] = mpi_alloc_limb_space(dsize);
		mpihelp_lshift(tp, dp, dsize, normalization_steps);
		dp = tp;

		 
		nlimb = mpihelp_lshift(rp, np, nsize, normalization_steps);
		if (nlimb) {
			rp[nsize] = nlimb;
			rsize = nsize + 1;
		} else
			rsize = nsize;
	} else {
		 
		if (dp == rp || (quot && (dp == qp))) {
			mpi_ptr_t tp;

			tp = marker[markidx++] = mpi_alloc_limb_space(dsize);
			MPN_COPY(tp, dp, dsize);
			dp = tp;
		}

		 
		if (rp != np)
			MPN_COPY(rp, np, nsize);

		rsize = nsize;
	}

	q_limb = mpihelp_divrem(qp, 0, rp, rsize, dp, dsize);

	if (quot) {
		qsize = rsize - dsize;
		if (q_limb) {
			qp[qsize] = q_limb;
			qsize += 1;
		}

		quot->nlimbs = qsize;
		quot->sign = sign_quotient;
	}

	rsize = dsize;
	MPN_NORMALIZE(rp, rsize);

	if (normalization_steps && rsize) {
		mpihelp_rshift(rp, rp, rsize, normalization_steps);
		rsize -= rp[rsize - 1] == 0?1:0;
	}

	rem->nlimbs = rsize;
	rem->sign	= sign_remainder;
	while (markidx) {
		markidx--;
		mpi_free_limb_space(marker[markidx]);
	}
}
