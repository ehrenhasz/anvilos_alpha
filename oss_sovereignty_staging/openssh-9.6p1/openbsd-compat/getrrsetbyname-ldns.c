 

 

 

#include "includes.h"

#if !defined (HAVE_GETRRSETBYNAME) && defined (HAVE_LDNS)

#include <stdlib.h>
#include <string.h>

#include <ldns/ldns.h>

#include "getrrsetbyname.h"
#include "log.h"
#include "xmalloc.h"

#define malloc(x)	(xmalloc(x))
#define calloc(x, y)	(xcalloc((x),(y)))

int
getrrsetbyname(const char *hostname, unsigned int rdclass,
	       unsigned int rdtype, unsigned int flags,
	       struct rrsetinfo **res)
{
	int result;
	unsigned int i, j, index_ans, index_sig;
	struct rrsetinfo *rrset = NULL;
	struct rdatainfo *rdata;
	size_t len;
	ldns_resolver *ldns_res = NULL;
	ldns_rdf *domain = NULL;
	ldns_pkt *pkt = NULL;
	ldns_rr_list *rrsigs = NULL, *rrdata = NULL;
	ldns_status err;
	ldns_rr *rr;

	 
	if (rdclass > 0xffff || rdtype > 0xffff) {
		result = ERRSET_INVAL;
		goto fail;
	}

	 
	if (rdclass == 0xff || rdtype == 0xff) {
		result = ERRSET_INVAL;
		goto fail;
	}

	 
	if (flags) {
		result = ERRSET_INVAL;
		goto fail;
	}

	 
	domain = ldns_dname_new_frm_str(hostname);
	if ((err = ldns_resolver_new_frm_file(&ldns_res, NULL)) != \
	    LDNS_STATUS_OK) {
		result = ERRSET_FAIL;
		goto fail;
	}

#ifdef LDNS_DEBUG
	ldns_resolver_set_debug(ldns_res, true);
#endif  

	ldns_resolver_set_dnssec(ldns_res, true);  

	 
	pkt = ldns_resolver_query(ldns_res, domain, rdtype, rdclass, LDNS_RD);

	 
	if (!pkt || ldns_pkt_ancount(pkt) < 1) {
		result = ERRSET_FAIL;
		goto fail;
	}

	 
	rrset = calloc(1, sizeof(struct rrsetinfo));
	if (rrset == NULL) {
		result = ERRSET_NOMEMORY;
		goto fail;
	}

	rrdata = ldns_pkt_rr_list_by_type(pkt, rdtype, LDNS_SECTION_ANSWER);
	rrset->rri_nrdatas = ldns_rr_list_rr_count(rrdata);
	if (!rrset->rri_nrdatas) {
		result = ERRSET_NODATA;
		goto fail;
	}

	 
	len = ldns_rdf_size(ldns_rr_owner(ldns_rr_list_rr(rrdata, 0)));
	if ((rrset->rri_name = malloc(len)) == NULL) {
		result = ERRSET_NOMEMORY;
		goto fail;
	}
	memcpy(rrset->rri_name,
	    ldns_rdf_data(ldns_rr_owner(ldns_rr_list_rr(rrdata, 0))), len);

	rrset->rri_rdclass = ldns_rr_get_class(ldns_rr_list_rr(rrdata, 0));
	rrset->rri_rdtype = ldns_rr_get_type(ldns_rr_list_rr(rrdata, 0));
	rrset->rri_ttl = ldns_rr_ttl(ldns_rr_list_rr(rrdata, 0));

