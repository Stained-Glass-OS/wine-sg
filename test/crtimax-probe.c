/* ucrtbase imaxdiv / wcstoimax / wcstoumax / _wcstoimax_l / _wcstoumax_l
 * (wine-sg 0421), looked up by name as a program linked to them loads them.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

typedef struct { __int64 quot, rem; } sg_imaxdiv_t;
typedef sg_imaxdiv_t (__cdecl *imaxdiv_fn)(__int64, __int64);
typedef __int64 (__cdecl *wcstoimax_fn)(const WCHAR *, WCHAR **, int);
typedef unsigned __int64 (__cdecl *wcstoumax_fn)(const WCHAR *, WCHAR **, int);
typedef __int64 (__cdecl *wcstoimax_l_fn)(const WCHAR *, WCHAR **, int, void *);

int main( int argc, char **argv )
{
    const char *dll = argc > 1 ? argv[1] : "ucrtbase.dll";
    HMODULE m = LoadLibraryA( dll );
    imaxdiv_fn idiv = (imaxdiv_fn)GetProcAddress( m, "imaxdiv" );
    wcstoimax_fn toi = (wcstoimax_fn)GetProcAddress( m, "wcstoimax" );
    wcstoumax_fn tou = (wcstoumax_fn)GetProcAddress( m, "wcstoumax" );
    wcstoimax_l_fn toil = (wcstoimax_l_fn)GetProcAddress( m, "_wcstoimax_l" );
    wcstoimax_l_fn toul = (wcstoimax_l_fn)GetProcAddress( m, "_wcstoumax_l" );
    WCHAR *end;
    if (!m) { printf( "NODLL %s\n", dll ); return 1; }
    if (idiv) { sg_imaxdiv_t d = idiv( -7000000000000LL, 3 ); printf( "IMAXDIV %lld %lld\n", d.quot, d.rem ); }
    else printf( "IMAXDIV missing\n" );
    if (toi) { __int64 v = toi( L"-9000000000123xyz", &end, 10 ); printf( "WCSTOIMAX %lld rest=%ls\n", v, end ); }
    else printf( "WCSTOIMAX missing\n" );
    if (tou) printf( "WCSTOUMAX %llu\n", tou( L"0xFFFFFFFFFFFFFFFF", NULL, 16 ) );
    else printf( "WCSTOUMAX missing\n" );
    if (toil) printf( "WCSTOIMAX_L %lld\n", toil( L"123456789012", NULL, 10, NULL ) );
    else printf( "WCSTOIMAX_L missing\n" );
    if (toul) printf( "WCSTOUMAX_L %llu\n", (unsigned __int64)toul( L"777", NULL, 8, NULL ) );
    else printf( "WCSTOUMAX_L missing\n" );
    return 0;
}
