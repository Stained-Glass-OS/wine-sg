/* evrpresenter-probe: a DirectShow graph renders through the EVR filter with a presenter of the
 * program's own, as MPC-HC's EVR custom presenter does (patches/sg/6532).
 *
 * The probe writes a small uncompressed AVI (64x64, 25 fps, a red picture whose green grows each
 * frame), gives the EVR three streams, takes its IMFVideoRenderer through
 * GetService(MR_VIDEO_RENDER_SERVICE) and installs its presenter. The presenter finds the mixer,
 * the event sink and the clock, and like MPC-HC's draws frames only once OnClockStart said the
 * clock runs. It checks that frames come, that they are the video (red, not black) and change,
 * and that the clock's time moves on.
 * Prints name=value lines; see test/evrpresenter-gate.sh.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <dshow.h>
#include <d3d9.h>
#include <dxva2api.h>
#include <evr.h>
#include <mfapi.h>
#include <mferror.h>
#include <stdio.h>

DEFINE_GUID(CLSID_EnhancedVideoRenderer_, 0xfa10746c, 0x9b63, 0x4b6c, 0xbc, 0x49, 0xfc, 0x30, 0x0e, 0xa5, 0xf2, 0x56);
DEFINE_GUID(MR_VIDEO_RENDER_SERVICE_, 0x1092a86c, 0xab1a, 0x459a, 0xa3, 0x36, 0x83, 0x1f, 0xbc, 0x4d, 0x11, 0xff);
DEFINE_GUID(MR_VIDEO_MIXER_SERVICE_, 0x073cd2fc, 0x6cf4, 0x40b7, 0x88, 0x59, 0xe8, 0x95, 0x52, 0xc8, 0x41, 0xf8);
DEFINE_GUID(MR_VIDEO_ACCELERATION_SERVICE_, 0xefef5175, 0x5c7d, 0x4ce2, 0xbb, 0xbd, 0x34, 0xff, 0x8b, 0xca, 0x65, 0x54);

#define W 64
#define H 64
#define FRAMES 75

/* ---- the AVI file ---- */

static void put32( FILE *f, DWORD v ) { fwrite( &v, 4, 1, f ); }
static void put16( FILE *f, WORD v ) { fwrite( &v, 2, 1, f ); }
static void fourcc( FILE *f, const char *c ) { fwrite( c, 4, 1, f ); }

static void write_avi( const char *path )
{
    FILE *f = fopen( path, "wb" );
    DWORD frame_size = W * H * 4, movi_size = 4 + FRAMES * (8 + frame_size);
    DWORD strl_size = 4 + 8 + 56 + 8 + 40, hdrl_size = 4 + 8 + 56 + 8 + strl_size;
    DWORD idx_size = FRAMES * 16, riff_size = 4 + 8 + hdrl_size + 8 + movi_size + 8 + idx_size;
    static DWORD pixels[W * H];
    DWORD i, p, offset = 4;

    fourcc( f, "RIFF" ); put32( f, riff_size ); fourcc( f, "AVI " );
    fourcc( f, "LIST" ); put32( f, hdrl_size ); fourcc( f, "hdrl" );
    fourcc( f, "avih" ); put32( f, 56 );
    put32( f, 40000 ); put32( f, frame_size * 25 ); put32( f, 0 ); put32( f, 0x10 /* AVIF_HASINDEX */ );
    put32( f, FRAMES ); put32( f, 0 ); put32( f, 1 ); put32( f, frame_size ); put32( f, W ); put32( f, H );
    put32( f, 0 ); put32( f, 0 ); put32( f, 0 ); put32( f, 0 );
    fourcc( f, "LIST" ); put32( f, strl_size ); fourcc( f, "strl" );
    fourcc( f, "strh" ); put32( f, 56 );
    fourcc( f, "vids" ); fourcc( f, "DIB " ); put32( f, 0 ); put16( f, 0 ); put16( f, 0 ); put32( f, 0 );
    put32( f, 1 ); put32( f, 25 ); put32( f, 0 ); put32( f, FRAMES ); put32( f, frame_size ); put32( f, 0xffffffff );
    put32( f, 0 ); put16( f, 0 ); put16( f, 0 ); put16( f, W ); put16( f, H );
    fourcc( f, "strf" ); put32( f, 40 );
    put32( f, 40 ); put32( f, W ); put32( f, H ); put16( f, 1 ); put16( f, 32 ); put32( f, BI_RGB ); put32( f, frame_size );
    put32( f, 0 ); put32( f, 0 ); put32( f, 0 ); put32( f, 0 );
    fourcc( f, "LIST" ); put32( f, movi_size ); fourcc( f, "movi" );
    for (i = 0; i < FRAMES; i++)
    {
        for (p = 0; p < W * H; p++) pixels[p] = 0xff000000 | 0x00ff0000 | ((i * 3) << 8);   /* red, green growing */
        fourcc( f, "00db" ); put32( f, frame_size ); fwrite( pixels, frame_size, 1, f );
    }
    fourcc( f, "idx1" ); put32( f, idx_size );
    for (i = 0; i < FRAMES; i++)
    {
        fourcc( f, "00db" ); put32( f, 0x10 /* AVIIF_KEYFRAME */ ); put32( f, offset ); put32( f, frame_size );
        offset += 8 + frame_size;
    }
    fclose( f );
}

