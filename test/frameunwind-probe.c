/* frameunwind-probe: vcruntime140/ucrtbase __FrameUnwindFilter, a spec stub (Stained Glass OS wine-sg 8258;
 * test/frameunwind-gate.sh): LibreOffice, Anki and Dependencies import it. Another exception goes on to the
 * next handler; a C++ exception thrown while a frame unwinds ends the program through terminate().
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

typedef void (__cdecl *terminate_handler)(void);
static void __cdecl on_terminate( void )
{
    printf( "TERMINATE called\n" );
    fflush( stdout );
    ExitProcess( 42 );
}

int main( int argc, char **argv )
{
    HMODULE vcrt = LoadLibraryA( "vcruntime140.dll" ), ucrt = LoadLibraryA( "ucrtbase.dll" );
    int (__cdecl *pFilter)(EXCEPTION_POINTERS *) = (void *)GetProcAddress( vcrt, "__FrameUnwindFilter" );
    terminate_handler (__cdecl *pSetTerminate)(terminate_handler) = (void *)GetProcAddress( ucrt, "set_terminate" );
    EXCEPTION_RECORD rec;
    CONTEXT ctx;
    EXCEPTION_POINTERS ptrs = { &rec, &ctx };
    int fails = 0, ret;

    memset( &rec, 0, sizeof(rec) );
    if (argc > 1)
    {
        /* child: a C++ exception during unwinding */
        pSetTerminate( on_terminate );
        rec.ExceptionCode = 0xe06d7363;
        rec.NumberParameters = sizeof(void *) == 8 ? 4 : 3;
        rec.ExceptionInformation[0] = 0x19930520;
        ret = pFilter( &ptrs );
        printf( "filter returned %d\n", ret );
        return 0;
    }
    if (!pFilter || !pSetTerminate) { printf( "FAIL  exports\nRESULT: FAIL\n" ); return 1; }
    rec.ExceptionCode = EXCEPTION_ACCESS_VIOLATION;
    ret = pFilter( &ptrs );
    printf( "%s  an access violation: EXCEPTION_CONTINUE_SEARCH (%d)\n", ret == EXCEPTION_CONTINUE_SEARCH ? "PASS" : "FAIL", ret );
    if (ret != EXCEPTION_CONTINUE_SEARCH) fails++;
    {
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        char cmd[MAX_PATH + 16];
        DWORD code = 0;

        sprintf( cmd, "\"%s\" child", argv[0] );
        if (CreateProcessA( NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi ))
        {
            WaitForSingleObject( pi.hProcess, 20000 );
            GetExitCodeProcess( pi.hProcess, &code );
        }
        printf( "%s  a C++ exception while unwinding: terminate() (child exit %lu)\n", code == 42 ? "PASS" : "FAIL", code );
        if (code != 42) fails++;
    }
    printf( "RESULT: %s\n", fails ? "FAIL" : "PASS" );
    return fails != 0;
}
