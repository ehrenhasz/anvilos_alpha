


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifndef UNIX_LINT
# include <io.h>
# include <ctype.h>

# include <direct.h>

# include <windows.h>
# include <shlobj.h>
#endif

#ifdef UNIX_LINT

char *searchpath(char *name);
#endif

#if defined(UNIX_LINT)
# include <unistd.h>
# include <errno.h>
#endif

#include "version.h"

#if defined(UNIX_LINT)
# define vim_mkdir(x, y) mkdir((char *)(x), y)
#else
# define vim_mkdir(x, y) _mkdir((char *)(x))
#endif

#define sleep(n) Sleep((n) * 1000)




#define BUFSIZE (MAX_PATH*2)		
#define NUL 0

#define FAIL 0
#define OK 1

#ifndef FALSE
# define FALSE 0
#endif
#ifndef TRUE
# define TRUE 1
#endif


# ifndef KEY_WOW64_64KEY
#  define KEY_WOW64_64KEY 0x0100
# endif
# ifndef KEY_WOW64_32KEY
#  define KEY_WOW64_32KEY 0x0200
# endif

#ifdef __MINGW32__
# define UNUSED __attribute__((unused))
#else
# define UNUSED
#endif

#define VIM_STARTMENU "Programs\\Vim " VIM_VERSION_SHORT

int	interactive;		


    static void *
alloc(int len)
{
    void *p;

    p = malloc(len);
    if (p == NULL)
    {
	printf("ERROR: out of memory\n");
	exit(1);
    }
    return p;
}


    static int
mytoupper(int c)
{
    if (c >= 'a' && c <= 'z')
	return c - 'a' + 'A';
    return c;
}

    static void
myexit(int n)
{
    if (!interactive)
    {
	
	printf("Press Enter to continue\n");
	rewind(stdin);
	(void)getchar();
    }
    exit(n);
}


typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS)(HANDLE, PBOOL);

    static BOOL
is_64bit_os(void)
{
#ifdef _WIN64
    return TRUE;
#else
    BOOL bIsWow64 = FALSE;
    LPFN_ISWOW64PROCESS pIsWow64Process;

    pIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(
	    GetModuleHandle("kernel32"), "IsWow64Process");
    if (pIsWow64Process != NULL)
	pIsWow64Process(GetCurrentProcess(), &bIsWow64);
    return bIsWow64;
#endif
}

    static char *
searchpath(char *name)
{
    static char widename[2 * BUFSIZE];
    static char location[2 * BUFSIZE + 2];

    
    
    MultiByteToWideChar(CP_ACP, 0, (LPCTSTR)name, -1,
	    (LPWSTR)widename, BUFSIZE);
    if (FindExecutableW((LPCWSTR)widename, (LPCWSTR)"",
		(LPWSTR)location) > (HINSTANCE)32)
    {
	WideCharToMultiByte(CP_ACP, 0, (LPWSTR)location, -1,
		(LPSTR)widename, 2 * BUFSIZE, NULL, NULL);
	return widename;
    }
    return NULL;
}


    static char *
searchpath_save(char *name)
{
    char	*p;
    char	*s;

    p = searchpath(name);
    if (p == NULL)
	return NULL;
    s = alloc(strlen(p) + 1);
    strcpy(s, p);
    return s;
}


#ifndef CSIDL_COMMON_PROGRAMS
# define CSIDL_COMMON_PROGRAMS 0x0017
#endif
#ifndef CSIDL_COMMON_DESKTOPDIRECTORY
# define CSIDL_COMMON_DESKTOPDIRECTORY 0x0019
#endif


    int
get_shell_folder_path(
	char *shell_folder_path,
	const char *shell_folder_name)
{
    
    LPITEMIDLIST    pidl = 0; 
    LPMALLOC	    pMalloc;  
    int		    csidl;
    int		    alt_csidl = -1;
    static int	    desktop_csidl = -1;
    static int	    programs_csidl = -1;
    int		    *pcsidl;
    int		    r;

    if (strcmp(shell_folder_name, "desktop") == 0)
    {
	pcsidl = &desktop_csidl;
	csidl = CSIDL_COMMON_DESKTOPDIRECTORY;
	alt_csidl = CSIDL_DESKTOP;
    }
    else if (strncmp(shell_folder_name, "Programs", 8) == 0)
    {
	pcsidl = &programs_csidl;
	csidl = CSIDL_COMMON_PROGRAMS;
	alt_csidl = CSIDL_PROGRAMS;
    }
    else
    {
	printf("\nERROR (internal) unrecognised shell_folder_name: \"%s\"\n\n",
							   shell_folder_name);
	return FAIL;
    }

    
    if (*pcsidl >= 0)
    {
	csidl = *pcsidl;
	alt_csidl = -1;
    }

retry:
    
    if (NOERROR != SHGetMalloc(&pMalloc))
    {
	printf("\nERROR getting interface for shell_folder_name: \"%s\"\n\n",
							   shell_folder_name);
	return FAIL;
    }

    
    if (NOERROR != SHGetSpecialFolderLocation(0, csidl, &pidl))
    {
	if (alt_csidl < 0 || NOERROR != SHGetSpecialFolderLocation(0,
							    alt_csidl, &pidl))
	{
	    printf("\nERROR getting ITEMIDLIST for shell_folder_name: \"%s\"\n\n",
							   shell_folder_name);
	    return FAIL;
	}
	csidl = alt_csidl;
	alt_csidl = -1;
    }

    
    r = SHGetPathFromIDList(pidl, shell_folder_path);

    
    pMalloc->lpVtbl->Free(pMalloc, pidl);
    
    pMalloc->lpVtbl->Release(pMalloc);

    if (!r)
    {
	if (alt_csidl >= 0)
	{
	    
	    
	    csidl = alt_csidl;
	    alt_csidl = -1;
	    goto retry;
	}
	printf("\nERROR translating ITEMIDLIST for shell_folder_name: \"%s\"\n\n",
							   shell_folder_name);
	return FAIL;
    }

    
    
    
    if (alt_csidl >= 0)
    {
	char tbuf[BUFSIZE];
	FILE *fd;

	strcpy(tbuf, shell_folder_path);
	strcat(tbuf, "\\vim write test");
	fd = fopen(tbuf, "w");
	if (fd == NULL)
	{
	    csidl = alt_csidl;
	    alt_csidl = -1;
	    goto retry;
	}
	fclose(fd);
	unlink(tbuf);
    }

    
    if (*pcsidl < 0)
	*pcsidl = csidl;

    if (strncmp(shell_folder_name, "Programs\\", 9) == 0)
	strcat(shell_folder_path, shell_folder_name + 8);

    return OK;
}


