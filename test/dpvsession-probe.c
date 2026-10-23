/* dpvsession-probe: DirectPlay Voice session state (Stained Glass OS wine-sg 6064), for
 * test/dpvsession-gate.sh. Hosts a DirectPlay 8 session on localhost and runs a voice server and client on
 * it. Prints KEY=VALUE lines; HRESULTs as hex. */
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <objbase.h>
#include <dplay8.h>
#include <dvoice.h>
#include <stdio.h>

static HRESULT WINAPI dp_handler( void *ctx, DWORD type, void *msg ) { return S_OK; }

static int connect_results;
static HRESULT connect_hr;
static HRESULT WINAPI dv_handler( void *ctx, DWORD type, void *msg )
{
    if (type == DVMSGID_CONNECTRESULT)
    {
        connect_results++;
        connect_hr = ((DVMSG_CONNECTRESULT *)msg)->hrResult;
    }
    return DV_OK;
}

#define P(...) printf(__VA_ARGS__)

int main( void )
{
    static const GUID app = { 0x5e1a6a5d, 0x1c2b, 0x4d3e, { 0x9f, 0x10, 0x21, 0x32, 0x43, 0x54, 0x65, 0x76 } };
    IDirectPlay8Server *dps = NULL;
    IDirectPlay8Client *dpc = NULL;
    IDirectPlay8Address *addr = NULL;
    IDirectPlayVoiceServer *vs = NULL, *vs2 = NULL;
    IDirectPlayVoiceClient *vc = NULL, *vc2 = NULL;
    IDirectPlayVoiceTest *vt = NULL;
    DPN_APPLICATION_DESC desc = { 0 };
    DVSESSIONDESC sd = { 0 }, got;
    DVSOUNDDEVICECONFIG sound = { 0 };
    DVCLIENTCONFIG cfg = { 0 };
    DVCAPS caps = { 0 };
    DVID targets[3] = { 7, 8, 9 }, out[4];
    DWORD n, mask = DVMSGID_CONNECTRESULT, badmask = 0x999;
    DWORD port = 20000 + GetCurrentProcessId() % 20000;
    HRESULT hr;

    setvbuf( stdout, NULL, _IONBF, 0 );
    CoInitialize( NULL );
    CoCreateInstance( &CLSID_DirectPlay8Server, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectPlay8Server, (void **)&dps );
    CoCreateInstance( &CLSID_DirectPlay8Client, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectPlay8Client, (void **)&dpc );
    CoCreateInstance( &CLSID_DirectPlayVoiceServer, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectPlayVoiceServer, (void **)&vs );
    CoCreateInstance( &CLSID_DirectPlayVoiceServer, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectPlayVoiceServer, (void **)&vs2 );
    CoCreateInstance( &CLSID_DirectPlayVoiceClient, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectPlayVoiceClient, (void **)&vc );
    CoCreateInstance( &CLSID_DirectPlayVoiceClient, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectPlayVoiceClient, (void **)&vc2 );
    CoCreateInstance( &CLSID_DirectPlayVoiceTest, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectPlayVoiceTest, (void **)&vt );
    CoCreateInstance( &CLSID_DirectPlay8Address, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectPlay8Address, (void **)&addr );
    P( "objects=%d\n", dps && dpc && vs && vs2 && vc && vc2 && vt && addr );
    if (!dps || !dpc || !vs || !vs2 || !vc || !vc2 || !vt || !addr) return 1;

    /* before the transport hosts anything */
    P( "InitNull=%#lx\n", IDirectPlayVoiceServer_Initialize( vs, NULL, dv_handler, NULL, NULL, 0 ) );
    P( "InitUninit=%#lx\n", IDirectPlayVoiceServer_Initialize( vs, (IUnknown *)dps, dv_handler, NULL, NULL, 0 ) );
    IDirectPlay8Server_Initialize( dps, NULL, dp_handler, 0 );
    P( "InitNoSession=%#lx\n", IDirectPlayVoiceServer_Initialize( vs, (IUnknown *)dps, dv_handler, NULL, NULL, 0 ) );
    P( "InitNotDplay=%#lx\n", IDirectPlayVoiceServer_Initialize( vs, (IUnknown *)addr, dv_handler, NULL, NULL, 0 ) );
    P( "StartUninit=%#lx\n", IDirectPlayVoiceServer_StartSession( vs, &sd, 0 ) );

    IDirectPlay8Address_SetSP( addr, &CLSID_DP8SP_TCPIP );
    IDirectPlay8Address_AddComponent( addr, DPNA_KEY_HOSTNAME, L"localhost", sizeof(L"localhost"), DPNA_DATATYPE_STRING );
    IDirectPlay8Address_AddComponent( addr, DPNA_KEY_PORT, &port, sizeof(port), DPNA_DATATYPE_DWORD );
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DPNSESSION_CLIENT_SERVER;
    desc.guidApplication = app;
    desc.pwszSessionName = (WCHAR *)L"sg voice";
    hr = IDirectPlay8Server_Host( dps, &desc, &addr, 1, NULL, NULL, NULL, 0 );
    P( "Host=%#lx\n", hr );

    P( "InitBadMask=%#lx\n", IDirectPlayVoiceServer_Initialize( vs, (IUnknown *)dps, dv_handler, NULL, &badmask, 1 ) );
    P( "InitMaskNoHandler=%#lx\n", IDirectPlayVoiceServer_Initialize( vs, (IUnknown *)dps, NULL, NULL, &mask, 1 ) );
    P( "Init=%#lx\n", IDirectPlayVoiceServer_Initialize( vs, (IUnknown *)dps, dv_handler, NULL, NULL, 0 ) );
    P( "InitAgain=%#lx\n", IDirectPlayVoiceServer_Initialize( vs, (IUnknown *)dps, dv_handler, NULL, NULL, 0 ) );

    /* the session */
    sd.dwSize = sizeof(sd);
    sd.dwSessionType = DVSESSIONTYPE_MIXING;
    sd.guidCT = DPVCTGUID_DEFAULT;
    P( "GetDescNotHosting=%#lx\n", (got.dwSize = sizeof(got), IDirectPlayVoiceServer_GetSessionDesc( vs, &got )) );
    sd.dwSessionType = 9;
    P( "StartBadType=%#lx\n", IDirectPlayVoiceServer_StartSession( vs, &sd, 0 ) );
    sd.dwSessionType = DVSESSIONTYPE_MIXING;
    sd.guidCT = app;
    P( "StartBadCompression=%#lx\n", IDirectPlayVoiceServer_StartSession( vs, &sd, 0 ) );
    sd.guidCT = DPVCTGUID_DEFAULT;
    sd.dwSize = 3;
    P( "StartBadSize=%#lx\n", IDirectPlayVoiceServer_StartSession( vs, &sd, 0 ) );
    sd.dwSize = sizeof(sd);
    P( "Start=%#lx\n", IDirectPlayVoiceServer_StartSession( vs, &sd, 0 ) );
    P( "StartAgain=%#lx\n", IDirectPlayVoiceServer_StartSession( vs, &sd, 0 ) );
    memset( &got, 0, sizeof(got) );
    got.dwSize = sizeof(got);
    hr = IDirectPlayVoiceServer_GetSessionDesc( vs, &got );
    P( "GetDesc=%#lx %lu %d\n", hr, got.dwSessionType, IsEqualGUID( &got.guidCT, &DPVCTGUID_DEFAULT ) );
    sd.dwBufferQuality = 50;
    sd.dwSessionType = DVSESSIONTYPE_PEER;
    hr = IDirectPlayVoiceServer_SetSessionDesc( vs, &sd );
    IDirectPlayVoiceServer_GetSessionDesc( vs, &got );
    P( "SetDesc=%#lx %lu %lu\n", hr, got.dwBufferQuality, got.dwSessionType );
    caps.dwSize = sizeof(caps);
    P( "Caps=%#lx\n", IDirectPlayVoiceServer_GetCaps( vs, &caps ) );
    P( "TargetsNotAllowed=%#lx\n", IDirectPlayVoiceServer_SetTransmitTargets( vs, 5, targets, 3, 0 ) );
    P( "SetMaskOk=%#lx\n", IDirectPlayVoiceServer_SetNotifyMask( vs, &mask, 1 ) );
    P( "Stop=%#lx\n", IDirectPlayVoiceServer_StopSession( vs, 0 ) );
    P( "StopAgain=%#lx\n", IDirectPlayVoiceServer_StopSession( vs, 0 ) );
    /* a session where the server controls the targets */
    sd.dwFlags = DVSESSION_SERVERCONTROLTARGET;
    sd.dwSessionType = DVSESSIONTYPE_FORWARDING;
    P( "StartControl=%#lx\n", IDirectPlayVoiceServer_StartSession( vs, &sd, 0 ) );
    P( "SetTargets=%#lx\n", IDirectPlayVoiceServer_SetTransmitTargets( vs, 5, targets, 3, 0 ) );
    n = 1;
    hr = IDirectPlayVoiceServer_GetTransmitTargets( vs, 5, out, &n, 0 );
    P( "GetTargetsSmall=%#lx %lu\n", hr, n );
    n = 4;
    hr = IDirectPlayVoiceServer_GetTransmitTargets( vs, 5, out, &n, 0 );
    P( "GetTargets=%#lx %lu %lu %lu %lu\n", hr, n, out[0], out[1], out[2] );
    n = 4;
    hr = IDirectPlayVoiceServer_GetTransmitTargets( vs, 6, out, &n, 0 );
    P( "GetTargetsOther=%#lx %lu\n", hr, n );

    /* a client: on a transport in no session, and on the host's */
    IDirectPlay8Client_Initialize( dpc, NULL, dp_handler, 0 );
    P( "ClientNoSession=%#lx\n", IDirectPlayVoiceClient_Initialize( vc, (IUnknown *)dpc, dv_handler, NULL, NULL, 0 ) );
    P( "ServerOnClient=%#lx\n", IDirectPlayVoiceServer_Initialize( vs2, (IUnknown *)dpc, dv_handler, NULL, NULL, 0 ) );
    sound.dwSize = sizeof(sound);
    cfg.dwSize = sizeof(cfg);
    P( "ConnectUninit=%#lx\n", IDirectPlayVoiceClient_Connect( vc, &sound, &cfg, DVFLAGS_SYNC ) );
    P( "ClientInit=%#lx\n", IDirectPlayVoiceClient_Initialize( vc, (IUnknown *)dps, dv_handler, NULL, &mask, 1 ) );
    P( "ConnectBadSize=%#lx\n", (cfg.dwSize = 2, IDirectPlayVoiceClient_Connect( vc, &sound, &cfg, DVFLAGS_SYNC )) );
    cfg.dwSize = sizeof(cfg);
    P( "ConnectSync=%#lx\n", IDirectPlayVoiceClient_Connect( vc, &sound, &cfg, DVFLAGS_SYNC ) );
    hr = IDirectPlayVoiceClient_Connect( vc, &sound, &cfg, 0 );
    P( "ConnectAsync=%#lx %d %#lx\n", hr, connect_results, connect_hr );
    P( "ClientNotConnected=%#lx\n", IDirectPlayVoiceClient_Disconnect( vc, 0 ) );
    P( "ClientCaps=%#lx\n", IDirectPlayVoiceClient_GetCaps( vc, &caps ) );
    P( "ClientUninitDesc=%#lx\n", (got.dwSize = sizeof(got), IDirectPlayVoiceClient_GetSessionDesc( vc2, &got )) );
    P( "SetupQuery=%#lx\n", IDirectPlayVoiceTest_CheckAudioSetup( vt, &DSDEVID_DefaultVoicePlayback,
                                                                  &DSDEVID_DefaultVoiceCapture, NULL, DVFLAGS_QUERYONLY ) );

    IDirectPlayVoiceClient_Release( vc );
    IDirectPlayVoiceClient_Release( vc2 );
    IDirectPlayVoiceServer_Release( vs );
    IDirectPlayVoiceServer_Release( vs2 );
    IDirectPlayVoiceTest_Release( vt );
    IDirectPlay8Server_Close( dps, 0 );
    IDirectPlay8Client_Close( dpc, 0 );
    P( "done=1\n" );
    return 0;
}