/* ---- the presenter ---- */

struct presenter
{
    IMFVideoPresenter IMFVideoPresenter_iface;
    IMFTopologyServiceLookupClient IMFTopologyServiceLookupClient_iface;
    IMFGetService IMFGetService_iface;
    IMFVideoDeviceID IMFVideoDeviceID_iface;
    LONG ref;
    CRITICAL_SECTION cs;
    IDirect3DDevice9 *device;
    IDirect3DDeviceManager9 *manager;
    IMFTransform *mixer;
    IMediaEventSink *sink;
    IMFClock *clock;
    IMFMediaType *type;
    IDirect3DSurface9 *target, *readback;
    IMFSample *sample;
    BOOL started;
    LONG frames, distinct;
    DWORD last_color, first_color;
};

static struct presenter presenter;

static HRESULT presenter_qi( REFIID riid, void **out )
{
    if (IsEqualGUID( riid, &IID_IUnknown ) || IsEqualGUID( riid, &IID_IMFVideoPresenter ) ||
        IsEqualGUID( riid, &IID_IMFClockStateSink ))
        *out = &presenter.IMFVideoPresenter_iface;
    else if (IsEqualGUID( riid, &IID_IMFTopologyServiceLookupClient ))
        *out = &presenter.IMFTopologyServiceLookupClient_iface;
    else if (IsEqualGUID( riid, &IID_IMFGetService ))
        *out = &presenter.IMFGetService_iface;
    else if (IsEqualGUID( riid, &IID_IMFVideoDeviceID ))
        *out = &presenter.IMFVideoDeviceID_iface;
    else
    {
        *out = NULL;
        return E_NOINTERFACE;
    }
    InterlockedIncrement( &presenter.ref );
    return S_OK;
}

static ULONG presenter_addref( void ) { return InterlockedIncrement( &presenter.ref ); }
static ULONG presenter_release( void ) { return InterlockedDecrement( &presenter.ref ); }

static void negotiate( void )
{
    IMFMediaType *type;
    DWORD i;

    for (i = 0; SUCCEEDED(IMFTransform_GetOutputAvailableType( presenter.mixer, 0, i, &type )); i++)
    {
        GUID subtype;
        if (SUCCEEDED(IMFMediaType_GetGUID( type, &MF_MT_SUBTYPE, &subtype )) &&
            subtype.Data1 == D3DFMT_X8R8G8B8 && SUCCEEDED(IMFTransform_SetOutputType( presenter.mixer, 0, type, 0 )))
        {
            if (presenter.type) IMFMediaType_Release( presenter.type );
            presenter.type = type;
            return;
        }
        IMFMediaType_Release( type );
    }
    printf( "negotiate: no X8R8G8B8 output\n" );
}

