/* serverfds-probe N DIR: open N files at once (each one's descriptor is held by
 * the wineserver) and say how many opened. For serverfds-gate.sh. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    int n = argc > 2 ? atoi(argv[1]) : 0, i, ok = 0;
    DWORD err = 0;
    HANDLE *h = calloc(n, sizeof(*h));
    char path[MAX_PATH];

    for (i = 0; i < n; i++)
    {
        snprintf(path, sizeof(path), "%s\\f%04d.txt", argv[2], i);
        h[i] = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        if (h[i] == INVALID_HANDLE_VALUE) { err = GetLastError(); break; }
        ok++;
    }
    printf("OPENED %d ERR %lu\n", ok, err);
    fflush(stdout);
    for (i = 0; i < ok; i++) CloseHandle(h[i]);
    return ok == n ? 0 : 1;
}
