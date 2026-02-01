 

 
#include <linux/lz4.h>
#include "lz4defs.h"
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/unaligned.h>

 

#define DEBUGLOG(l, ...) {}	 

#ifndef assert
#define assert(condition) ((void)0)
#endif

 
static FORCE_INLINE int LZ4_decompress_generic(
	 const char * const src,
	 char * const dst,
	 int srcSize,
		 
	 int outputSize,
	  
	 endCondition_directive endOnInput,
	  
	 earlyEnd_directive partialDecoding,
	  
	 dict_directive dict,
	  
	 const BYTE * const lowPrefix,
	  
	 const BYTE * const dictStart,
	  
	 const size_t dictSize
	 )
{
	const BYTE *ip = (const BYTE *) src;
	const BYTE * const iend = ip + srcSize;

	BYTE *op = (BYTE *) dst;
	BYTE * const oend = op + outputSize;
	BYTE *cpy;

	const BYTE * const dictEnd = (const BYTE *)dictStart + dictSize;
	static const unsigned int inc32table[8] = {0, 1, 2, 1, 0, 4, 4, 4};
	static const int dec64table[8] = {0, 0, 0, -1, -4, 1, 2, 3};

	const int safeDecode = (endOnInput == endOnInputSize);
	const int checkOffset = ((safeDecode) && (dictSize < (int)(64 * KB)));

	 
	const BYTE *const shortiend = iend -
		(endOnInput ? 14 : 8)   - 2  ;
	const BYTE *const shortoend = oend -
		(endOnInput ? 14 : 8)   - 18  ;

	DEBUGLOG(5, "%s (srcSize:%i, dstSize:%i)", __func__,
		 srcSize, outputSize);

	 
	assert(lowPrefix <= op);
	assert(src != NULL);

	 
	if ((endOnInput) && (unlikely(outputSize == 0)))
		return ((srcSize == 1) && (*ip == 0)) ? 0 : -1;

	if ((!endOnInput) && (unlikely(outputSize == 0)))
		return (*ip == 0 ? 1 : -1);

	if ((endOnInput) && unlikely(srcSize == 0))
		return -1;

	 
	while (1) {
		size_t length;
		const BYTE *match;
		size_t offset;

		 
		unsigned int const token = *ip++;
		length = token>>ML_BITS;

		 
		assert(!endOnInput || ip <= iend);

		 
		if ((endOnInput ? length != RUN_MASK : length <= 8)
		    
		   && likely((endOnInput ? ip < shortiend : 1) &
			     (op <= shortoend))) {
			 
			LZ4_memcpy(op, ip, endOnInput ? 16 : 8);
			op += length; ip += length;

			 
			length = token & ML_MASK;  
			offset = LZ4_readLE16(ip);
			ip += 2;
			match = op - offset;
			assert(match <= op);  

			 
			if ((length != ML_MASK) &&
			    (offset >= 8) &&
			    (dict == withPrefix64k || match >= lowPrefix)) {
				 
				LZ4_memcpy(op + 0, match + 0, 8);
				LZ4_memcpy(op + 8, match + 8, 8);
				LZ4_memcpy(op + 16, match + 16, 2);
				op += length + MINMATCH;
				 
				continue;
			}

			 
			goto _copy_match;
		}

		 
		if (length == RUN_MASK) {
			unsigned int s;

			if (unlikely(endOnInput ? ip >= iend - RUN_MASK : 0)) {
				 
				goto _output_error;
			}
			do {
				s = *ip++;
				length += s;
			} while (likely(endOnInput
				? ip < iend - RUN_MASK
				: 1) & (s == 255));

			if ((safeDecode)
			    && unlikely((uptrval)(op) +
					length < (uptrval)(op))) {
				 
				goto _output_error;
			}
			if ((safeDecode)
			    && unlikely((uptrval)(ip) +
					length < (uptrval)(ip))) {
				 
				goto _output_error;
			}
		}

		 
		cpy = op + length;
		LZ4_STATIC_ASSERT(MFLIMIT >= WILDCOPYLENGTH);

		if (((endOnInput) && ((cpy > oend - MFLIMIT)
			|| (ip + length > iend - (2 + 1 + LASTLITERALS))))
			|| ((!endOnInput) && (cpy > oend - WILDCOPYLENGTH))) {
			if (partialDecoding) {
				if (cpy > oend) {
					 
					cpy = oend;
					length = oend - op;
				}
				if ((endOnInput)
					&& (ip + length > iend)) {
					 
					goto _output_error;
				}
			} else {
				if ((!endOnInput)
					&& (cpy != oend)) {
					 
					goto _output_error;
				}
				if ((endOnInput)
					&& ((ip + length != iend)
					|| (cpy > oend))) {
					 
					goto _output_error;
				}
			}

			 
			LZ4_memmove(op, ip, length);
			ip += length;
			op += length;

			 
			if (!partialDecoding || (cpy == oend) || (ip >= (iend - 2)))
				break;
		} else {
			 
			LZ4_wildCopy(op, ip, cpy);
			ip += length;
			op = cpy;
		}

		 
		offset = LZ4_readLE16(ip);
		ip += 2;
		match = op - offset;

		 
		length = token & ML_MASK;

_copy_match:
		if ((checkOffset) && (unlikely(match + dictSize < lowPrefix))) {
			 
			goto _output_error;
		}

		 
		 
		if (!partialDecoding) {
			assert(oend > op);
			assert(oend - op >= 4);

			LZ4_write32(op, (U32)offset);
		}

		if (length == ML_MASK) {
			unsigned int s;

			do {
				s = *ip++;

				if ((endOnInput) && (ip > iend - LASTLITERALS))
					goto _output_error;

				length += s;
			} while (s == 255);

			if ((safeDecode)
				&& unlikely(
					(uptrval)(op) + length < (uptrval)op)) {
				 
				goto _output_error;
			}
		}

		length += MINMATCH;

		 
		if ((dict == usingExtDict) && (match < lowPrefix)) {
			if (unlikely(op + length > oend - LASTLITERALS)) {
				 
				if (!partialDecoding)
					goto _output_error;
				length = min(length, (size_t)(oend - op));
			}

			if (length <= (size_t)(lowPrefix - match)) {
				 
				memmove(op, dictEnd - (lowPrefix - match),
					length);
				op += length;
			} else {
				 
				size_t const copySize = (size_t)(lowPrefix - match);
				size_t const restSize = length - copySize;

				LZ4_memcpy(op, dictEnd - copySize, copySize);
				op += copySize;
				if (restSize > (size_t)(op - lowPrefix)) {
					 
					BYTE * const endOfMatch = op + restSize;
					const BYTE *copyFrom = lowPrefix;

					while (op < endOfMatch)
						*op++ = *copyFrom++;
				} else {
					LZ4_memcpy(op, lowPrefix, restSize);
					op += restSize;
				}
			}
			continue;
		}

		 
		cpy = op + length;

		 
		assert(op <= oend);
		if (partialDecoding &&
		    (cpy > oend - MATCH_SAFEGUARD_DISTANCE)) {
			size_t const mlen = min(length, (size_t)(oend - op));
			const BYTE * const matchEnd = match + mlen;
			BYTE * const copyEnd = op + mlen;

			if (matchEnd > op) {
				 
				while (op < copyEnd)
					*op++ = *match++;
			} else {
				LZ4_memcpy(op, match, mlen);
			}
			op = copyEnd;
			if (op == oend)
				break;
			continue;
		}

		if (unlikely(offset < 8)) {
			op[0] = match[0];
			op[1] = match[1];
			op[2] = match[2];
			op[3] = match[3];
			match += inc32table[offset];
			LZ4_memcpy(op + 4, match, 4);
			match -= dec64table[offset];
		} else {
			LZ4_copy8(op, match);
			match += 8;
		}

		op += 8;

		if (unlikely(cpy > oend - MATCH_SAFEGUARD_DISTANCE)) {
			BYTE * const oCopyLimit = oend - (WILDCOPYLENGTH - 1);

			if (cpy > oend - LASTLITERALS) {
				 
				goto _output_error;
			}

			if (op < oCopyLimit) {
				LZ4_wildCopy(op, match, oCopyLimit);
				match += oCopyLimit - op;
				op = oCopyLimit;
			}
			while (op < cpy)
				*op++ = *match++;
		} else {
			LZ4_copy8(op, match);
			if (length > 16)
				LZ4_wildCopy(op + 8, match + 8, cpy);
		}
		op = cpy;  
	}

	 
	if (endOnInput) {
		 
		return (int) (((char *)op) - dst);
	} else {
		 
		return (int) (((const char *)ip) - src);
	}

	 
_output_error:
	return (int) (-(((const char *)ip) - src)) - 1;
}