	debug2("ldns: got %u answers from DNS", rrset->rri_nrdatas);

	 
	if (ldns_pkt_ad(pkt)) {
		rrset->rri_flags |= RRSET_VALIDATED;
	} else {  
		ldns_rr_list * trusted_keys = ldns_rr_list_new();

		debug2("ldns: trying to validate RRset");
		 
		rrsigs = ldns_pkt_rr_list_by_type(pkt, LDNS_RR_TYPE_RRSIG,
		    LDNS_SECTION_ANSWER);

		rrset->rri_nsigs = ldns_rr_list_rr_count(rrsigs);
		debug2("ldns: got %u signature(s) (RRTYPE %u) from DNS",
		       rrset->rri_nsigs, LDNS_RR_TYPE_RRSIG);

		if ((err = ldns_verify_trusted(ldns_res, rrdata, rrsigs,
		     trusted_keys)) == LDNS_STATUS_OK) {
			rrset->rri_flags |= RRSET_VALIDATED;
			debug2("ldns: RRset is signed with a valid key");
		} else {
			debug2("ldns: RRset validation failed: %s",
			    ldns_get_errorstr_by_id(err));
		}

		ldns_rr_list_deep_free(trusted_keys);
	}

	 
	rrset->rri_rdatas = calloc(rrset->rri_nrdatas,
	   sizeof(struct rdatainfo));

	if (rrset->rri_rdatas == NULL) {
		result = ERRSET_NOMEMORY;
		goto fail;
	}

	 
	if (rrset->rri_nsigs > 0) {
		rrset->rri_sigs = calloc(rrset->rri_nsigs,
		    sizeof(struct rdatainfo));

		if (rrset->rri_sigs == NULL) {
			result = ERRSET_NOMEMORY;
			goto fail;
		}
	}

	 
	for (i=0, index_ans=0, index_sig=0; i< pkt->_header->_ancount; i++) {
		rdata = NULL;
		rr = ldns_rr_list_rr(ldns_pkt_answer(pkt), i);

		if (ldns_rr_get_class(rr) == rrset->rri_rdclass &&
		    ldns_rr_get_type(rr) == rrset->rri_rdtype) {
			rdata = &rrset->rri_rdatas[index_ans++];
		}

		if (rr->_rr_class == rrset->rri_rdclass &&
		    rr->_rr_type == LDNS_RR_TYPE_RRSIG &&
		    rrset->rri_sigs) {
			rdata = &rrset->rri_sigs[index_sig++];
		}

		if (rdata) {
			size_t rdata_offset = 0;

			rdata->rdi_length = 0;
			for (j=0; j< rr->_rd_count; j++) {
				rdata->rdi_length +=
				    ldns_rdf_size(ldns_rr_rdf(rr, j));
			}

			rdata->rdi_data = malloc(rdata->rdi_length);
			if (rdata->rdi_data == NULL) {
				result = ERRSET_NOMEMORY;
				goto fail;
			}

			 
			for (j=0; j< rr->_rd_count; j++) {
				len = ldns_rdf_size(ldns_rr_rdf(rr, j));
				memcpy(rdata->rdi_data + rdata_offset,
				       ldns_rdf_data(ldns_rr_rdf(rr, j)), len);
				rdata_offset += len;
			}
		}
	}

	*res = rrset;
	result = ERRSET_SUCCESS;

fail:
	 
	ldns_rdf_deep_free(domain);
	ldns_pkt_free(pkt);
	ldns_rr_list_deep_free(rrsigs);
	ldns_rr_list_deep_free(rrdata);
	ldns_resolver_deep_free(ldns_res);

	return result;
}


void
freerrset(struct rrsetinfo *rrset)
{
	u_int16_t i;

	if (rrset == NULL)
		return;

	if (rrset->rri_rdatas) {
		for (i = 0; i < rrset->rri_nrdatas; i++) {
			if (rrset->rri_rdatas[i].rdi_data == NULL)
				break;
			free(rrset->rri_rdatas[i].rdi_data);
		}
		free(rrset->rri_rdatas);
	}

	if (rrset->rri_sigs) {
		for (i = 0; i < rrset->rri_nsigs; i++) {
			if (rrset->rri_sigs[i].rdi_data == NULL)
				break;
			free(rrset->rri_sigs[i].rdi_data);
		}
		free(rrset->rri_sigs);
	}

	if (rrset->rri_name)
		free(rrset->rri_name);
	free(rrset);
}


#endif  
