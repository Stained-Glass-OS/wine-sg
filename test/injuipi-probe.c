#define _WIN32_WINNT 0x0A00
#define WINVER 0x0A00
/* injuipi-probe: UIPI for injected input (Stained Glass OS wine-sg 8273; test/injuipi-gate.sh). A medium-
 * integrity process's SendInput click and injected touch tap (InjectSyntheticPointerInput, 8256) do not reach a
 * high-integrity window; a high-integrity tap does; a medium click still reaches another medium process's window.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <aclapi.h>
#include <stdio.h>

static int clicks;
static LRESULT CALLBACK proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    if (msg == WM_LBUTTONDOWN) clicks++;
    return DefWindowProcW( hwnd, msg, wp, lp );
}

static int run_target( void )
{
    WNDCLASSW wc = { 0, proc, 0, 0, NULL, NULL, NULL, (HBRUSH)(COLOR_WINDOW + 1), NULL, L"uipitarget" };
    HANDLE ready = OpenEventW( EVENT_MODIFY_STATE, FALSE, L"sg_injuipi_ready" );
    HWND hwnd;
    DWORD end;
    MSG msg;

    RegisterClassW( &wc );
    hwnd = CreateWindowExW( WS_EX_TOPMOST, L"uipitarget", L"medium", WS_POPUP | WS_VISIBLE, 100, 100, 300, 200, NULL, NULL, NULL, NULL );
    SetForegroundWindow( hwnd );
    end = GetTickCount() + 300;
    while (GetTickCount() < end) { while (PeekMessageW( &msg, NULL, 0, 0, PM_REMOVE )) DispatchMessageW( &msg ); Sleep( 10 ); }
    SetEvent( ready );
    end = GetTickCount() + 4000;
    while (GetTickCount() < end && !clicks) { while (PeekMessageW( &msg, NULL, 0, 0, PM_REMOVE )) DispatchMessageW( &msg ); Sleep( 10 ); }
    return clicks;
}

static int run_child( const WCHAR *mode )
{
    HMODULE user32 = GetModuleHandleW( L"user32.dll" );
    HANDLE (WINAPI *pCreate)(POINTER_INPUT_TYPE, ULONG, POINTER_FEEDBACK_MODE) = (void *)GetProcAddress( user32, "CreateSyntheticPointerDevice" );
    BOOL (WINAPI *pInject)(HANDLE, const POINTER_TYPE_INFO *, UINT32) = (void *)GetProcAddress( user32, "InjectSyntheticPointerInput" );
    if (!wcscmp( mode, L"tap" ))
    {
        POINTER_TYPE_INFO info;
        HANDLE dev = pCreate( PT_TOUCH, 1, POINTER_FEEDBACK_NONE );
        memset( &info, 0, sizeof(info) );
        info.type = PT_TOUCH;
        info.touchInfo.pointerInfo.pointerType = PT_TOUCH;
        info.touchInfo.pointerInfo.ptPixelLocation.x = 250;
        info.touchInfo.pointerInfo.ptPixelLocation.y = 200;
        info.touchInfo.pointerInfo.pointerFlags = POINTER_FLAG_DOWN | POINTER_FLAG_INCONTACT | POINTER_FLAG_INRANGE | POINTER_FLAG_PRIMARY;
        pInject( dev, &info, 1 );
        Sleep( 200 );
        info.touchInfo.pointerInfo.pointerFlags = POINTER_FLAG_UP | POINTER_FLAG_PRIMARY;
        pInject( dev, &info, 1 );
    }
    else
    {
        INPUT in[3] = { 0 };
        in[0].type = in[1].type = in[2].type = INPUT_MOUSE;
        in[0].mi.dx = MulDiv( 250, 65535, GetSystemMetrics( SM_CXSCREEN ) - 1 );
        in[0].mi.dy = MulDiv( 200, 65535, GetSystemMetrics( SM_CYSCREEN ) - 1 );
        in[0].mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
        in[1].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        in[2].mi.dwFlags = MOUSEEVENTF_LEFTUP;
        {
            UINT sent = SendInput( 3, in, sizeof(INPUT) );
            Sleep( 300 );
            return sent;
        }
    }
    Sleep( 300 );
    return 0;
}

static void pump( DWORD ms )
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while (GetTickCount() < end)
    {
        while (PeekMessageW( &msg, NULL, 0, 0, PM_REMOVE )) DispatchMessageW( &msg );
        Sleep( 10 );
    }
}

static DWORD spawn( const WCHAR *mode, BOOL medium_il, BOOL wait )
{
    struct { TOKEN_MANDATORY_LABEL label; BYTE sid[SECURITY_MAX_SID_SIZE]; } il;
    DWORD size = sizeof(il.sid);
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    WCHAR cmd[MAX_PATH + 64], self[MAX_PATH];
    HANDLE token, medium;

    OpenProcessToken( GetCurrentProcess(), TOKEN_ALL_ACCESS, &token );
    DuplicateTokenEx( token, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenPrimary, &medium );
    CreateWellKnownSid( medium_il ? WinMediumLabelSid : WinHighLabelSid, NULL, il.sid, &size );
    il.label.Label.Sid = il.sid;
    il.label.Label.Attributes = SE_GROUP_INTEGRITY;
    SetTokenInformation( medium, TokenIntegrityLevel, &il.label, sizeof(il.label) + size );
    GetModuleFileNameW( NULL, self, MAX_PATH );
    _snwprintf( cmd, ARRAYSIZE(cmd), L"\"%ls\" child %ls", self, mode );
    DWORD code = 0;

    if (!CreateProcessAsUserW( medium, NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi )) return ~0u;
    if (!wait) { CloseHandle( pi.hThread ); return (DWORD)(ULONG_PTR)pi.hProcess; }
    while (MsgWaitForMultipleObjects( 1, &pi.hProcess, FALSE, 30000, QS_ALLINPUT ) == WAIT_OBJECT_0 + 1) pump( 10 );
    pump( 300 );
    GetExitCodeProcess( pi.hProcess, &code );
    CloseHandle( pi.hProcess ); CloseHandle( pi.hThread );
    return code;
}

int wmain( int argc, WCHAR **argv )
{
    WNDCLASSW wc = { 0, proc, 0, 0, NULL, NULL, NULL, (HBRUSH)(COLOR_WINDOW + 1), NULL, L"uipitap" };
    HWND hwnd;
    int fails = 0;

    if (argc > 2 && !wcscmp( argv[1], L"child" )) return !wcscmp( argv[2], L"target" ) ? run_target() : run_child( argv[2] );
    RegisterClassW( &wc );
    hwnd = CreateWindowExW( WS_EX_TOPMOST, L"uipitap", L"high", WS_POPUP | WS_VISIBLE, 100, 100, 300, 200, NULL, NULL, NULL, NULL );
    SetForegroundWindow( hwnd );
    pump( 300 );
    {
        DWORD sent;
        clicks = 0; sent = spawn( L"click", TRUE, TRUE );
        printf( "%s  a medium SendInput click does not reach the high window (%d)\n", clicks ? "FAIL" : "PASS", clicks ); if (clicks) fails++;
        printf( "%s  and SendInput still reports its 3 events sent (%lu), as on Windows\n", sent == 3 ? "PASS" : "FAIL", sent ); if (sent != 3) fails++;
    }
    clicks = 0; spawn( L"tap", TRUE, TRUE );
    printf( "%s  a medium injected tap does not reach it (%d)\n", clicks ? "FAIL" : "PASS", clicks ); if (clicks) fails++;
    clicks = 0; spawn( L"tap", FALSE, TRUE );
    printf( "%s  a high-integrity tap does (%d)\n", clicks ? "PASS" : "FAIL", clicks ); if (!clicks) fails++;
    ShowWindow( hwnd, SW_HIDE );
    pump( 200 );
    {
        HANDLE ready = CreateEventW( NULL, TRUE, FALSE, L"sg_injuipi_ready" ), target;
        DWORD got = 0;

        /* the event must be openable from medium integrity */
        SetSecurityInfo( ready, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION | LABEL_SECURITY_INFORMATION, NULL, NULL, NULL, NULL );
        target = (HANDLE)(ULONG_PTR)spawn( L"target", TRUE, FALSE );
        WaitForSingleObject( ready, 10000 );
        spawn( L"click", TRUE, TRUE );
        WaitForSingleObject( target, 10000 );
        GetExitCodeProcess( target, &got );
        printf( "%s  a medium click still reaches another medium process's window (%lu)\n", got >= 1 && got != STILL_ACTIVE ? "PASS" : "FAIL", got );
        if (!(got >= 1 && got != STILL_ACTIVE)) fails++;
    }
    printf( "RESULT: %s\n", fails ? "FAIL" : "PASS" );
    return fails != 0;
}
