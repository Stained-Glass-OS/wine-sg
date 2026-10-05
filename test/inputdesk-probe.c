/* GetUserObjectInformation(UOI_IO): whether a desktop is the one taking
 * input (patches/sg/0860). Remote-desktop programs ask it of the thread's
 * desktop before they capture the screen; AnyDesk got "invalid parameter"
 * and told the other side the image was inaccessible.
 * Prints "name value" lines for test/inputdesk-gate.sh. */
#include <windows.h>
#include <stdio.h>

#ifndef UOI_IO
#define UOI_IO 6
#endif

static void ask( const char *name, HANDLE obj )
{
    BOOL io = 0xdead;
    DWORD needed = 0;
    BOOL ret;

    SetLastError( 0xdeadbeef );
    ret = GetUserObjectInformationW( obj, UOI_IO, &io, sizeof(io), &needed );
    printf( "%s ret=%d io=%d needed=%lu err=%lu\n", name, ret, ret ? io : -1, needed,
            ret ? 0 : GetLastError() );
}

int main( void )
{
    HDESK own = GetThreadDesktop( GetCurrentThreadId() );
    HDESK other, input;
    HWINSTA winsta = GetProcessWindowStation();
    BOOL io;
    DWORD needed = 0;
    BOOL ret;

    /* make sure the thread's desktop is the input desktop (the first one of
     * the window station is) */
    input = OpenInputDesktop( 0, FALSE, DESKTOP_READOBJECTS );
    printf( "input-open %d\n", input != NULL );
    if (input) CloseDesktop( input );

    ask( "own", own );
    other = CreateDesktopW( L"sg-inputdesk-other", NULL, NULL, 0, GENERIC_ALL, NULL );
    printf( "other-created %d\n", other != NULL );
    if (other) ask( "other", other );
    ask( "winsta", winsta );

    SetLastError( 0xdeadbeef );
    ret = GetUserObjectInformationW( own, UOI_IO, &io, 1, &needed );
    printf( "small ret=%d needed=%lu err=%lu\n", ret, needed, GetLastError() );

    /* switching input to the other desktop moves the answer */
    if (other && SwitchDesktop( other ))
    {
        ask( "own-after-switch", own );
        ask( "other-after-switch", other );
        SwitchDesktop( own );
    }
    else printf( "switch-failed %lu\n", GetLastError() );
    ask( "own-back", own );
    return 0;
}
