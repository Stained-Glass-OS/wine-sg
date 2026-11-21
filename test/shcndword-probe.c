/* shcndword-probe: shell32 SHChangeNotify with SHCNF_DWORD items (Stained Glass OS wine-sg 8289;
 * test/shcndword-gate.sh): SHCNE_ASSOCCHANGED announced that way reaches the registered windows, and an item
 * event's two values arrive as an SHChangeDWORDAsIDList; both were dropped with a FIXME (calibre).
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>

#define WM_SHNOTIFY (WM_USER + 7)
static LONG last_event;
static DWORD last_item1 = 0xdead, got;

static LRESULT CALLBACK proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    if (msg == WM_SHNOTIFY)
    {
        PIDLIST_ABSOLUTE *pidls;
        LONG event;
        HANDLE lock = SHChangeNotification_Lock( (HANDLE)wp, (DWORD)lp, &pidls, &event );

        got++;
        last_event = event;
        if (pidls && pidls[0] && pidls[0]->mkid.cb == 10)
            last_item1 = ((SHChangeDWORDAsIDList *)pidls[0])->dwItem1;
        SHChangeNotification_Unlock( lock );
        return 0;
    }
    return DefWindowProcA( hwnd, msg, wp, lp );
}

int main( void )
{
    WNDCLASSA wc = { 0, proc, 0, 0, NULL, NULL, NULL, NULL, NULL, "shcndword" };
    SHChangeNotifyEntry entry = { NULL, TRUE };
    HWND hwnd;
    ULONG id;
    int fails = 0;

    RegisterClassA( &wc );
    hwnd = CreateWindowA( "shcndword", NULL, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL );
    id = SHChangeNotifyRegister( hwnd, SHCNRF_ShellLevel | SHCNRF_NewDelivery, SHCNE_ALLEVENTS, WM_SHNOTIFY, 1, &entry );
    printf( "      window %p, registration %lu\n", hwnd, id );

    SHChangeNotify( SHCNE_ASSOCCHANGED, SHCNF_DWORD | SHCNF_FLUSH, NULL, NULL );
    printf( "%s  SHCNE_ASSOCCHANGED with SHCNF_DWORD is delivered (%lu, event %#lx)\n",
            got == 1 && last_event == SHCNE_ASSOCCHANGED ? "PASS" : "FAIL", got, last_event );
    if (!(got == 1 && last_event == SHCNE_ASSOCCHANGED)) fails++;

    SHChangeNotify( SHCNE_UPDATEIMAGE, SHCNF_DWORD | SHCNF_FLUSH, (void *)(ULONG_PTR)42, NULL );
    printf( "%s  SHCNE_UPDATEIMAGE's value arrives as an SHChangeDWORDAsIDList (%lu, event %#lx, item %lu)\n",
            got == 2 && last_event == SHCNE_UPDATEIMAGE && last_item1 == 42 ? "PASS" : "FAIL", got, last_event, last_item1 );
    if (!(got == 2 && last_event == SHCNE_UPDATEIMAGE && last_item1 == 42)) fails++;

    SHChangeNotifyDeregister( id );
    printf( "RESULT: %s\n", fails ? "FAIL" : "PASS" );
    return fails != 0;
}
