



#if !defined(AFX_STDAFX_H__3389658B_AD83_11D3_9C1E_0090278BBD99__INCLUDED_)
#define AFX_STDAFX_H__3389658B_AD83_11D3_9C1E_0090278BBD99__INCLUDED_

#if defined(_MSC_VER) && _MSC_VER > 1000
#pragma once
#endif










#ifndef STRICT
# define STRICT
#endif

#define INC_OLE2	


#if defined(_MSC_VER) && _MSC_VER >= 1400
# define _CRT_SECURE_NO_DEPRECATE
# define _CRT_NONSTDC_NO_DEPRECATE
#endif

#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>
#include <wchar.h>

#define ResultFromShort(i)  ResultFromScode(MAKE_SCODE(SEVERITY_SUCCESS, 0, (USHORT)(i)))



#pragma data_seg(".text")
#define INITGUID
#include <initguid.h>
























DEFINE_GUID(CLSID_ShellExtension, 0x51eee242, 0xad87, 0x11d3, 0x9c, 0x1e, 0x0, 0x90, 0x27, 0x8b, 0xbd, 0x99);


class CShellExtClassFactory : public IClassFactory
{
protected:
    ULONG	m_cRef;

public:
    CShellExtClassFactory();
    ~CShellExtClassFactory();

    
    STDMETHODIMP		QueryInterface(REFIID, LPVOID FAR *);
    STDMETHODIMP_(ULONG)	AddRef();
    STDMETHODIMP_(ULONG)	Release();

    
    STDMETHODIMP		CreateInstance(LPUNKNOWN, REFIID, LPVOID FAR *);
    STDMETHODIMP		LockServer(BOOL);
};
typedef CShellExtClassFactory *LPCSHELLEXTCLASSFACTORY;
#define MAX_HWND 100


class CShellExt : public IContextMenu,
			 IShellExtInit
{
private:
    BOOL LoadMenuIcon();

protected:
    ULONG	 m_cRef;
    LPDATAOBJECT m_pDataObj;
    UINT	 m_edit_existing_off;
    HBITMAP	 m_hVimIconBitmap;

    
    static BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam);

    STDMETHODIMP PushToWindow(HWND hParent,
	    LPCSTR pszWorkingDir,
	    LPCSTR pszCmd,
	    LPCSTR pszParam,
	    int iShowCmd,
	    int idHWnd);

    STDMETHODIMP InvokeSingleGvim(HWND hParent,
	    LPCWSTR workingDir,
	    LPCSTR pszCmd,
	    LPCSTR pszParam,
	    int iShowCmd,
	    int gvimExtraOptions);

public:
    int		 m_cntOfHWnd;
    HWND	 m_hWnd[MAX_HWND];
    CShellExt();
    ~CShellExt();

    
    STDMETHODIMP QueryInterface(REFIID, LPVOID FAR *);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    
    STDMETHODIMP QueryContextMenu(HMENU hMenu,
	    UINT indexMenu,
	    UINT idCmdFirst,
	    UINT idCmdLast,
	    UINT uFlags);

    STDMETHODIMP InvokeCommand(LPCMINVOKECOMMANDINFO lpcmi);

    STDMETHODIMP GetCommandString(UINT_PTR idCmd,
	    UINT uFlags,
	    UINT FAR *reserved,
	    LPSTR pszName,
	    UINT cchMax);

    
    STDMETHODIMP Initialize(LPCITEMIDLIST pIDFolder,
	    LPDATAOBJECT pDataObj,
	    HKEY hKeyID);
};

typedef CShellExt *LPCSHELLEXT;
#pragma data_seg()

#endif