int LZ4_decompress_safe(const char *source, char *dest,
	int compressedSize, int maxDecompressedSize)
{
	return LZ4_decompress_generic(source, dest,
				      compressedSize, maxDecompressedSize,
				      endOnInputSize, decode_full_block,
				      noDict, (BYTE *)dest, NULL, 0);
}

int LZ4_decompress_safe_partial(const char *src, char *dst,
	int compressedSize, int targetOutputSize, int dstCapacity)
{
	dstCapacity = min(targetOutputSize, dstCapacity);
	return LZ4_decompress_generic(src, dst, compressedSize, dstCapacity,
				      endOnInputSize, partial_decode,
				      noDict, (BYTE *)dst, NULL, 0);
}

int LZ4_decompress_fast(const char *source, char *dest, int originalSize)
{
	return LZ4_decompress_generic(source, dest, 0, originalSize,
				      endOnOutputSize, decode_full_block,
				      withPrefix64k,
				      (BYTE *)dest - 64 * KB, NULL, 0);
}

 

static int LZ4_decompress_safe_withPrefix64k(const char *source, char *dest,
				      int compressedSize, int maxOutputSize)
{
	return LZ4_decompress_generic(source, dest,
				      compressedSize, maxOutputSize,
				      endOnInputSize, decode_full_block,
				      withPrefix64k,
				      (BYTE *)dest - 64 * KB, NULL, 0);
}

