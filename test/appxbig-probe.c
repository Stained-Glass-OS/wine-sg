/* test/appxbig-gate.sh's probe (7718): a package of many files, read as AddPackageAsync reads it.
 *   appxbig-probe FILE   CreatePackageReader, every payload file, each looked up in the block map:
 *                        Reader=hr, Files=n, Found=n, Ms=elapsed
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <shlwapi.h>
#include <stdio.h>

#define SLOT(obj, n, type) ((type)((*(void ***)(obj))[n]))
static const GUID clsid_factory = {0x5842a140,0xff9f,0x4166,{0x8f,0x5c,0x62,0xf5,0xb7,0xb0,0xc7,0x81}};
static const GUID iid_factory = {0xbeb94909,0xe451,0x438b,{0xb5,0xa7,0xd7,0x9e,0x76,0x7b,0x75,0xd8}};

int wmain( int argc, WCHAR **argv )
{
    void *factory = NULL, *reader = NULL, *files = NULL, *file, *blockmap = NULL, *bmfile;
    IStream *stream;
    DWORD start;
    BOOL has = FALSE;
    unsigned int count = 0, found = 0;
    WCHAR *name;
    HRESULT hr;

    setvbuf( stdout, NULL, _IONBF, 0 );
    CoInitializeEx( NULL, COINIT_MULTITHREADED );
    if (argc < 2) return 2;
    if (FAILED(CoCreateInstance( &clsid_factory, NULL, CLSCTX_INPROC_SERVER, &iid_factory, &factory ))) return 1;
    if (FAILED(SHCreateStreamOnFileEx( argv[1], STGM_READ, 0, FALSE, NULL, &stream ))) return 1;
    start = GetTickCount();
    hr = SLOT( factory, 4, HRESULT (WINAPI *)( void *, IStream *, void ** ) )( factory, stream, &reader );
    printf( "Reader=0x%08lx\n", hr );
    if (FAILED(hr)) return 0;
    SLOT( reader, 3, HRESULT (WINAPI *)( void *, void ** ) )( reader, &blockmap );
    SLOT( reader, 6, HRESULT (WINAPI *)( void *, void ** ) )( reader, &files );
    while (SUCCEEDED(SLOT( files, 4, HRESULT (WINAPI *)( void *, BOOL * ) )( files, &has )) && has)
    {
        if (FAILED(SLOT( files, 3, HRESULT (WINAPI *)( void *, void ** ) )( files, &file ))) break;
        count++;
        if (SUCCEEDED(SLOT( file, 5, HRESULT (WINAPI *)( void *, WCHAR ** ) )( file, &name )))
        {
            if (SUCCEEDED(SLOT( blockmap, 3, HRESULT (WINAPI *)( void *, const WCHAR *, void ** ) )( blockmap, name, &bmfile )))
                found++;
            CoTaskMemFree( name );
        }
        SLOT( files, 5, HRESULT (WINAPI *)( void *, BOOL * ) )( files, &has );
    }
    printf( "Files=%u\nFound=%u\nMs=%lu\n", count, found, GetTickCount() - start );
    return 0;
}
