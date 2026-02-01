
 
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/root_dev.h>
#include <linux/kernel.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <net/ipconfig.h>

#define DEFAULT_MNT_OPTS \
	"vers=1.0,cifsacl,mfsymlinks,rsize=1048576,wsize=65536,uid=0,gid=0," \
	"hard,rootfs"

static char root_dev[2048] __initdata = "";
static char root_opts[1024] __initdata = DEFAULT_MNT_OPTS;

static __be32 __init parse_srvaddr(char *start, char *end)
{
	 
	char addr[sizeof("aaa.bbb.ccc.ddd")];
	int i = 0;

	while (start < end && i < sizeof(addr) - 1) {
		if (isdigit(*start) || *start == '.')
			addr[i++] = *start;
		start++;
	}
	addr[i] = '\0';
	return in_aton(addr);
}

 
		s = strchrnul(s, ',');
		 
		len = s - line + 1;
		if (len > sizeof(root_dev)) {
			pr_err("Root-CIFS: UNC path too long\n");
			return 1;
		}
		strscpy(root_dev, line, len);
		srvaddr = parse_srvaddr(&line[2], s);
		if (*s) {
			int n = snprintf(root_opts,
					 sizeof(root_opts), "%s,%s",
					 DEFAULT_MNT_OPTS, s + 1);
			if (n >= sizeof(root_opts)) {
				pr_err("Root-CIFS: mount options string too long\n");
				root_opts[sizeof(root_opts)-1] = '\0';
				return 1;
			}
		}
	}

	root_server_addr = srvaddr;

	return 1;
}

__setup("cifsroot=", cifs_root_setup);

int __init cifs_root_data(char **dev, char **opts)
{
	if (!root_dev[0] || root_server_addr == htonl(INADDR_NONE)) {
		pr_err("Root-CIFS: no SMB server address\n");
		return -1;
	}

	*dev = root_dev;
	*opts = root_opts;

	return 0;
}