static int LZ4_decompress_safe_withSmallPrefix(const char *source, char *dest,
					       int compressedSize,
					       int maxOutputSize,
					       size_t prefixSize)
{
	return LZ4_decompress_generic(source, dest,
				      compressedSize, maxOutputSize,
				      endOnInputSize, decode_full_block,
				      noDict,
				      (BYTE *)dest - prefixSize, NULL, 0);
}

static int LZ4_decompress_safe_forceExtDict(const char *source, char *dest,
					    int compressedSize, int maxOutputSize,
					    const void *dictStart, size_t dictSize)
{
	return LZ4_decompress_generic(source, dest,
				      compressedSize, maxOutputSize,
				      endOnInputSize, decode_full_block,
				      usingExtDict, (BYTE *)dest,
				      (const BYTE *)dictStart, dictSize);
}

static int LZ4_decompress_fast_extDict(const char *source, char *dest,
				       int originalSize,
				       const void *dictStart, size_t dictSize)
{
	return LZ4_decompress_generic(source, dest,
				      0, originalSize,
				      endOnOutputSize, decode_full_block,
				      usingExtDict, (BYTE *)dest,
				      (const BYTE *)dictStart, dictSize);
}

 
static FORCE_INLINE
int LZ4_decompress_safe_doubleDict(const char *source, char *dest,
				   int compressedSize, int maxOutputSize,
				   size_t prefixSize,
				   const void *dictStart, size_t dictSize)
{
	return LZ4_decompress_generic(source, dest,
				      compressedSize, maxOutputSize,
				      endOnInputSize, decode_full_block,
				      usingExtDict, (BYTE *)dest - prefixSize,
				      (const BYTE *)dictStart, dictSize);
}

static FORCE_INLINE
int LZ4_decompress_fast_doubleDict(const char *source, char *dest,
				   int originalSize, size_t prefixSize,
				   const void *dictStart, size_t dictSize)
{
	return LZ4_decompress_generic(source, dest,
				      0, originalSize,
				      endOnOutputSize, decode_full_block,
				      usingExtDict, (BYTE *)dest - prefixSize,
				      (const BYTE *)dictStart, dictSize);
}

 

int LZ4_setStreamDecode(LZ4_streamDecode_t *LZ4_streamDecode,
	const char *dictionary, int dictSize)
{
	LZ4_streamDecode_t_internal *lz4sd =
		&LZ4_streamDecode->internal_donotuse;

	lz4sd->prefixSize = (size_t) dictSize;
	lz4sd->prefixEnd = (const BYTE *) dictionary + dictSize;
	lz4sd->externalDict = NULL;
	lz4sd->extDictSize	= 0;
	return 1;
}

 
int LZ4_decompress_safe_continue(LZ4_streamDecode_t *LZ4_streamDecode,
	const char *source, char *dest, int compressedSize, int maxOutputSize)
{
	LZ4_streamDecode_t_internal *lz4sd =
		&LZ4_streamDecode->internal_donotuse;
	int result;

	if (lz4sd->prefixSize == 0) {
		 
		assert(lz4sd->extDictSize == 0);
		result = LZ4_decompress_safe(source, dest,
			compressedSize, maxOutputSize);
		if (result <= 0)
			return result;
		lz4sd->prefixSize = result;
		lz4sd->prefixEnd = (BYTE *)dest + result;
	} else if (lz4sd->prefixEnd == (BYTE *)dest) {
		 
		if (lz4sd->prefixSize >= 64 * KB - 1)
			result = LZ4_decompress_safe_withPrefix64k(source, dest,
				compressedSize, maxOutputSize);
		else if (lz4sd->extDictSize == 0)
			result = LZ4_decompress_safe_withSmallPrefix(source,
				dest, compressedSize, maxOutputSize,
				lz4sd->prefixSize);
		else
			result = LZ4_decompress_safe_doubleDict(source, dest,
				compressedSize, maxOutputSize,
				lz4sd->prefixSize,
				lz4sd->externalDict, lz4sd->extDictSize);
		if (result <= 0)
			return result;
		lz4sd->prefixSize += result;
		lz4sd->prefixEnd  += result;
	} else {
		 
		lz4sd->extDictSize = lz4sd->prefixSize;
		lz4sd->externalDict = lz4sd->prefixEnd - lz4sd->extDictSize;
		result = LZ4_decompress_safe_forceExtDict(source, dest,
			compressedSize, maxOutputSize,
			lz4sd->externalDict, lz4sd->extDictSize);
		if (result <= 0)
			return result;
		lz4sd->prefixSize = result;
		lz4sd->prefixEnd  = (BYTE *)dest + result;
	}

	return result;
}

