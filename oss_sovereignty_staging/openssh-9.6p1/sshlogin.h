 

 

void	record_login(pid_t, const char *, const char *, uid_t,
    const char *, struct sockaddr *, socklen_t);
void   record_logout(pid_t, const char *, const char *);
time_t	get_last_login_time(uid_t, const char *, char *, size_t);

#ifdef LOGIN_NEEDS_UTMPX
void	record_utmp_only(pid_t, const char *, const char *, const char *,
		struct sockaddr *, socklen_t);
#endif
