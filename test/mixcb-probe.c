/* mixcb-probe.c: a native DLL for test/mixcb-gate.sh (Stained Glass OS wine-sg 9453). Called from managed
 * code in an application domain, it hands a callback to msvcm80/msvcm90's
 * <CrtImplementationDetails>::DoCallBackInDefaultDomain, as a C++/CLI module's startup does, and reports
 * the domain (ICLRRuntimeHost::GetCurrentAppDomainId) before, inside and after the callback.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>
#include <objbase.h>

/* the hosting interfaces, as far as the probe uses them (mingw has no metahost.h) */
typedef struct { void *qi, *addref, *release, *get_runtime, *version_from_file, *installed;
                 HRESULT (WINAPI *loaded)( void *, HANDLE, IEnumUnknown ** ); } metahost_vtbl;
typedef struct { void *qi, *addref, *release, *version, *dir, *is_loaded, *error_string, *load_library, *proc;
                 HRESULT (WINAPI *get_interface)( void *, REFCLSID, REFIID, void ** ); } runtimeinfo_vtbl;
typedef struct { void *qi, *addref, *release, *start, *stop, *set_host_control, *get_clr_control, *unload, *execute;
                 HRESULT (WINAPI *current_domain)( void *, DWORD * ); } runtimehost_vtbl;
typedef struct { const metahost_vtbl *vtbl; } ICLRMetaHost;
typedef struct { const runtimeinfo_vtbl *vtbl; } ICLRRuntimeInfo;
typedef struct { const runtimehost_vtbl *vtbl; } ICLRRuntimeHost;
static const GUID CLSID_CLRMetaHost = {0x9280188d,0x0e8e,0x4867,{0xb3,0x0c,0x7f,0xa8,0x38,0x84,0xe8,0xde}};
static const GUID IID_ICLRMetaHost = {0xd332db9e,0xb9b3,0x4125,{0x82,0x07,0xa1,0x48,0x84,0xf5,0x32,0x16}};
static const GUID IID_ICLRRuntimeInfo = {0xbd39d1d2,0xba2f,0x486a,{0x89,0xb0,0xb4,0xb0,0xcb,0x46,0x68,0x91}};
static const GUID CLSID_CLRRuntimeHost = {0x90f1a06e,0x7712,0x4762,{0x86,0xb5,0x7a,0x5e,0xba,0x6b,0xdb,0x02}};
static const GUID IID_ICLRRuntimeHost = {0x90f1a06c,0x7712,0x4762,{0x86,0xb5,0x7a,0x5e,0xba,0x6b,0xdb,0x02}};
#define ICLRMetaHost_EnumerateLoadedRuntimes(p,a,b) (p)->vtbl->loaded(p,a,b)
#define ICLRRuntimeInfo_GetInterface(p,a,b,c) (p)->vtbl->get_interface(p,a,b,c)
#define ICLRRuntimeHost_GetCurrentAppDomainId(p,a) (p)->vtbl->current_domain(p,a)

static ICLRRuntimeHost *host;
static int called;
static DWORD inside = 999;

static HRESULT WINAPI callback( void *cookie )
{
    called += cookie == (void *)&called;
    ICLRRuntimeHost_GetCurrentAppDomainId( host, &inside );
    return S_OK;
}

__declspec(dllexport) int __stdcall sg_callback_test( const WCHAR *dll, WCHAR *out, int len )
{
    void (__cdecl *docallback)( HRESULT (WINAPI *)(void *), void * );
    ICLRMetaHost *metahost;
    ICLRRuntimeInfo *info;
    IEnumUnknown *runtimes;
    IUnknown *unk;
    DWORD before = 999, after = 999;
    HMODULE msvcm;

    called = 0;
    inside = 999;
    HRESULT (WINAPI *create)( REFCLSID, REFIID, void ** );

    if (!(create = (void *)GetProcAddress( GetModuleHandleW( L"mscoree.dll" ), "CLRCreateInstance" ))) return 1;
    if (FAILED(create( &CLSID_CLRMetaHost, &IID_ICLRMetaHost, (void **)&metahost ))) return 1;
    if (FAILED(ICLRMetaHost_EnumerateLoadedRuntimes( metahost, GetCurrentProcess(), &runtimes ))) return 2;
    if (runtimes->lpVtbl->Next( runtimes, 1, &unk, NULL ) != S_OK) return 3;
    if (FAILED(unk->lpVtbl->QueryInterface( unk, &IID_ICLRRuntimeInfo, (void **)&info ))) return 4;
    if (FAILED(ICLRRuntimeInfo_GetInterface( info, &CLSID_CLRRuntimeHost, &IID_ICLRRuntimeHost, (void **)&host ))) return 5;
    if (!(msvcm = LoadLibraryW( dll ))) return 6;
#ifdef _WIN64
    docallback = (void *)GetProcAddress( msvcm, "?DoCallBackInDefaultDomain@<CrtImplementationDetails>@@YAXP6AJPEAX@Z0@Z" );
#else
    docallback = (void *)GetProcAddress( msvcm, "?DoCallBackInDefaultDomain@<CrtImplementationDetails>@@YAXP6GJPAX@Z0@Z" );
#endif
    if (!docallback) return 7;
    ICLRRuntimeHost_GetCurrentAppDomainId( host, &before );
    docallback( callback, &called );
    ICLRRuntimeHost_GetCurrentAppDomainId( host, &after );
    _snwprintf( out, len, L"%lu,%d,%lu,%lu", before, called, inside, after );
    out[len - 1] = 0;
    return 0;
}
