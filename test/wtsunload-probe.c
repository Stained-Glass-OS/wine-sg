/* wtsunload-probe: a program that loads wtsapi32, registers a window for session notifications,
 * unregisters it and frees the library keeps running (patches/sg/6531). The library's session
 * thread woke up in unloaded code (MPC-HC crashed at start). Without a compositor to follow the
 * thread retries every 5 s; the probe waits past that. Prints name=value lines.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <wtsapi32.h>
#include <stdio.h>

typedef BOOL (WINAPI *reg_fn)( HWND, DWORD );
typedef BOOL (WINAPI *unreg_fn)( HWND );

int main( void )
{
    HWND hwnd = CreateWindowW( L"STATIC", L"wtsunload", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL );
    HMODULE mod;
    BOOL reg = FALSE, unreg = FALSE;
    int round;

    setvbuf( stdout, NULL, _IONBF, 0 );
    for (round = 0; round < 2; round++)
    {
        mod = LoadLibraryW( L"wtsapi32.dll" );
        if (mod)
        {
            reg = ((reg_fn)GetProcAddress( mod, "WTSRegisterSessionNotification" ))( hwnd, NOTIFY_FOR_THIS_SESSION );
            unreg = ((unreg_fn)GetProcAddress( mod, "WTSUnRegisterSessionNotification" ))( hwnd );
            FreeLibrary( mod );
        }
        printf( "round%d register=%d unregister=%d\n", round, reg, unreg );
        Sleep( 7000 );   /* past the session thread's retry */
    }
    printf( "survived=1\n" );
    printf( "done=1\n" );
    return 0;
}
