



#include "gvimext.h"

static char *searchpath(char *name);




FORMATETC fmte = {CF_HDROP,
		  (DVTARGETDEVICE FAR *)NULL,
		  DVASPECT_CONTENT,
		  -1,
		  TYMED_HGLOBAL
		 };
STGMEDIUM medium;
HRESULT hres = 0;
UINT cbFiles = 0;


#define BUFSIZE 1100






#define EDIT_WITH_VIM_USE_TABPAGES (2)
#define EDIT_WITH_VIM_IN_DIFF_MODE (1)
#define EDIT_WITH_VIM_NO_OPTIONS   (0)






    static void
getGvimName(char *name, int runtime)
{
    HKEY	keyhandle;
    DWORD	hlen;

    
    name[0] = 0;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\Vim\\Gvim", 0,
				       KEY_READ, &keyhandle) == ERROR_SUCCESS)
    {
	hlen = BUFSIZE;
	if (RegQueryValueEx(keyhandle, "path", 0, NULL, (BYTE *)name, &hlen)
							     != ERROR_SUCCESS)
	    name[0] = 0;
	else
	    name[hlen] = 0;
	RegCloseKey(keyhandle);
    }

    
    if (name[0] == 0)
	strcpy(name, searchpath((char *)"gvim.exe"));

    if (!runtime)
    {
	
	
	if (name[0] == 0)
	    strcpy(name, searchpath((char *)"gvim.bat"));
	if (name[0] == 0)
	    strcpy(name, "gvim");	
    }
}

    static void
getGvimInvocation(char *name, int runtime)
{
    getGvimName(name, runtime);
    
    strcat(name, " --literal");
}

    static void
getGvimInvocationW(wchar_t *nameW)
{
    char *name;

    name = (char *)malloc(BUFSIZE);
    getGvimInvocation(name, 0);
    mbstowcs(nameW, name, BUFSIZE);
    free(name);
}






    static void
getRuntimeDir(char *buf)
{
    int		idx;

    getGvimName(buf, 1);
    if (buf[0] != 0)
    {
	
	if (strchr(buf, '/') == NULL && strchr(buf, '\\') == NULL)
	    strcpy(buf, searchpath(buf));

	
	for (idx = (int)strlen(buf) - 1; idx >= 0; idx--)
	    if (buf[idx] == '\\' || buf[idx] == '/')
	    {
		buf[idx + 1] = 0;
		break;
	    }
    }
}

    WCHAR *
utf8_to_utf16(const char *s)
{
    int size = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    WCHAR *buf = (WCHAR *)malloc(size * sizeof(WCHAR));
    MultiByteToWideChar(CP_UTF8, 0, s, -1, buf, size);
    return buf;
}

    HBITMAP
IconToBitmap(HICON hIcon, HBRUSH hBackground, int width, int height)
{
    HDC hDC = GetDC(NULL);
    HDC hMemDC = CreateCompatibleDC(hDC);
    HBITMAP hMemBmp = CreateCompatibleBitmap(hDC, width, height);
    HBITMAP hResultBmp = NULL;
    HGDIOBJ hOrgBMP = SelectObject(hMemDC, hMemBmp);

    DrawIconEx(hMemDC, 0, 0, hIcon, width, height, 0, hBackground, DI_NORMAL);

    hResultBmp = hMemBmp;
    hMemBmp = NULL;

    SelectObject(hMemDC, hOrgBMP);
    DeleteDC(hMemDC);
    ReleaseDC(NULL, hDC);
    DestroyIcon(hIcon);
    return hResultBmp;
}




#ifndef FEAT_GETTEXT
# define _(x)	    x
# define W_impl(x) _wcsdup(L##x)
# define W(x)	    W_impl(x)
# define set_gettext_codeset()	    NULL
# define restore_gettext_codeset(x)
#else
# define _(x)	    (*dyn_libintl_gettext)(x)
# define W(x)	    utf8_to_utf16(x)
# define VIMPACKAGE "vim"
# ifndef GETTEXT_DLL
#  define GETTEXT_DLL "libintl.dll"
#  define GETTEXT_DLL_ALT "libintl-8.dll"
# endif


static char *null_libintl_gettext(const char *);
static char *null_libintl_textdomain(const char *);
static char *null_libintl_bindtextdomain(const char *, const char *);
static char *null_libintl_bind_textdomain_codeset(const char *, const char *);
static int dyn_libintl_init(char *dir);
static void dyn_libintl_end(void);

