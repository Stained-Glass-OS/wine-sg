/* A drawing program taking the pen (test/penpointer-gate.sh, wine-sg 1000):
 * a window over the screen that prints what a pen and touches give it --
 *   devices count=N type=T            GetPointerDevices
 *   pointer MSG id=N type=T pressure=P tiltx=X tilty=Y penflags=F penmask=M flags=F history=H rects=R
 *                                     WM_POINTER* with GetPointerPenInfo
 *   mouse MSG extra=E                 WM_LBUTTON*, the first WM_MOUSEMOVE after a pointer message
 *   wintab devices=N pressure-max=M   Wintab's device (WTInfo)
 *   wintab packet pressure=P          WT_PACKET
 *   wheel delta=D | hwheel delta=D | ctrl-wheel delta=D
 * then "ready". It runs until killed. */
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <stdio.h>

static FILE *out;
static int moves_after_pointer;

/* Wintab (the public Wintab 1.4 specification's structures) */
typedef DWORD FIX32;
typedef struct
{
    WCHAR lcName[40];
    UINT lcOptions, lcStatus, lcLocks, lcMsgBase, lcDevice, lcPktRate;
    DWORD lcPktData, lcPktMode, lcMoveMask, lcBtnDnMask, lcBtnUpMask;
    LONG lcInOrgX, lcInOrgY, lcInOrgZ, lcInExtX, lcInExtY, lcInExtZ;
    LONG lcOutOrgX, lcOutOrgY, lcOutOrgZ, lcOutExtX, lcOutExtY, lcOutExtZ;
    FIX32 lcSensX, lcSensY, lcSensZ;
    BOOL lcSysMode;
    int lcSysOrgX, lcSysOrgY, lcSysExtX, lcSysExtY;
    FIX32 lcSysSensX, lcSysSensY;
} LOGCONTEXTW_;
typedef struct { LONG axMin, axMax; UINT axUnits; FIX32 axResolution; } AXIS_;
#define WTI_DEFSYSCTX 4
#define WTI_INTERFACE 1
#define IFC_NDEVICES 4
#define WTI_DEVICES 100
#define DVC_NPRESSURE 15
#define CXO_MESSAGES 0x0004
#define PK_X 0x0080
#define PK_Y 0x0100
#define PK_NORMAL_PRESSURE 0x0400
#define WT_PACKET 0x7ff0
typedef struct { LONG x, y; UINT pressure; } PACKET_;
static UINT (WINAPI *pWTInfoW)( UINT, UINT, void * );
static HANDLE (WINAPI *pWTOpenW)( HWND, LOGCONTEXTW_ *, BOOL );
static BOOL (WINAPI *pWTPacket)( HANDLE, UINT, void * );

static const char *msg_name( UINT msg )
{
    switch (msg)
    {
    case WM_POINTERENTER: return "enter";
    case WM_POINTERLEAVE: return "leave";
    case WM_POINTERDOWN: return "down";
    case WM_POINTERUP: return "up";
    case WM_POINTERUPDATE: return "update";
    }
    return "?";
}

static LRESULT CALLBACK wndproc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    switch (msg)
    {
    case WM_POINTERENTER:
    case WM_POINTERLEAVE:
    case WM_POINTERDOWN:
    case WM_POINTERUP:
    case WM_POINTERUPDATE:
    {
        UINT32 id = GET_POINTERID_WPARAM( wp ), count = 0;
        POINTER_INPUT_TYPE type = 0;
        POINTER_PEN_INFO pen;
        RECT dev, disp;
        BOOL ok;

        GetPointerType( id, &type );
        memset( &pen, 0, sizeof(pen) );
        ok = GetPointerPenInfo( id, &pen );
        GetPointerPenInfoHistory( id, &count, NULL );
        fprintf( out, "pointer %s id=%u type=%d ok=%d pressure=%u tiltx=%d tilty=%d penflags=%#x penmask=%#x flags=%#x "
                 "history=%u rects=%d x=%ld\n", msg_name( msg ), id, (int)type, ok, pen.pressure, pen.tiltX, pen.tiltY,
                 pen.penFlags, pen.penMask, pen.pointerInfo.pointerFlags, count,
                 GetPointerDeviceRects( pen.pointerInfo.sourceDevice, &dev, &disp ), pen.pointerInfo.ptPixelLocation.x );
        fflush( out );
        moves_after_pointer = 1;
        return 0;
    }
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
        fprintf( out, "mouse %s extra=%#lx\n", msg == WM_LBUTTONDOWN ? "ldown" : msg == WM_LBUTTONUP ? "lup" : "rdown",
                 (unsigned long)GetMessageExtraInfo() );
        fflush( out );
        return 0;
    case WM_MOUSEMOVE:
        if (moves_after_pointer)
        {
            moves_after_pointer = 0;
            fprintf( out, "mouse move extra=%#lx\n", (unsigned long)GetMessageExtraInfo() );
            fflush( out );
        }
        return 0;
    case WM_MOUSEWHEEL:
        fprintf( out, "%swheel delta=%d\n", (GET_KEYSTATE_WPARAM( wp ) & MK_CONTROL) ? "ctrl-" : "",
                 GET_WHEEL_DELTA_WPARAM( wp ) );
        fflush( out );
        return 0;
    case WM_MOUSEHWHEEL:
        fprintf( out, "hwheel delta=%d\n", GET_WHEEL_DELTA_WPARAM( wp ) );
        fflush( out );
        return 0;
    case WT_PACKET:
    {
        PACKET_ pkt;
        if (pWTPacket && pWTPacket( (HANDLE)lp, wp, &pkt ))
        {
            fprintf( out, "wintab packet pressure=%u\n", pkt.pressure );
            fflush( out );
        }
        return 0;
    }
    }
    return DefWindowProcW( hwnd, msg, wp, lp );
}

