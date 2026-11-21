/* uncaughtexc-probe: ucrtbase __uncaught_exceptions, which was not exported (Stained Glass OS wine-sg 8267;
 * test/uncaughtexc-gate.sh): std::uncaught_exceptions() of programs built with Visual C++ 2017 and later --
 * the count of this thread's exceptions in flight (the same counter __processing_throw exposes).
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

int main( void )
{
    HMODULE ucrt = LoadLibraryA( "ucrtbase.dll" );
    int (__cdecl *pCount)(void) = (void *)GetProcAddress( ucrt, "__uncaught_exceptions" );
    int *(__cdecl *pProcessing)(void) = (void *)GetProcAddress( ucrt, "__processing_throw" );
    BOOL (__cdecl *pAny)(void) = (void *)GetProcAddress( ucrt, "__uncaught_exception" );
    int fails = 0, a, b;

    if (!pCount) { printf( "FAIL  ucrtbase exports __uncaught_exceptions\nRESULT: FAIL\n" ); return 1; }
    a = pCount();
    (*pProcessing())++; (*pProcessing())++;
    b = pCount();
    printf( "%s  none in flight, then two (%d, %d; __uncaught_exception %d)\n", a == 0 && b == 2 && pAny() ? "PASS" : "FAIL", a, b, pAny() );
    if (!(a == 0 && b == 2)) fails++;
    *pProcessing() = 0;
    printf( "RESULT: %s\n", fails ? "FAIL" : "PASS" );
    return fails != 0;
}
