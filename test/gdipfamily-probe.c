/* gdipfamily-probe NAME -- GdipCreateFontFamilyFromName(NAME): prints
 * "status N family F". For gdipfamily-gate.sh. */
#include <windows.h>
#include <stdio.h>
typedef struct { UINT32 v; void *cb; BOOL a, b; } SI;
int __stdcall GdiplusStartup(ULONG_PTR *, const SI *, void *);
int __stdcall GdipCreateFontFamilyFromName(const WCHAR *, void *, void **);
int __stdcall GdipGetFamilyName(void *, WCHAR *, LANGID);
int wmain(int argc, WCHAR **argv)
{
    ULONG_PTR t;
    SI si = { 1, 0, 0, 0 };
    void *f = 0;
    WCHAR got[64] = L"";
    int s;
    if (argc < 2) return 2;
    GdiplusStartup(&t, &si, 0);
    s = GdipCreateFontFamilyFromName(argv[1], 0, &f);
    if (!s) GdipGetFamilyName(f, got, 0);
    printf("status %d family %ls\n", s, got);
    return 0;
}
