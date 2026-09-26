/* A console program started with no terminal and its output redirected
 * (patches/sg/0403): it can open CONOUT$ -- it has a console, as on
 * Windows -- and what it writes to its standard output still goes to the
 * file it was given. */
#include <windows.h>
#include <stdio.h>

int main( int argc, char **argv )
{
    HANDLE out = GetStdHandle( STD_OUTPUT_HANDLE ), con;
    char line[128];
    DWORD n, mode;
    int len;

    con = CreateFileA( "CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL );
    len = sprintf( line, "conout=%s stdout-is-console=%d\n", con != INVALID_HANDLE_VALUE ? "yes" : "no",
                   GetConsoleMode( out, &mode ) );
    WriteFile( out, line, len, &n, NULL );
    if (con != INVALID_HANDLE_VALUE)
    {
        CONSOLE_SCREEN_BUFFER_INFO info;
        len = sprintf( line, "screen-buffer=%d\n", GetConsoleScreenBufferInfo( con, &info ) );
        WriteFile( out, line, len, &n, NULL );
        CloseHandle( con );
    }
    return 3;
}