int main( void )
{
    WNDCLASSW wc = {0};
    HWND hwnd;
    MSG msg;
    UINT32 n = 0;
    POINTER_DEVICE_INFO devs[8];
    HMODULE wintab;
    int w = GetSystemMetrics( SM_CXSCREEN ), h = GetSystemMetrics( SM_CYSCREEN );

    out = stdout;
    GetPointerDevices( &n, NULL );
    if (n > 8) n = 8;
    if (n && GetPointerDevices( &n, devs ))
        fprintf( out, "devices count=%u type=%d\n", n, devs[0].pointerDeviceType );
    else
        fprintf( out, "devices count=%u\n", n );

    wc.lpfnWndProc = wndproc;
    wc.hInstance = GetModuleHandleW( NULL );
    wc.hCursor = LoadCursorW( NULL, (LPCWSTR)IDC_ARROW );
    wc.hbrBackground = GetStockObject( WHITE_BRUSH );
    wc.lpszClassName = L"penpointer-probe";
    RegisterClassW( &wc );
    /* PENPROBE_WINDOWED: a window short of the screen (no full-screen
     * cursor clipping), else over all of it */
    if (GetEnvironmentVariableA( "PENPROBE_WINDOWED", NULL, 0 ))
        hwnd = CreateWindowExW( WS_EX_TOPMOST, wc.lpszClassName, L"penpointer-probe", WS_POPUP | WS_VISIBLE,
                                w / 8, h / 8, w * 3 / 4, h * 3 / 4, NULL, NULL, wc.hInstance, NULL );
    else
        hwnd = CreateWindowExW( 0, wc.lpszClassName, L"penpointer-probe", WS_POPUP | WS_VISIBLE, 0, 0, w, h,
                                NULL, NULL, wc.hInstance, NULL );
    SetForegroundWindow( hwnd );
    SetFocus( hwnd );

    if ((wintab = LoadLibraryW( L"wintab32.dll" )))
    {
        pWTInfoW = (void *)GetProcAddress( wintab, "WTInfoW" );
        pWTOpenW = (void *)GetProcAddress( wintab, "WTOpenW" );
        pWTPacket = (void *)GetProcAddress( wintab, "WTPacket" );
    }
    if (pWTInfoW && pWTOpenW && !GetEnvironmentVariableA( "PENPROBE_NOWINTAB", NULL, 0 ))
    {
        UINT ndev = 0;
        AXIS_ pressure = {0};
        LOGCONTEXTW_ lc;

        pWTInfoW( WTI_INTERFACE, IFC_NDEVICES, &ndev );
        pWTInfoW( WTI_DEVICES, DVC_NPRESSURE, &pressure );
        fprintf( out, "wintab devices=%u pressure-max=%ld\n", ndev, pressure.axMax );
        memset( &lc, 0, sizeof(lc) );
        if (ndev && pWTInfoW( WTI_DEFSYSCTX, 0, &lc ))
        {
            lc.lcOptions |= CXO_MESSAGES;
            lc.lcPktData = PK_X | PK_Y | PK_NORMAL_PRESSURE;
            lc.lcPktMode = 0;
            lc.lcMoveMask = PK_X | PK_Y | PK_NORMAL_PRESSURE;
            fprintf( out, "wintab open=%d\n", pWTOpenW( hwnd, &lc, TRUE ) != NULL );
        }
    }
    else fprintf( out, "wintab none\n" );
    fprintf( out, "ready\n" );
    fflush( out );

    while (GetMessageW( &msg, NULL, 0, 0 ))
    {
        TranslateMessage( &msg );
        DispatchMessageW( &msg );
    }
    return 0;
}
