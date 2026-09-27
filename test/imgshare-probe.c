/* imgshare-probe NAME [write]: load imgshare.dll, check every byte of its
 * blob, print where it is mapped, optionally write to its writable data,
 * then hold for the gate to look at its memory (wine-sg 0431).
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

typedef const unsigned char *(*get_blob_fn)( SIZE_T * );
typedef unsigned char *(*get_wdata_fn)(void);

int main( int argc, char **argv )
{
    HMODULE m = LoadLibraryA( "imgshare.dll" );
    get_blob_fn get_blob;
    get_wdata_fn get_wdata;
    IMAGE_NT_HEADERS *nt;
    const unsigned char *b;
    unsigned char *w;
    SIZE_T size, i, bad = 0;

    setvbuf( stdout, NULL, _IONBF, 0 );
    if (!m) { printf( "NODLL %lu\n", GetLastError() ); return 1; }
    get_blob = (get_blob_fn)GetProcAddress( m, "get_blob" );
    get_wdata = (get_wdata_fn)GetProcAddress( m, "get_wdata" );
    b = get_blob( &size );
    for (i = 0; i < size; i++) if (b[i] != (unsigned char)(i * 7 + (i >> 12))) bad++;
    nt = (IMAGE_NT_HEADERS *)((char *)m + ((IMAGE_DOS_HEADER *)m)->e_lfanew);
    printf( "MAPPED %p %lx\n", m, (unsigned long)nt->OptionalHeader.SizeOfImage );
    printf( "BLOB %s %lu bytes, %lu wrong\n", argv[1], (unsigned long)size, (unsigned long)bad );
    w = get_wdata();
    if (argc > 2) { w[0] = 0x55; w[4096] = 0x66; }
    printf( "WDATA %s %u %u\n", argv[1], w[0], w[1] );
    Sleep( 60000 );
    printf( "WDATA_AFTER %s %u\n", argv[1], w[0] );
    return 0;
}
