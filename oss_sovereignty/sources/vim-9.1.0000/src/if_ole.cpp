

#if defined(FEAT_OLE) && defined(FEAT_GUI_MSWIN)

extern "C" {
# include "vim.h"
}

#include <windows.h>
#include <oleauto.h>

extern "C" {
extern HWND s_hwnd;
extern HWND vim_parent_hwnd;
}

#if (defined(_MSC_VER) && (_MSC_VER >= 1700)) || (__cplusplus >= 201103L)
# define FINAL final
#else
# define FINAL
#endif

#include "if_ole.h"	
#include "iid_ole.c"	


#ifdef __MINGW32__
WINOLEAUTAPI UnRegisterTypeLib(REFGUID libID, WORD wVerMajor,
	    WORD wVerMinor, LCID lcid, SYSKIND syskind);
#endif



class CVim;
class CVimCF;



static unsigned long cf_id = 0;


static unsigned long app_id = 0;


static CVimCF *cf = 0;


static CVim *app = 0;


#define MYCLSID CLSID_Vim
#define MYLIBID LIBID_Vim
#define MYIID IID_IVim

#define MAJORVER 1
#define MINORVER 0
#define LOCALE 0x0409

#define MYNAME "Vim"
#define MYPROGID "Vim.Application.1"
#define MYVIPROGID "Vim.Application"

#define MAX_CLSID_LEN 100





class CVim FINAL : public IVim
{
public:
    virtual ~CVim();
    static CVim *Create(int *pbDoRestart);

    
    STDMETHOD(QueryInterface)(REFIID riid, void ** ppv);
    STDMETHOD_(unsigned long, AddRef)(void);
    STDMETHOD_(unsigned long, Release)(void);

    
    STDMETHOD(GetTypeInfoCount)(UINT *pCount);
    STDMETHOD(GetTypeInfo)(UINT iTypeInfo, LCID, ITypeInfo **ppITypeInfo);
    STDMETHOD(GetIDsOfNames)(const IID &iid, OLECHAR **names, UINT n, LCID, DISPID *dispids);
    STDMETHOD(Invoke)(DISPID member, const IID &iid, LCID, WORD flags, DISPPARAMS *dispparams, VARIANT *result, EXCEPINFO *excepinfo, UINT *argerr);

    
    STDMETHOD(SendKeys)(BSTR keys);
    STDMETHOD(Eval)(BSTR expr, BSTR *result);
    STDMETHOD(SetForeground)(void);
    STDMETHOD(GetHwnd)(UINT_PTR *result);

private:
    
    CVim() : ref(0), typeinfo(0) {};

    
    unsigned long ref;

    
    ITypeInfo *typeinfo;
};



CVim *CVim::Create(int *pbDoRestart)
{
    HRESULT hr;
    CVim *me = 0;
    ITypeLib *typelib = 0;
    ITypeInfo *typeinfo = 0;

    *pbDoRestart = FALSE;

    
    me = new CVim();
    if (me == NULL)
    {
	MessageBox(0, "Cannot create application object", "Vim Initialisation", 0);
	return NULL;
    }

    
    hr = LoadRegTypeLib(MYLIBID, 1, 0, 0x00, &typelib);
    if (FAILED(hr))
    {
	HKEY hKey;

	
	
	if (RegCreateKeyEx(HKEY_CLASSES_ROOT, MYVIPROGID, 0, NULL,
		  REG_OPTION_NON_VOLATILE,
		  KEY_ALL_ACCESS, NULL, &hKey, NULL))
	{
	    delete me;
	    return NULL; 
	}
	RegCloseKey(hKey);

	if (MessageBox(0, "Cannot load registered type library.\nDo you want to register Vim now?",
		    "Vim Initialisation", MB_YESNO | MB_ICONQUESTION) != IDYES)
	{
	    delete me;
	    return NULL;
	}

	RegisterMe(FALSE);

	
	hr = LoadRegTypeLib(MYLIBID, 1, 0, 0x00, &typelib);
	if (FAILED(hr))
	{
	    MessageBox(0, "You must restart Vim in order for the registration to take effect.",
						     "Vim Initialisation", 0);
	    *pbDoRestart = TRUE;
	    delete me;
	    return NULL;
	}
    }

    
    hr = typelib->GetTypeInfoOfGuid(MYIID, &typeinfo);
    typelib->Release();

    if (FAILED(hr))
    {
	MessageBox(0, "Cannot get interface type information",
						     "Vim Initialisation", 0);
	delete me;
	return NULL;
    }

    
    me->typeinfo = typeinfo;
    return me;
}

