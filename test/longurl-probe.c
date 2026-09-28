/* longurl-probe URL-LENGTH: ShellExecute "sglongurl:" plus that many
 * characters, the scheme's handler registered by the gate; prints the
 * result. longurl-probe --handler OUT "%1": the handler, writes the length
 * of the URL it was given to OUT. (patches/sg/0472) */
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    static WCHAR url[40000];
    int n, i;

    if (argc == 4 && !strcmp(argv[1], "--handler"))
    {
        FILE *f = fopen(argv[2], "w");
        if (f) { fprintf(f, "%d\n", (int)strlen(argv[3])); fclose(f); }
        return 0;
    }
    if (argc < 2) return 2;
    n = atoi(argv[1]);
    wcscpy(url, L"sglongurl:x?body=");
    for (i = 0; i < n; i++) wcscat(url, L"%41");
    printf("url %d\n", (int)wcslen(url));
    fflush(stdout);
    printf("rc %d\n", (int)(INT_PTR)ShellExecuteW(NULL, NULL, url, NULL, NULL, SW_SHOWNORMAL));
    return 0;
}
