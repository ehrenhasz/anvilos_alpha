 

#include "config.h"
#include "includes.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "log.h"

#ifdef USE_SOLARIS_PROCESS_CONTRACTS

#include <libcontract.h>
#include <sys/contract/process.h>
#include <sys/ctfs.h>

#define CT_TEMPLATE	CTFS_ROOT "/process/template"
#define CT_LATEST	CTFS_ROOT "/process/latest"

static int tmpl_fd = -1;

 
static ctid_t
get_active_process_contract_id(void)
{
	int stat_fd;
	ctid_t ctid = -1;
	ct_stathdl_t stathdl;

	if ((stat_fd = open64(CT_LATEST, O_RDONLY)) == -1) {
		error("%s: Error opening 'latest' process "
		    "contract: %s", __func__, strerror(errno));
		return -1;
	}
	if (ct_status_read(stat_fd, CTD_COMMON, &stathdl) != 0) {
		error("%s: Error reading process contract "
		    "status: %s", __func__, strerror(errno));
		goto out;
	}
	if ((ctid = ct_status_get_id(stathdl)) < 0) {
		error("%s: Error getting process contract id: %s",
		    __func__, strerror(errno));
		goto out;
	}

	ct_status_free(stathdl);
 out:
	close(stat_fd);
	return ctid;
}

void
solaris_contract_pre_fork(void)
{
	if ((tmpl_fd = open64(CT_TEMPLATE, O_RDWR)) == -1) {
		error("%s: open %s: %s", __func__,
		    CT_TEMPLATE, strerror(errno));
		return;
	}

	debug2("%s: setting up process contract template on fd %d",
	    __func__, tmpl_fd);

	 
	if (ct_pr_tmpl_set_param(tmpl_fd, CT_PR_PGRPONLY) != 0) {
		error("%s: Error setting process contract parameter set "
		    "(pgrponly): %s", __func__, strerror(errno));
		goto fail;
	}
	if (ct_pr_tmpl_set_fatal(tmpl_fd, CT_PR_EV_HWERR) != 0) {
		error("%s: Error setting process contract template "
		    "fatal events: %s", __func__, strerror(errno));
		goto fail;
	}
	if (ct_tmpl_set_critical(tmpl_fd, 0) != 0) {
		error("%s: Error setting process contract template "
		    "critical events: %s", __func__, strerror(errno));
		goto fail;
	}
	if (ct_tmpl_set_informative(tmpl_fd, CT_PR_EV_HWERR) != 0) {
		error("%s: Error setting process contract template "
		    "informative events: %s", __func__, strerror(errno));
		goto fail;
	}

	 
	if (ct_tmpl_activate(tmpl_fd) != 0) {
		error("%s: Error activating process contract "
		    "template: %s", __func__, strerror(errno));
		goto fail;
	}
	return;

 fail:
	if (tmpl_fd != -1) {
		close(tmpl_fd);
		tmpl_fd = -1;
	}
}

void
solaris_contract_post_fork_child()
{
	debug2("%s: clearing process contract template on fd %d",
	    __func__, tmpl_fd);

	 
	if (ct_tmpl_clear(tmpl_fd) != 0)
		error("%s: Error clearing active process contract "
		    "template: %s", __func__, strerror(errno));

	close(tmpl_fd);
	tmpl_fd = -1;
}

void
solaris_contract_post_fork_parent(pid_t pid)
{
	ctid_t ctid;
	char ctl_path[256];
	int r, ctl_fd = -1, stat_fd = -1;

	debug2("%s: clearing template (fd %d)", __func__, tmpl_fd);

	if (tmpl_fd == -1)
		return;

	 
	if ((r = ct_tmpl_clear(tmpl_fd)) != 0)
		error("%s: Error clearing active process contract "
		    "template: %s", __func__, strerror(errno));

	close(tmpl_fd);
	tmpl_fd = -1;

	 
	if (r != 0 || pid <= 0)
		return;

	 
	ctid = get_active_process_contract_id();

	debug2("%s: abandoning contract id %ld", __func__, ctid);

	snprintf(ctl_path, sizeof(ctl_path),
	    CTFS_ROOT "/process/%ld/ctl", ctid);
	if ((ctl_fd = open64(ctl_path, O_WRONLY)) < 0) {
		error("%s: Error opening process contract "
		    "ctl file: %s", __func__, strerror(errno));
		goto fail;
	}
	if (ct_ctl_abandon(ctl_fd) < 0) {
		error("%s: Error abandoning process contract: %s",
		    __func__, strerror(errno));
		goto fail;
	}
	close(ctl_fd);
	return;

 fail:
	if (tmpl_fd != -1) {
		close(tmpl_fd);
		tmpl_fd = -1;
	}
	if (stat_fd != -1)
		close(stat_fd);
	if (ctl_fd != -1)
		close(ctl_fd);
}
#endif

#ifdef USE_SOLARIS_PROJECTS
#include <sys/task.h>
#include <project.h>

 
void
solaris_set_default_project(struct passwd *pw)
{
	struct project  *defaultproject;
	struct project   tempproject;
	char buf[1024];

	 
	if ((defaultproject = getdefaultproj(pw->pw_name, &tempproject, &buf,
	    sizeof(buf))) != NULL) {
		 
		if (setproject(defaultproject->pj_name, pw->pw_name,
		    TASK_NORMAL) != 0)
			debug("setproject(%s): %s", defaultproject->pj_name,
			    strerror(errno));
	} else {
		 
		debug("getdefaultproj(%s): %s", pw->pw_name, strerror(errno));
	}
}
#endif  