CVim::~CVim()
{
    if (typeinfo && vim_parent_hwnd == NULL)
	typeinfo->Release();
    typeinfo = 0;
}

STDMETHODIMP
CVim::QueryInterface(REFIID riid, void **ppv)
{
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IDispatch) || IsEqualIID(riid, MYIID))
    {
	AddRef();
	*ppv = this;
	return S_OK;
    }

    *ppv = 0;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG)
CVim::AddRef()
{
    return ++ref;
}

STDMETHODIMP_(ULONG)
CVim::Release()
{
    
    
    
    if (ref > 0)
	--ref;
    return ref;
}

STDMETHODIMP
CVim::GetTypeInfoCount(UINT *pCount)
{
    *pCount = 1;
    return S_OK;
}

STDMETHODIMP
CVim::GetTypeInfo(UINT iTypeInfo, LCID, ITypeInfo **ppITypeInfo)
{
    *ppITypeInfo = 0;

    if (iTypeInfo != 0)
	return DISP_E_BADINDEX;

    typeinfo->AddRef();
    *ppITypeInfo = typeinfo;
    return S_OK;
}

STDMETHODIMP
CVim::GetIDsOfNames(
	const IID &iid,
	OLECHAR **names,
	UINT n,
	LCID,
	DISPID *dispids)
{
    if (iid != IID_NULL)
	return DISP_E_UNKNOWNINTERFACE;

    return typeinfo->GetIDsOfNames(names, n, dispids);
}

STDMETHODIMP
CVim::Invoke(
	DISPID member,
	const IID &iid,
	LCID,
	WORD flags,
	DISPPARAMS *dispparams,
	VARIANT *result,
	EXCEPINFO *excepinfo,
	UINT *argerr)
{
    if (iid != IID_NULL)
	return DISP_E_UNKNOWNINTERFACE;

    ::SetErrorInfo(0, NULL);
    return typeinfo->Invoke(static_cast<IDispatch*>(this),
			    member, flags, dispparams,
			    result, excepinfo, argerr);
}

STDMETHODIMP
CVim::GetHwnd(UINT_PTR *result)
{
    *result = (UINT_PTR)s_hwnd;
    return S_OK;
}

STDMETHODIMP
CVim::SetForeground(void)
{
    
    gui_mch_set_foreground();
    return S_OK;
}

STDMETHODIMP
CVim::SendKeys(BSTR keys)
{
    int len;
    char *buffer;
    char_u *str;
    char_u *ptr;

    
    len = WideCharToMultiByte(CP_ACP, 0, keys, -1, 0, 0, 0, 0);
    buffer = (char *)alloc(len+1);

    if (buffer == NULL)
	return E_OUTOFMEMORY;

    len = WideCharToMultiByte(CP_ACP, 0, keys, -1, buffer, len, 0, 0);

    if (len == 0)
    {
	vim_free(buffer);
	return E_INVALIDARG;
    }

    
    str = replace_termcodes((char_u *)buffer, &ptr, 0, REPTERM_DO_LT, NULL);

    
    if (ptr)
	vim_free((char_u *)(buffer));

    
    if ((int)(STRLEN(str)) > (vim_free_in_input_buf() - 10))
    {
	vim_free(str);
	return E_INVALIDARG;
    }

    
    if (*str == NUL)
	vim_free(str);
    else
	PostMessage(NULL, WM_OLE, 0, (LPARAM)str);

    return S_OK;
}

STDMETHODIMP
CVim::Eval(BSTR expr, BSTR *result)
{
#ifdef FEAT_EVAL
    int len;
    char *buffer;
    char *str;
    wchar_t *w_buffer;

    
    len = WideCharToMultiByte(CP_ACP, 0, expr, -1, 0, 0, 0, 0);
    if (len == 0)
	return E_INVALIDARG;

    buffer = (char *)alloc(len);

    if (buffer == NULL)
	return E_OUTOFMEMORY;

    
    len = WideCharToMultiByte(CP_ACP, 0, expr, -1, buffer, len, 0, 0);
    if (len == 0)
	return E_INVALIDARG;

    
    ++emsg_skip;
    str = (char *)eval_to_string((char_u *)buffer, TRUE, FALSE);
    --emsg_skip;
    vim_free(buffer);
    if (str == NULL)
	return E_FAIL;

    
    MultiByteToWideChar_alloc(CP_ACP, 0, str, -1, &w_buffer, &len);
    vim_free(str);
    if (w_buffer == NULL)
	return E_OUTOFMEMORY;

    if (len == 0)
    {
	vim_free(w_buffer);
	return E_FAIL;
    }

    
    *result = SysAllocString(w_buffer);
    vim_free(w_buffer);

    return S_OK;
#else
    return E_NOTIMPL;
#endif
}





