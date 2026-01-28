



#ifndef GROUPACCESS_H
#define GROUPACCESS_H

int	 ga_init(const char *, gid_t);
int	 ga_match(char * const *, int);
int	 ga_match_pattern_list(const char *);
void	 ga_free(void);

#endif