#define TARGET_COUNT  9

struct
{
    char	*name;		
    char	*batname;	
    char	*lnkname;	
    char	*exename;	
    char	*exenamearg;	
    char	*exearg;	
    char	*oldbat;	
    char	*oldexe;	
    char	batpath[BUFSIZE];  
				   
} targets[TARGET_COUNT] =
{
    {"all",	"batch files", NULL, NULL, NULL, NULL, NULL, NULL, ""},
    {"vim",	"vim.bat",	"Vim.lnk",
			"vim.exe",    "vim.exe",  "", NULL, NULL, ""},
    {"gvim",	"gvim.bat",	"gVim.lnk",
			"gvim.exe",   "gvim.exe", "", NULL, NULL, ""},
    {"evim",	"evim.bat",	"gVim Easy.lnk",
			"evim.exe",   "gvim.exe", "-y", NULL, NULL, ""},
    {"view",	"view.bat",	"Vim Read-only.lnk",
			"view.exe",   "vim.exe",  "-R", NULL, NULL, ""},
    {"gview",	"gview.bat",	"gVim Read-only.lnk",
			"gview.exe",  "gvim.exe", "-R", NULL, NULL, ""},
    {"vimdiff", "vimdiff.bat",	"Vim Diff.lnk",
			"vimdiff.exe","vim.exe",  "-d", NULL, NULL, ""},
    {"gvimdiff","gvimdiff.bat",	"gVim Diff.lnk",
			"gvimdiff.exe","gvim.exe", "-d", NULL, NULL, ""},
    {"vimtutor","vimtutor.bat", "Vim tutor.lnk",
			"vimtutor.bat",  "vimtutor.bat", "", NULL, NULL, ""},
};


#define VIMBAT_UNINSTKEY    "rem # uninstall key: " VIM_VERSION_NODOT " #"

#define ICON_COUNT 3
char *(icon_names[ICON_COUNT]) =
	{"gVim " VIM_VERSION_SHORT,
	 "gVim Easy " VIM_VERSION_SHORT,
	 "gVim Read only " VIM_VERSION_SHORT};
char *(icon_link_names[ICON_COUNT]) =
	{"gVim " VIM_VERSION_SHORT ".lnk",
	 "gVim Easy " VIM_VERSION_SHORT ".lnk",
	 "gVim Read only " VIM_VERSION_SHORT ".lnk"};


#if defined(DOSINST)

    static void
run_command(char *cmd)
{
    char	*cmd_path;
    char	cmd_buf[BUFSIZE * 2 + 35];
    char	*p;

    
    
    cmd_path = searchpath_save("cmd.exe");
    if (cmd_path != NULL)
    {
	
	
	
	
	
	while ((p = strchr(cmd_path, '/')) != NULL)
	    *p = '\\';
	sprintf(cmd_buf, "%s /c start \"vimcmd\" /wait %s", cmd_path, cmd);
	free(cmd_path);
    }
    else
    {
	
	sprintf(cmd_buf, "start /w %s", cmd);
    }
    system(cmd_buf);
}
#endif


    void
add_pathsep(char *name)
{
    int		len = strlen(name);

    if (len > 0 && name[len - 1] != '\\' && name[len - 1] != '/')
	strcat(name, "\\");
}


    int
change_drive(int drive)
{
    char temp[3] = "-:";
    temp[0] = (char)(drive + 'A' - 1);
    return !SetCurrentDirectory(temp);
}


    int
mch_chdir(char *path)
{
    if (path[0] == NUL)		
	return 0;
    if (path[1] == ':')		
    {
	if (change_drive(mytoupper(path[0]) - 'A' + 1))
	    return -1;		
	path += 2;
    }
    if (*path == NUL)		
	return 0;
    return chdir(path);		
}


    static char *
my_fullpath(char *buf, char *fname UNUSED, int len)
{
    
    
    DWORD len_read = GetModuleFileName(NULL, buf, (size_t)len);

    return (len_read > 0 && len_read < (DWORD)len) ? buf : NULL;
}


    static void
remove_tail(char *path)
{
    int		i;

    for (i = strlen(path) - 1; i > 0; --i)
	if (path[i] == '/' || path[i] == '\\')
	{
	    path[i] = NUL;
	    break;
	}
}


char	installdir[MAX_PATH-9];	
				
				
int	runtimeidx;		
char	*sysdrive;		


    static void
do_inits(char **argv)
{
    
    if (my_fullpath(installdir, argv[0], sizeof(installdir)) == NULL)
    {
	printf("ERROR: Cannot get name of executable\n");
	myexit(1);
    }
    
    remove_tail(installdir);

    
    mch_chdir(installdir);

    
    
    sysdrive = getenv("SYSTEMDRIVE");
    if (sysdrive == NULL || *sysdrive == NUL)
	sysdrive = "C:\\";
}
