/* A global hook's DLL for test/hookcount-gate.sh (patches/sg/9655). Every
 * process on the desktop loads it; only the probe sets a callback, which runs
 * once, inside the hook (the hook chain in use), on the probe's WM_APP+1. */
#include <windows.h>

static void (*volatile callback)(void);

__declspec(dllexport) void hc_set_callback( void (*cb)(void) ) { callback = cb; }

__declspec(dllexport) LRESULT CALLBACK hc_msgproc( int code, WPARAM wp, LPARAM lp )
{
    MSG *msg = (MSG *)lp;
    if (code == HC_ACTION && callback && msg->message == WM_APP + 1)
    {
        void (*cb)(void) = callback;
        callback = NULL;
        cb();
    }
    return CallNextHookEx( 0, code, wp, lp );
}
