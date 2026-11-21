/* cachecfg-probe: wininet GetUrlCacheConfigInfo reports the content container's quota (its cache limit) and its
 * normal and exempt usage in kilobytes (Stained Glass OS wine-sg 8247; test/cachecfg-gate.sh); they were a fixed
 * 4 GB-ish index figure and zeros.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <wininet.h>
#include <stdio.h>

typedef struct { DWORD size, container, quota, res4; BOOL peruser; DWORD sync, npaths; WCHAR path[MAX_PATH]; DWORD cachesize, normal, exempt; } CFGW;

static int failures;
static void check( int ok, const char *what, long got )
{
    printf( "%s  %s (%ld)\n", ok ? "PASS" : "FAIL", what, got );
    if (!ok) failures++;
}

static BOOL get( CFGW *c )
{
    BOOL (WINAPI *p)(CFGW *, DWORD *, DWORD) = (void *)GetProcAddress( GetModuleHandleA( "wininet.dll" ), "GetUrlCacheConfigInfoW" );
    memset( c, 0, sizeof(*c) );
    c->size = sizeof(*c);
    return p( c, NULL, 0x800 | 0x2000 | 0x4000 /* QUOTA, CONTENT_USAGE, STICKY_CONTENT_USAGE */ );
}

static BOOL add( const char *url, DWORD type )
{
    char file[MAX_PATH];
    static char data[20000];
    FILETIME zero = { 0 };
    DWORD written;
    HANDLE h;

    if (!CreateUrlCacheEntryA( url, sizeof(data), "bin", file, 0 )) return FALSE;
    h = CreateFileA( file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );
    memset( data, 'x', sizeof(data) );
    WriteFile( h, data, sizeof(data), &written, NULL );
    CloseHandle( h );
    return CommitUrlCacheEntryA( url, file, zero, zero, type, NULL, 0, NULL, NULL );
}

int main( void )
{
    CFGW before, after, sticky;
    DWORD limit = 0, size = sizeof(limit), want;
    BOOL ret;

    LoadLibraryA( "wininet.dll" );
    if (RegGetValueA( HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings\\Cache\\Content",
                      "CacheLimit", RRF_RT_REG_DWORD, NULL, &limit, &size ))
        want = 0x07ff5400 / 1024; /* the default limit */
    else
        want = limit;
    ret = get( &before );
    printf( "      quota %lu KB, normal %lu KB, exempt %lu KB, path %ls\n", before.quota, before.normal, before.exempt, before.path );
    check( ret && before.quota == want, "dwQuota is the content cache limit in KB", before.quota );
    check( add( "http://sg.test/cachecfg/normal.bin", NORMAL_CACHE_ENTRY ), "a 20000-byte normal entry committed", 0 );
    get( &after );
    check( after.normal >= before.normal + 19 && after.normal <= before.normal + 20, "dwNormalUsage grew by it (KB)", after.normal - before.normal );
    check( add( "http://sg.test/cachecfg/sticky.bin", NORMAL_CACHE_ENTRY | STICKY_CACHE_ENTRY ), "a sticky entry committed", 0 );
    get( &sticky );
    check( sticky.exempt >= after.exempt + 19 && sticky.normal == after.normal, "dwExemptUsage grew by the sticky one, normal did not", sticky.exempt - after.exempt );
    DeleteUrlCacheEntryA( "http://sg.test/cachecfg/normal.bin" );
    DeleteUrlCacheEntryA( "http://sg.test/cachecfg/sticky.bin" );
    get( &after );
    check( after.normal == before.normal && after.exempt == before.exempt, "both back after deleting the entries", after.normal );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
