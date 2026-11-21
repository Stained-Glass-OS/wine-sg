/* synthpointer-probe: user32 CreateSyntheticPointerDevice / InjectSyntheticPointerInput /
 * DestroySyntheticPointerDevice (Stained Glass OS wine-sg 8256; test/synthpointer-gate.sh). Firefox and
 * Thunderbird import them; they were a stub and two missing exports. An injected touch contact reaches a
 * window that does not handle pointer messages as the mouse: a left click where the finger went down.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

static int downs, ups;
static POINT down_at;

static LRESULT CALLBACK proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    if (msg == WM_LBUTTONDOWN) { downs++; down_at.x = (short)LOWORD(lp); down_at.y = (short)HIWORD(lp); }
    if (msg == WM_LBUTTONUP) ups++;
    return DefWindowProcA( hwnd, msg, wp, lp );
}

static void pump( void )
{
    MSG msg;
    DWORD end = GetTickCount() + 500;
    while (GetTickCount() < end)
    {
        while (PeekMessageA( &msg, 0, 0, 0, PM_REMOVE )) { TranslateMessage( &msg ); DispatchMessageA( &msg ); }
        Sleep( 10 );
    }
}

static int failures;
static void check( int ok, const char *what, long v )
{
    printf( "%s  %s (%ld)\n", ok ? "PASS" : "FAIL", what, v );
    if (!ok) failures++;
}

int main( void )
{
    HMODULE user32 = GetModuleHandleA( "user32.dll" );
    HSYNTHETICPOINTERDEVICE (WINAPI *pCreate)(POINTER_INPUT_TYPE, ULONG, POINTER_FEEDBACK_MODE) = (void *)GetProcAddress( user32, "CreateSyntheticPointerDevice" );
    BOOL (WINAPI *pInject)(HSYNTHETICPOINTERDEVICE, const POINTER_TYPE_INFO *, UINT32) = (void *)GetProcAddress( user32, "InjectSyntheticPointerInput" );
    void (WINAPI *pDestroy)(HSYNTHETICPOINTERDEVICE) = (void *)GetProcAddress( user32, "DestroySyntheticPointerDevice" );
    WNDCLASSA cls = { 0, proc, 0, 0, NULL, NULL, NULL, (HBRUSH)(COLOR_WINDOW + 1), NULL, "synthpointer" };
    POINTER_TYPE_INFO info;
    HSYNTHETICPOINTERDEVICE dev;
    HWND hwnd;
    BOOL ret;

    if (!pCreate || !pInject || !pDestroy) { printf( "FAIL  the three functions are exported\nRESULT: FAIL\n" ); return 1; }
    printf( "PASS  the three functions are exported\n" );
    RegisterClassA( &cls );
    hwnd = CreateWindowExA( WS_EX_TOPMOST, "synthpointer", "t", WS_POPUP | WS_VISIBLE, 100, 100, 300, 200, NULL, NULL, NULL, NULL );
    SetForegroundWindow( hwnd );
    pump();

    check( !pCreate( PT_MOUSE, 1, POINTER_FEEDBACK_DEFAULT ), "a mouse device is refused", GetLastError() );
    check( !pCreate( PT_PEN, 2, POINTER_FEEDBACK_DEFAULT ), "a pen device with two contacts is refused", GetLastError() );
    dev = pCreate( PT_TOUCH, 2, POINTER_FEEDBACK_NONE );
    check( dev != NULL, "a touch device with two contacts", 0 );

    memset( &info, 0, sizeof(info) );
    info.type = PT_TOUCH;
    info.touchInfo.pointerInfo.pointerType = PT_TOUCH;
    info.touchInfo.pointerInfo.ptPixelLocation.x = 220;
    info.touchInfo.pointerInfo.ptPixelLocation.y = 170;
    info.touchInfo.pointerInfo.pointerFlags = POINTER_FLAG_DOWN | POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT | POINTER_FLAG_PRIMARY;
    ret = pInject( dev, &info, 1 );
    pump();
    info.touchInfo.pointerInfo.pointerFlags = POINTER_FLAG_UP | POINTER_FLAG_PRIMARY;
    ret = pInject( dev, &info, 1 ) && ret;
    pump();
    check( ret, "InjectSyntheticPointerInput down and up", 0 );
    check( downs == 1 && ups == 1, "the window got one left button down and one up", downs * 10 + ups );
    check( down_at.x == 120 && down_at.y == 70, "  where the contact was (120,70 in the window)", down_at.x * 1000 + down_at.y );
    info.type = PT_PEN;
    check( !pInject( dev, &info, 1 ), "a pen contact on a touch device is refused", GetLastError() );
    check( !pInject( dev, &info, 3 ), "more contacts than the device has are refused", GetLastError() );
    pDestroy( dev );
    info.type = PT_TOUCH;
    check( !pInject( dev, &info, 1 ), "a destroyed device is refused", GetLastError() );
    DestroyWindow( hwnd );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