static void process_output( void )
{
    MFT_OUTPUT_DATA_BUFFER buffer;
    D3DLOCKED_RECT lr;
    DWORD status, color;
    HRESULT hr;

    if (!presenter.started || !presenter.type) return;   /* (as MPC-HC: frames only while the clock runs) */
    if (!presenter.target)
    {
        IDirect3DDevice9_CreateRenderTarget( presenter.device, W, H, D3DFMT_X8R8G8B8, 0, 0, FALSE, &presenter.target, NULL );
        IDirect3DDevice9_CreateOffscreenPlainSurface( presenter.device, W, H, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM,
                                                      &presenter.readback, NULL );
        MFCreateVideoSampleFromSurface( (IUnknown *)presenter.target, &presenter.sample );
    }
    for (;;)
    {
        memset( &buffer, 0, sizeof(buffer) );
        buffer.pSample = presenter.sample;
        hr = IMFTransform_ProcessOutput( presenter.mixer, 0, 1, &buffer, &status );
        if (buffer.pEvents) IMFCollection_Release( buffer.pEvents );
        if (FAILED(hr)) break;
        IDirect3DDevice9_GetRenderTargetData( presenter.device, presenter.target, presenter.readback );
        if (SUCCEEDED(IDirect3DSurface9_LockRect( presenter.readback, &lr, NULL, D3DLOCK_READONLY )))
        {
            color = ((DWORD *)lr.pBits)[(H / 2) * lr.Pitch / 4 + W / 2] & 0xffffff;
            IDirect3DSurface9_UnlockRect( presenter.readback );
            if (!presenter.frames) presenter.first_color = color;
            if (color != presenter.last_color) presenter.distinct++;
            presenter.last_color = color;
        }
        presenter.frames++;
    }
}

static HRESULT WINAPI vp_QueryInterface( IMFVideoPresenter *iface, REFIID riid, void **out ) { return presenter_qi( riid, out ); }
static ULONG WINAPI vp_AddRef( IMFVideoPresenter *iface ) { return presenter_addref(); }
static ULONG WINAPI vp_Release( IMFVideoPresenter *iface ) { return presenter_release(); }
static HRESULT WINAPI vp_OnClockStart( IMFVideoPresenter *iface, MFTIME systime, LONGLONG offset )
{
    EnterCriticalSection( &presenter.cs );
    presenter.started = TRUE;
    process_output();
    LeaveCriticalSection( &presenter.cs );
    return S_OK;
}
static HRESULT WINAPI vp_OnClockStop( IMFVideoPresenter *iface, MFTIME systime ) { presenter.started = FALSE; return S_OK; }
static HRESULT WINAPI vp_OnClockPause( IMFVideoPresenter *iface, MFTIME systime ) { presenter.started = FALSE; return S_OK; }
static HRESULT WINAPI vp_OnClockRestart( IMFVideoPresenter *iface, MFTIME systime ) { presenter.started = TRUE; return S_OK; }
static HRESULT WINAPI vp_OnClockSetRate( IMFVideoPresenter *iface, MFTIME systime, float rate ) { return S_OK; }
static HRESULT WINAPI vp_ProcessMessage( IMFVideoPresenter *iface, MFVP_MESSAGE_TYPE message, ULONG_PTR param )
{
    EnterCriticalSection( &presenter.cs );
    if (message == MFVP_MESSAGE_INVALIDATEMEDIATYPE) negotiate();
    else if (message == MFVP_MESSAGE_PROCESSINPUTNOTIFY) process_output();
    LeaveCriticalSection( &presenter.cs );
    return S_OK;
}
static HRESULT WINAPI vp_GetCurrentMediaType( IMFVideoPresenter *iface, IMFVideoMediaType **type )
{
    if (!presenter.type) return MF_E_NOT_INITIALIZED;
    return IMFMediaType_QueryInterface( presenter.type, &IID_IMFVideoMediaType, (void **)type );
}
static IMFVideoPresenterVtbl vp_vtbl =
{
    vp_QueryInterface, vp_AddRef, vp_Release, vp_OnClockStart, vp_OnClockStop, vp_OnClockPause, vp_OnClockRestart,
    vp_OnClockSetRate, vp_ProcessMessage, vp_GetCurrentMediaType,
};

