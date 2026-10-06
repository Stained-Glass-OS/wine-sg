/* neterrtext-probe: the system's message for each error code given, one per
 * line, as FormatMessage gives it (what NET USE prints after "System error N
 * has occurred."). For test/neterrtext-gate.sh. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    int i;
    for (i = 1; i < argc; i++)
    {
        char buf[1024];
        DWORD n = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                                 (DWORD)atoi(argv[i]), MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), buf, sizeof(buf), NULL);
        while (n && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) buf[--n] = 0;
        printf("%s|%s\n", argv[i], n ? buf : "(none)");
    }
    return 0;
}
