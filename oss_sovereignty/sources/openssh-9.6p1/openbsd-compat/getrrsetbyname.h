







#ifndef _GETRRSETBYNAME_H
#define _GETRRSETBYNAME_H

#include "includes.h"

#ifndef HAVE_GETRRSETBYNAME

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <netdb.h>
#include <resolv.h>

#ifndef HFIXEDSZ
#define HFIXEDSZ 12
#endif

#ifndef T_RRSIG
#define T_RRSIG 46
#endif


#ifndef RRSET_VALIDATED
# define RRSET_VALIDATED	1
#endif


#ifndef ERRSET_SUCCESS
# define ERRSET_SUCCESS		0
# define ERRSET_NOMEMORY	1
# define ERRSET_FAIL		2
# define ERRSET_INVAL		3
# define ERRSET_NONAME		4
# define ERRSET_NODATA		5
#endif

struct rdatainfo {
	unsigned int		rdi_length;	
	unsigned char		*rdi_data;	
};

struct rrsetinfo {
	unsigned int		rri_flags;	
	unsigned int		rri_rdclass;	
	unsigned int		rri_rdtype;	
	unsigned int		rri_ttl;	
	unsigned int		rri_nrdatas;	
	unsigned int		rri_nsigs;	
	char			*rri_name;	
	struct rdatainfo	*rri_rdatas;	
	struct rdatainfo	*rri_sigs;	
};

int		getrrsetbyname(const char *, unsigned int, unsigned int, unsigned int, struct rrsetinfo **);
void		freerrset(struct rrsetinfo *);

#endif 

#endif 
