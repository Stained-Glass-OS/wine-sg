/* uilanginfo-probe: GetUILanguageInfo (Stained Glass OS wine-sg 8241; test/uilanginfo-gate.sh); it was a spec
 * stub in kernelbase and missing from kernel32.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

static int failures;
static void check( int ok, const char *what, long got )
{
    printf( "%s  %s (%#lx)\n", ok ? "PASS" : "FAIL", what, got );
    if (!ok) failures++;
}

int main( void )
{
    BOOL (WINAPI *pInfo)(DWORD, const WCHAR *, WCHAR *, DWORD *, DWORD *) =
        (void *)GetProcAddress( GetModuleHandleA( "kernel32.dll" ), "GetUILanguageInfo" );
    WCHAR fallback[16];
    DWORD count, attr;
    BOOL ret;

    check( pInfo != NULL, "kernel32 exports GetUILanguageInfo", 0 );
    if (!pInfo) goto done;
    count = 0; attr = 0;
    ret = pInfo( MUI_LANGUAGE_NAME, L"en-US\0", NULL, &count, &attr );
    check( ret && count == 2 && attr == (MUI_FULL_LANGUAGE | MUI_LANGUAGE_INSTALLED | MUI_LANGUAGE_LICENSED),
           "en-US: the size of an empty fallback list; full, installed, licensed", attr );
    count = ARRAYSIZE(fallback); memset( fallback, 0xcc, sizeof(fallback) );
    ret = pInfo( MUI_LANGUAGE_NAME, L"de-DE\0", fallback, &count, &attr );
    check( ret && count == 2 && !fallback[0] && !fallback[1], "de-DE: no fallback languages", count );
    ret = pInfo( MUI_LANGUAGE_ID, L"0409\0", NULL, NULL, &attr );
    check( ret && (attr & MUI_LANGUAGE_INSTALLED), "by LANGID \"0409\"", attr );
    SetLastError( 0xdead );
    ret = pInfo( MUI_LANGUAGE_NAME, L"xx-NOPE\0", NULL, NULL, &attr );
    check( !ret && GetLastError() == ERROR_OBJECT_NOT_FOUND, "an unknown language: not found", GetLastError() );
    SetLastError( 0xdead );
    ret = pInfo( MUI_LANGUAGE_ID | MUI_LANGUAGE_NAME, L"en-US\0", NULL, NULL, &attr );
    check( !ret && GetLastError() == ERROR_INVALID_PARAMETER, "both flags: ERROR_INVALID_PARAMETER", GetLastError() );
    SetLastError( 0xdead );
    count = 5;
    ret = pInfo( MUI_LANGUAGE_NAME, L"en-US\0", NULL, &count, &attr );
    check( !ret && GetLastError() == ERROR_INVALID_PARAMETER, "a size without a buffer: ERROR_INVALID_PARAMETER", GetLastError() );
    SetLastError( 0xdead );
    count = 1;
    ret = pInfo( MUI_LANGUAGE_NAME, L"en-US\0", fallback, &count, &attr );
    check( !ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER && count == 2, "too small a buffer", GetLastError() );
done:
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
