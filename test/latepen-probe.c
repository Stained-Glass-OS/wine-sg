/* A program already running when a pen comes and goes (test/latepen-gate.sh,
 * wine-sg 1420): a window that prints what it sees of the pens --
 *   devices pens=N touch=M          GetPointerDevices, at the start and
 *                                   whenever it changes (polled)
 *   pointer MSG type=T pressure=P device=D flags=F
 *                                   WM_POINTER* with GetPointerPenInfo
 *   devchange arrival|removal device=D
 *                                   WM_POINTERDEVICECHANGE (registered with
 *                                   RegisterPointerDeviceNotifications)
 *   wintab devices=N                Wintab's devices (WTInfo), at the start
 *                                   and whenever it changes (polled)
 *   wintab open=0|1                 a context opened (once Wintab has a device)
 *   wintab packet pressure=P        WT_PACKET
 * then "ready". It runs until killed. */
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <stdio.h>

static FILE *out;

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
#define WTI_DEFSYSCTX 4
#define WTI_INTERFACE 1
#define IFC_NDEVICES 4
#define CXO_MESSAGES 0x0004
#define PK_X 0x0080
#define PK_Y 0x0100
#define PK_NORMAL_PRESSURE 0x0400
#define WT_PACKET 0x7ff0
typedef struct { LONG x, y; UINT pressure; } PACKET_;
static UINT (WINAPI *pWTInfoW)( UINT, UINT, void * );
static HANDLE (WINAPI *pWTOpenW)( HWND, LOGCONTEXTW_ *, BOOL );
static BOOL (WINAPI *pWTPacket)( HANDLE, UINT, void * );
static HANDLE wintab_ctx;
static int last_pens = -1, last_touch = -1, last_wintab = -1;

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

static void poll_devices( HWND hwnd )
{
    POINTER_DEVICE_INFO devs[16];
    UINT32 n = 0, i;
    int pens = 0, touch = 0;
    UINT ndev = 0;

    GetPointerDevices( &n, NULL );
    if (n > 16) n = 16;
    if (n && !GetPointerDevices( &n, devs )) n = 0;
    for (i = 0; i < n; i++)
    {
        if (devs[i].pointerDeviceType == POINTER_DEVICE_TYPE_EXTERNAL_PEN ||
            devs[i].pointerDeviceType == POINTER_DEVICE_TYPE_INTEGRATED_PEN) pens++;
        else if (devs[i].pointerDeviceType == POINTER_DEVICE_TYPE_TOUCH) touch++;
    }
    if (pens != last_pens || touch != last_touch)
    {
        fprintf( out, "devices pens=%d touch=%d\n", pens, touch );
        fflush( out );
        last_pens = pens;
        last_touch = touch;
    }
    if (!pWTInfoW) return;
    pWTInfoW( WTI_INTERFACE, IFC_NDEVICES, &ndev );
    if ((int)ndev != last_wintab)
    {
        fprintf( out, "wintab devices=%u\n", ndev );
        fflush( out );
        last_wintab = ndev;
    }
    if (ndev && !wintab_ctx)
    {
        LOGCONTEXTW_ lc;
        memset( &lc, 0, sizeof(lc) );
        if (pWTInfoW( WTI_DEFSYSCTX, 0, &lc ))
        {
            lc.lcOptions |= CXO_MESSAGES;
            lc.lcPktData = PK_X | PK_Y | PK_NORMAL_PRESSURE;
            lc.lcPktMode = 0;
            lc.lcMoveMask = PK_X | PK_Y | PK_NORMAL_PRESSURE;
            wintab_ctx = pWTOpenW( hwnd, &lc, TRUE );
            fprintf( out, "wintab open=%d\n", wintab_ctx != NULL );
            fflush( out );
        }
    }
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
        UINT32 id = GET_POINTERID_WPARAM( wp );
        POINTER_INPUT_TYPE type = 0;
        POINTER_PEN_INFO pen;

        GetPointerType( id, &type );
        memset( &pen, 0, sizeof(pen) );
        if (type == PT_PEN) GetPointerPenInfo( id, &pen );
        fprintf( out, "pointer %s type=%d pressure=%u device=%p flags=%#x\n", msg_name( msg ), (int)type,
                 pen.pressure, pen.pointerInfo.sourceDevice, (unsigned int)pen.pointerInfo.pointerFlags );
        fflush( out );
        return 0;
    }
    case WM_POINTERDEVICECHANGE:
        fprintf( out, "devchange %s device=%p\n", wp == PDC_ARRIVAL ? "arrival" : wp == PDC_REMOVAL ? "removal" : "other",
                 (void *)lp );
        fflush( out );
        return 0;
    case WM_TIMER:
        poll_devices( hwnd );
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
    case WM_DESTROY:
        PostQuitMessage( 0 );
        return 0;
    }
    return DefWindowProcW( hwnd, msg, wp, lp );
}

int main( void )
{
    WNDCLASSW wc = {0};
    HWND hwnd;
    MSG msg;
    HMODULE wintab;
    int w = GetSystemMetrics( SM_CXSCREEN ), h = GetSystemMetrics( SM_CYSCREEN );

    out = stdout;
    wc.lpfnWndProc = wndproc;
    wc.hInstance = GetModuleHandleW( NULL );
    wc.hCursor = LoadCursorW( NULL, (LPCWSTR)IDC_ARROW );
    wc.hbrBackground = GetStockObject( WHITE_BRUSH );
    wc.lpszClassName = L"latepen-probe";
    RegisterClassW( &wc );
    hwnd = CreateWindowExW( WS_EX_TOPMOST, wc.lpszClassName, L"latepen-probe", WS_POPUP | WS_VISIBLE,
                            w / 8, h / 8, w * 3 / 4, h * 3 / 4, NULL, NULL, wc.hInstance, NULL );
    SetForegroundWindow( hwnd );
    SetFocus( hwnd );
    RegisterPointerDeviceNotifications( hwnd, FALSE );
    if (GetEnvironmentVariableA( "LATEPEN_WINTAB", NULL, 0 ) && (wintab = LoadLibraryW( L"wintab32.dll" )))
    {
        pWTInfoW = (void *)GetProcAddress( wintab, "WTInfoW" );
        pWTOpenW = (void *)GetProcAddress( wintab, "WTOpenW" );
        pWTPacket = (void *)GetProcAddress( wintab, "WTPacket" );
    }
    poll_devices( hwnd );
    SetTimer( hwnd, 1, 200, NULL );
    fprintf( out, "ready\n" );
    fflush( out );

    while (GetMessageW( &msg, NULL, 0, 0 ))
    {
        TranslateMessage( &msg );
        DispatchMessageW( &msg );
    }
    return 0;
}
