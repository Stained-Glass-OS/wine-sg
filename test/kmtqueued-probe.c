/* kmtqueued-probe: D3DKMTSetQueuedLimit (Stained Glass OS wine-sg 6063), for test/kmtqueued-gate.sh.
 * Prints KEY=VALUE lines; statuses as hex. */
#define WIN32_NO_STATUS
#include <windows.h>
#include <winternl.h>
#include <ddk/d3dkmthk.h>
#include <stdio.h>

static NTSTATUS (WINAPI *pOpenAdapterFromHdc)(D3DKMT_OPENADAPTERFROMHDC *);
static NTSTATUS (WINAPI *pCreateDevice)(D3DKMT_CREATEDEVICE *);
static NTSTATUS (WINAPI *pDestroyDevice)(const D3DKMT_DESTROYDEVICE *);
static NTSTATUS (WINAPI *pSetQueuedLimit)(D3DKMT_SETQUEUEDLIMIT *);

static NTSTATUS limit( D3DKMT_HANDLE dev, D3DKMT_QUEUEDLIMIT_TYPE type, UINT *value )
{
    D3DKMT_SETQUEUEDLIMIT q = {0};
    NTSTATUS st;
    q.hDevice = dev;
    q.Type = type;
    q.QueuedPresentLimit = *value;
    st = pSetQueuedLimit( &q );
    *value = q.QueuedPresentLimit;
    return st;
}

int main( void )
{
    HMODULE gdi = LoadLibraryA( "gdi32.dll" );
    D3DKMT_OPENADAPTERFROMHDC adapter = {0};
    D3DKMT_CREATEDEVICE dev = {0}, dev2 = {0};
    D3DKMT_DESTROYDEVICE destroy = {0};
    UINT v;
    NTSTATUS st;

    pOpenAdapterFromHdc = (void *)GetProcAddress( gdi, "D3DKMTOpenAdapterFromHdc" );
    pCreateDevice = (void *)GetProcAddress( gdi, "D3DKMTCreateDevice" );
    pDestroyDevice = (void *)GetProcAddress( gdi, "D3DKMTDestroyDevice" );
    pSetQueuedLimit = (void *)GetProcAddress( gdi, "D3DKMTSetQueuedLimit" );
    adapter.hDc = GetDC( NULL );
    printf( "OpenAdapter=%#lx\n", (unsigned long)pOpenAdapterFromHdc( &adapter ) );
    dev.hAdapter = dev2.hAdapter = adapter.hAdapter;
    printf( "CreateDevice=%#lx\n", (unsigned long)pCreateDevice( &dev ) );
    pCreateDevice( &dev2 );

    v = 0; st = limit( dev.hDevice, D3DKMT_GET_QUEUEDLIMIT_PRESENT, &v );
    printf( "GetInitial=%#lx %u\n", (unsigned long)st, v );
    v = 2; st = limit( dev.hDevice, D3DKMT_SET_QUEUEDLIMIT_PRESENT, &v );
    printf( "Set=%#lx\n", (unsigned long)st );
    v = 0; st = limit( dev.hDevice, D3DKMT_GET_QUEUEDLIMIT_PRESENT, &v );
    printf( "Get=%#lx %u\n", (unsigned long)st, v );
    v = 0; st = limit( dev2.hDevice, D3DKMT_GET_QUEUEDLIMIT_PRESENT, &v );
    printf( "GetOther=%#lx %u\n", (unsigned long)st, v );
    v = 2; st = limit( 0xdead, D3DKMT_SET_QUEUEDLIMIT_PRESENT, &v );
    printf( "BadDevice=%#lx\n", (unsigned long)st );
    v = 2; st = limit( dev.hDevice, 7, &v );
    printf( "BadType=%#lx\n", (unsigned long)st );
    printf( "Null=%#lx\n", (unsigned long)pSetQueuedLimit( NULL ) );
    destroy.hDevice = dev.hDevice;
    pDestroyDevice( &destroy );
    v = 2; st = limit( dev.hDevice, D3DKMT_SET_QUEUEDLIMIT_PRESENT, &v );
    printf( "Destroyed=%#lx\n", (unsigned long)st );
    printf( "done=1\n" );
    return 0;
}
