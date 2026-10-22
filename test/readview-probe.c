/* readview-probe DLL... -- print each file's version as GetFileVersionInfo reads it ("a.b.c.d")
 * and, for the first one, whether LoadLibraryEx(LOAD_LIBRARY_AS_IMAGE_RESOURCE) gives a
 * view of that file's own resources (a string resource 1 of "readview"), and whether
 * LoadLibrary, to run it, still gives Wine's builtin (it exports memcpy; ours exports nothing).
 * For readview-gate.sh. */
#include <windows.h>
#include <stdio.h>
static void version(const WCHAR *path)
{
    DWORD h, n = GetFileVersionInfoSizeW(path, &h);
    VS_FIXEDFILEINFO *f;
    UINT s;
    void *b;
    if (!n || !(b = malloc(n)) || !GetFileVersionInfoW(path, 0, n, b) || !VerQueryValueW(b, L"\\", (void **)&f, &s))
    {
        printf("version err %lu\n", GetLastError());
        return;
    }
    printf("version %lu.%lu.%lu.%lu\n", HIWORD(f->dwFileVersionMS), LOWORD(f->dwFileVersionMS),
           HIWORD(f->dwFileVersionLS), LOWORD(f->dwFileVersionLS));
}
int wmain(int argc, WCHAR **argv)
{
    WCHAR buf[64] = L"";
    HMODULE m;
    int i;
    for (i = 1; i < argc; i++) version(argv[i]);
    if (argc < 2) return 2;
    if (!(m = LoadLibraryExW(argv[1], 0, LOAD_LIBRARY_AS_IMAGE_RESOURCE))) { printf("resource err %lu\n", GetLastError()); return 1; }
    LoadStringW(m, 1, buf, ARRAYSIZE(buf));
    printf("resource %ls\n", buf[0] ? buf : L"(none)");
    FreeLibrary(m);
    if (!(m = LoadLibraryW(argv[1]))) { printf("load err %lu\n", GetLastError()); return 1; }
    printf("load %s\n", GetProcAddress(m, "memcpy") ? "builtin" : "native");
    return 0;
}
