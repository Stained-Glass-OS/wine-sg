/* flushprinter-probe: winspool FlushPrinter, which was not exported (Stained Glass OS wine-sg 8270;
 * test/flushprinter-gate.sh). A print server handle whose output goes to a file (as a print processor sets
 * it up) gets the flushed bytes like WritePrinter's, and the call waits the given time.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <winspool.h>
#include <stdio.h>

int main( void )
{
    HMODULE ws = LoadLibraryA( "winspool.drv" );
    BOOL (WINAPI *pFlush)(HANDLE, void *, DWORD, DWORD *, DWORD) = (void *)GetProcAddress( ws, "FlushPrinter" );
    BOOL (WINAPI *pSetOutput)(HANDLE, HANDLE, HANDLE) = (void *)GetProcAddress( ws, "__wine_set_printer_output" );
    BOOL (WINAPI *pOpen)(WCHAR *, HANDLE *, void *) = (void *)GetProcAddress( ws, "OpenPrinterW" );
    char path[MAX_PATH], back[32] = { 0 };
    HANDLE printer = NULL, file;
    DWORD written = 0, start, took, got = 0;
    BOOL ret;
    int fails = 0;

    if (!pFlush) { printf( "FAIL  winspool exports FlushPrinter\nRESULT: FAIL\n" ); return 1; }
    ret = pFlush( NULL, "x", 1, &written, 0 );
    printf( "%s  no printer: FALSE, ERROR_INVALID_HANDLE (%lu)\n", !ret && GetLastError() == ERROR_INVALID_HANDLE ? "PASS" : "FAIL", GetLastError() );
    if (ret || GetLastError() != ERROR_INVALID_HANDLE) fails++;

    GetTempPathA( MAX_PATH, path );
    strcat( path, "sg-flushprinter.bin" );
    file = CreateFileA( path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL );
    if (!pOpen( NULL, &printer, NULL ) || !pSetOutput( printer, NULL, file ))
    { printf( "FAIL  a print server handle with file output (%lu)\nRESULT: FAIL\n", GetLastError() ); return 1; }
    start = GetTickCount();
    ret = pFlush( printer, "\x1b" "E reset", 8, &written, 300 );
    took = GetTickCount() - start;
    printf( "%s  FlushPrinter sends the bytes (%d, %lu written)\n", ret && written == 8 ? "PASS" : "FAIL", ret, written );
    if (!(ret && written == 8)) fails++;
    printf( "%s  and waits the idle time (%lu ms)\n", took >= 250 ? "PASS" : "FAIL", took );
    if (took < 250) fails++;
    SetFilePointer( file, 0, NULL, FILE_BEGIN );
    ReadFile( file, back, sizeof(back), &got, NULL );
    printf( "%s  the output holds them\n", got == 8 && !memcmp( back, "\x1b" "E reset", 8 ) ? "PASS" : "FAIL" );
    if (!(got == 8 && !memcmp( back, "\x1b" "E reset", 8 ))) fails++;
    pSetOutput( printer, NULL, NULL );
    ClosePrinter( printer );
    CloseHandle( file );
    DeleteFileA( path );
    printf( "RESULT: %s\n", fails ? "FAIL" : "PASS" );
    return fails != 0;
}
