 

 
#ifndef MATCH_H
#define MATCH_H

int	 match_pattern(const char *, const char *);
int	 match_pattern_list(const char *, const char *, int);
int	 match_usergroup_pattern_list(const char *, const char *);
int	 match_hostname(const char *, const char *);
int	 match_host_and_ip(const char *, const char *, const char *);
int	 match_user(const char *, const char *, const char *, const char *);
char	*match_list(const char *, const char *, u_int *);
char	*match_filter_denylist(const char *, const char *);
char	*match_filter_allowlist(const char *, const char *);

 
int	 addr_match_list(const char *, const char *);
int	 addr_match_cidr_list(const char *, const char *);
#endif
