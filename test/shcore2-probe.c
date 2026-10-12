/* shcore batch (patches/sg/2055), run by test/shcore2-gate.sh: IStream_Copy,
 * SHCreateThreadWithHandle, GetDpiForShellUIComponent, the HKCU-then-HKLM
 * registry readers, SHIsEmptyStream and MapWin32ErrorToSTG, looked up with
 * GetProcAddress in shcore.dll.
 *
 *   shcore2-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <shlwapi.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECK(expr) check((expr), #expr)

static HMODULE sc;
static void *fn(const char *name, int ordinal)
{
    void *p = (void *)GetProcAddress(sc, name);
    if (!p && ordinal) p = (void *)GetProcAddress(sc, MAKEINTRESOURCEA(ordinal));
    if (!p) { printf("FAIL  missing export %s\n", name); failures++; }
    return p;
}

static volatile LONG ran;
static DWORD WINAPI thread_proc(void *arg) { InterlockedExchange(&ran, (LONG)(LONG_PTR)arg); return 7; }
static IStream *mem_stream(const char *text)
{
    IStream *s = NULL;
    ULONG n;
    LARGE_INTEGER zero = { 0 };
    CreateStreamOnHGlobal(NULL, TRUE, &s);
    if (text) s->lpVtbl->Write(s, text, strlen(text), &n);
    s->lpVtbl->Seek(s, zero, STREAM_SEEK_SET, NULL);
    return s;
}

static void set_str(HKEY root, const WCHAR *name, const WCHAR *value)
{
    HKEY key;
    RegCreateKeyExW(root, L"Software\\SgShcoreTest", 0, NULL, 0, KEY_WRITE, NULL, &key, NULL);
    RegSetValueExW(key, name, 0, REG_SZ, (const BYTE *)value, (wcslen(value) + 1) * sizeof(WCHAR));
    RegCloseKey(key);
}

static void set_dw(HKEY root, const WCHAR *name, DWORD value)
{
    HKEY key;
    RegCreateKeyExW(root, L"Software\\SgShcoreTest", 0, NULL, 0, KEY_WRITE, NULL, &key, NULL);
    RegSetValueExW(key, name, 0, REG_DWORD, (const BYTE *)&value, sizeof(value));
    RegCloseKey(key);
}

int main(void)
{
    HRESULT (WINAPI *IStream_Copy_)(IStream *, IStream *, DWORD);
    BOOL (WINAPI *CreateWithHandle)(LPTHREAD_START_ROUTINE, void *, DWORD, LPTHREAD_START_ROUTINE, HANDLE *);
    UINT (WINAPI *DpiFor)(INT);
    LSTATUS (WINAPI *GetFromBoth)(const WCHAR *, const WCHAR *, DWORD, DWORD *, void *, DWORD *);
    BOOL (WINAPI *GetBool)(const WCHAR *, const WCHAR *, BOOL);
    BOOL (WINAPI *IsEmpty)(IStream *);
    HRESULT (WINAPI *MapErr)(DWORD);
    IStream *a, *b;
    char buf[32];
    ULONG n;
    HANDLE thread;
    DWORD exitcode, type, size;
    WCHAR text[64];
    HDC hdc;

    CoInitialize(NULL);
    sc = LoadLibraryA("shcore.dll");
    IStream_Copy_ = fn("IStream_Copy", 0);
    CreateWithHandle = fn("SHCreateThreadWithHandle", 0);
    DpiFor = fn("GetDpiForShellUIComponent", 0);
    GetFromBoth = fn("SHRegGetValueFromHKCUHKLM", 122);
    GetBool = fn("SHRegGetBoolValueFromHKCUHKLM", 123);
    IsEmpty = fn("SHIsEmptyStream", 102);
    MapErr = fn("MapWin32ErrorToSTG", 103);

    /* streams */
    a = mem_stream("0123456789");
    b = mem_stream(NULL);
    CHECK(IStream_Copy_(a, b, 4) == S_OK);
    {
        LARGE_INTEGER zero = { 0 };
        b->lpVtbl->Seek(b, zero, STREAM_SEEK_SET, NULL);
        memset(buf, 0, sizeof(buf));
        b->lpVtbl->Read(b, buf, sizeof(buf) - 1, &n);
        CHECK(n == 4 && !strcmp(buf, "0123"));
        memset(buf, 0, sizeof(buf));
        a->lpVtbl->Read(a, buf, 1, &n);
        CHECK(buf[0] == '4');                                  /* the source went on from where it was */
        b->lpVtbl->Seek(b, zero, STREAM_SEEK_END, NULL);
    }
    CHECK(IStream_Copy_(a, b, 100) == S_OK);
    {
        LARGE_INTEGER zero = { 0 };
        b->lpVtbl->Seek(b, zero, STREAM_SEEK_SET, NULL);
        memset(buf, 0, sizeof(buf));
        b->lpVtbl->Read(b, buf, sizeof(buf) - 1, &n);
        CHECK(!strcmp(buf, "012356789"));
    }
    CHECK(IStream_Copy_(NULL, b, 1) == E_INVALIDARG);
    CHECK(IStream_Copy_(a, NULL, 1) == E_INVALIDARG);
    a->lpVtbl->Release(a);
    b->lpVtbl->Release(b);

    a = mem_stream(NULL);
    CHECK(IsEmpty(a));
    b = mem_stream("x");
    CHECK(!IsEmpty(b));
    a->lpVtbl->Release(a);
    b->lpVtbl->Release(b);

    /* a thread with its handle */
    thread = (HANDLE)1;
    ran = 0;
    CHECK(CreateWithHandle(thread_proc, (void *)5, 0, NULL, &thread) && thread && thread != (HANDLE)1);
    CHECK(WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0 && ran == 5);
    CHECK(GetExitCodeThread(thread, &exitcode) && exitcode == 7);
    CloseHandle(thread);
    ran = 0;
    CHECK(CreateWithHandle(thread_proc, (void *)6, 0, NULL, NULL));
    {
        int i;
        for (i = 0; i < 100 && ran != 6; i++) Sleep(20);
        CHECK(ran == 6);
    }

    /* scale */
    hdc = GetDC(NULL);
    CHECK(DpiFor(0) == (UINT)GetDeviceCaps(hdc, LOGPIXELSX) && DpiFor(0) >= 96);
    ReleaseDC(NULL, hdc);

    /* the registry: the user's value before the machine's */
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\SgShcoreTest");
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, L"Software\\SgShcoreTest");
    set_str(HKEY_CURRENT_USER, L"Both", L"user");
    set_str(HKEY_LOCAL_MACHINE, L"Both", L"machine");
    set_str(HKEY_LOCAL_MACHINE, L"MachineOnly", L"machine only");
    set_dw(HKEY_CURRENT_USER, L"Off", 0);
    set_dw(HKEY_LOCAL_MACHINE, L"Off", 1);
    set_dw(HKEY_LOCAL_MACHINE, L"On", 5);
    set_str(HKEY_LOCAL_MACHINE, L"Yes", L"yes");
    set_str(HKEY_LOCAL_MACHINE, L"No", L"No");
    set_str(HKEY_LOCAL_MACHINE, L"Junk", L"maybe");

    size = sizeof(text); type = 0;
    CHECK(GetFromBoth(L"Software\\SgShcoreTest", L"Both", RRF_RT_REG_SZ, &type, text, &size) == 0 && !wcscmp(text, L"user") && type == REG_SZ);
    size = sizeof(text);
    CHECK(GetFromBoth(L"Software\\SgShcoreTest", L"MachineOnly", RRF_RT_REG_SZ, &type, text, &size) == 0 && !wcscmp(text, L"machine only"));
    size = sizeof(text);
    CHECK(GetFromBoth(L"Software\\SgShcoreTest", L"Nothing", RRF_RT_REG_SZ, &type, text, &size) == ERROR_FILE_NOT_FOUND);
    size = sizeof(text);
    CHECK(GetFromBoth(L"Software\\SgNoSuchKey", L"Both", RRF_RT_REG_SZ, &type, text, &size) != 0);
    CHECK(GetFromBoth(NULL, L"Both", RRF_RT_REG_SZ, NULL, NULL, NULL) == ERROR_INVALID_PARAMETER);

    CHECK(GetBool(L"Software\\SgShcoreTest", L"Off", TRUE) == FALSE);           /* the user's 0 wins */
    CHECK(GetBool(L"Software\\SgShcoreTest", L"On", FALSE) == TRUE);
    CHECK(GetBool(L"Software\\SgShcoreTest", L"Yes", FALSE) == TRUE);
    CHECK(GetBool(L"Software\\SgShcoreTest", L"No", TRUE) == FALSE);
    CHECK(GetBool(L"Software\\SgShcoreTest", L"Junk", TRUE) == TRUE);
    CHECK(GetBool(L"Software\\SgShcoreTest", L"Junk", FALSE) == FALSE);
    CHECK(GetBool(L"Software\\SgShcoreTest", L"Missing", TRUE) == TRUE);
    CHECK(GetBool(L"Software\\SgShcoreTest", L"Missing", FALSE) == FALSE);
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\SgShcoreTest");
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, L"Software\\SgShcoreTest");

    /* storage errors */
    CHECK(MapErr(0) == S_OK);
    CHECK(MapErr(ERROR_FILE_NOT_FOUND) == (HRESULT)0x80030002);
    CHECK(MapErr(ERROR_ACCESS_DENIED) == (HRESULT)0x80030005);
    CHECK(MapErr(ERROR_SHARING_VIOLATION) == (HRESULT)0x80030020);
    CHECK(MapErr(ERROR_INVALID_PARAMETER) == (HRESULT)0x80030057);
    CHECK(MapErr(ERROR_DISK_FULL) == (HRESULT)0x80030070);
    CHECK(MapErr(ERROR_PRIVILEGE_NOT_HELD) == HRESULT_FROM_WIN32(ERROR_PRIVILEGE_NOT_HELD));

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
