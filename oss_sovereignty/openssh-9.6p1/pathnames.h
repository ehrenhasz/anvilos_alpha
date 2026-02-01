 

 

#define ETCDIR				"/etc"

#ifndef SSHDIR
#define SSHDIR				ETCDIR "/ssh"
#endif

#ifndef _PATH_SSH_PIDDIR
#define _PATH_SSH_PIDDIR		"/var/run"
#endif

 
#define _PATH_SSH_SYSTEM_HOSTFILE	SSHDIR "/ssh_known_hosts"
 
#define _PATH_SSH_SYSTEM_HOSTFILE2	SSHDIR "/ssh_known_hosts2"

 
#define _PATH_SERVER_CONFIG_FILE	SSHDIR "/sshd_config"
#define _PATH_HOST_CONFIG_FILE		SSHDIR "/ssh_config"
#define _PATH_HOST_DSA_KEY_FILE		SSHDIR "/ssh_host_dsa_key"
#define _PATH_HOST_ECDSA_KEY_FILE	SSHDIR "/ssh_host_ecdsa_key"
#define _PATH_HOST_ED25519_KEY_FILE	SSHDIR "/ssh_host_ed25519_key"
#define _PATH_HOST_XMSS_KEY_FILE	SSHDIR "/ssh_host_xmss_key"
#define _PATH_HOST_RSA_KEY_FILE		SSHDIR "/ssh_host_rsa_key"
#define _PATH_DH_MODULI			SSHDIR "/moduli"

#ifndef _PATH_SSH_PROGRAM
#define _PATH_SSH_PROGRAM		"/usr/bin/ssh"
#endif

 
#define _PATH_SSH_DAEMON_PID_FILE	_PATH_SSH_PIDDIR "/sshd.pid"

 
#define _PATH_SSH_USER_DIR		".ssh"

 
#define _PATH_SSH_USER_HOSTFILE		"~/" _PATH_SSH_USER_DIR "/known_hosts"
 
#define _PATH_SSH_USER_HOSTFILE2	"~/" _PATH_SSH_USER_DIR "/known_hosts2"

 
#define _PATH_SSH_CLIENT_ID_DSA		_PATH_SSH_USER_DIR "/id_dsa"
#define _PATH_SSH_CLIENT_ID_ECDSA	_PATH_SSH_USER_DIR "/id_ecdsa"
#define _PATH_SSH_CLIENT_ID_RSA		_PATH_SSH_USER_DIR "/id_rsa"
#define _PATH_SSH_CLIENT_ID_ED25519	_PATH_SSH_USER_DIR "/id_ed25519"
#define _PATH_SSH_CLIENT_ID_XMSS	_PATH_SSH_USER_DIR "/id_xmss"
#define _PATH_SSH_CLIENT_ID_ECDSA_SK	_PATH_SSH_USER_DIR "/id_ecdsa_sk"
#define _PATH_SSH_CLIENT_ID_ED25519_SK	_PATH_SSH_USER_DIR "/id_ed25519_sk"

 
#define _PATH_SSH_USER_CONFFILE		_PATH_SSH_USER_DIR "/config"

 
#define _PATH_SSH_USER_PERMITTED_KEYS	_PATH_SSH_USER_DIR "/authorized_keys"

 
#define _PATH_SSH_USER_PERMITTED_KEYS2	_PATH_SSH_USER_DIR "/authorized_keys2"

 
#define _PATH_SSH_USER_RC		_PATH_SSH_USER_DIR "/rc"
#define _PATH_SSH_SYSTEM_RC		SSHDIR "/sshrc"

 
#define _PATH_SSH_HOSTS_EQUIV		SSHDIR "/shosts.equiv"
#define _PATH_RHOSTS_EQUIV		"/etc/hosts.equiv"

 
#ifndef _PATH_SSH_ASKPASS_DEFAULT
#define _PATH_SSH_ASKPASS_DEFAULT	"/usr/X11R6/bin/ssh-askpass"
#endif

 
#ifndef _PATH_SSH_KEY_SIGN
#define _PATH_SSH_KEY_SIGN		"/usr/libexec/ssh-keysign"
#endif

 
#ifndef _PATH_SSH_PKCS11_HELPER
#define _PATH_SSH_PKCS11_HELPER		"/usr/libexec/ssh-pkcs11-helper"
#endif

 
#ifndef _PATH_SSH_SK_HELPER
#define _PATH_SSH_SK_HELPER		"/usr/libexec/ssh-sk-helper"
#endif

 
#ifndef _PATH_XAUTH
#define _PATH_XAUTH			"/usr/X11R6/bin/xauth"
#endif

 
#ifndef _PATH_UNIX_X
#define _PATH_UNIX_X "/tmp/.X11-unix/X%u"
#endif

 
#ifndef _PATH_CP
#define _PATH_CP			"cp"
#endif

 
#ifndef _PATH_SFTP_SERVER
#define _PATH_SFTP_SERVER		"/usr/libexec/sftp-server"
#endif

 
#ifndef _PATH_PRIVSEP_CHROOT_DIR
#define _PATH_PRIVSEP_CHROOT_DIR	"/var/empty"
#endif

 
#ifndef _PATH_PASSWD_PROG
#define _PATH_PASSWD_PROG             "/usr/bin/passwd"
#endif

#ifndef _PATH_LS
#define _PATH_LS			"ls"
#endif

 
#ifndef ASKPASS_PROGRAM
#define ASKPASS_PROGRAM         "/usr/lib/ssh/ssh-askpass"
#endif  
