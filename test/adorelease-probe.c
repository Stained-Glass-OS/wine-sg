/* adorelease-probe: ADO connections that were never opened are released without touching a data
 * source they do not have (Stained Glass OS wine-sg 6033). The heap is first filled with freed blocks
 * of a pattern, so an unset pointer in a new connection is that pattern, not a lucky zero. */
#define COBJMACROS
#include <windows.h>
#include <stdio.h>

static const GUID CLSID_Connection_ = {0x00000514,0,0x10,{0x80,0,0,0xaa,0,0x6d,0x2e,0xa4}};

int main(void)
{
    IUnknown *conn[64];
    void *blocks[256];
    int i, n = 0, size;

    CoInitialize( NULL );
    for (size = 64; size <= 1024; size += 8)
    {
        for (i = 0; i < 4 && n < 256; i++)
        {
            blocks[n] = HeapAlloc( GetProcessHeap(), 0, size );
            memset( blocks[n++], 0x5b, size );
        }
    }
    while (n) HeapFree( GetProcessHeap(), 0, blocks[--n] );
    for (i = 0; i < 64; i++)
        if (FAILED(CoCreateInstance( &CLSID_Connection_, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)&conn[i] )))
        {
            printf( "create=failed\n" );
            return 1;
        }
    for (i = 0; i < 64; i++) IUnknown_Release( conn[i] );
    printf( "released=ok\n" );
    return 0;
}
