 
#@INCLUDE_NEXT@ @NEXT_ERRNO_H@

#ifndef _@GUARD_PREFIX@_ERRNO_H
#define _@GUARD_PREFIX@_ERRNO_H


 
# if defined _WIN32 && ! defined __CYGWIN__

 

#  ifndef ENOMSG
#   define ENOMSG    122
#   define GNULIB_defined_ENOMSG 1
#  endif

#  ifndef EIDRM
#   define EIDRM     111
#   define GNULIB_defined_EIDRM 1
#  endif

#  ifndef ENOLINK
#   define ENOLINK   121
#   define GNULIB_defined_ENOLINK 1
#  endif

#  ifndef EPROTO
#   define EPROTO    134
#   define GNULIB_defined_EPROTO 1
#  endif

#  ifndef EBADMSG
#   define EBADMSG   104
#   define GNULIB_defined_EBADMSG 1
#  endif

#  ifndef EOVERFLOW
#   define EOVERFLOW 132
#   define GNULIB_defined_EOVERFLOW 1
#  endif

#  ifndef ENOTSUP
#   define ENOTSUP   129
#   define GNULIB_defined_ENOTSUP 1
#  endif

#  ifndef ENETRESET
#   define ENETRESET 117
#   define GNULIB_defined_ENETRESET 1
#  endif

#  ifndef ECONNABORTED
#   define ECONNABORTED 106
#   define GNULIB_defined_ECONNABORTED 1
#  endif

#  ifndef ECANCELED
#   define ECANCELED 105
#   define GNULIB_defined_ECANCELED 1
#  endif

#  ifndef EOWNERDEAD
#   define EOWNERDEAD 133
#   define GNULIB_defined_EOWNERDEAD 1
#  endif

#  ifndef ENOTRECOVERABLE
#   define ENOTRECOVERABLE 127
#   define GNULIB_defined_ENOTRECOVERABLE 1
#  endif

#  ifndef EINPROGRESS
#   define EINPROGRESS     112
#   define EALREADY        103
#   define ENOTSOCK        128
#   define EDESTADDRREQ    109
#   define EMSGSIZE        115
#   define EPROTOTYPE      136
#   define ENOPROTOOPT     123
#   define EPROTONOSUPPORT 135
#   define EOPNOTSUPP      130
#   define EAFNOSUPPORT    102
#   define EADDRINUSE      100
#   define EADDRNOTAVAIL   101
#   define ENETDOWN        116
#   define ENETUNREACH     118
#   define ECONNRESET      108
#   define ENOBUFS         119
#   define EISCONN         113
#   define ENOTCONN        126
#   define ETIMEDOUT       138
#   define ECONNREFUSED    107
#   define ELOOP           114
#   define EHOSTUNREACH    110
#   define EWOULDBLOCK     140
#   define GNULIB_defined_ESOCK 1
#  endif

#  ifndef ETXTBSY
#   define ETXTBSY         139
#   define ENODATA         120   
#   define ENOSR           124   
#   define ENOSTR          125   
#   define ETIME           137   
#   define EOTHER          131   
#   define GNULIB_defined_ESTREAMS 1
#  endif

 
#  define ESOCKTNOSUPPORT 10044   
#  define EPFNOSUPPORT    10046   
#  define ESHUTDOWN       10058   
#  define ETOOMANYREFS    10059   
#  define EHOSTDOWN       10064   
#  define EPROCLIM        10067   
#  define EUSERS          10068   
#  define EDQUOT          10069
#  define ESTALE          10070
#  define EREMOTE         10071   
#  define GNULIB_defined_EWINSOCK 1

# endif


 
# if @EMULTIHOP_HIDDEN@
#  define EMULTIHOP @EMULTIHOP_VALUE@
#  define GNULIB_defined_EMULTIHOP 1
# endif
# if @ENOLINK_HIDDEN@
#  define ENOLINK   @ENOLINK_VALUE@
#  define GNULIB_defined_ENOLINK 1
# endif
# if @EOVERFLOW_HIDDEN@
#  define EOVERFLOW @EOVERFLOW_VALUE@
#  define GNULIB_defined_EOVERFLOW 1
# endif


 

# ifndef ENOMSG
#  define ENOMSG    2000
#  define GNULIB_defined_ENOMSG 1
# endif

# ifndef EIDRM
#  define EIDRM     2001
#  define GNULIB_defined_EIDRM 1
# endif

# ifndef ENOLINK
#  define ENOLINK   2002
#  define GNULIB_defined_ENOLINK 1
# endif

# ifndef EPROTO
#  define EPROTO    2003
#  define GNULIB_defined_EPROTO 1
# endif

# ifndef EMULTIHOP
#  define EMULTIHOP 2004
#  define GNULIB_defined_EMULTIHOP 1
# endif

# ifndef EBADMSG
#  define EBADMSG   2005
#  define GNULIB_defined_EBADMSG 1
# endif

# ifndef EOVERFLOW
#  define EOVERFLOW 2006
#  define GNULIB_defined_EOVERFLOW 1
# endif

# ifndef ENOTSUP
#  define ENOTSUP   2007
#  define GNULIB_defined_ENOTSUP 1
# endif

# ifndef ENETRESET
#  define ENETRESET 2011
#  define GNULIB_defined_ENETRESET 1
# endif

# ifndef ECONNABORTED
#  define ECONNABORTED 2012
#  define GNULIB_defined_ECONNABORTED 1
# endif

# ifndef ESTALE
#  define ESTALE    2009
#  define GNULIB_defined_ESTALE 1
# endif

# ifndef EDQUOT
#  define EDQUOT 2010
#  define GNULIB_defined_EDQUOT 1
# endif

# ifndef ECANCELED
#  define ECANCELED 2008
#  define GNULIB_defined_ECANCELED 1
# endif

 

# ifndef EOWNERDEAD
#  if defined __sun
     
#   define EOWNERDEAD      58
#   define ENOTRECOVERABLE 59
#  elif defined _WIN32 && ! defined __CYGWIN__
     
#   if defined __MINGW32__ && !defined USE_WINDOWS_THREADS
      
#    define EOWNERDEAD      43
#    define ENOTRECOVERABLE 44
#   else
      
#    define EOWNERDEAD      133
#    define ENOTRECOVERABLE 127
#   endif
#  else
#   define EOWNERDEAD      2013
#   define ENOTRECOVERABLE 2014
#  endif
#  define GNULIB_defined_EOWNERDEAD 1
#  define GNULIB_defined_ENOTRECOVERABLE 1
# endif

# ifndef EILSEQ
#  define EILSEQ 2015
#  define GNULIB_defined_EILSEQ 1
# endif

#endif  
#endif  