static HINSTANCE hLibintlDLL = 0;
static char *(*dyn_libintl_gettext)(const char *) = null_libintl_gettext;
static char *(*dyn_libintl_textdomain)(const char *) = null_libintl_textdomain;
static char *(*dyn_libintl_bindtextdomain)(const char *, const char *)
						= null_libintl_bindtextdomain;
static char *(*dyn_libintl_bind_textdomain_codeset)(const char *, const char *)
				       = null_libintl_bind_textdomain_codeset;






    static int
dyn_libintl_init(char *dir)
{
    int		i;
    static struct
    {
	char	    *name;
	FARPROC	    *ptr;
    } libintl_entry[] =
    {
	{(char *)"gettext",		(FARPROC*)&dyn_libintl_gettext},
	{(char *)"textdomain",		(FARPROC*)&dyn_libintl_textdomain},
	{(char *)"bindtextdomain",	(FARPROC*)&dyn_libintl_bindtextdomain},
	{(char *)"bind_textdomain_codeset", (FARPROC*)&dyn_libintl_bind_textdomain_codeset},
	{NULL, NULL}
    };
    DWORD	len, len2;
    LPWSTR	buf = NULL;
    LPWSTR	buf2 = NULL;

    
    if (hLibintlDLL)
	return 1;

    
    
    len = GetEnvironmentVariableW(L"PATH", NULL, 0);
    len2 = MAX_PATH + 1 + len;
    buf = (LPWSTR)malloc(len * sizeof(WCHAR));
    buf2 = (LPWSTR)malloc(len2 * sizeof(WCHAR));
    if (buf != NULL && buf2 != NULL)
    {
	GetEnvironmentVariableW(L"PATH", buf, len);
# ifdef _WIN64
	_snwprintf(buf2, len2, L"%S\\GvimExt64;%s", dir, buf);
# else
	_snwprintf(buf2, len2, L"%S\\GvimExt32;%s", dir, buf);
# endif
	SetEnvironmentVariableW(L"PATH", buf2);
	hLibintlDLL = LoadLibrary(GETTEXT_DLL);
# ifdef GETTEXT_DLL_ALT
	if (!hLibintlDLL)
	    hLibintlDLL = LoadLibrary(GETTEXT_DLL_ALT);
# endif
	SetEnvironmentVariableW(L"PATH", buf);
    }
    free(buf);
    free(buf2);
    if (!hLibintlDLL)
	return 0;

    
    for (i = 0; libintl_entry[i].name != NULL
					 && libintl_entry[i].ptr != NULL; ++i)
    {
	if ((*libintl_entry[i].ptr = GetProcAddress(hLibintlDLL,
					      libintl_entry[i].name)) == NULL)
	{
	    dyn_libintl_end();
	    return 0;
	}
    }
    return 1;
}

    static void
dyn_libintl_end(void)
{
    if (hLibintlDLL)
	FreeLibrary(hLibintlDLL);
    hLibintlDLL			= NULL;
    dyn_libintl_gettext		= null_libintl_gettext;
    dyn_libintl_textdomain	= null_libintl_textdomain;
    dyn_libintl_bindtextdomain	= null_libintl_bindtextdomain;
    dyn_libintl_bind_textdomain_codeset	= null_libintl_bind_textdomain_codeset;
}

    static char *
null_libintl_gettext(const char *msgid)
{
    return (char *)msgid;
}

    static char *
null_libintl_textdomain(const char * )
{
    return NULL;
}

    static char *
null_libintl_bindtextdomain(const char * , const char * )
{
    return NULL;
}

    static char *
null_libintl_bind_textdomain_codeset(const char * , const char * )
{
    return NULL;
}




    static void
dyn_gettext_load(void)
{
    char    szBuff[BUFSIZE];
    DWORD   len;

    
    
    getRuntimeDir(szBuff);
    if (szBuff[0] != 0)
    {
	len = (DWORD)strlen(szBuff);
	if (dyn_libintl_init(szBuff))
	{
	    strcpy(szBuff + len, "lang");

	    (*dyn_libintl_bindtextdomain)(VIMPACKAGE, szBuff);
	    (*dyn_libintl_textdomain)(VIMPACKAGE);
	}
    }
}

    static void
dyn_gettext_free(void)
{
    dyn_libintl_end();
}




    static char *
