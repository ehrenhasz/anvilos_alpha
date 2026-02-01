 

#ifndef LONG_OPTIONS_H_
# define LONG_OPTIONS_H_ 1

void parse_long_options (int _argc,
                         char **_argv,
                         const char *_command_name,
                         const char *_package,
                         const char *_version,
                         void (*_usage) (int),
                           ...);

void parse_gnu_standard_options_only (int argc,
                                      char **argv,
                                      const char *command_name,
                                      const char *package,
                                      const char *version,
                                      bool scan_all,
                                      void (*usage_func) (int),
                                        ...);

#endif  