static HRESULT WINAPI lc_QueryInterface( IMFTopologyServiceLookupClient *iface, REFIID riid, void **out ) { return presenter_qi( riid, out ); }
static ULONG WINAPI lc_AddRef( IMFTopologyServiceLookupClient *iface ) { return presenter_addref(); }
static ULONG WINAPI lc_Release( IMFTopologyServiceLookupClient *iface ) { return presenter_release(); }
static HRESULT WINAPI lc_InitServicePointers( IMFTopologyServiceLookupClient *iface, IMFTopologyServiceLookup *lookup )
{
    DWORD count = 1;
    HRESULT hr;

    hr = IMFTopologyServiceLookup_LookupService( lookup, MF_SERVICE_LOOKUP_GLOBAL, 0, &MR_VIDEO_MIXER_SERVICE_,
                                                  &IID_IMFTransform, (void **)&presenter.mixer, &count );
    count = 1;
    if (SUCCEEDED(hr)) hr = IMFTopologyServiceLookup_LookupService( lookup, MF_SERVICE_LOOKUP_GLOBAL, 0,
            &MR_VIDEO_RENDER_SERVICE_, &IID_IMediaEventSink, (void **)&presenter.sink, &count );
    count = 1;
    if (FAILED(IMFTopologyServiceLookup_LookupService( lookup, MF_SERVICE_LOOKUP_GLOBAL, 0, &MR_VIDEO_RENDER_SERVICE_,
            &IID_IMFClock, (void **)&presenter.clock, &count ))) presenter.clock = NULL;
    return hr;
}
static HRESULT WINAPI lc_ReleaseServicePointers( IMFTopologyServiceLookupClient *iface ) { return S_OK; }
static IMFTopologyServiceLookupClientVtbl lc_vtbl =
{
    lc_QueryInterface, lc_AddRef, lc_Release, lc_InitServicePointers, lc_ReleaseServicePointers,
};

static HRESULT WINAPI gs_QueryInterface( IMFGetService *iface, REFIID riid, void **out ) { return presenter_qi( riid, out ); }
static ULONG WINAPI gs_AddRef( IMFGetService *iface ) { return presenter_addref(); }
static ULONG WINAPI gs_Release( IMFGetService *iface ) { return presenter_release(); }
static HRESULT WINAPI gs_GetService( IMFGetService *iface, REFGUID service, REFIID riid, void **obj )
{
    if ((IsEqualGUID( service, &MR_VIDEO_RENDER_SERVICE_ ) || IsEqualGUID( service, &MR_VIDEO_ACCELERATION_SERVICE_ )) &&
        IsEqualGUID( riid, &IID_IDirect3DDeviceManager9 ))
        return IDirect3DDeviceManager9_QueryInterface( presenter.manager, riid, obj );
    *obj = NULL;
    return MF_E_UNSUPPORTED_SERVICE;
}
static IMFGetServiceVtbl gs_vtbl = { gs_QueryInterface, gs_AddRef, gs_Release, gs_GetService };

static HRESULT WINAPI di_QueryInterface( IMFVideoDeviceID *iface, REFIID riid, void **out ) { return presenter_qi( riid, out ); }
static ULONG WINAPI di_AddRef( IMFVideoDeviceID *iface ) { return presenter_addref(); }
static ULONG WINAPI di_Release( IMFVideoDeviceID *iface ) { return presenter_release(); }
static HRESULT WINAPI di_GetDeviceID( IMFVideoDeviceID *iface, IID *id ) { *id = IID_IDirect3DDevice9; return S_OK; }
static IMFVideoDeviceIDVtbl di_vtbl = { di_QueryInterface, di_AddRef, di_Release, di_GetDeviceID };