set_gettext_codeset(void)
{
    char *prev = dyn_libintl_bind_textdomain_codeset(VIMPACKAGE, NULL);
    prev = _strdup((prev != NULL) ? prev : "char");
    dyn_libintl_bind_textdomain_codeset(VIMPACKAGE, "utf-8");

    return prev;
}




    static void
restore_gettext_codeset(char *prev)
{
    dyn_libintl_bind_textdomain_codeset(VIMPACKAGE, prev);
    free(prev);
}
#endif 




UINT      g_cRefThisDll = 0;    
HINSTANCE g_hmodThisDll = NULL;	





extern "C" int APIENTRY
DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID  )
{
    switch (dwReason)
    {
    case DLL_PROCESS_ATTACH:
	
	g_hmodThisDll = hInstance;
	break;

    case DLL_PROCESS_DETACH:
	break;
    }

    return 1;   
}

    static void
inc_cRefThisDLL()
{
#ifdef FEAT_GETTEXT
    if (g_cRefThisDll == 0)
	dyn_gettext_load();
#endif
    InterlockedIncrement((LPLONG)&g_cRefThisDll);
}

    static void
dec_cRefThisDLL()
{
#ifdef FEAT_GETTEXT
    if (InterlockedDecrement((LPLONG)&g_cRefThisDll) == 0)
	dyn_gettext_free();
#else
    InterlockedDecrement((LPLONG)&g_cRefThisDll);
#endif
}





STDAPI DllCanUnloadNow(void)
{
    return (g_cRefThisDll == 0 ? S_OK : S_FALSE);
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppvOut)
{
    *ppvOut = NULL;

    if (IsEqualIID(rclsid, CLSID_ShellExtension))
    {
	CShellExtClassFactory *pcf = new CShellExtClassFactory;

	return pcf->QueryInterface(riid, ppvOut);
    }

    return CLASS_E_CLASSNOTAVAILABLE;
}

CShellExtClassFactory::CShellExtClassFactory()
{
    m_cRef = 0L;

    inc_cRefThisDLL();
}

CShellExtClassFactory::~CShellExtClassFactory()
{
    dec_cRefThisDLL();
}

STDMETHODIMP CShellExtClassFactory::QueryInterface(REFIID riid,
						   LPVOID FAR *ppv)
{
    *ppv = NULL;

    

    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory))
    {
	*ppv = (LPCLASSFACTORY)this;

	AddRef();

	return NOERROR;
    }

    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CShellExtClassFactory::AddRef()
{
    return InterlockedIncrement((LPLONG)&m_cRef);
}

STDMETHODIMP_(ULONG) CShellExtClassFactory::Release()
{
    if (InterlockedDecrement((LPLONG)&m_cRef))
	return m_cRef;

    delete this;

    return 0L;
}

STDMETHODIMP CShellExtClassFactory::CreateInstance(LPUNKNOWN pUnkOuter,
						      REFIID riid,
						      LPVOID *ppvObj)
{
    *ppvObj = NULL;

    

    if (pUnkOuter)
	return CLASS_E_NOAGGREGATION;

    
    
    

    LPCSHELLEXT pShellExt = new CShellExt();  

    if (NULL == pShellExt)
	return E_OUTOFMEMORY;

    return pShellExt->QueryInterface(riid, ppvObj);
}


STDMETHODIMP CShellExtClassFactory::LockServer(BOOL  )
{
    return NOERROR;
}


CShellExt::CShellExt()
{
    m_cRef = 0L;
    m_pDataObj = NULL;

    inc_cRefThisDLL();

    LoadMenuIcon();
}

CShellExt::~CShellExt()
{
    if (m_pDataObj)
	m_pDataObj->Release();

    dec_cRefThisDLL();

    if (m_hVimIconBitmap)
	DeleteObject(m_hVimIconBitmap);
}

