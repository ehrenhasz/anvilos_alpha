 

 

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libzfs.h>
#include <libshare.h>
#include "libshare_impl.h"
#include "smb.h"

static boolean_t smb_available(void);

static smb_share_t *smb_shares;
static int smb_disable_share(sa_share_impl_t impl_share);
static boolean_t smb_is_share_active(sa_share_impl_t impl_share);

 
static int
smb_retrieve_shares(void)
{
	int rc = SA_OK;
	char file_path[PATH_MAX], line[512], *token, *key, *value;
	char *dup_value = NULL, *path = NULL, *comment = NULL, *name = NULL;
	char *guest_ok = NULL;
	DIR *shares_dir;
	FILE *share_file_fp = NULL;
	struct dirent *directory;
	struct stat eStat;
	smb_share_t *shares, *new_shares = NULL;

	 
	shares_dir = opendir(SHARE_DIR);
	if (shares_dir == NULL)
		return (SA_SYSTEM_ERR);

	 
	while ((directory = readdir(shares_dir))) {
		int fd;

		if (directory->d_name[0] == '.')
			continue;

		snprintf(file_path, sizeof (file_path),
		    "%s/%s", SHARE_DIR, directory->d_name);

		if ((fd = open(file_path, O_RDONLY | O_CLOEXEC)) == -1) {
			rc = SA_SYSTEM_ERR;
			goto out;
		}

		if (fstat(fd, &eStat) == -1) {
			close(fd);
			rc = SA_SYSTEM_ERR;
			goto out;
		}

		if (!S_ISREG(eStat.st_mode)) {
			close(fd);
			continue;
		}

		if ((share_file_fp = fdopen(fd, "r")) == NULL) {
			close(fd);
			rc = SA_SYSTEM_ERR;
			goto out;
		}

		name = strdup(directory->d_name);
		if (name == NULL) {
			rc = SA_NO_MEMORY;
			goto out;
		}

		while (fgets(line, sizeof (line), share_file_fp)) {
			if (line[0] == '#')
				continue;

			 
			while (line[strlen(line) - 1] == '\r' ||
			    line[strlen(line) - 1] == '\n')
				line[strlen(line) - 1] = '\0';

			 
			token = strchr(line, '=');
			if (token == NULL)
				continue;

			key = line;
			value = token + 1;
			*token = '\0';

			dup_value = strdup(value);
			if (dup_value == NULL) {
				rc = SA_NO_MEMORY;
				goto out;
			}

			if (strcmp(key, "path") == 0) {
				free(path);
				path = dup_value;
			} else if (strcmp(key, "comment") == 0) {
				free(comment);
				comment = dup_value;
			} else if (strcmp(key, "guest_ok") == 0) {
				free(guest_ok);
				guest_ok = dup_value;
			} else
				free(dup_value);

			dup_value = NULL;

			if (path == NULL || comment == NULL || guest_ok == NULL)
				continue;  
			else {
				shares = (smb_share_t *)
				    malloc(sizeof (smb_share_t));
				if (shares == NULL) {
					rc = SA_NO_MEMORY;
					goto out;
				}

				(void) strlcpy(shares->name, name,
				    sizeof (shares->name));

				(void) strlcpy(shares->path, path,
				    sizeof (shares->path));

				(void) strlcpy(shares->comment, comment,
				    sizeof (shares->comment));

				shares->guest_ok = atoi(guest_ok);

				shares->next = new_shares;
				new_shares = shares;

				free(path);
				free(comment);
				free(guest_ok);

				path = NULL;
				comment = NULL;
				guest_ok = NULL;
			}
		}

out:
		if (share_file_fp != NULL) {
			fclose(share_file_fp);
			share_file_fp = NULL;
		}

		free(name);
		free(path);
		free(comment);
		free(guest_ok);

		name = NULL;
		path = NULL;
		comment = NULL;
		guest_ok = NULL;
	}
	closedir(shares_dir);

	smb_shares = new_shares;

	return (rc);
}

 
static int
smb_enable_share_one(const char *sharename, const char *sharepath)
{
	char name[SMB_NAME_MAX], comment[SMB_COMMENT_MAX];

	 
	strlcpy(name, sharename, sizeof (name));
	for (char *itr = name; *itr != '\0'; ++itr)
		switch (*itr) {
		case '/':
		case '-':
		case ':':
		case ' ':
			*itr = '_';
		}

	 
	snprintf(comment, sizeof (comment), "Comment: %s", sharepath);

	char *argv[] = {
		(char *)NET_CMD_PATH,
		(char *)"-S",
		(char *)NET_CMD_ARG_HOST,
		(char *)"usershare",
		(char *)"add",
		name,
		(char *)sharepath,
		comment,
		(char *)"Everyone:F",
		NULL,
	};

	if (libzfs_run_process(argv[0], argv, 0) != 0)
		return (SA_SYSTEM_ERR);

	 
	(void) smb_retrieve_shares();

	return (SA_OK);
}

 
static int
smb_enable_share(sa_share_impl_t impl_share)
{
	if (!smb_available())
		return (SA_SYSTEM_ERR);

	if (smb_is_share_active(impl_share))
		smb_disable_share(impl_share);

	if (impl_share->sa_shareopts == NULL)  
		return (SA_SYSTEM_ERR);

	if (strcmp(impl_share->sa_shareopts, "off") == 0)
		return (SA_OK);

	 
	return (smb_enable_share_one(impl_share->sa_zfsname,
	    impl_share->sa_mountpoint));
}

 
static int
smb_disable_share_one(const char *sharename)
{
	 
	char *argv[] = {
		(char *)NET_CMD_PATH,
		(char *)"-S",
		(char *)NET_CMD_ARG_HOST,
		(char *)"usershare",
		(char *)"delete",
		(char *)sharename,
		NULL,
	};

	if (libzfs_run_process(argv[0], argv, 0) != 0)
		return (SA_SYSTEM_ERR);
	else
		return (SA_OK);
}

 
static int
smb_disable_share(sa_share_impl_t impl_share)
{
	if (!smb_available()) {
		 
		return (SA_OK);
	}

	for (const smb_share_t *i = smb_shares; i != NULL; i = i->next)
		if (strcmp(impl_share->sa_mountpoint, i->path) == 0)
			return (smb_disable_share_one(i->name));

	return (SA_OK);
}

 
static int
smb_validate_shareopts(const char *shareopts)
{
	 
	if ((strcmp(shareopts, "off") == 0) || (strcmp(shareopts, "on") == 0))
		return (SA_OK);

	return (SA_SYNTAX_ERR);
}

 
static boolean_t
smb_is_share_active(sa_share_impl_t impl_share)
{
	if (!smb_available())
		return (B_FALSE);

	 
	smb_retrieve_shares();

	for (const smb_share_t *i = smb_shares; i != NULL; i = i->next)
		if (strcmp(impl_share->sa_mountpoint, i->path) == 0)
			return (B_TRUE);

	return (B_FALSE);
}

static int
smb_update_shares(void)
{
	 
	return (0);
}

const sa_fstype_t libshare_smb_type = {
	.enable_share = smb_enable_share,
	.disable_share = smb_disable_share,
	.is_shared = smb_is_share_active,

	.validate_shareopts = smb_validate_shareopts,
	.commit_shares = smb_update_shares,
};

 
static boolean_t
smb_available(void)
{
	static int avail;

	if (!avail) {
		struct stat statbuf;

		if (access(NET_CMD_PATH, F_OK) != 0 ||
		    lstat(SHARE_DIR, &statbuf) != 0 ||
		    !S_ISDIR(statbuf.st_mode))
			avail = -1;
		else
			avail = 1;
	}

	return (avail == 1);
}
