/* consel-clip -- for consel-gate.sh: "get" prints the clipboard's text,
 * "set TEXT" puts TEXT there. */
#include <windows.h>
#include <stdio.h>
int main(int argc, char **argv)
{
    if (argc > 2 && !strcmp(argv[1], "set"))
    {
        size_t n = strlen(argv[2]) + 1;
        HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, n);
        memcpy(GlobalLock(h), argv[2], n); GlobalUnlock(h);
        if (!OpenClipboard(NULL)) return 1;
        EmptyClipboard(); SetClipboardData(CF_TEXT, h); CloseClipboard();
        return 0;
    }
    if (!OpenClipboard(NULL)) return 1;
    {
        HANDLE h = GetClipboardData(CF_TEXT);
        const char *p = h ? GlobalLock(h) : NULL;
        printf("%s\n", p ? p : "");
        if (p) GlobalUnlock(h);
    }
    CloseClipboard();
    return 0;
}