class CVimCF FINAL : public IClassFactory
{
public:
    static CVimCF *Create();
    virtual ~CVimCF() {};

    STDMETHOD(QueryInterface)(REFIID riid, void ** ppv);
    STDMETHOD_(unsigned long, AddRef)(void);
    STDMETHOD_(unsigned long, Release)(void);
    STDMETHOD(CreateInstance)(IUnknown *punkOuter, REFIID riid, void ** ppv);
    STDMETHOD(LockServer)(BOOL lock);

private:
    
    CVimCF() : ref(0) {};

    
    unsigned long ref;
};



CVimCF *CVimCF::Create()
{
    CVimCF *me = new CVimCF();

    if (me == NULL)
	MessageBox(0, "Cannot create class factory", "Vim Initialisation", 0);

    return me;
}

STDMETHODIMP
CVimCF::QueryInterface(REFIID riid, void **ppv)
{
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory))
    {
	AddRef();
	*ppv = this;
	return S_OK;
    }

    *ppv = 0;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG)
CVimCF::AddRef()
{
    return ++ref;
}

STDMETHODIMP_(ULONG)
CVimCF::Release()
{
    
    
    
    if (ref > 0)
	--ref;
    return ref;
}


STDMETHODIMP
CVimCF::CreateInstance(IUnknown *punkOuter, REFIID riid, void **ppv)
{
    return app->QueryInterface(riid, ppv);
}


STDMETHODIMP
CVimCF::LockServer(BOOL lock)
{
    return S_OK;
}




static void SetKeyAndValue(const char *path, const char *subkey, const char *value);
static void GUIDtochar(const GUID &guid, char *GUID, int length);
static void RecursiveDeleteKey(HKEY hKeyParent, const char *child);
static const int GUID_STRING_SIZE = 39;




extern "C" void RegisterMe(int silent)
{
    BOOL ok = TRUE;

    
    char module[MAX_PATH];

    ::GetModuleFileName(NULL, module, MAX_PATH);

    
    UnregisterMe(FALSE);

    
    char clsid[GUID_STRING_SIZE];
    GUIDtochar(MYCLSID, clsid, sizeof(clsid));

    
    char libid[GUID_STRING_SIZE];
    GUIDtochar(MYLIBID, libid, sizeof(libid));

    
    char Key[MAX_CLSID_LEN];
    strcpy(Key, "CLSID\\");
    strcat(Key, clsid);

    
    SetKeyAndValue(Key, NULL, MYNAME);
    SetKeyAndValue(Key, "LocalServer32", module);
    SetKeyAndValue(Key, "ProgID", MYPROGID);
    SetKeyAndValue(Key, "VersionIndependentProgID", MYVIPROGID);
    SetKeyAndValue(Key, "TypeLib", libid);

    
    SetKeyAndValue(MYVIPROGID, NULL, MYNAME);
    SetKeyAndValue(MYVIPROGID, "CLSID", clsid);
    SetKeyAndValue(MYVIPROGID, "CurVer", MYPROGID);

    
    SetKeyAndValue(MYPROGID, NULL, MYNAME);
    SetKeyAndValue(MYPROGID, "CLSID", clsid);

    wchar_t w_module[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, module, -1, w_module, MAX_PATH);

    ITypeLib *typelib = NULL;
    if (LoadTypeLib(w_module, &typelib) != S_OK)
    {
	if (!silent)
	    MessageBox(0, "Cannot load type library to register",
						       "Vim Registration", 0);
	ok = FALSE;
    }
    else
    {
	if (RegisterTypeLib(typelib, w_module, NULL) != S_OK)
	{
	    if (!silent)
		MessageBox(0, "Cannot register type library",
						       "Vim Registration", 0);
	    ok = FALSE;
	}
	typelib->Release();
    }

    if (ok && !silent)
	MessageBox(0, "Registered successfully", "Vim", 0);
}





