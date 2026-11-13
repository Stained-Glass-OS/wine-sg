/* test/pkginfo2-gate.sh's probe (7754): the package-graph functions the Windows App SDK's runtime
 * (Microsoft.WindowsAppRuntime.dll) looks up from the app model API sets when it loads -- it stops
 * its process (c0000409) when one is missing; Office's aimgr.exe did, under Word's Copilot box.
 *   exports      each one through the API set DLL WinAppSDK loads it from
 *   current      GetCurrentPackageInfo2 (install and external paths), GetCurrentPackageInfo3 (paths
 *                and the graph's generation)
 *   open NAME    OpenPackageInfoByFullName, GetPackageInfo, GetPackageInfo2, ClosePackageInfo
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef struct { void *reserved; } *REF;
typedef struct { UINT32 reserved, arch; UINT64 version; WCHAR *name, *publisher, *resource, *publisher_id; } ID;
typedef struct { UINT32 reserved, flags; WCHAR *path, *full, *family; ID id; } INFO;

typedef LONG (WINAPI *cpi2_func)( UINT32, UINT32, UINT32 *, BYTE *, UINT32 * );
typedef HRESULT (WINAPI *cpi3_func)( UINT32, UINT32, UINT32 *, void *, UINT32 * );
typedef LONG (WINAPI *open_func)( const WCHAR *, UINT32, REF * );
typedef LONG (WINAPI *pi_func)( REF, UINT32, UINT32 *, BYTE *, UINT32 * );
typedef LONG (WINAPI *pi2_func)( REF, UINT32, UINT32, UINT32 *, BYTE *, UINT32 * );
typedef LONG (WINAPI *close_func)( REF );
typedef UINT32 (WINAPI *rev_func)( void );

static FARPROC get( const char *dll, const char *name )
{
    HMODULE mod = LoadLibraryExA( dll, NULL, 0 );
    FARPROC proc = mod ? GetProcAddress( mod, name ) : NULL;
    printf( "Export.%s=%d\n", name, proc != NULL );
    return proc;
}

static void dump( const char *key, LONG ret, BYTE *buf, UINT32 count )
{
    UINT32 i;
    printf( "%sRet=%ld\n", key, ret );
    if (ret) return;
    printf( "%sCount=%u\n", key, count );
    for (i = 0; i < count; i++)
    {
        INFO *info = (INFO *)buf + i;
        printf( "%s%u=%ls|%ls|%ls|%#x\n", key, i, info->full, info->family, info->path, info->flags );
    }
}

static LONG sized( cpi2_func f, UINT32 flags, UINT32 type, BYTE **buf, UINT32 *count )
{
    UINT32 size = 0;
    LONG ret = f( flags, type, &size, NULL, count );
    if (ret != ERROR_INSUFFICIENT_BUFFER) return ret;
    *buf = HeapAlloc( GetProcessHeap(), 0, size );
    return f( flags, type, &size, *buf, count );
}

int wmain( int argc, WCHAR **argv )
{
    const char *rt = "api-ms-win-appmodel-runtime-l1-1-3.dll", *in = "api-ms-win-appmodel-runtime-internal-l1-1-6.dll";
    cpi2_func cpi2 = (cpi2_func)get( rt, "GetCurrentPackageInfo2" );
    pi2_func pi2 = (pi2_func)get( rt, "GetPackageInfo2" );
    cpi3_func cpi3 = (cpi3_func)get( in, "GetCurrentPackageInfo3" );
    rev_func rev = (rev_func)get( "api-ms-win-appmodel-runtime-l1-1-6.dll", "GetPackageGraphRevisionId" );
    open_func open = (open_func)get( "kernel32.dll", "OpenPackageInfoByFullName" );
    pi_func pi = (pi_func)get( "kernel32.dll", "GetPackageInfo" );
    close_func close = (close_func)get( "kernel32.dll", "ClosePackageInfo" );
    BYTE *buf = NULL;
    UINT32 count = 0, size, gen = 0;
    LONG ret;
    HRESULT hr;

    if (argc < 2 || !cpi2 || !pi2 || !cpi3 || !rev || !open || !pi || !close) return 0;
    if (!wcscmp( argv[1], L"current" ))
    {
        ret = sized( cpi2, 0, 0, &buf, &count );
        dump( "Install", ret, buf, count );
        ret = sized( cpi2, 0x10, 5, &buf, &count );
        dump( "HeadExternal", ret, buf, count );
        size = 0;
        printf( "Info3Size=0x%08lx\n", cpi3( 0, 0, &size, NULL, &count ) );
        buf = HeapAlloc( GetProcessHeap(), 0, size ? size : 1 );
        printf( "Info3=0x%08lx\n", cpi3( 0, 0, &size, buf, &count ) );
        printf( "Info3Count=%u\n", count );
        size = sizeof(gen);
        hr = cpi3( 0, 16, &size, &gen, &count );
        printf( "Generation=0x%08lx %u\n", hr, gen );
        printf( "Revision=%u\n", rev() );
    }
    else if (!wcscmp( argv[1], L"open" ) && argc > 2)
    {
        REF ref = NULL;
        ret = open( argv[2], 0, &ref );
        printf( "Open=%ld\n", ret );
        if (ret) return 0;
        size = 0;
        ret = pi( ref, 0, &size, NULL, &count );
        printf( "SizeRet=%ld\n", ret );
        buf = HeapAlloc( GetProcessHeap(), 0, size );
        ret = pi( ref, 0, &size, buf, &count );
        dump( "Info", ret, buf, count );
        size = 0x10000;
        buf = HeapAlloc( GetProcessHeap(), 0, size );
        ret = pi2( ref, 0x10, 2, &size, buf, &count );
        dump( "HeadEffective", ret, buf, count );
        printf( "Close=%ld\n", close( ref ) );
    }
    return 0;
}
