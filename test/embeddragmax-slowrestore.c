/* embeddragmax-slowrestore: a CBT hook, for every process, that holds up a
 * Linux program's frame (SgLinuxWindow) being restored for 1.5 s -- the time
 * in which, dragged out of being maximized, the button may come up.
 * Built twice by embeddragmax-gate.sh: as a DLL (the hook) and with
 * -DSLOWRESTORE_EXE as the program that sets it and waits to be killed.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>

#ifndef SLOWRESTORE_EXE
static HINSTANCE self;

BOOL WINAPI DllMain( HINSTANCE inst, DWORD reason, void *reserved )
{
    if (reason == DLL_PROCESS_ATTACH) self = inst;
    return TRUE;
}

__declspec(dllexport) LRESULT CALLBACK slow_restore( int code, WPARAM wp, LPARAM lp )
{
    WCHAR class[32];
    if (code == HCBT_MINMAX && LOWORD( lp ) == SW_RESTORE && GetClassNameW( (HWND)wp, class, ARRAYSIZE(class) ) &&
        !lstrcmpW( class, L"SgLinuxWindow" ) && IsZoomed( (HWND)wp ))
        Sleep( 1500 );
    return CallNextHookEx( 0, code, wp, lp );
}
#else
#include <stdio.h>
int main( void )
{
    HMODULE mod = LoadLibraryA( "slowrestore.dll" );
    HOOKPROC proc = mod ? (HOOKPROC)GetProcAddress( mod, "slow_restore" ) : NULL;
    HHOOK hook = proc ? SetWindowsHookExW( WH_CBT, proc, mod, 0 ) : NULL;
    MSG msg;
    printf( "hook=%d\n", hook != NULL );
    fflush( stdout );
    if (!hook) return 1;
    while (GetMessageW( &msg, 0, 0, 0 )) DispatchMessageW( &msg );
    return 0;
}
#endif
