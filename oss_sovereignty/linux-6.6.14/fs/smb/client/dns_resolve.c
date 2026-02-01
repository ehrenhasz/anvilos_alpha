
 

#include <linux/inet.h>
#include <linux/slab.h>
#include <linux/dns_resolver.h>
#include "dns_resolve.h"
#include "cifsglob.h"
#include "cifsproto.h"
#include "cifs_debug.h"

 
int
dns_resolve_server_name_to_ip(const char *unc, struct sockaddr *ip_addr, time64_t *expiry)
{
	const char *hostname, *sep;
	char *ip;
	int len, rc;

	if (!ip_addr || !unc)
		return -EINVAL;

	len = strlen(unc);
	if (len < 3) {
		cifs_dbg(FYI, "%s: unc is too short: %s\n", __func__, unc);
		return -EINVAL;
	}

	 
	len -= 2;
	hostname = unc + 2;

	 
	sep = memchr(hostname, '/', len);
	if (sep)
		len = sep - hostname;
	else
		cifs_dbg(FYI, "%s: probably server name is whole unc: %s\n",
			 __func__, unc);

	 
	rc = cifs_convert_address(ip_addr, hostname, len);
	if (rc > 0) {
		cifs_dbg(FYI, "%s: unc is IP, skipping dns upcall: %*.*s\n", __func__, len, len,
			 hostname);
		return 0;
	}

	 
	rc = dns_query(current->nsproxy->net_ns, NULL, hostname, len,
		       NULL, &ip, expiry, false);
	if (rc < 0) {
		cifs_dbg(FYI, "%s: unable to resolve: %*.*s\n",
			 __func__, len, len, hostname);
	} else {
		cifs_dbg(FYI, "%s: resolved: %*.*s to %s expiry %llu\n",
			 __func__, len, len, hostname, ip,
			 expiry ? (*expiry) : 0);

		rc = cifs_convert_address(ip_addr, ip, strlen(ip));
		kfree(ip);

		if (!rc) {
			cifs_dbg(FYI, "%s: unable to determine ip address\n", __func__);
			rc = -EHOSTUNREACH;
		} else
			rc = 0;
	}
	return rc;
}