int main( void )
{
    char path[MAX_PATH];
    WCHAR wpath[MAX_PATH];
    IGraphBuilder *graph;
    IFilterGraph2 *graph2;
    IBaseFilter *evr, *source;
    IEVRFilterConfig *config;
    IMFGetService *gs;
    IMFVideoRenderer *renderer = NULL;
    IMediaControl *control;
    IEnumPins *pins;
    IPin *pin;
    IDirect3D9 *d3d;
    D3DPRESENT_PARAMETERS pp = { 0 };
    LONGLONG t1 = 0, t2 = 0;
    MFTIME s1, s2;
    MFCLOCK_STATE state = 0;
    UINT token;
    HWND hwnd;
    HRESULT hr;

    setvbuf( stdout, NULL, _IONBF, 0 );
    CoInitializeEx( NULL, COINIT_MULTITHREADED );
    GetTempPathA( MAX_PATH, path );
    strcat( path, "evrpresenter.avi" );
    write_avi( path );
    MultiByteToWideChar( CP_ACP, 0, path, -1, wpath, MAX_PATH );

    presenter.IMFVideoPresenter_iface.lpVtbl = &vp_vtbl;
    presenter.IMFTopologyServiceLookupClient_iface.lpVtbl = &lc_vtbl;
    presenter.IMFGetService_iface.lpVtbl = &gs_vtbl;
    presenter.IMFVideoDeviceID_iface.lpVtbl = &di_vtbl;
    presenter.ref = 1;
    InitializeCriticalSection( &presenter.cs );
    hwnd = CreateWindowA( "STATIC", "evrpresenter", WS_POPUP, 0, 0, W, H, NULL, NULL, NULL, NULL );
    d3d = Direct3DCreate9( D3D_SDK_VERSION );
    pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.BackBufferFormat = D3DFMT_X8R8G8B8; pp.hDeviceWindow = hwnd;
    hr = IDirect3D9_CreateDevice( d3d, 0, D3DDEVTYPE_HAL, hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED,
                                  &pp, &presenter.device );
    if (SUCCEEDED(hr)) hr = DXVA2CreateDirect3DDeviceManager9( &token, &presenter.manager );
    if (SUCCEEDED(hr)) hr = IDirect3DDeviceManager9_ResetDevice( presenter.manager, presenter.device, token );
    printf( "device=%d hr=%#lx\n", SUCCEEDED(hr), hr );
    if (FAILED(hr)) return 1;

    CoCreateInstance( &CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, &IID_IGraphBuilder, (void **)&graph );
    hr = CoCreateInstance( &CLSID_EnhancedVideoRenderer_, NULL, CLSCTX_INPROC_SERVER, &IID_IBaseFilter, (void **)&evr );
    if (FAILED(hr)) { printf( "evr hr=%#lx\n", hr ); return 1; }
    IGraphBuilder_AddFilter( graph, evr, L"EVR" );
    IBaseFilter_QueryInterface( evr, &IID_IEVRFilterConfig, (void **)&config );
    hr = IEVRFilterConfig_SetNumberOfStreams( config, 3 );
    printf( "three_streams=%d hr=%#lx\n", SUCCEEDED(hr), hr );
    IBaseFilter_QueryInterface( evr, &IID_IMFGetService, (void **)&gs );
    hr = IMFGetService_GetService( gs, &MR_VIDEO_RENDER_SERVICE_, &IID_IMFVideoRenderer, (void **)&renderer );
    printf( "renderer_service=%d hr=%#lx\n", SUCCEEDED(hr) && renderer, hr );
    if (!renderer) return 1;
    hr = IMFVideoRenderer_InitializeRenderer( renderer, NULL, &presenter.IMFVideoPresenter_iface );
    printf( "own_presenter=%d hr=%#lx\n", SUCCEEDED(hr), hr );

    hr = IGraphBuilder_AddSourceFilter( graph, wpath, L"source", &source );
    IBaseFilter_EnumPins( source, &pins );
    IEnumPins_Next( pins, 1, &pin, NULL );
    IGraphBuilder_QueryInterface( graph, &IID_IFilterGraph2, (void **)&graph2 );
    hr = IFilterGraph2_RenderEx( graph2, pin, AM_RENDEREX_RENDERTOEXISTINGRENDERERS, NULL );
    printf( "rendered=%d hr=%#lx\n", SUCCEEDED(hr), hr );
    if (FAILED(hr)) return 1;

    IGraphBuilder_QueryInterface( graph, &IID_IMediaControl, (void **)&control );
    IMediaControl_Run( control );
    Sleep( 600 );
    if (presenter.clock) IMFClock_GetCorrelatedTime( presenter.clock, 0, &t1, &s1 );
    Sleep( 1500 );
    if (presenter.clock)
    {
        IMFClock_GetCorrelatedTime( presenter.clock, 0, &t2, &s2 );
        IMFClock_GetState( presenter.clock, 0, &state );
    }
    EnterCriticalSection( &presenter.cs );
    printf( "clock=%d running=%d advanced=%d t1=%lld t2=%lld\n", presenter.clock != NULL, state == MFCLOCK_STATE_RUNNING,
            t2 - t1 > 10000000, (long long)t1, (long long)t2 );
    printf( "clock_start=%d\n", presenter.started );
    printf( "frames=%d count=%ld\n", presenter.frames >= 20, presenter.frames );
    printf( "picture=%d first=%06lx last=%06lx\n", presenter.frames && (presenter.first_color & 0xff0000) >= 0xe00000 &&
            presenter.last_color != 0, presenter.first_color, presenter.last_color );
    printf( "moving=%d distinct=%ld\n", presenter.distinct >= 10, presenter.distinct );
    LeaveCriticalSection( &presenter.cs );
    IMediaControl_Stop( control );
    printf( "done=1\n" );
    return 0;
}
