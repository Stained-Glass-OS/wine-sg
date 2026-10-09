/* The DLL test/mitigate-gate.sh builds three times for test/mitigate-probe.c
 * (patches/sg/1705): SGIMG_ID 1 beside the probe, 2 in the system
 * directory (PreferSystem32Images), and the hook DLL a second process puts
 * into the probe's thread (ExtensionPointDisable). */
#include <windows.h>

#ifndef SGIMG_ID
#define SGIMG_ID 0
#endif

__declspec(dllexport) int sgimg_id(void)
{
    return SGIMG_ID;
}

__declspec(dllexport) LRESULT CALLBACK sghook_proc( int code, WPARAM wp, LPARAM lp )
{
    char name[64];
    HANDLE event;

    wsprintfA( name, "sghook-%lu", GetCurrentProcessId() );
    if ((event = OpenEventA( EVENT_MODIFY_STATE, FALSE, name )))
    {
        SetEvent( event );
        CloseHandle( event );
    }
    return CallNextHookEx( NULL, code, wp, lp );
}

BOOL WINAPI DllMain( HINSTANCE inst, DWORD reason, void *reserved )
{
    return TRUE;
}
