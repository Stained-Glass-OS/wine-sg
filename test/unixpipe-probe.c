/* unixpipe-probe.c (wine-sg 6094): a standard handle that is a Unix pipe answers the named pipe calls the way
 * a Windows anonymous pipe does. Writes KEY=VALUE lines to the file named by argv[1]. */
#include <windows.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    HANDLE h = GetStdHandle(STD_ERROR_HANDLE), d;
    DWORD mode = PIPE_READMODE_BYTE | PIPE_WAIT, cur = 0, inst = 0, flags = 99, out = 0, in = 0;
    FILE *f = fopen(argv[1], "w");
    BOOL ok;

    fprintf(f, "type=%lu\n", GetFileType(h));
    DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(), &d, 0, FALSE, DUPLICATE_SAME_ACCESS);
    ok = SetNamedPipeHandleState(d, &mode, NULL, NULL);
    fprintf(f, "set_byte_wait=%d %lu\n", ok, ok ? 0 : GetLastError());
    mode = PIPE_READMODE_MESSAGE;
    ok = SetNamedPipeHandleState(d, &mode, NULL, NULL);
    fprintf(f, "set_message=%d\n", ok);
    ok = GetNamedPipeHandleStateW(d, &cur, &inst, NULL, NULL, NULL, 0);
    fprintf(f, "get=%d mode=%lu instances=%lu\n", ok, cur, inst);
    ok = GetNamedPipeInfo(d, &flags, &out, &in, &inst);
    fprintf(f, "info=%d flags=%lu\n", ok, flags);
    fprintf(f, "done=1\n");
    fclose(f);
    return 0;
}
