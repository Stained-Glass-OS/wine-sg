/* File Explorer's cloud status (patch 6246): in a registered sync root the file system folder's Status
 * column is on by default and says each placeholder's state -- online only, on this device, always on
 * this device, sync pending -- with System.StorageProviderState; outside one it is off by default. */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef struct { DWORD StructSize; const WCHAR *ProviderName, *ProviderVersion; const void *SyncRootIdentity;
    DWORD SyncRootIdentityLength; const void *FileIdentity; DWORD FileIdentityLength; GUID ProviderId; } REG;
typedef struct { DWORD StructSize; USHORT HP, HM, PP, PM; DWORD InSync, HardLink, Mgmt; } POL;
typedef struct { const WCHAR *Name; FILE_BASIC_INFO Basic; LARGE_INTEGER Size; const void *Id; DWORD IdLen; DWORD Flags;
    HRESULT Result; LONGLONG Usn; } PHC;

static const GUID FMTID_StorageProvider = { 0xe77e90df, 0x6271, 0x4f5b, { 0x83, 0x4f, 0x2d, 0xd1, 0xf2, 0x45, 0xdd, 0xa4 } };
#define STATUS_COLUMN 5

static HRESULT (WINAPI *pRegister)(const WCHAR *, const REG *, const POL *, DWORD);
static HRESULT (WINAPI *pCreate)(const WCHAR *, PHC *, DWORD, DWORD, LONG *);
static HRESULT (WINAPI *pConvert)(HANDLE, const void *, DWORD, DWORD, void *, void *);
static HRESULT (WINAPI *pPin)(HANDLE, DWORD, DWORD, void *);

static IShellFolder2 *folder_at(const WCHAR *path)
{
    IShellFolder *desktop, *f = NULL;
    IShellFolder2 *f2 = NULL;
    LPITEMIDLIST pidl;
    SHGetDesktopFolder(&desktop);
    if (FAILED(SHParseDisplayName(path, NULL, &pidl, 0, NULL))) return NULL;
    IShellFolder_BindToObject(desktop, pidl, NULL, &IID_IShellFolder, (void **)&f);
    if (f) IShellFolder_QueryInterface(f, &IID_IShellFolder2, (void **)&f2);
    return f2;
}

static void status_of(IShellFolder2 *folder, const WCHAR *name, WCHAR *text, DWORD *state)
{
    LPITEMIDLIST child = NULL;
    SHELLDETAILS sd;
    SHCOLUMNID scid = { FMTID_StorageProvider, 3 };
    VARIANT v;

    text[0] = 0;
    *state = 0xdead;
    IShellFolder2_ParseDisplayName(folder, NULL, NULL, (WCHAR *)name, NULL, &child, NULL);
    if (!child) { lstrcpyW(text, L"(no item)"); return; }
    if (SUCCEEDED(IShellFolder2_GetDetailsOf(folder, child, STATUS_COLUMN, &sd)))
        StrRetToBufW(&sd.str, child, text, 128);
    VariantInit(&v);
    if (SUCCEEDED(IShellFolder2_GetDetailsEx(folder, child, &scid, &v)))
        *state = V_VT(&v) == VT_UI4 ? V_UI4(&v) : V_VT(&v) == VT_EMPTY ? 0 : 0xbad;
}

static HANDLE make_file(const WCHAR *path, const char *data)
{
    DWORD n;
    HANDLE h = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(h, data, lstrlenA(data), &n, NULL);
    return h;
}

