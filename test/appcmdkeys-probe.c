/* appcmdkeys-probe: the application-command keys as WM_APPCOMMAND (patches/sg/6524).
 *
 *   probe hook          a shell hook window (RegisterShellHookWindow): prints every HSHELL_APPCOMMAND
 *   probe target        a window that leaves its keys to DefWindowProc: prints its keys and app commands
 *   probe target eat    one that handles WM_KEYDOWN itself (no app command may come of it)
 * Each runs until C:\stop exists. Prints name=value lines; see test/appcmdkeys-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>

#ifndef FAPPCOMMAND_MASK
#define FAPPCOMMAND_MASK 0xF000
#endif

static UINT shellhook;
static BOOL eat;

static LRESULT CALLBACK hook_proc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
    if (msg == shellhook && wparam == HSHELL_APPCOMMAND)
    {
        printf( "hook_appcmd cmd=%u device=%#x keys=%#x\n", HIWORD( lparam ) & ~FAPPCOMMAND_MASK,
                HIWORD( lparam ) & FAPPCOMMAND_MASK, LOWORD( lparam ) );
        return 0;
    }
    return DefWindowProcW( hwnd, msg, wparam, lparam );
}

static LRESULT CALLBACK target_proc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
    switch (msg)
    {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        printf( "key vk=%#x\n", (unsigned int)wparam );
        if (eat) return 0;
        break;
    case WM_APPCOMMAND:
        printf( "self_appcmd cmd=%u device=%#x window=%d\n", HIWORD( lparam ) & ~FAPPCOMMAND_MASK,
                HIWORD( lparam ) & FAPPCOMMAND_MASK, (HWND)wparam == hwnd );
        break;
    }
    return DefWindowProcW( hwnd, msg, wparam, lparam );
}

int main( int argc, char **argv )
{
    WNDCLASSW cls = { 0 };
    DWORD start = GetTickCount();
    const WCHAR *title;
    HWND hwnd;
    MSG msg;

    setvbuf( stdout, NULL, _IONBF, 0 );
    if (argc < 2) return 1;
    cls.hInstance = GetModuleHandleW( NULL );
    cls.hCursor = LoadCursorW( NULL, (LPCWSTR)IDC_ARROW );
    cls.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    if (!strcmp( argv[1], "hook" ))
    {
        shellhook = RegisterWindowMessageW( L"SHELLHOOK" );
        cls.lpfnWndProc = hook_proc;
        cls.lpszClassName = L"sgappcmd_hook";
        RegisterClassW( &cls );
        hwnd = CreateWindowW( cls.lpszClassName, L"sgappcmd-hook", WS_OVERLAPPED, 0, 0, 10, 10, NULL, NULL, NULL, NULL );
        printf( "hook_ready=%d\n", hwnd && RegisterShellHookWindow( hwnd ) );
    }
    else
    {
        eat = argc > 2 && !strcmp( argv[2], "eat" );
        title = eat ? L"sgappcmd-eat" : L"sgappcmd-target";
        cls.lpfnWndProc = target_proc;
        cls.lpszClassName = L"sgappcmd_target";
        RegisterClassW( &cls );
        hwnd = CreateWindowW( cls.lpszClassName, title, WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 300, 200,
                              NULL, NULL, NULL, NULL );
        SetForegroundWindow( hwnd );
        SetFocus( hwnd );
        printf( "target_ready=%d\n", hwnd != NULL );
    }
    SetTimer( NULL, 0, 200, NULL );
    while (GetMessageW( &msg, NULL, 0, 0 ))
    {
        if (msg.message == WM_TIMER && !msg.hwnd)
        {
            if (GetFileAttributesW( L"C:\\stop" ) != INVALID_FILE_ATTRIBUTES || GetTickCount() - start > 120000) break;
            continue;
        }
        TranslateMessage( &msg );
        DispatchMessageW( &msg );
    }
    if (!strcmp( argv[1], "hook" )) DeregisterShellHookWindow( hwnd );
    DestroyWindow( hwnd );
    printf( "done=1\n" );
    return 0;
}
