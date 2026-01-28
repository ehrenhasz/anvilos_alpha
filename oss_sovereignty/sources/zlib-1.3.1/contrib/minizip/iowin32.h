

#include <windows.h>


#ifdef __cplusplus
extern "C" {
#endif

void fill_win32_filefunc(zlib_filefunc_def* pzlib_filefunc_def);
void fill_win32_filefunc64(zlib_filefunc64_def* pzlib_filefunc_def);
void fill_win32_filefunc64A(zlib_filefunc64_def* pzlib_filefunc_def);
void fill_win32_filefunc64W(zlib_filefunc64_def* pzlib_filefunc_def);

#ifdef __cplusplus
}
#endif