int main(void)
{
    HMODULE m = LoadLibraryA("cldapi.dll");
    REG reg = {sizeof(reg), L"SGStatusTest", L"1.0", "SRI", 3, "FI", 2, {1, 2, 3, {4, 5, 6, 7, 8, 9, 10, 12}}};
    POL pol = {sizeof(pol)};
    PHC ph[2];
    LONG processed = 0;
    IShellFolder2 *root, *outside;
    SHELLDETAILS sd;
    WCHAR text[128];
    DWORD flags = 0, state;
    HANDLE h;
    HRESULT hr;

    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);
    *(FARPROC *)&pRegister = GetProcAddress(m, "CfRegisterSyncRoot");
    *(FARPROC *)&pCreate = GetProcAddress(m, "CfCreatePlaceholders");
    *(FARPROC *)&pConvert = GetProcAddress(m, "CfConvertToPlaceholder");
    *(FARPROC *)&pPin = GetProcAddress(m, "CfSetPinState");
    if (!pRegister || !pCreate || !pConvert || !pPin) { printf("no cldapi\nRESULT: FAIL\n"); return 1; }

    CreateDirectoryW(L"C:\\sroot", NULL);
    hr = pRegister(L"C:\\sroot", &reg, &pol, 0);
    CHECK(hr == S_OK, "sync root registered: %#lx", hr);

    memset(ph, 0, sizeof(ph));
    ph[0].Name = L"online.bin"; ph[0].Size.QuadPart = 1000; ph[0].Id = "ID-O"; ph[0].IdLen = 4; ph[0].Flags = 2;   /* in sync */
    ph[1].Name = L"pending.bin"; ph[1].Size.QuadPart = 10; ph[1].Id = "ID-P"; ph[1].IdLen = 4; ph[1].Flags = 0;
    hr = pCreate(L"C:\\sroot", ph, 2, 0, &processed);
    CHECK(hr == S_OK && processed == 2, "placeholders made: %#lx %ld", hr, processed);
    h = make_file(L"C:\\sroot\\local.txt", "here");
    CHECK(pConvert(h, "ID-L", 4, 1, NULL, NULL) == S_OK, "converted local.txt");
    CloseHandle(h);
    h = make_file(L"C:\\sroot\\pinned.txt", "kept");
    CHECK(pConvert(h, "ID-K", 4, 1, NULL, NULL) == S_OK, "converted pinned.txt");
    CHECK(pPin(h, 1, 0, NULL) == S_OK, "pinned");
    CloseHandle(h);
    CloseHandle(make_file(L"C:\\sroot\\plain.txt", "plain"));

    root = folder_at(L"C:\\sroot");
    outside = folder_at(L"C:\\windows");
    CHECK(root && outside, "folders bound");
    if (!root || !outside) { printf("RESULT: FAIL\n"); return 1; }

    memset(&sd, 0, sizeof(sd));
    hr = IShellFolder2_GetDetailsOf(root, NULL, STATUS_COLUMN, &sd);
    text[0] = 0;
    if (SUCCEEDED(hr)) StrRetToBufW(&sd.str, NULL, text, 128);
    CHECK(hr == S_OK && !lstrcmpW(text, L"Status"), "the column is Status: %#lx %ls", hr, text);
    IShellFolder2_GetDefaultColumnState(root, STATUS_COLUMN, &flags);
    CHECK(flags & SHCOLSTATE_ONBYDEFAULT, "shown in a sync root: %#lx", flags);
    flags = 0;
    IShellFolder2_GetDefaultColumnState(outside, STATUS_COLUMN, &flags);
    CHECK(!(flags & SHCOLSTATE_ONBYDEFAULT), "not shown elsewhere: %#lx", flags);

    status_of(root, L"online.bin", text, &state);
    CHECK(!lstrcmpW(text, L"Available when online") && state == 1, "online.bin: %ls %lu", text, state);
    status_of(root, L"local.txt", text, &state);
    CHECK(!lstrcmpW(text, L"Available on this device") && state == 2, "local.txt: %ls %lu", text, state);
    status_of(root, L"pinned.txt", text, &state);
    CHECK(!lstrcmpW(text, L"Always available on this device") && state == 3, "pinned.txt: %ls %lu", text, state);
    status_of(root, L"pending.bin", text, &state);
    CHECK(!lstrcmpW(text, L"Sync pending") && state == 4, "pending.bin: %ls %lu", text, state);
    status_of(root, L"plain.txt", text, &state);
    CHECK(!text[0] && state == 0, "a file that is no placeholder: '%ls' %lu", text, state);

    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
