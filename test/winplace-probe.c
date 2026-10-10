/* GetWindowPlacement flags and SetParent cycles (patches/sg/2232): a
 * maximized window has WPF_RESTORETOMAXIMIZED -- also when asked from another
 * process whose owner does not answer; SetParent refuses an owned popup that
 * already belongs to the window by owner or parent links. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

int main(int argc, char **argv)
{
    WINDOWPLACEMENT wp = { sizeof(wp) };
    HWND h, parent, c1, c2, c3, c4;

    if (argc > 2 && !strcmp( argv[1], "--query" ))
    {
        HWND w = (HWND)(ULONG_PTR)strtoull( argv[2], NULL, 16 );
        BOOL ret = GetWindowPlacement( w, &wp );
        check( ret && wp.showCmd == SW_SHOWMAXIMIZED, "[other process] a maximized window of a busy owner: showCmd" );
        check( wp.flags == WPF_RESTORETOMAXIMIZED, "[other process] ... and WPF_RESTORETOMAXIMIZED" );
        return failures != 0;
    }

    h = CreateWindowA( "static", "x", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, 0, 0, 0, 0 );
    ShowWindow( h, SW_SHOWNORMAL );
    GetWindowPlacement( h, &wp );
    check( wp.showCmd == SW_SHOWNORMAL && wp.flags == 0, "normal window: no flags" );
    ShowWindow( h, SW_SHOWMAXIMIZED );
    GetWindowPlacement( h, &wp );
    check( wp.showCmd == SW_SHOWMAXIMIZED && wp.flags == WPF_RESTORETOMAXIMIZED, "maximized window: WPF_RESTORETOMAXIMIZED" );
    ShowWindow( h, SW_SHOWMINIMIZED );
    GetWindowPlacement( h, &wp );
    check( wp.showCmd == SW_SHOWMINIMIZED && wp.flags == WPF_RESTORETOMAXIMIZED, "minimized from maximized: WPF_RESTORETOMAXIMIZED" );
    ShowWindow( h, SW_RESTORE );
    GetWindowPlacement( h, &wp );
    check( wp.showCmd == SW_SHOWMAXIMIZED && wp.flags == WPF_RESTORETOMAXIMIZED, "restored to maximized" );
    ShowWindow( h, SW_SHOWMAXIMIZED );

    {
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        char cmd[MAX_PATH + 64];
        DWORD code = 1;
        sprintf( cmd, "\"%s\" --query %llx", argv[0], (unsigned long long)(ULONG_PTR)h );
        if (CreateProcessA( NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi ))
        {
            WaitForSingleObject( pi.hProcess, 30000 );   /* never pumps messages */
            GetExitCodeProcess( pi.hProcess, &code );
            check( code == 0, "the other process saw the placement" );
            CloseHandle( pi.hProcess ); CloseHandle( pi.hThread );
        }
        else check( 0, "start the query process" );
    }
    DestroyWindow( h );

    parent = CreateWindowExA( 0, "static", NULL, WS_OVERLAPPEDWINDOW, 100, 100, 200, 200, 0, 0, 0, NULL );
    c1 = CreateWindowExA( 0, "static", NULL, WS_CHILD, 0, 0, 50, 50, parent, 0, 0, NULL );
    c2 = CreateWindowExA( 0, "static", NULL, WS_POPUP, 0, 0, 50, 50, c1, 0, 0, NULL );
    c3 = CreateWindowExA( 0, "static", NULL, WS_CHILD, 0, 0, 50, 50, c2, 0, 0, NULL );
    c4 = CreateWindowExA( 0, "static", NULL, WS_POPUP, 0, 0, 50, 50, c3, 0, 0, NULL );
    check( !SetParent( parent, c1 ), "SetParent to a child of itself fails" );
    check( !SetParent( parent, c2 ), "SetParent to a popup owned by its child fails" );
    check( !SetParent( parent, c4 ), "SetParent to a popup owned by its descendant fails" );
    check( SetParent( parent, c3 ) != 0, "SetParent to a child window works" );
    DestroyWindow( parent );

    printf( failures ? "RESULT: FAIL\n" : "RESULT: PASS\n" );
    return failures != 0;
}
