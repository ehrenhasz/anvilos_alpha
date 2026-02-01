
 

#include <linux/asn1_encoder.h>
#include <linux/bug.h>
#include <linux/string.h>
#include <linux/module.h>

 
unsigned char *
asn1_encode_integer(unsigned char *data, const unsigned char *end_data,
		    s64 integer)
{
	int data_len = end_data - data;
	unsigned char *d = &data[2];
	bool found = false;
	int i;

	if (WARN(integer < 0,
		 "BUG: integer encode only supports positive integers"))
		return ERR_PTR(-EINVAL);

	if (IS_ERR(data))
		return data;

	 
	if (data_len < 3)
		return ERR_PTR(-EINVAL);

	 
	data_len -= 2;

	data[0] = _tag(UNIV, PRIM, INT);
	if (integer == 0) {
		*d++ = 0;
		goto out;
	}

	for (i = sizeof(integer); i > 0 ; i--) {
		int byte = integer >> (8 * (i - 1));

		if (!found && byte == 0)
			continue;

		 
		if (!found && (byte & 0x80)) {
			 
			*d++ = 0;
			data_len--;
		}

		found = true;
		if (data_len == 0)
			return ERR_PTR(-EINVAL);

		*d++ = byte;
		data_len--;
	}

 out:
	data[1] = d - data - 2;

	return d;
}
EXPORT_SYMBOL_GPL(asn1_encode_integer);

 
static int asn1_encode_oid_digit(unsigned char **_data, int *data_len, u32 oid)
{
	unsigned char *data = *_data;
	int start = 7 + 7 + 7 + 7;
	int ret = 0;

	if (*data_len < 1)
		return -EINVAL;

	 
	if (oid == 0) {
		*data++ = 0x80;
		(*data_len)--;
		goto out;
	}

	while (oid >> start == 0)
		start -= 7;

	while (start > 0 && *data_len > 0) {
		u8 byte;

		byte = oid >> start;
		oid = oid - (byte << start);
		start -= 7;
		byte |= 0x80;
		*data++ = byte;
		(*data_len)--;
	}

	if (*data_len > 0) {
		*data++ = oid;
		(*data_len)--;
	} else {
		ret = -EINVAL;
	}

 out:
	*_data = data;
	return ret;
}

 
unsigned char *
asn1_encode_oid(unsigned char *data, const unsigned char *end_data,
		u32 oid[], int oid_len)
{
	int data_len = end_data - data;
	unsigned char *d = data + 2;
	int i, ret;

	if (WARN(oid_len < 2, "OID must have at least two elements"))
		return ERR_PTR(-EINVAL);

	if (WARN(oid_len > 32, "OID is too large"))
		return ERR_PTR(-EINVAL);

	if (IS_ERR(data))
		return data;


	 
	if (data_len < 3)
		return ERR_PTR(-EINVAL);

	data[0] = _tag(UNIV, PRIM, OID);
	*d++ = oid[0] * 40 + oid[1];

	data_len -= 3;

	for (i = 2; i < oid_len; i++) {
		ret = asn1_encode_oid_digit(&d, &data_len, oid[i]);
		if (ret < 0)
			return ERR_PTR(ret);
	}

	data[1] = d - data - 2;

	return d;
}
EXPORT_SYMBOL_GPL(asn1_encode_oid);

 
static int asn1_encode_length(unsigned char **data, int *data_len, int len)
{
	if (*data_len < 1)
		return -EINVAL;

	if (len < 0) {
		*((*data)++) = 0;
		(*data_len)--;
		return 0;
	}

	if (len <= 0x7f) {
		*((*data)++) = len;
		(*data_len)--;
		return 0;
	}

	if (*data_len < 2)
		return -EINVAL;

	if (len <= 0xff) {
		*((*data)++) = 0x81;
		*((*data)++) = len & 0xff;
		*data_len -= 2;
		return 0;
	}

	if (*data_len < 3)
		return -EINVAL;

	if (len <= 0xffff) {
		*((*data)++) = 0x82;
		*((*data)++) = (len >> 8) & 0xff;
		*((*data)++) = len & 0xff;
		*data_len -= 3;
		return 0;
	}

	if (WARN(len > 0xffffff, "ASN.1 length can't be > 0xffffff"))
		return -EINVAL;

	if (*data_len < 4)
		return -EINVAL;
	*((*data)++) = 0x83;
	*((*data)++) = (len >> 16) & 0xff;
	*((*data)++) = (len >> 8) & 0xff;
	*((*data)++) = len & 0xff;
	*data_len -= 4;

	return 0;
}

 
unsigned char *
asn1_encode_tag(unsigned char *data, const unsigned char *end_data,
		u32 tag, const unsigned char *string, int len)
{
	int data_len = end_data - data;
	int ret;

	if (WARN(tag > 30, "ASN.1 tag can't be > 30"))
		return ERR_PTR(-EINVAL);

	if (!string && WARN(len > 127,
			    "BUG: recode tag is too big (>127)"))
		return ERR_PTR(-EINVAL);

	if (IS_ERR(data))
		return data;

	if (!string && len > 0) {
		 
		data -= 2;
		data_len = 2;
	}

	if (data_len < 2)
		return ERR_PTR(-EINVAL);

	*(data++) = _tagn(CONT, CONS, tag);
	data_len--;
	ret = asn1_encode_length(&data, &data_len, len);
	if (ret < 0)
		return ERR_PTR(ret);

	if (!string)
		return data;

	if (data_len < len)
		return ERR_PTR(-EINVAL);

	memcpy(data, string, len);
	data += len;

	return data;
}
EXPORT_SYMBOL_GPL(asn1_encode_tag);

 
unsigned char *
asn1_encode_octet_string(unsigned char *data,
			 const unsigned char *end_data,
			 const unsigned char *string, u32 len)
{
	int data_len = end_data - data;
	int ret;

	if (IS_ERR(data))
		return data;

	 
	if (data_len < 2)
		return ERR_PTR(-EINVAL);

	*(data++) = _tag(UNIV, PRIM, OTS);
	data_len--;

	ret = asn1_encode_length(&data, &data_len, len);
	if (ret)
		return ERR_PTR(ret);

	if (data_len < len)
		return ERR_PTR(-EINVAL);

	memcpy(data, string, len);
	data += len;

	return data;
}
EXPORT_SYMBOL_GPL(asn1_encode_octet_string);

 
unsigned char *
asn1_encode_sequence(unsigned char *data, const unsigned char *end_data,
		     const unsigned char *seq, int len)
{
	int data_len = end_data - data;
	int ret;

	if (!seq && WARN(len > 127,
			 "BUG: recode sequence is too big (>127)"))
		return ERR_PTR(-EINVAL);

	if (IS_ERR(data))
		return data;

	if (!seq && len >= 0) {
		 
		data -= 2;
		data_len = 2;
	}

	if (data_len < 2)
		return ERR_PTR(-EINVAL);

	*(data++) = _tag(UNIV, CONS, SEQ);
	data_len--;

	ret = asn1_encode_length(&data, &data_len, len);
	if (ret)
		return ERR_PTR(ret);

	if (!seq)
		return data;

	if (data_len < len)
		return ERR_PTR(-EINVAL);

	memcpy(data, seq, len);
	data += len;

	return data;
}
EXPORT_SYMBOL_GPL(asn1_encode_sequence);

 
unsigned char *
asn1_encode_boolean(unsigned char *data, const unsigned char *end_data,
		    bool val)
{
	int data_len = end_data - data;

	if (IS_ERR(data))
		return data;

	 
	if (data_len < 3)
		return ERR_PTR(-EINVAL);

	*(data++) = _tag(UNIV, PRIM, BOOL);
	data_len--;

	asn1_encode_length(&data, &data_len, 1);

	if (val)
		*(data++) = 1;
	else
		*(data++) = 0;

	return data;
}
EXPORT_SYMBOL_GPL(asn1_encode_boolean);

MODULE_LICENSE("GPL");