STDMETHODIMP CShellExt::QueryInterface(REFIID riid, LPVOID FAR *ppv)
{
    *ppv = NULL;

    if (IsEqualIID(riid, IID_IShellExtInit) || IsEqualIID(riid, IID_IUnknown))
    {
	*ppv = (LPSHELLEXTINIT)this;
    }
    else if (IsEqualIID(riid, IID_IContextMenu))
    {
	*ppv = (LPCONTEXTMENU)this;
    }

    if (*ppv)
    {
	AddRef();

	return NOERROR;
    }

    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CShellExt::AddRef()
{
    return InterlockedIncrement((LPLONG)&m_cRef);
}

STDMETHODIMP_(ULONG) CShellExt::Release()
{

    if (InterlockedDecrement((LPLONG)&m_cRef))
	return m_cRef;

    delete this;

    return 0L;
}






















STDMETHODIMP CShellExt::Initialize(LPCITEMIDLIST  ,
				   LPDATAOBJECT pDataObj,
				   HKEY  )
{
    
    if (m_pDataObj)
	m_pDataObj->Release();

    

    if (pDataObj)
    {
	m_pDataObj = pDataObj;
	pDataObj->AddRef();
    }

    return NOERROR;
}





















STDMETHODIMP CShellExt::QueryContextMenu(HMENU hMenu,
					 UINT indexMenu,
					 UINT idCmdFirst,
					 UINT  ,
					 UINT  )
{
    UINT idCmd = idCmdFirst;

    hres = m_pDataObj->GetData(&fmte, &medium);
    if (medium.hGlobal)
	cbFiles = DragQueryFileW((HDROP)medium.hGlobal, (UINT)-1, 0, 0);

    

    
    m_cntOfHWnd = 0;

    HKEY keyhandle;
    bool showExisting = true;
    bool showIcons = true;

    
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\Vim\\Gvim", 0,
				       KEY_READ, &keyhandle) == ERROR_SUCCESS)
    {
	if (RegQueryValueEx(keyhandle, "DisableEditWithExisting", 0, NULL,
						 NULL, NULL) == ERROR_SUCCESS)
	    showExisting = false;
	if (RegQueryValueEx(keyhandle, "DisableContextMenuIcons", 0, NULL,
						 NULL, NULL) == ERROR_SUCCESS)
	    showIcons = false;
	RegCloseKey(keyhandle);
    }

    
    char *prev = set_gettext_codeset();

    
    if (showExisting)
	EnumWindows(EnumWindowsProc, (LPARAM)this);

    MENUITEMINFOW mii = { sizeof(MENUITEMINFOW) };
    mii.fMask = MIIM_STRING | MIIM_ID;
    if (showIcons)
    {
	mii.fMask |= MIIM_BITMAP;
	mii.hbmpItem = m_hVimIconBitmap;
    }

    if (cbFiles > 1)
    {
	mii.wID = idCmd++;
	mii.dwTypeData = W(_("Edit with Vim using &tabpages"));
	mii.cch = wcslen(mii.dwTypeData);
	InsertMenuItemW(hMenu, indexMenu++, TRUE, &mii);
	free(mii.dwTypeData);

	mii.wID = idCmd++;
	mii.dwTypeData = W(_("Edit with single &Vim"));
	mii.cch = wcslen(mii.dwTypeData);
	InsertMenuItemW(hMenu, indexMenu++, TRUE, &mii);
	free(mii.dwTypeData);

	if (cbFiles <= 4)
	{
	    
	    mii.wID = idCmd++;
	    mii.dwTypeData = W(_("Diff with Vim"));
	    mii.cch = wcslen(mii.dwTypeData);
	    InsertMenuItemW(hMenu, indexMenu++, TRUE, &mii);
	    free(mii.dwTypeData);
	    m_edit_existing_off = 3;
	}
	else
	    m_edit_existing_off = 2;

    }
    else
    {
	mii.wID = idCmd++;
	mii.dwTypeData = W(_("Edit with &Vim"));
	mii.cch = wcslen(mii.dwTypeData);
	InsertMenuItemW(hMenu, indexMenu++, TRUE, &mii);
	free(mii.dwTypeData);
	m_edit_existing_off = 1;
    }

    HMENU hSubMenu = NULL;
    if (m_cntOfHWnd > 1)
    {
	hSubMenu = CreatePopupMenu();
	mii.fMask |= MIIM_SUBMENU;
	mii.wID = idCmd;
	mii.dwTypeData = W(_("Edit with existing Vim"));
	mii.cch = wcslen(mii.dwTypeData);
	mii.hSubMenu = hSubMenu;
	InsertMenuItemW(hMenu, indexMenu++, TRUE, &mii);
	free(mii.dwTypeData);
	mii.fMask = mii.fMask & ~MIIM_SUBMENU;
	mii.hSubMenu = NULL;
    }
    
    for (int i = 0; i < m_cntOfHWnd; i++)
    {
	WCHAR title[BUFSIZE];
	WCHAR temp[BUFSIZE];
	int index;
	HMENU hmenu;

	
	if (GetWindowTextW(m_hWnd[i], title, BUFSIZE - 1) == 0)
	    continue;
	
	WCHAR *pos = wcschr(title, L'(');
	if (pos != NULL)
	{
	    if (pos > title && pos[-1] == L' ')
		--pos;
	    *pos = 0;
	}
	
	if (m_cntOfHWnd > 1)
	    temp[0] = L'\0';
	else
	{
	    WCHAR *s = W(_("Edit with existing Vim - "));
	    wcsncpy(temp, s, BUFSIZE - 1);
	    temp[BUFSIZE - 1] = L'\0';
	    free(s);
	}
	wcsncat(temp, title, BUFSIZE - 1 - wcslen(temp));
	temp[BUFSIZE - 1] = L'\0';

	mii.wID = idCmd++;
	mii.dwTypeData = temp;
	mii.cch = wcslen(mii.dwTypeData);
	if (m_cntOfHWnd > 1)
	{
	    hmenu = hSubMenu;
	    index = i;
	}
	else
	{
	    hmenu = hMenu;
	    index = indexMenu++;
	}
	InsertMenuItemW(hmenu, index, TRUE, &mii);
    }
    

    
    restore_gettext_codeset(prev);

    
    return ResultFromShort(idCmd-idCmdFirst);
}
















