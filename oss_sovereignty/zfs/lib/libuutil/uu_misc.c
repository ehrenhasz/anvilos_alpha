#include "libuutil_common.h"
#include <assert.h>
#include <errno.h>
#include <libintl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/debug.h>
#include <unistd.h>
#include <ctype.h>
#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN "SYS_TEST"
#endif
#if defined(PTHREAD_ONCE_KEY_NP)
static pthread_key_t	uu_error_key = PTHREAD_ONCE_KEY_NP;
#else	 
static pthread_key_t	uu_error_key = 0;
static pthread_mutex_t	uu_key_lock = PTHREAD_MUTEX_INITIALIZER;
#endif	 
static int		uu_error_key_setup = 0;
static pthread_mutex_t	uu_panic_lock = PTHREAD_MUTEX_INITIALIZER;
static const char	*uu_panic_format;
static va_list		uu_panic_args;
static pthread_t	uu_panic_thread;
static uint32_t		_uu_main_error;
static __thread int	_uu_main_thread = 0;
void
uu_set_error(uint_t code)
{
	if (_uu_main_thread) {
		_uu_main_error = code;
		return;
	}
#if defined(PTHREAD_ONCE_KEY_NP)
	if (pthread_key_create_once_np(&uu_error_key, NULL) != 0)
		uu_error_key_setup = -1;
	else
		uu_error_key_setup = 1;
#else	 
	if (uu_error_key_setup == 0) {
		(void) pthread_mutex_lock(&uu_key_lock);
		if (uu_error_key_setup == 0) {
			if (pthread_key_create(&uu_error_key, NULL) != 0)
				uu_error_key_setup = -1;
			else
				uu_error_key_setup = 1;
		}
		(void) pthread_mutex_unlock(&uu_key_lock);
	}
#endif	 
	if (uu_error_key_setup > 0)
		(void) pthread_setspecific(uu_error_key,
		    (void *)(uintptr_t)code);
}
uint32_t
uu_error(void)
{
	if (_uu_main_thread)
		return (_uu_main_error);
	if (uu_error_key_setup < 0)	 
		return (UU_ERROR_UNKNOWN);
	return ((uint32_t)(uintptr_t)pthread_getspecific(uu_error_key));
}
const char *
uu_strerror(uint32_t code)
{
	const char *str;
	switch (code) {
	case UU_ERROR_NONE:
		str = dgettext(TEXT_DOMAIN, "No error");
		break;
	case UU_ERROR_INVALID_ARGUMENT:
		str = dgettext(TEXT_DOMAIN, "Invalid argument");
		break;
	case UU_ERROR_UNKNOWN_FLAG:
		str = dgettext(TEXT_DOMAIN, "Unknown flag passed");
		break;
	case UU_ERROR_NO_MEMORY:
		str = dgettext(TEXT_DOMAIN, "Out of memory");
		break;
	case UU_ERROR_CALLBACK_FAILED:
		str = dgettext(TEXT_DOMAIN, "Callback-initiated failure");
		break;
	case UU_ERROR_NOT_SUPPORTED:
		str = dgettext(TEXT_DOMAIN, "Operation not supported");
		break;
	case UU_ERROR_EMPTY:
		str = dgettext(TEXT_DOMAIN, "No value provided");
		break;
	case UU_ERROR_UNDERFLOW:
		str = dgettext(TEXT_DOMAIN, "Value too small");
		break;
	case UU_ERROR_OVERFLOW:
		str = dgettext(TEXT_DOMAIN, "Value too large");
		break;
	case UU_ERROR_INVALID_CHAR:
		str = dgettext(TEXT_DOMAIN,
		    "Value contains unexpected character");
		break;
	case UU_ERROR_INVALID_DIGIT:
		str = dgettext(TEXT_DOMAIN,
		    "Value contains digit not in base");
		break;
	case UU_ERROR_SYSTEM:
		str = dgettext(TEXT_DOMAIN, "Underlying system error");
		break;
	case UU_ERROR_UNKNOWN:
		str = dgettext(TEXT_DOMAIN, "Error status not known");
		break;
	default:
		errno = ESRCH;
		str = NULL;
		break;
	}
	return (str);
}
void
uu_panic(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	(void) pthread_mutex_lock(&uu_panic_lock);
	if (uu_panic_thread == 0) {
		uu_panic_thread = pthread_self();
		uu_panic_format = format;
		va_copy(uu_panic_args, args);
	}
	(void) pthread_mutex_unlock(&uu_panic_lock);
	(void) vfprintf(stderr, format, args);
	va_end(args);
	if (uu_panic_thread == pthread_self())
		abort();
	else
		for (;;)
			(void) pause();
}
static void
uu_lockup(void)
{
	(void) pthread_mutex_lock(&uu_panic_lock);
#if !defined(PTHREAD_ONCE_KEY_NP)
	(void) pthread_mutex_lock(&uu_key_lock);
#endif
	uu_avl_lockup();
	uu_list_lockup();
}
static void
uu_release(void)
{
	(void) pthread_mutex_unlock(&uu_panic_lock);
#if !defined(PTHREAD_ONCE_KEY_NP)
	(void) pthread_mutex_unlock(&uu_key_lock);
#endif
	uu_avl_release();
	uu_list_release();
}
static void
uu_release_child(void)
{
	uu_panic_format = NULL;
	uu_panic_thread = 0;
	uu_release();
}
#ifdef __GNUC__
static void
uu_init(void) __attribute__((constructor));
#else
#pragma init(uu_init)
#endif
static void
uu_init(void)
{
	_uu_main_thread = 1;
	(void) pthread_atfork(uu_lockup, uu_release, uu_release_child);
}