extern "C" void UnregisterMe(int bNotifyUser)
{
    
    ITypeLib *typelib;
    if (SUCCEEDED(LoadRegTypeLib(MYLIBID, MAJORVER, MINORVER, LOCALE, &typelib)))
    {
	TLIBATTR *tla;
	if (SUCCEEDED(typelib->GetLibAttr(&tla)))
	{
	    UnRegisterTypeLib(tla->guid, tla->wMajorVerNum, tla->wMinorVerNum,
			      tla->lcid, tla->syskind);
	    typelib->ReleaseTLibAttr(tla);
	}
	typelib->Release();
    }

    
    char clsid[GUID_STRING_SIZE];
    GUIDtochar(MYCLSID, clsid, sizeof(clsid));

    
    char Key[MAX_CLSID_LEN];
    strcpy(Key, "CLSID\\");
    strcat(Key, clsid);

    
    RecursiveDeleteKey(HKEY_CLASSES_ROOT, Key);

    
    RecursiveDeleteKey(HKEY_CLASSES_ROOT, MYVIPROGID);

    
    RecursiveDeleteKey(HKEY_CLASSES_ROOT, MYPROGID);

    if (bNotifyUser)
	MessageBox(0, "Unregistered successfully", "Vim", 0);
}




static void GUIDtochar(const GUID &guid, char *GUID, int length)
{
    
    LPOLESTR wGUID = NULL;
    StringFromCLSID(guid, &wGUID);

    
    wcstombs(GUID, wGUID, length);

    
    CoTaskMemFree(wGUID);
}


static void RecursiveDeleteKey(HKEY hKeyParent, const char *child)
{
    
    HKEY hKeyChild;
    LONG result = RegOpenKeyEx(hKeyParent, child, 0,
			       KEY_ALL_ACCESS, &hKeyChild);
    if (result != ERROR_SUCCESS)
	return;

    
    FILETIME time;
    char buffer[1024];
    DWORD size = 1024;

    while (RegEnumKeyEx(hKeyChild, 0, buffer, &size, NULL,
			NULL, NULL, &time) == S_OK)
    {
	
	RecursiveDeleteKey(hKeyChild, buffer);
	size = 256;
    }

    
    RegCloseKey(hKeyChild);

    
    RegDeleteKey(hKeyParent, child);
}


static void SetKeyAndValue(const char *key, const char *subkey, const char *value)
{
    HKEY hKey;
    char buffer[1024];

    strcpy(buffer, key);

    
    if (subkey)
    {
	strcat(buffer, "\\");
	strcat(buffer, subkey);
    }

    
    long result = RegCreateKeyEx(HKEY_CLASSES_ROOT,
				 buffer,
				 0, NULL, REG_OPTION_NON_VOLATILE,
				 KEY_ALL_ACCESS, NULL,
				 &hKey, NULL);
    if (result != ERROR_SUCCESS)
	return;

    
    if (value)
	RegSetValueEx(hKey, NULL, 0, REG_SZ, (BYTE *)value,
		      (DWORD)STRLEN(value)+1);

    RegCloseKey(hKey);
}


extern "C" void InitOLE(int *pbDoRestart)
{
    HRESULT hr;

    *pbDoRestart = FALSE;

    
    hr = OleInitialize(NULL);
    if (FAILED(hr))
    {
	MessageBox(0, "Cannot initialise OLE", "Vim Initialisation", 0);
	goto error0;
    }

    
    app = CVim::Create(pbDoRestart);
    if (app == NULL)
	goto error1;

    
    cf = CVimCF::Create();
    if (cf == NULL)
	goto error1;

    
    hr = CoRegisterClassObject(
	MYCLSID,
	cf,
	CLSCTX_LOCAL_SERVER,
	REGCLS_MULTIPLEUSE,
	&cf_id);

    if (FAILED(hr))
    {
	MessageBox(0, "Cannot register class factory", "Vim Initialisation", 0);
	goto error1;
    }

    
    hr = RegisterActiveObject(
	app,
	MYCLSID,
	0,
	&app_id);

    if (FAILED(hr))
    {
	MessageBox(0, "Cannot register application object", "Vim Initialisation", 0);
	goto error1;
    }

    return;

    
error1:
    UninitOLE();
error0:
    return;
}

extern "C" void UninitOLE()
{
    
    if (app_id)
    {
	RevokeActiveObject(app_id, NULL);
	app_id = 0;
    }

    
    if (cf_id)
    {
	CoRevokeClassObject(cf_id);
	cf_id = 0;
    }

    
    OleUninitialize();

    
    if (app)
    {
	delete app;
	app = NULL;
    }

    
    if (cf)
    {
	delete cf;
	cf = NULL;
    }
}
#endif 
