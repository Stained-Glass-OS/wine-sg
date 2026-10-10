/* shell32 batch (patches/sg/2048), run by test/shellord-gate.sh. Families:
 * IShellLink::SetPath resolving a program name, the stock icon of an unknown
 * id, ExtractAssociatedIcon's fallback, the shell's number and size text,
 * SHLocal*, SHIsBadInterfacePtr, DragQueryFileAorW, the She* path helpers and
 * PathProcessCommand. Functions are looked up with GetProcAddress.
 *
 *   shellord-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#define PPCF_ADDQUOTES 0x01
#define PPCF_INCLUDEARGS 0x02
#define PPCF_ADDARGUMENTS 0x03
#define PPCF_NODIRECTORIES 0x10
#define PPCF_FORCEQUALIFY 0x40
#define PPCF_LONGESTPOSSIBLE 0x80

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECK(expr) check((expr), #expr)

static HMODULE sh;
static void *fn(const char *name, int ordinal)
{
    void *p = (void *)GetProcAddress(sh, name);
    if (!p && ordinal) p = (void *)GetProcAddress(sh, MAKEINTRESOURCEA(ordinal));
    if (!p) { printf("FAIL  missing export %s\n", name); failures++; }
    return p;
}

static void touch(const char *path)
{
    HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
}

static int path_after_setpath(const char *given, char *got, HRESULT *hr)
{
    IShellLinkA *link;
    HRESULT r = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkA, (void **)&link);
    if (FAILED(r)) return 0;
    *hr = IShellLinkA_SetPath(link, given);
    strcpy(got, "garbage");
    IShellLinkA_GetPath(link, got, MAX_PATH, NULL, SLGP_RAWPATH);
    IShellLinkA_Release(link);
    return 1;
}

int main(void)
{
    char tmp[MAX_PATH], dir[MAX_PATH], a[MAX_PATH], b[MAX_PATH], want[MAX_PATH], got[MAX_PATH];
    WCHAR wout[MAX_PATH], win[MAX_PATH];
    HRESULT hr;
    SHSTOCKICONINFO sii;
    char buf[300];
    HICON icon;
    WORD index;
    LONG n;

    /* ordinal lookups for the ones that may not be named */
    UINT (WINAPI *DragQueryFileAorW)(HDROP, UINT, void *, UINT, BOOL, BOOL);
    HLOCAL (WINAPI *SHLocalAlloc)(UINT, SIZE_T);
    HLOCAL (WINAPI *SHLocalFree)(HLOCAL);
    HLOCAL (WINAPI *SHLocalReAlloc)(HLOCAL, SIZE_T, UINT);
    WCHAR *(WINAPI *AddCommasW)(DWORD, WCHAR *);
    WCHAR *(WINAPI *ShortSizeFormatW)(DWORD, WCHAR *);
    BOOL (WINAPI *SHIsBadInterfacePtr)(const void *, UINT);
    WCHAR *(WINAPI *SheRemoveQuotesW)(WCHAR *);
    char *(WINAPI *SheRemoveQuotesA)(char *);
    DWORD (WINAPI *SheFullPathA)(const char *, DWORD, char *);
    DWORD (WINAPI *SheFullPathW)(const WCHAR *, DWORD, WCHAR *);
    BOOL (WINAPI *SheShortenPathA)(char *, BOOL);
    BOOL (WINAPI *SheShortenPathW)(WCHAR *, BOOL);
    int (WINAPI *SheGetCurDrive)(void);
    LONG (WINAPI *PathProcessCommand)(const void *, void *, DWORD, DWORD);
    HRESULT (WINAPI *SHGetStockIconInfo_)(int, UINT, SHSTOCKICONINFO *);
    HICON (WINAPI *ExtractAssociatedIconA_)(HINSTANCE, char *, WORD *);

    CoInitialize(NULL);
    sh = LoadLibraryA("shell32.dll");
    DragQueryFileAorW = fn("DragQueryFileAorW", 0); SHLocalAlloc = fn("SHLocalAlloc", 200); SHLocalFree = fn("SHLocalFree", 201);
    SHLocalReAlloc = fn("SHLocalReAlloc", 202); AddCommasW = fn("AddCommasW", 203); ShortSizeFormatW = fn("ShortSizeFormatW", 204);
    SHIsBadInterfacePtr = fn("SHIsBadInterfacePtr", 84); SheRemoveQuotesW = fn("SheRemoveQuotesW", 0); SheRemoveQuotesA = fn("SheRemoveQuotesA", 0);
    SheFullPathA = fn("SheFullPathA", 0); SheFullPathW = fn("SheFullPathW", 0); SheShortenPathA = fn("SheShortenPathA", 0);
    SheShortenPathW = fn("SheShortenPathW", 0); SheGetCurDrive = fn("SheGetCurDrive", 0);
    PathProcessCommand = fn("PathProcessCommand", 653);
    SHGetStockIconInfo_ = fn("SHGetStockIconInfo", 0); ExtractAssociatedIconA_ = fn("ExtractAssociatedIconA", 0);

    GetTempPathA(sizeof(tmp), tmp);
    sprintf(dir, "%sshellord-probe", tmp);
    CreateDirectoryA(dir, NULL);
    GetLongPathNameA(dir, dir, sizeof(dir));

    /* IShellLink::SetPath: a program name gets its extension and place */
    sprintf(a, "%s\\prog.exe", dir); touch(a);
    sprintf(a, "%s\\script.bat", dir); touch(a);
    sprintf(a, "%s\\prog", dir);
    sprintf(want, "%s\\prog.exe", dir);
    CHECK(path_after_setpath(a, got, &hr) && !lstrcmpiA(got, want));
    sprintf(a, "%s\\script", dir);
    sprintf(want, "%s\\script.bat", dir);
    CHECK(path_after_setpath(a, got, &hr) && !lstrcmpiA(got, want));
    sprintf(a, "%s\\prog.exe", dir);
    CHECK(path_after_setpath(a, got, &hr) && hr == S_OK && !lstrcmpiA(got, a));
    SearchPathA(NULL, "rundll32.exe", NULL, MAX_PATH, want, NULL);
    CHECK(path_after_setpath("rundll32", got, &hr) && !lstrcmpiA(got, want));
    CHECK(path_after_setpath("\"c:\\nonexistent\\file\"", got, &hr) && !strcmp(got, "C:\\nonexistent\\file"));
    CHECK(path_after_setpath("c:\\nonexistent\\file", got, &hr) && hr == S_FALSE && !strcmp(got, "C:\\nonexistent\\file"));

    /* an unknown stock icon */
    memset(&sii, 0x23, sizeof(sii));
    sii.cbSize = sizeof(sii);
    CHECK(SHGetStockIconInfo_(300, SHGSI_ICONLOCATION, &sii) == E_INVALIDARG && sii.iSysImageIndex == -1 && sii.iIcon == -1);
    sii.cbSize = sizeof(sii);
    CHECK(SHGetStockIconInfo_(SIID_FOLDER, SHGSI_ICONLOCATION, &sii) == S_OK && wcsstr(sii.szPath, L"shell32.dll"));

    /* the icon of a file without one */
    index = 0; buf[0] = 0;
    icon = ExtractAssociatedIconA_(NULL, buf, &index);
    CHECK(icon != NULL && !buf[0] && index == 0);
    if (icon) DestroyIcon(icon);
    index = 5000; strcpy(buf, "user32.dll");
    icon = ExtractAssociatedIconA_(NULL, buf, &index);
    CharLowerBuffA(buf, strlen(buf));
    CHECK(icon != NULL && strstr(buf, "shell32.dll") && index != 5000);
    if (icon) DestroyIcon(icon);
    index = 0xcaca; strcpy(buf, "dummy.exe");
    icon = ExtractAssociatedIconA_(NULL, buf, &index);
    CharLowerBuffA(buf, strlen(buf));
    CHECK(icon != NULL && strstr(buf, "shell32.dll") && index != 0xcaca);
    if (icon) DestroyIcon(icon);

    /* number and size text */
    wcscpy(wout, L"########");
    CHECK(AddCommasW(1234567, wout) == wout && !wcscmp(wout, L"1,234,567"));
    CHECK(AddCommasW(999, wout) == wout && !wcscmp(wout, L"999"));
    CHECK(AddCommasW(0, wout) == wout && !wcscmp(wout, L"0"));
    CHECK(AddCommasW(4294967295u, wout) == wout && !wcscmp(wout, L"4,294,967,295"));
    CHECK(ShortSizeFormatW(1536, wout) == wout && !wcscmp(wout, L"1.50 KB"));
    CHECK(ShortSizeFormatW(500, wout) == wout && !wcscmp(wout, L"500 bytes"));

    /* the shell's local heap */
    {
        BYTE *p = SHLocalAlloc(LMEM_ZEROINIT, 16);
        CHECK(p != NULL && p[0] == 0 && p[15] == 0);
        if (p)
        {
            p[3] = 42;
            p = SHLocalReAlloc(p, 4096, LMEM_ZEROINIT | LMEM_MOVEABLE);
            CHECK(p != NULL && p[3] == 42 && p[4000] == 0);
            CHECK(SHLocalFree(p) == NULL);
        }
    }

    /* interface pointers */
    {
        IStream *stream = NULL;
        BYTE *page = VirtualAlloc(NULL, 8192, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        struct { void **vtbl; } fake;
        DWORD old;

        CreateStreamOnHGlobal(NULL, TRUE, &stream);
        CHECK(!SHIsBadInterfacePtr(stream, 14));
        IStream_Release(stream);
        CHECK(SHIsBadInterfacePtr(NULL, 3));
        VirtualProtect(page + 4096, 4096, PAGE_NOACCESS, &old);
        fake.vtbl = (void **)(page + 4096 - 2 * sizeof(void *));
        CHECK(!SHIsBadInterfacePtr(&fake, 2));
        CHECK(SHIsBadInterfacePtr(&fake, 4));
        VirtualFree(page, 0, MEM_RELEASE);
    }

    /* dropped files */
    {
        static const char files[] = "C:\\a.txt\0C:\\b.txt\0";
        HGLOBAL h = GlobalAlloc(GHND, sizeof(DROPFILES) + sizeof(files) + 1);
        DROPFILES *df = GlobalLock(h);
        char name[64];
        WCHAR wname[64];
        df->pFiles = sizeof(DROPFILES);
        memcpy((char *)df + sizeof(DROPFILES), files, sizeof(files));
        GlobalUnlock(h);
        CHECK(DragQueryFileAorW((HDROP)h, 0xffffffff, NULL, 0, TRUE, FALSE) == 2);
        name[0] = 0;
        CHECK(DragQueryFileAorW((HDROP)h, 1, name, sizeof(name), TRUE, FALSE) == 8 && !strcmp(name, "C:\\b.txt"));
        GlobalFree(h);
        (void)wname;
    }

    /* She* helpers */
    {
        WCHAR q[64] = L"\"C:\\a b\" /x", plain[16] = L"plain", empty[4] = L"";
        char qa[64] = "\"C:\\a b\" /x", plaina[16] = "plain";
        CHECK(SheRemoveQuotesW(q) == q && !wcscmp(q, L"C:\\a b"));
        CHECK(SheRemoveQuotesW(plain) == plain && !wcscmp(plain, L"plain"));
        CHECK(SheRemoveQuotesW(empty) == empty && !empty[0]);
        CHECK(SheRemoveQuotesA(qa) == qa && !strcmp(qa, "C:\\a b"));
        CHECK(SheRemoveQuotesA(plaina) == plaina && !strcmp(plaina, "plain"));
        SetCurrentDirectoryA("C:\\windows");
        CHECK(SheFullPathA("rel.txt", sizeof(a), a) == strlen("C:\\windows\\rel.txt") && !lstrcmpiA(a, "C:\\windows\\rel.txt"));
        CHECK(SheFullPathW(L"x\\..\\y.txt", ARRAY_SIZE(wout), wout) > 0 && !wcsicmp(wout, L"C:\\windows\\y.txt"));
        CHECK(SheGetCurDrive() == 2);
        sprintf(a, "%s\\A Long Folder Name", dir);
        CreateDirectoryA(a, NULL);
        strcpy(b, a);
        CHECK(SheShortenPathA(b, TRUE) && strlen(b) < strlen(a) && strchr(b, '~'));
        CHECK(SheShortenPathA(b, FALSE) && !lstrcmpiA(b, a));
        MultiByteToWideChar(CP_ACP, 0, a, -1, win, ARRAY_SIZE(win));
        MultiByteToWideChar(CP_ACP, 0, a, -1, wout, ARRAY_SIZE(wout));
        CHECK(SheShortenPathW(win, TRUE) && wcschr(win, '~'));
        CHECK(SheShortenPathW(win, FALSE) && !wcsicmp(win, wout));
        strcpy(b, "C:\\no\\such\\path");
        CHECK(!SheShortenPathA(b, TRUE) && !strcmp(b, "C:\\no\\such\\path"));
        RemoveDirectoryA(a);
    }

    /* PathProcessCommand */
    {
        char line[2 * MAX_PATH];
        WCHAR wline[2 * MAX_PATH];
        WCHAR expect[2 * MAX_PATH];
        char t[MAX_PATH];

        sprintf(t, "%s\\my prog.exe", dir); touch(t);
        sprintf(t, "%s\\a.exe", dir); touch(t);
        sprintf(t, "%s\\a b.exe", dir); touch(t);

        sprintf(line, "%s\\my prog.exe -flag", dir);
        MultiByteToWideChar(CP_ACP, 0, line, -1, wline, ARRAY_SIZE(wline));
        swprintf(expect, ARRAY_SIZE(expect), L"\"%hs\\my prog.exe\" -flag", dir);
        n = PathProcessCommand(wline, wout, ARRAY_SIZE(wout), PPCF_ADDARGUMENTS);
        CHECK(n == (LONG)wcslen(expect) && !wcscmp(wout, expect));
        swprintf(expect, ARRAY_SIZE(expect), L"%hs\\my prog.exe", dir);
        n = PathProcessCommand(wline, wout, ARRAY_SIZE(wout), 0);
        CHECK(n == (LONG)wcslen(expect) && !wcscmp(wout, expect));
        swprintf(expect, ARRAY_SIZE(expect), L"\"%hs\\my prog.exe\"", dir);
        n = PathProcessCommand(wline, wout, ARRAY_SIZE(wout), PPCF_ADDQUOTES);
        CHECK(n == (LONG)wcslen(expect) && !wcscmp(wout, expect));

        sprintf(line, "\"%s\\my prog.exe\" -flag x", dir);
        MultiByteToWideChar(CP_ACP, 0, line, -1, wline, ARRAY_SIZE(wline));
        swprintf(expect, ARRAY_SIZE(expect), L"%hs\\my prog.exe -flag x", dir);
        n = PathProcessCommand(wline, wout, ARRAY_SIZE(wout), PPCF_INCLUDEARGS);
        CHECK(n == (LONG)wcslen(expect) && !wcscmp(wout, expect));

        CHECK(PathProcessCommand(L"foo bar baz", wout, ARRAY_SIZE(wout), PPCF_INCLUDEARGS) == 11 && !wcscmp(wout, L"foo bar baz"));
        CHECK(PathProcessCommand(L"foo bar baz", wout, ARRAY_SIZE(wout), 0) == 3 && !wcscmp(wout, L"foo"));
        CHECK(PathProcessCommand(L"foo bar baz", wout, 4, PPCF_INCLUDEARGS) == -1);

        sprintf(line, "%s\\a b.exe", dir);
        MultiByteToWideChar(CP_ACP, 0, line, -1, wline, ARRAY_SIZE(wline));
        swprintf(expect, ARRAY_SIZE(expect), L"%hs\\a b.exe", dir);
        n = PathProcessCommand(wline, wout, ARRAY_SIZE(wout), PPCF_ADDARGUMENTS);   /* the shorter one is a program too */
        CHECK(n == (LONG)wcslen(expect) && !wcscmp(wout, expect));
        swprintf(expect, ARRAY_SIZE(expect), L"\"%hs\\a b.exe\"", dir);
        n = PathProcessCommand(wline, wout, ARRAY_SIZE(wout), PPCF_ADDARGUMENTS | PPCF_LONGESTPOSSIBLE);
        CHECK(n == (LONG)wcslen(expect) && !wcscmp(wout, expect));

        MultiByteToWideChar(CP_ACP, 0, dir, -1, wline, ARRAY_SIZE(wline));
        CHECK(PathProcessCommand(wline, wout, ARRAY_SIZE(wout), PPCF_NODIRECTORIES) == -1);
        n = PathProcessCommand(L"rundll32 /x", wout, ARRAY_SIZE(wout), PPCF_INCLUDEARGS | PPCF_FORCEQUALIFY);
        SearchPathA(NULL, "rundll32.exe", NULL, MAX_PATH, want, NULL);
        swprintf(expect, ARRAY_SIZE(expect), L"%hs /x", want);
        CHECK(n == (LONG)wcslen(expect) && !wcsicmp(wout, expect));
        CHECK(PathProcessCommand(NULL, wout, ARRAY_SIZE(wout), 0) == -1);
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
