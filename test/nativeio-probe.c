/* nativeio-probe: files as a native child's standard handles (patches/sg/0078).
 * "detached" = CREATE_NO_WINDOW, as a GUI program's child is.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>
/* files as a native child's stdin/stdout/stderr; flags: argv[1] "detached" = CREATE_NO_WINDOW */
int main( int argc, char **argv )
{
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    STARTUPINFOW si = { sizeof(si) }; PROCESS_INFORMATION pi;
    WCHAR tmp[MAX_PATH], in[MAX_PATH], out[MAX_PATH];
    WCHAR cmd[] = L"Z:\\usr\\bin\\sh -c \"read x; echo got:$x; echo err >&2\"";
    HANDLE hin, hout; char buf[256] = ""; DWORD got;
    GetTempPathW( MAX_PATH, tmp ); GetTempFileNameW( tmp, L"sgi", 0, in ); GetTempFileNameW( tmp, L"sgo", 0, out );
    hin = CreateFileW( in, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CREATE_ALWAYS, 0, NULL );
    WriteFile( hin, "hello\n", 6, &got, NULL ); SetFilePointer( hin, 0, NULL, FILE_BEGIN );
    hout = CreateFileW( out, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CREATE_ALWAYS, 0, NULL );
    si.dwFlags = STARTF_USESTDHANDLES; si.hStdInput = hin; si.hStdOutput = hout; si.hStdError = hout;
    if (!CreateProcessW( NULL, cmd, NULL, NULL, TRUE, argc > 1 ? CREATE_NO_WINDOW : 0, NULL, NULL, &si, &pi )) { printf( "fail %lu\n", GetLastError() ); return 1; }
    WaitForSingleObject( pi.hProcess, 10000 ); Sleep( 300 );
    SetFilePointer( hout, 0, NULL, FILE_BEGIN ); ReadFile( hout, buf, sizeof(buf) - 1, &got, NULL ); buf[got] = 0;
    printf( "%s: [%s]\n", argc > 1 ? "detached" : "console", buf );
    return 0;
}
