/* mmiotemp-probe: winmm mmioOpen with MMIO_GETTEMP (Stained Glass OS wine-sg 6066), for test/mmiotemp-gate.sh.
 * Prints KEY=VALUE lines. */
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <string.h>

int main( void )
{
    char buf[128], buf2[128], tmp[MAX_PATH];
    WCHAR wbuf[128];
    MMIOINFO info;
    HMMIO h;

    CreateDirectoryA( "C:\\mmtmp", NULL );
    GetTempPathA( sizeof(tmp), tmp );

    buf[0] = 0;
    h = mmioOpenA( buf, NULL, MMIO_GETTEMP );
    printf( "Empty=%Iu %d %d\n", (ULONG_PTR)h, !_strnicmp( buf, tmp, strlen( tmp ) ),
            GetFileAttributesA( buf ) != INVALID_FILE_ATTRIBUTES );
    strcpy( buf2, buf );
    buf[0] = 0;
    mmioOpenA( buf, NULL, MMIO_GETTEMP );
    printf( "Unique=%d\n", strcmp( buf, buf2 ) != 0 );
    strcpy( buf, "C:\\mmtmp\\take.wav" );
    h = mmioOpenA( buf, NULL, MMIO_GETTEMP | MMIO_READ );
    printf( "InDir=%Iu %d\n", (ULONG_PTR)h, !_strnicmp( buf, "C:\\mmtmp\\", 9 ) );
    strcpy( buf, "C:\\mmtmp" );
    h = mmioOpenA( buf, NULL, MMIO_GETTEMP );
    printf( "Dir=%Iu %d\n", (ULONG_PTR)h, !_strnicmp( buf, "C:\\mmtmp\\", 9 ) );
    memset( &info, 0, sizeof(info) );
    h = mmioOpenA( NULL, &info, MMIO_GETTEMP );
    printf( "Null=%Iu %u\n", (ULONG_PTR)h, info.wErrorRet );
    wbuf[0] = 0;
    h = mmioOpenW( wbuf, NULL, MMIO_GETTEMP );
    printf( "Wide=%Iu %d\n", (ULONG_PTR)h, wbuf[0] != 0 && GetFileAttributesW( wbuf ) != INVALID_FILE_ATTRIBUTES );
    printf( "done=1\n" );
    return 0;
}