#ifdef USE_SOLARIS_PRIVS
# ifdef HAVE_PRIV_H
#  include <priv.h>
# endif

priv_set_t *
solaris_basic_privset(void)
{
	priv_set_t *pset;

#ifdef HAVE_PRIV_BASICSET
	if ((pset = priv_allocset()) == NULL) {
		error("priv_allocset: %s", strerror(errno));
		return NULL;
	}
	priv_basicset(pset);
#else
	if ((pset = priv_str_to_set("basic", ",", NULL)) == NULL) {
		error("priv_str_to_set: %s", strerror(errno));
		return NULL;
	}
#endif
	return pset;
}

void
solaris_drop_privs_pinfo_net_fork_exec(void)
{
	priv_set_t *pset = NULL, *npset = NULL;

	 

	if ((pset = priv_allocset()) == NULL)
		fatal("priv_allocset: %s", strerror(errno));
	if ((npset = solaris_basic_privset()) == NULL)
		fatal("solaris_basic_privset: %s", strerror(errno));

	if (priv_addset(npset, PRIV_FILE_CHOWN) != 0 ||
	    priv_addset(npset, PRIV_FILE_DAC_READ) != 0 ||
	    priv_addset(npset, PRIV_FILE_DAC_SEARCH) != 0 ||
	    priv_addset(npset, PRIV_FILE_DAC_WRITE) != 0 ||
	    priv_addset(npset, PRIV_FILE_OWNER) != 0)
		fatal("priv_addset: %s", strerror(errno));

	if (priv_delset(npset, PRIV_PROC_EXEC) != 0 ||
#ifdef PRIV_NET_ACCESS
	    priv_delset(npset, PRIV_NET_ACCESS) != 0 ||
#endif
	    priv_delset(npset, PRIV_PROC_FORK) != 0 ||
	    priv_delset(npset, PRIV_PROC_INFO) != 0 ||
	    priv_delset(npset, PRIV_PROC_SESSION) != 0)
		fatal("priv_delset: %s", strerror(errno));

#ifdef PRIV_XPOLICY
	 
	if (getpflags(PRIV_XPOLICY) == 1) {
		if (getppriv(PRIV_LIMIT, pset) != 0)
			fatal("getppriv: %s", strerror(errno));
		priv_intersect(pset, npset);
		if (setppriv(PRIV_SET, PRIV_LIMIT, npset) != 0)
			fatal("setppriv: %s", strerror(errno));
	} else
#endif
	{
		 
		priv_emptyset(pset);
		if (setppriv(PRIV_SET, PRIV_LIMIT, pset) != 0)
			fatal("setppriv: %s", strerror(errno));
	}

	if (getppriv(PRIV_PERMITTED, pset) != 0)
		fatal("getppriv: %s", strerror(errno));

	priv_intersect(pset, npset);

	if (setppriv(PRIV_SET, PRIV_PERMITTED, npset) != 0 ||
	    setppriv(PRIV_SET, PRIV_INHERITABLE, npset) != 0)
		fatal("setppriv: %s", strerror(errno));

	priv_freeset(pset);
	priv_freeset(npset);
}

void
solaris_drop_privs_root_pinfo_net(void)
{
	priv_set_t *pset = NULL;

	 
	if ((pset = solaris_basic_privset()) == NULL)
		fatal("solaris_basic_privset: %s", strerror(errno));

	if (priv_delset(pset, PRIV_FILE_LINK_ANY) != 0 ||
#ifdef PRIV_NET_ACCESS
	    priv_delset(pset, PRIV_NET_ACCESS) != 0 ||
#endif
	    priv_delset(pset, PRIV_PROC_INFO) != 0 ||
	    priv_delset(pset, PRIV_PROC_SESSION) != 0)
		fatal("priv_delset: %s", strerror(errno));

	if (setppriv(PRIV_SET, PRIV_PERMITTED, pset) != 0 ||
	    setppriv(PRIV_SET, PRIV_LIMIT, pset) != 0 ||
	    setppriv(PRIV_SET, PRIV_INHERITABLE, pset) != 0)
		fatal("setppriv: %s", strerror(errno));

	priv_freeset(pset);
}

void
solaris_drop_privs_root_pinfo_net_exec(void)
{
	priv_set_t *pset = NULL;


	 
	if ((pset = solaris_basic_privset()) == NULL)
		fatal("solaris_basic_privset: %s", strerror(errno));

	if (priv_delset(pset, PRIV_FILE_LINK_ANY) != 0 ||
#ifdef PRIV_NET_ACCESS
	    priv_delset(pset, PRIV_NET_ACCESS) != 0 ||
#endif
	    priv_delset(pset, PRIV_PROC_EXEC) != 0 ||
	    priv_delset(pset, PRIV_PROC_INFO) != 0)
		fatal("priv_delset: %s", strerror(errno));

	if (setppriv(PRIV_SET, PRIV_PERMITTED, pset) != 0 ||
	    setppriv(PRIV_SET, PRIV_LIMIT, pset) != 0 ||
	    setppriv(PRIV_SET, PRIV_INHERITABLE, pset) != 0)
		fatal("setppriv: %s", strerror(errno));

	priv_freeset(pset);
}

#endif
