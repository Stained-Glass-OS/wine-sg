/* vsync-probe: DwmFlush paces its caller to the display's refresh
 * (patches/sg/0170), and Firefox's vsync-over-IPC pattern stays at that pace.
 *
 * Firefox's vsync thread loops { notify vsync; DwmFlush(); }.  With the GPU
 * process each notification is an IPC message: written to an overlapped
 * named pipe, completed through an I/O completion port on the other side's
 * I/O thread, which wakes the compositor's thread with a posted message
 * (MsgWaitForMultipleObjectsEx).  Wine's DwmFlush returned at once, so the
 * loop sent hundreds of thousands of messages a second: the GPU process's
 * I/O thread never caught up, synchronous requests to it timed out, the
 * parent's queued messages grew to tens of GB and it never exited.
 *
 *   vsync-probe   prints   rate=HZ
 *                          flush30_ms=MS expect=MS
 *                          on_vblank=N/30
 *                          ipc_vsyncs=N per_s=HZ roundtrip_ms=MS
 *                          dxgi30_ms=MS (IDXGIOutput::WaitForVBlank, 0173;
 *                          dxgi=none without an output)
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <dwmapi.h>
#include <dxgi.h>
#include <stdio.h>

#define MSG_VSYNC (WM_APP + 1)
#define MSG_REPLY (WM_APP + 2)

static LARGE_INTEGER freq;
static double now_ms( void )
{
    LARGE_INTEGER c;
    QueryPerformanceCounter( &c );
    return c.QuadPart * 1000.0 / freq.QuadPart;
}

/* the channel: a pipe pair made as Firefox's CreateRawPipe does */
static HANDLE srv, cli, port, ui_ready;
static DWORD ui_tid;
static volatile LONG stop, vsyncs_sent, vsyncs_seen, reply_seen;

struct msg { DWORD type; DWORD seq; char pad[56]; };   /* 64 bytes */

static DWORD WINAPI vsync_thread( void *arg )   /* D3DVsyncSource::VBlankLoop */
{
    OVERLAPPED ov;
    struct msg m = { 1 };
    DWORD n;
    while (!stop)
    {
        memset( &ov, 0, sizeof(ov) );
        ov.hEvent = CreateEventW( NULL, TRUE, FALSE, NULL );
        m.seq = vsyncs_sent;
        if (!WriteFile( cli, &m, sizeof(m), &n, &ov ) && GetLastError() == ERROR_IO_PENDING)
            GetOverlappedResult( cli, &ov, &n, TRUE );
        CloseHandle( ov.hEvent );
        InterlockedIncrement( &vsyncs_sent );
        DwmFlush();
    }
    return 0;
}

static DWORD WINAPI io_thread( void *arg )   /* MessagePumpForIO: reads complete on the port */
{
    static struct msg buf;
    OVERLAPPED ov, *pov;
    ULONG_PTR key;
    DWORD n;

    memset( &ov, 0, sizeof(ov) );
    if (!ReadFile( srv, &buf, sizeof(buf), &n, &ov ) && GetLastError() != ERROR_IO_PENDING) return 1;
    while (!stop)
    {
        if (!GetQueuedCompletionStatus( port, &n, &key, &pov, 200 )) continue;
        if (pov != &ov) continue;
        PostThreadMessageW( ui_tid, buf.type == 1 ? MSG_VSYNC : MSG_REPLY, 0, 0 );
        memset( &ov, 0, sizeof(ov) );
        if (!ReadFile( srv, &buf, sizeof(buf), &n, &ov ) && GetLastError() != ERROR_IO_PENDING) return 1;
    }
    CancelIo( srv );
    return 0;
}

static DWORD WINAPI ui_thread( void *arg )   /* the compositor: WinUtils::WaitForMessage */
{
    MSG msg;
    PeekMessageW( &msg, NULL, 0, 0, PM_NOREMOVE );
    SetEvent( ui_ready );
    while (!stop)
    {
        MsgWaitForMultipleObjectsEx( 0, NULL, 200, QS_ALLINPUT, MWMO_INPUTAVAILABLE );
        while (PeekMessageW( &msg, NULL, 0, 0, PM_REMOVE ))
        {
            if (msg.message == MSG_VSYNC) InterlockedIncrement( &vsyncs_seen );
            if (msg.message == MSG_REPLY) InterlockedIncrement( &reply_seen );
        }
    }
    return 0;
}

