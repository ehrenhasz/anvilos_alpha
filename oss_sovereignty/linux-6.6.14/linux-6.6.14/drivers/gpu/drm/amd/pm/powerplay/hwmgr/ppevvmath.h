#include <asm/div64.h>
#define SHIFT_AMOUNT 16  
#define PRECISION 5  
#define SHIFTED_2 (2 << SHIFT_AMOUNT)
#define MAX (1 << (SHIFT_AMOUNT - 1)) - 1  
typedef union _fInt {
    int full;
    struct _partial {
        unsigned int decimal: SHIFT_AMOUNT;  
        int real: 32 - SHIFT_AMOUNT;
    } partial;
} fInt;
static fInt ConvertToFraction(int);                        
static fInt Convert_ULONG_ToFraction(uint32_t);            
static fInt GetScaledFraction(int, int);                   
static int ConvertBackToInteger(fInt);                     
static fInt fNegate(fInt);                                 
static fInt fAdd (fInt, fInt);                             
static fInt fSubtract (fInt A, fInt B);                    
static fInt fMultiply (fInt, fInt);                        
static fInt fDivide (fInt A, fInt B);                      
static fInt fGetSquare(fInt);                              
static fInt fSqrt(fInt);                                   
static int uAbs(int);                                      
static int uPow(int base, int exponent);                   
static void SolveQuadracticEqn(fInt, fInt, fInt, fInt[]);  
static bool Equal(fInt, fInt);                             
static bool GreaterThan(fInt A, fInt B);                   
static fInt fExponential(fInt exponent);                   
static fInt fNaturalLog(fInt value);                       
static fInt fDecodeLinearFuse(uint32_t fuse_value, fInt f_min, fInt f_range, uint32_t bitlength);
static fInt fDecodeLogisticFuse(uint32_t fuse_value, fInt f_average, fInt f_range, uint32_t bitlength);
static fInt fDecodeLeakageID (uint32_t leakageID_fuse, fInt ln_max_div_min, fInt f_min, uint32_t bitlength);
static fInt Divide (int, int);                             
static fInt fNegate(fInt);
static int uGetScaledDecimal (fInt);                       
static int GetReal (fInt A);                               
static fInt fExponential(fInt exponent)         
{
	uint32_t i;
	bool bNegated = false;
	fInt fPositiveOne = ConvertToFraction(1);
	fInt fZERO = ConvertToFraction(0);
	fInt lower_bound = Divide(78, 10000);
	fInt solution = fPositiveOne;  
	fInt error_term;
	static const uint32_t k_array[11] = {55452, 27726, 13863, 6931, 4055, 2231, 1178, 606, 308, 155, 78};
	static const uint32_t expk_array[11] = {2560000, 160000, 40000, 20000, 15000, 12500, 11250, 10625, 10313, 10156, 10078};
	if (GreaterThan(fZERO, exponent)) {
		exponent = fNegate(exponent);
		bNegated = true;
	}
	while (GreaterThan(exponent, lower_bound)) {
		for (i = 0; i < 11; i++) {
			if (GreaterThan(exponent, GetScaledFraction(k_array[i], 10000))) {
				exponent = fSubtract(exponent, GetScaledFraction(k_array[i], 10000));
				solution = fMultiply(solution, GetScaledFraction(expk_array[i], 10000));
			}
		}
	}
	error_term = fAdd(fPositiveOne, exponent);
	solution = fMultiply(solution, error_term);
	if (bNegated)
		solution = fDivide(fPositiveOne, solution);
	return solution;
}
static fInt fNaturalLog(fInt value)
{
	uint32_t i;
	fInt upper_bound = Divide(8, 1000);
	fInt fNegativeOne = ConvertToFraction(-1);
	fInt solution = ConvertToFraction(0);  
	fInt error_term;
	static const uint32_t k_array[10] = {160000, 40000, 20000, 15000, 12500, 11250, 10625, 10313, 10156, 10078};
	static const uint32_t logk_array[10] = {27726, 13863, 6931, 4055, 2231, 1178, 606, 308, 155, 78};
	while (GreaterThan(fAdd(value, fNegativeOne), upper_bound)) {
		for (i = 0; i < 10; i++) {
			if (GreaterThan(value, GetScaledFraction(k_array[i], 10000))) {
				value = fDivide(value, GetScaledFraction(k_array[i], 10000));
				solution = fAdd(solution, GetScaledFraction(logk_array[i], 10000));
			}
		}
	}
	error_term = fAdd(fNegativeOne, value);
	return fAdd(solution, error_term);
}
static fInt fDecodeLinearFuse(uint32_t fuse_value, fInt f_min, fInt f_range, uint32_t bitlength)
{
	fInt f_fuse_value = Convert_ULONG_ToFraction(fuse_value);
	fInt f_bit_max_value = Convert_ULONG_ToFraction((uPow(2, bitlength)) - 1);
	fInt f_decoded_value;
	f_decoded_value = fDivide(f_fuse_value, f_bit_max_value);
	f_decoded_value = fMultiply(f_decoded_value, f_range);
	f_decoded_value = fAdd(f_decoded_value, f_min);
	return f_decoded_value;
}
static fInt fDecodeLogisticFuse(uint32_t fuse_value, fInt f_average, fInt f_range, uint32_t bitlength)
{
	fInt f_fuse_value = Convert_ULONG_ToFraction(fuse_value);
	fInt f_bit_max_value = Convert_ULONG_ToFraction((uPow(2, bitlength)) - 1);
	fInt f_CONSTANT_NEG13 = ConvertToFraction(-13);
	fInt f_CONSTANT1 = ConvertToFraction(1);
	fInt f_decoded_value;
	f_decoded_value = fSubtract(fDivide(f_bit_max_value, f_fuse_value), f_CONSTANT1);
	f_decoded_value = fNaturalLog(f_decoded_value);
	f_decoded_value = fMultiply(f_decoded_value, fDivide(f_range, f_CONSTANT_NEG13));
	f_decoded_value = fAdd(f_decoded_value, f_average);
	return f_decoded_value;
}
static fInt fDecodeLeakageID (uint32_t leakageID_fuse, fInt ln_max_div_min, fInt f_min, uint32_t bitlength)
{
	fInt fLeakage;
	fInt f_bit_max_value = Convert_ULONG_ToFraction((uPow(2, bitlength)) - 1);
	fLeakage = fMultiply(ln_max_div_min, Convert_ULONG_ToFraction(leakageID_fuse));
	fLeakage = fDivide(fLeakage, f_bit_max_value);
	fLeakage = fExponential(fLeakage);
	fLeakage = fMultiply(fLeakage, f_min);
	return fLeakage;
}
static fInt ConvertToFraction(int X)  
{
	fInt temp;
	if (X <= MAX)
		temp.full = (X << SHIFT_AMOUNT);
	else
		temp.full = 0;
	return temp;
}
static fInt fNegate(fInt X)
{
	fInt CONSTANT_NEGONE = ConvertToFraction(-1);
	return fMultiply(X, CONSTANT_NEGONE);
}
static fInt Convert_ULONG_ToFraction(uint32_t X)
{
	fInt temp;
	if (X <= MAX)
		temp.full = (X << SHIFT_AMOUNT);
	else
		temp.full = 0;
	return temp;
}
static fInt GetScaledFraction(int X, int factor)
{
	int times_shifted, factor_shifted;
	bool bNEGATED;
	fInt fValue;
	times_shifted = 0;
	factor_shifted = 0;
	bNEGATED = false;
	if (X < 0) {
		X = -1*X;
		bNEGATED = true;
	}
	if (factor < 0) {
		factor = -1*factor;
		bNEGATED = !bNEGATED;  
	}
	if ((X > MAX) || factor > MAX) {
		if ((X/factor) <= MAX) {
			while (X > MAX) {
				X = X >> 1;
				times_shifted++;
			}
			while (factor > MAX) {
				factor = factor >> 1;
				factor_shifted++;
			}
		} else {
			fValue.full = 0;
			return fValue;
		}
	}
	if (factor == 1)
		return ConvertToFraction(X);
	fValue = fDivide(ConvertToFraction(X * uPow(-1, bNEGATED)), ConvertToFraction(factor));
	fValue.full = fValue.full << times_shifted;
	fValue.full = fValue.full >> factor_shifted;
	return fValue;
}
static fInt fAdd (fInt X, fInt Y)
{
	fInt Sum;
	Sum.full = X.full + Y.full;
	return Sum;
}
static fInt fSubtract (fInt X, fInt Y)
{
	fInt Difference;
	Difference.full = X.full - Y.full;
	return Difference;
}
static bool Equal(fInt A, fInt B)
{
	if (A.full == B.full)
		return true;
	else
		return false;
}
static bool GreaterThan(fInt A, fInt B)
{
	if (A.full > B.full)
		return true;
	else
		return false;
}
static fInt fMultiply (fInt X, fInt Y)  
{
	fInt Product;
	int64_t tempProduct;
	tempProduct = ((int64_t)X.full) * ((int64_t)Y.full);  
	tempProduct = tempProduct >> 16;  
	Product.full = (int)tempProduct;  
	return Product;
}
static fInt fDivide (fInt X, fInt Y)
{
	fInt fZERO, fQuotient;
	int64_t longlongX, longlongY;
	fZERO = ConvertToFraction(0);
	if (Equal(Y, fZERO))
		return fZERO;
	longlongX = (int64_t)X.full;
	longlongY = (int64_t)Y.full;
	longlongX = longlongX << 16;  
	div64_s64(longlongX, longlongY);  
	fQuotient.full = (int)longlongX;
	return fQuotient;
}
static int ConvertBackToInteger (fInt A)  
{
	fInt fullNumber, scaledDecimal, scaledReal;
	scaledReal.full = GetReal(A) * uPow(10, PRECISION-1);  
	scaledDecimal.full = uGetScaledDecimal(A);
	fullNumber = fAdd(scaledDecimal, scaledReal);
	return fullNumber.full;
}
static fInt fGetSquare(fInt A)
{
	return fMultiply(A, A);
}
static fInt fSqrt(fInt num)
{
	fInt F_divide_Fprime, Fprime;
	fInt test;
	fInt twoShifted;
	int seed, counter, error;
	fInt x_new, x_old, C, y;
	fInt fZERO = ConvertToFraction(0);
	if (GreaterThan(fZERO, num) || Equal(fZERO, num))
		return fZERO;
	C = num;
	if (num.partial.real > 3000)
		seed = 60;
	else if (num.partial.real > 1000)
		seed = 30;
	else if (num.partial.real > 100)
		seed = 10;
	else
		seed = 2;
	counter = 0;
	if (Equal(num, fZERO))  
		return fZERO;
	twoShifted = ConvertToFraction(2);
	x_new = ConvertToFraction(seed);
	do {
		counter++;
		x_old.full = x_new.full;
		test = fGetSquare(x_old);  
		y = fSubtract(test, C);  
		Fprime = fMultiply(twoShifted, x_old);
		F_divide_Fprime = fDivide(y, Fprime);
		x_new = fSubtract(x_old, F_divide_Fprime);
		error = ConvertBackToInteger(x_new) - ConvertBackToInteger(x_old);
		if (counter > 20)  
			return x_new;
	} while (uAbs(error) > 0);
	return x_new;
}
static void SolveQuadracticEqn(fInt A, fInt B, fInt C, fInt Roots[])
{
	fInt *pRoots = &Roots[0];
	fInt temp, root_first, root_second;
	fInt f_CONSTANT10, f_CONSTANT100;
	f_CONSTANT100 = ConvertToFraction(100);
	f_CONSTANT10 = ConvertToFraction(10);
	while (GreaterThan(A, f_CONSTANT100) || GreaterThan(B, f_CONSTANT100) || GreaterThan(C, f_CONSTANT100)) {
		A = fDivide(A, f_CONSTANT10);
		B = fDivide(B, f_CONSTANT10);
		C = fDivide(C, f_CONSTANT10);
	}
	temp = fMultiply(ConvertToFraction(4), A);  
	temp = fMultiply(temp, C);  
	temp = fSubtract(fGetSquare(B), temp);  
	temp = fSqrt(temp);  
	root_first = fSubtract(fNegate(B), temp);  
	root_second = fAdd(fNegate(B), temp);  
	root_first = fDivide(root_first, ConvertToFraction(2));  
	root_first = fDivide(root_first, A);  
	root_second = fDivide(root_second, ConvertToFraction(2));  
	root_second = fDivide(root_second, A);  
	*(pRoots + 0) = root_first;
	*(pRoots + 1) = root_second;
}
static int GetReal (fInt A)
{
	return (A.full >> SHIFT_AMOUNT);
}
static fInt Divide (int X, int Y)
{
	fInt A, B, Quotient;
	A.full = X << SHIFT_AMOUNT;
	B.full = Y << SHIFT_AMOUNT;
	Quotient = fDivide(A, B);
	return Quotient;
}
static int uGetScaledDecimal (fInt A)  
{
	int dec[PRECISION];
	int i, scaledDecimal = 0, tmp = A.partial.decimal;
	for (i = 0; i < PRECISION; i++) {
		dec[i] = tmp / (1 << SHIFT_AMOUNT);
		tmp = tmp - ((1 << SHIFT_AMOUNT)*dec[i]);
		tmp *= 10;
		scaledDecimal = scaledDecimal + dec[i]*uPow(10, PRECISION - 1 - i);
	}
	return scaledDecimal;
}
static int uPow(int base, int power)
{
	if (power == 0)
		return 1;
	else
		return (base)*uPow(base, power - 1);
}
static int uAbs(int X)
{
	if (X < 0)
		return (X * -1);
	else
		return X;
}
static fInt fRoundUpByStepSize(fInt A, fInt fStepSize, bool error_term)
{
	fInt solution;
	solution = fDivide(A, fStepSize);
	solution.partial.decimal = 0;  
	if (error_term)
		solution.partial.real += 1;  
	solution = fMultiply(solution, fStepSize);
	solution = fAdd(solution, fStepSize);
	return solution;
}