STDMETHODIMP CShellExt::InvokeCommand(LPCMINVOKECOMMANDINFO lpcmi)
{
    HRESULT hr = E_INVALIDARG;
    int gvimExtraOptions;

    
    
    
    
    if (!HIWORD(lpcmi->lpVerb))
    {
	UINT idCmd = LOWORD(lpcmi->lpVerb);

	if (idCmd >= m_edit_existing_off)
	{
	    
	    hr = PushToWindow(lpcmi->hwnd,
		    lpcmi->lpDirectory,
		    lpcmi->lpVerb,
		    lpcmi->lpParameters,
		    lpcmi->nShow,
		    idCmd - m_edit_existing_off);
	}
	else
	{
	    switch (idCmd)
	    {
		case 0:
		    gvimExtraOptions = EDIT_WITH_VIM_USE_TABPAGES;
		    break;
		case 1:
		    gvimExtraOptions = EDIT_WITH_VIM_NO_OPTIONS;
		    break;
		case 2:
		    gvimExtraOptions = EDIT_WITH_VIM_IN_DIFF_MODE;
		    break;
		default:
		    
		    
		    
		    
		    
		    return E_FAIL;
	    }

            LPCMINVOKECOMMANDINFOEX lpcmiex = (LPCMINVOKECOMMANDINFOEX)lpcmi;
            LPCWSTR currentDirectory = lpcmi->cbSize == sizeof(CMINVOKECOMMANDINFOEX) ? lpcmiex->lpDirectoryW : NULL;

	    hr = InvokeSingleGvim(lpcmi->hwnd,
		    currentDirectory,
		    lpcmi->lpVerb,
		    lpcmi->lpParameters,
		    lpcmi->nShow,
		    gvimExtraOptions);
	}
    }
    return hr;
}

STDMETHODIMP CShellExt::PushToWindow(HWND  ,
				   LPCSTR  ,
				   LPCSTR  ,
				   LPCSTR  ,
				   int  ,
				   int idHWnd)
{
    HWND hWnd = m_hWnd[idHWnd];

    
    if (IsIconic(hWnd) != 0)
	ShowWindow(hWnd, SW_RESTORE);
    else
	ShowWindow(hWnd, SW_SHOW);
    SetForegroundWindow(hWnd);

    
    PostMessage(hWnd, WM_DROPFILES, (WPARAM)medium.hGlobal, 0);

    return NOERROR;
}

STDMETHODIMP CShellExt::GetCommandString(UINT_PTR  ,
					 UINT uFlags,
					 UINT FAR * ,
					 LPSTR pszName,
					 UINT cchMax)
{
    
    char *prev = set_gettext_codeset();

    WCHAR *s = W(_("Edits the selected file(s) with Vim"));
    if (uFlags == GCS_HELPTEXTW && cchMax > wcslen(s))
	wcscpy((WCHAR *)pszName, s);
    free(s);

    
    restore_gettext_codeset(prev);

    return NOERROR;
}

