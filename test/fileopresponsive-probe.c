/* A window stays responsive while its thread copies (patches/sg/0592): the
 * probe's window thread copies a folder of 1500 small files with
 * SHFileOperation while another thread pings the window every 100 ms
 * (SendMessageTimeout, 250 ms). All pings must be answered. */
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>

static HWND hwnd;
static volatile LONG done, pings, answered;

static DWORD WINAPI pinger( void *arg )
{
    DWORD_PTR r;
    while (!done)
    {
        Sleep( 100 );
        if (done) break;
        InterlockedIncrement( &pings );
        if (SendMessageTimeoutW( hwnd, WM_NULL, 0, 0, SMTO_BLOCK, 250, &r )) InterlockedIncrement( &answered );
    }
    return 0;
}

int main( void )
{
    SHFILEOPSTRUCTW op = { 0 };
    WCHAR path[MAX_PATH];
    DWORD t0;
    int i, ret;

    CreateDirectoryW( L"C:\\many", NULL );
    for (i = 0; i < 1500; i++)
    {
        HANDLE h;
        swprintf( path, MAX_PATH, L"C:\\many\\file%04d.txt", i );
        h = CreateFileW( path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );
        WriteFile( h, "0123456789abcdef0123456789abcdef", 32, (DWORD *)&ret, NULL );
        CloseHandle( h );
    }
    hwnd = CreateWindowW( L"STATIC", L"responsive", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 50, 50, 300, 200, 0, 0, 0, 0 );
    CloseHandle( CreateThread( NULL, 0, pinger, NULL, 0, NULL ) );
    op.hwnd = hwnd;
    op.wFunc = FO_COPY;
    op.pFrom = L"C:\\many\0";
    op.pTo = L"C:\\many-copy\0";
    op.fFlags = FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR | FOF_SILENT;
    t0 = GetTickCount();
    ret = SHFileOperationW( &op );
    done = 1;
    Sleep( 300 );
    printf( "copy ret=%d %lums pings=%ld answered=%ld copied=%d\n", ret, GetTickCount() - t0, pings, answered,
            GetFileAttributesW( L"C:\\many-copy\\file1499.txt" ) != INVALID_FILE_ATTRIBUTES );
    return !(ret == 0 && pings >= 3 && answered == pings);
}
