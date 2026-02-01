 

 

#ifndef _TEST_H_
#define _TEST_H_

#include "stdc.h"

 
#define TEST_PATMATCH	0x01
#define TEST_ARITHEXP	0x02
#define TEST_LOCALE	0x04
#define TEST_ARRAYEXP	0x08		 

extern int test_unop PARAMS((char *));
extern int test_binop PARAMS((char *));

extern int unary_test PARAMS((char *, char *, int));
extern int binary_test PARAMS((char *, char *, char *, int));

extern int test_command PARAMS((int, char **));

#endif  