BOOL CALLBACK CShellExt::EnumWindowsProc(HWND hWnd, LPARAM lParam)
{
    char temp[BUFSIZE];

    
    
    if (!IsWindowVisible(hWnd))
	return TRUE;
    
    
    
    if (GetClassName(hWnd, temp, sizeof(temp)) == 0)
	return TRUE;
    
    if (_strnicmp(temp, "vim", sizeof("vim")) != 0)
	return TRUE;
    
    CShellExt *cs = (CShellExt*) lParam;
    if (cs->m_cntOfHWnd >= MAX_HWND)
	return FALSE;	
    
    cs->m_hWnd[cs->m_cntOfHWnd] = hWnd;
    cs->m_cntOfHWnd ++;

    return TRUE; 
}

BOOL CShellExt::LoadMenuIcon()
{
    char vimExeFile[BUFSIZE];
    getGvimName(vimExeFile, 1);
    if (vimExeFile[0] == '\0')
	return FALSE;
    HICON hVimIcon;
    if (ExtractIconEx(vimExeFile, 0, NULL, &hVimIcon, 1) == 0)
	return FALSE;
    m_hVimIconBitmap = IconToBitmap(hVimIcon,
	    GetSysColorBrush(COLOR_MENU),
	    GetSystemMetrics(SM_CXSMICON),
	    GetSystemMetrics(SM_CYSMICON));
    return TRUE;
}

    static char *
searchpath(char *name)
{
    static char widename[2 * BUFSIZE];
    static char location[2 * BUFSIZE + 2];

    
    
    MultiByteToWideChar(CP_ACP, 0, (LPCSTR)name, -1,
	    (LPWSTR)widename, BUFSIZE);
    if (FindExecutableW((LPCWSTR)widename, (LPCWSTR)"",
		(LPWSTR)location) > (HINSTANCE)32)
    {
	WideCharToMultiByte(CP_ACP, 0, (LPWSTR)location, -1,
		(LPSTR)widename, 2 * BUFSIZE, NULL, NULL);
	return widename;
    }
    return (char *)"";
}


STDMETHODIMP CShellExt::InvokeSingleGvim(HWND hParent,
				   LPCWSTR  workingDir,
				   LPCSTR  ,
				   LPCSTR  ,
				   int  ,
				   int gvimExtraOptions)
{
    wchar_t	m_szFileUserClickedOn[BUFSIZE];
    wchar_t	*cmdStrW;
    size_t	cmdlen;
    size_t	len;
    UINT i;

    cmdlen = BUFSIZE;
    cmdStrW  = (wchar_t *) malloc(cmdlen * sizeof(wchar_t));
    if (cmdStrW == NULL)
	return E_FAIL;
    getGvimInvocationW(cmdStrW);

    if (gvimExtraOptions == EDIT_WITH_VIM_IN_DIFF_MODE)
	wcscat(cmdStrW, L" -d");
    else if (gvimExtraOptions == EDIT_WITH_VIM_USE_TABPAGES)
	wcscat(cmdStrW, L" -p");
    for (i = 0; i < cbFiles; i++)
    {
	DragQueryFileW((HDROP)medium.hGlobal,
		i,
		m_szFileUserClickedOn,
		sizeof(m_szFileUserClickedOn));

	len = wcslen(cmdStrW) + wcslen(m_szFileUserClickedOn) + 4;
	if (len > cmdlen)
	{
	    cmdlen = len + BUFSIZE;
	    wchar_t *cmdStrW_new = (wchar_t *)realloc(cmdStrW, cmdlen * sizeof(wchar_t));
	    if (cmdStrW_new == NULL)
	    {
		free(cmdStrW);
		return E_FAIL;
	    }
	    cmdStrW = cmdStrW_new;
	}
	wcscat(cmdStrW, L" \"");
	wcscat(cmdStrW, m_szFileUserClickedOn);
	wcscat(cmdStrW, L"\"");
    }

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    
    if (!CreateProcessW(NULL,	
		cmdStrW,	
		NULL,		
		NULL,		
		FALSE,		
		0,		
		NULL,		
		workingDir,	
		&si,		
		&pi)		
       )
    {
	
	char *prev = set_gettext_codeset();

	WCHAR *msg = W(_("Error creating process: Check if gvim is in your path!"));
	WCHAR *title = W(_("gvimext.dll error"));

	MessageBoxW(hParent, msg, title, MB_OK);

	free(msg);
	free(title);

	
	restore_gettext_codeset(prev);
    }
    else
    {
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
    }
    free(cmdStrW);

    return NOERROR;
}
