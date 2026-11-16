/* Files copied in Wine as text/uri-list for X programs (wine-sg 7789), for
 * test/urilist-gate.sh: puts CF_HDROP of PATH... on the clipboard, wide or
 * ANSI (DROPFILES.fWide), prints "SET n" and stays a while (the X selection
 * is answered from Wine's clipboard).
 *
 *   urilist-probe.exe wide|ansi SECONDS PATH...
 */
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>

int wmain( int argc, WCHAR **argv )
{
    BOOL wide;
    size_t size = sizeof(DROPFILES) + 4;
    HGLOBAL mem;
    char *p;
    int i;

    if (argc < 4) return 2;
    wide = !wcscmp( argv[1], L"wide" );
    for (i = 3; i < argc; i++) size += (wcslen( argv[i] ) + 1) * (wide ? sizeof(WCHAR) : 2);
    mem = GlobalAlloc( GMEM_MOVEABLE | GMEM_ZEROINIT, size );
    p = GlobalLock( mem );
    ((DROPFILES *)p)->pFiles = sizeof(DROPFILES);
    ((DROPFILES *)p)->fWide = wide;
    p += sizeof(DROPFILES);
    for (i = 3; i < argc; i++)
    {
        if (wide)
        {
            wcscpy( (WCHAR *)p, argv[i] );
            p += (wcslen( argv[i] ) + 1) * sizeof(WCHAR);
        }
        else p += WideCharToMultiByte( CP_ACP, 0, argv[i], -1, p, (int)size, NULL, NULL );
    }
    GlobalUnlock( mem );
    if (!OpenClipboard( NULL )) { printf( "FAIL open\n" ); return 1; }
    EmptyClipboard();
    if (!SetClipboardData( CF_HDROP, mem )) { printf( "FAIL set\n" ); return 1; }
    CloseClipboard();
    printf( "SET %d\n", argc - 3 );
    fflush( stdout );
    Sleep( _wtoi( argv[2] ) * 1000 );
    return 0;
}