int main( void )
{
    DWM_TIMING_INFO ti = { sizeof(ti) };
    double rate = 60, t0, t1, expect, roundtrip = -1;
    LARGE_INTEGER c;
    HANDLE th[3];
    int i, on_vblank = 0;
    WCHAR name[64];

    QueryPerformanceFrequency( &freq );
    if (SUCCEEDED( DwmGetCompositionTimingInfo( NULL, &ti ) ) && ti.rateRefresh.uiNumerator)
        rate = (double)ti.rateRefresh.uiNumerator / (ti.rateRefresh.uiDenominator ? ti.rateRefresh.uiDenominator : 1);
    printf( "rate=%.0f\n", rate );

    /* 1: thirty flushes take thirty frames */
    DwmFlush();
    t0 = now_ms();
    for (i = 0; i < 30; i++)
    {
        DwmFlush();
        /* and each returns just after a vertical blank */
        ti.cbSize = sizeof(ti);
        QueryPerformanceCounter( &c );
        if (SUCCEEDED( DwmGetCompositionTimingInfo( NULL, &ti ) ) && ti.qpcRefreshPeriod &&
            (c.QuadPart - (LONGLONG)ti.qpcVBlank) * 1000.0 / freq.QuadPart < 1000.0 / rate / 2)
            on_vblank++;
    }
    t1 = now_ms();
    expect = 30 * 1000.0 / rate;
    printf( "flush30_ms=%.0f expect=%.0f\n", t1 - t0, expect );
    printf( "on_vblank=%d/30\n", on_vblank );

    /* 1b: IDXGIOutput::WaitForVBlank waits too (Chromium's vsync thread) */
    {
        IDXGIFactory1 *factory;
        IDXGIAdapter1 *adapter;
        IDXGIOutput *output = NULL;

        if (SUCCEEDED( CreateDXGIFactory1( &IID_IDXGIFactory1, (void **)&factory ) ))
        {
            if (SUCCEEDED( IDXGIFactory1_EnumAdapters1( factory, 0, &adapter ) ))
            {
                IDXGIAdapter1_EnumOutputs( adapter, 0, &output );
                IDXGIAdapter1_Release( adapter );
            }
            IDXGIFactory1_Release( factory );
        }
        if (output)
        {
            HRESULT hr = IDXGIOutput_WaitForVBlank( output );
            t0 = now_ms();
            for (i = 0; i < 30 && SUCCEEDED( hr ); i++) hr = IDXGIOutput_WaitForVBlank( output );
            t1 = now_ms();
            if (SUCCEEDED( hr )) printf( "dxgi30_ms=%.0f\n", t1 - t0 );
            else printf( "dxgi30_ms=failed(%#lx)\n", hr );
            IDXGIOutput_Release( output );
        }
        else printf( "dxgi=none\n" );
    }

    /* 2: Firefox's vsync over its IPC channel, for one second */
    swprintf( name, 64, L"\\\\.\\pipe\\sg-vsync-probe.%lu", GetCurrentProcessId() );
    srv = CreateNamedPipeW( name, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
                            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE, 1, 4096, 4096, 5000, NULL );
    cli = CreateFileW( name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                       SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS | FILE_FLAG_OVERLAPPED, NULL );
    if (srv == INVALID_HANDLE_VALUE || cli == INVALID_HANDLE_VALUE) { printf( "pipe=failed %lu\n", GetLastError() ); return 1; }
    ConnectNamedPipe( srv, NULL );
    port = CreateIoCompletionPort( INVALID_HANDLE_VALUE, NULL, 0, 1 );
    CreateIoCompletionPort( srv, port, 1, 1 );
    ui_ready = CreateEventW( NULL, TRUE, FALSE, NULL );
    th[0] = CreateThread( NULL, 0, ui_thread, NULL, 0, &ui_tid );
    WaitForSingleObject( ui_ready, 5000 );
    th[1] = CreateThread( NULL, 0, io_thread, NULL, 0, NULL );
    th[2] = CreateThread( NULL, 0, vsync_thread, NULL, 0, NULL );
    Sleep( 500 );
    {
        /* a "synchronous request" answered through the same channel while
         * vsync runs: how long until the UI thread sees the reply */
        struct msg m = { 2 };
        OVERLAPPED ov = { 0 };
        DWORD n;
        LONG before = reply_seen;
        ov.hEvent = CreateEventW( NULL, TRUE, FALSE, NULL );
        t0 = now_ms();
        if (!WriteFile( cli, &m, sizeof(m), &n, &ov ) && GetLastError() == ERROR_IO_PENDING)
            GetOverlappedResult( cli, &ov, &n, TRUE );
        while (reply_seen == before && now_ms() - t0 < 5000) Sleep( 1 );
        if (reply_seen != before) roundtrip = now_ms() - t0;
        CloseHandle( ov.hEvent );
    }
    InterlockedExchange( &vsyncs_seen, 0 );
    t0 = now_ms();
    Sleep( 1000 );
    t1 = now_ms();
    i = vsyncs_seen;
    stop = 1;
    WaitForMultipleObjects( 3, th, TRUE, 5000 );
    printf( "ipc_vsyncs=%d per_s=%.0f roundtrip_ms=%.0f\n", i, i * 1000.0 / (t1 - t0), roundtrip );
    return 0;
}