int LZ4_decompress_fast_continue(LZ4_streamDecode_t *LZ4_streamDecode,
	const char *source, char *dest, int originalSize)
{
	LZ4_streamDecode_t_internal *lz4sd = &LZ4_streamDecode->internal_donotuse;
	int result;

	if (lz4sd->prefixSize == 0) {
		assert(lz4sd->extDictSize == 0);
		result = LZ4_decompress_fast(source, dest, originalSize);
		if (result <= 0)
			return result;
		lz4sd->prefixSize = originalSize;
		lz4sd->prefixEnd = (BYTE *)dest + originalSize;
	} else if (lz4sd->prefixEnd == (BYTE *)dest) {
		if (lz4sd->prefixSize >= 64 * KB - 1 ||
		    lz4sd->extDictSize == 0)
			result = LZ4_decompress_fast(source, dest,
						     originalSize);
		else
			result = LZ4_decompress_fast_doubleDict(source, dest,
				originalSize, lz4sd->prefixSize,
				lz4sd->externalDict, lz4sd->extDictSize);
		if (result <= 0)
			return result;
		lz4sd->prefixSize += originalSize;
		lz4sd->prefixEnd  += originalSize;
	} else {
		lz4sd->extDictSize = lz4sd->prefixSize;
		lz4sd->externalDict = lz4sd->prefixEnd - lz4sd->extDictSize;
		result = LZ4_decompress_fast_extDict(source, dest,
			originalSize, lz4sd->externalDict, lz4sd->extDictSize);
		if (result <= 0)
			return result;
		lz4sd->prefixSize = originalSize;
		lz4sd->prefixEnd = (BYTE *)dest + originalSize;
	}
	return result;
}

int LZ4_decompress_safe_usingDict(const char *source, char *dest,
				  int compressedSize, int maxOutputSize,
				  const char *dictStart, int dictSize)
{
	if (dictSize == 0)
		return LZ4_decompress_safe(source, dest,
					   compressedSize, maxOutputSize);
	if (dictStart+dictSize == dest) {
		if (dictSize >= 64 * KB - 1)
			return LZ4_decompress_safe_withPrefix64k(source, dest,
				compressedSize, maxOutputSize);
		return LZ4_decompress_safe_withSmallPrefix(source, dest,
			compressedSize, maxOutputSize, dictSize);
	}
	return LZ4_decompress_safe_forceExtDict(source, dest,
		compressedSize, maxOutputSize, dictStart, dictSize);
}

int LZ4_decompress_fast_usingDict(const char *source, char *dest,
				  int originalSize,
				  const char *dictStart, int dictSize)
{
	if (dictSize == 0 || dictStart + dictSize == dest)
		return LZ4_decompress_fast(source, dest, originalSize);

	return LZ4_decompress_fast_extDict(source, dest, originalSize,
		dictStart, dictSize);
}

#ifndef STATIC
EXPORT_SYMBOL(LZ4_decompress_safe);
EXPORT_SYMBOL(LZ4_decompress_safe_partial);
EXPORT_SYMBOL(LZ4_decompress_fast);
EXPORT_SYMBOL(LZ4_setStreamDecode);
EXPORT_SYMBOL(LZ4_decompress_safe_continue);
EXPORT_SYMBOL(LZ4_decompress_fast_continue);
EXPORT_SYMBOL(LZ4_decompress_safe_usingDict);
EXPORT_SYMBOL(LZ4_decompress_fast_usingDict);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("LZ4 decompressor");
#endif
