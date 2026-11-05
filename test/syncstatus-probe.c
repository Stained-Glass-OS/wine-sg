/* File Explorer's cloud status (patch 6246): in a registered sync root the file system folder's Status
 * column is on by default and says each placeholder's state -- online only, on this device, always on
 * this device, sync pending -- with System.StorageProviderState; outside one it is off by default.
 * 6248: the real file view (ExplorerBrowser, details) shows it -- it walks all the folder's columns,
 * not only up to the first one that is off -- and hides a hidden system file (desktop.ini) unless
 * protected operating system files are shown. */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <stdio.h>
#include <commctrl.h>

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

static HWND find_list(HWND parent)
{
    HWND child = NULL, found;
    WCHAR cls[64];
    while ((child = FindWindowExW(parent, child, NULL, NULL)))
    {
        GetClassNameW(child, cls, 64);
        if (!lstrcmpW(cls, L"SysListView32")) return child;
        if ((found = find_list(child))) return found;
    }
    return NULL;
}

static void pump(int ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while ((int)(end - GetTickCount()) > 0)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(10);
    }
}

static void set_advanced(const WCHAR *name, DWORD value)
{
    RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
                    name, REG_DWORD, &value, sizeof(value));
}

/* the item's row in the list (by its name with or without the extension), -1 if none */
static int find_row(HWND list, const WCHAR *name)
{
    WCHAR base[64];
    LVFINDINFOW find = { LVFI_STRING, name };
    int i = (int)SendMessageW(list, LVM_FINDITEMW, -1, (LPARAM)&find);
    if (i >= 0) return i;
    lstrcpyW(base, name);
    if (wcsrchr(base, '.')) *wcsrchr(base, '.') = 0;
    find.psz = base;
    return (int)SendMessageW(list, LVM_FINDITEMW, -1, (LPARAM)&find);
}

struct view { IExplorerBrowser *browser; HWND top, list; };

static void view_open(struct view *v, const WCHAR *path)
{
    FOLDERSETTINGS fs = { FVM_DETAILS, 0 };
    RECT rc = { 0, 0, 900, 400 };
    LPITEMIDLIST pidl = NULL;

    memset(v, 0, sizeof(*v));
    v->top = CreateWindowExW(0, L"STATIC", L"syncstatus", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 940, 480,
                             NULL, NULL, NULL, NULL);
    CoCreateInstance(&CLSID_ExplorerBrowser, NULL, CLSCTX_INPROC_SERVER, &IID_IExplorerBrowser, (void **)&v->browser);
    if (!v->browser) return;
    IExplorerBrowser_Initialize(v->browser, v->top, &rc, &fs);
    SHParseDisplayName(path, NULL, &pidl, 0, NULL);
    IExplorerBrowser_BrowseToIDList(v->browser, pidl, 0);
    pump(1500);
    v->list = find_list(v->top);
}

static void view_close(struct view *v)
{
    if (v->browser) { IExplorerBrowser_Destroy(v->browser); IExplorerBrowser_Release(v->browser); }
    DestroyWindow(v->top);
}

/* the headers, "|"-separated; the index of one named so in *at */
static void view_headers(struct view *v, WCHAR *out, const WCHAR *want, int *at)
{
    HWND header = v->list ? (HWND)SendMessageW(v->list, LVM_GETHEADER, 0, 0) : NULL;
    int i, n = header ? (int)SendMessageW(header, HDM_GETITEMCOUNT, 0, 0) : 0;
    WCHAR text[64];
    HDITEMW item;

    out[0] = 0;
    *at = -1;
    for (i = 0; i < n; i++)
    {
        item.mask = HDI_TEXT;
        item.pszText = text;
        item.cchTextMax = 64;
        text[0] = 0;
        SendMessageW(header, HDM_GETITEMW, i, (LPARAM)&item);
        if (i) lstrcatW(out, L"|");
        lstrcatW(out, text);
        if (!lstrcmpW(text, want)) *at = i;
    }
}

static int enum_has(IShellFolder2 *folder, DWORD flags, const WCHAR *name)
{
    IEnumIDList *list = NULL;
    LPITEMIDLIST child;
    STRRET str;
    WCHAR text[MAX_PATH];
    int found = 0;

    if (FAILED(IShellFolder2_EnumObjects(folder, NULL, flags, &list)) || !list) return -1;
    while (IEnumIDList_Next(list, 1, &child, NULL) == S_OK)
    {
        if (SUCCEEDED(IShellFolder2_GetDisplayNameOf(folder, child, SHGDN_INFOLDER | SHGDN_FORPARSING, &str)) &&
            SUCCEEDED(StrRetToBufW(&str, child, text, MAX_PATH)) && !lstrcmpiW(text, name))
            found = 1;
        CoTaskMemFree(child);
    }
    IEnumIDList_Release(list);
    return found;
}

/* 6256: the Status glyph drawn in a row's Status cell: how many pixels are mostly blue, mostly green */
static void status_glyph(HWND list, int row, int col, int *image, int *blue, int *green)
{
    RECT rc, client;
    BITMAPINFO bmi = {{ sizeof(BITMAPINFOHEADER) }};
    LVITEMW item = { LVIF_IMAGE };
    DWORD *bits;
    HBITMAP bmp, old;
    HDC dc;
    int x, y;

    *image = -9; *blue = *green = 0;
    if (row < 0 || col <= 0) return;
    item.iItem = row;
    item.iSubItem = col;
    if (SendMessageW(list, LVM_GETITEMW, 0, (LPARAM)&item)) *image = item.iImage;
    rc.top = col;
    rc.left = LVIR_ICON;
    SendMessageW(list, LVM_GETSUBITEMRECT, row, (LPARAM)&rc);
    GetClientRect(list, &client);
    bmi.bmiHeader.biWidth = client.right;
    bmi.bmiHeader.biHeight = -client.bottom;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    dc = CreateCompatibleDC(NULL);
    if (!(bmp = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, (void **)&bits, NULL, 0))) { DeleteDC(dc); return; }
    old = SelectObject(dc, bmp);
    SendMessageW(list, WM_PRINTCLIENT, (WPARAM)dc, PRF_CLIENT | PRF_ERASEBKGND);
    GdiFlush();
    for (y = max(rc.top, 0); y < min(rc.bottom, client.bottom); y++)
        for (x = max(rc.left, 0); x < min(rc.left + 20, client.right); x++)
        {
            DWORD p = bits[y * client.right + x];
            int r = (p >> 16) & 0xff, g = (p >> 8) & 0xff, b = p & 0xff;
            if (b > 120 && b > r + 60 && b > g + 20) (*blue)++;
            if (g > 90 && g > r + 40 && g > b + 40) (*green)++;
        }
    SelectObject(dc, old);
    DeleteObject(bmp);
    DeleteDC(dc);
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
    /* 6257: the cloud attributes are the file's own, as on Windows */
    {
        DWORD pinned = GetFileAttributesW(L"C:\\sroot\\pinned.txt"), online = GetFileAttributesW(L"C:\\sroot\\online.bin");
        DWORD local = GetFileAttributesW(L"C:\\sroot\\local.txt"), unpinned;
        HANDLE f;
        CHECK(pinned != INVALID_FILE_ATTRIBUTES && (pinned & 0x00080000), "a pinned file has FILE_ATTRIBUTE_PINNED: %#lx", pinned);
        CHECK(online != INVALID_FILE_ATTRIBUTES && (online & 0x00400000), "an online-only file has RECALL_ON_DATA_ACCESS: %#lx", online);
        CHECK(local != INVALID_FILE_ATTRIBUTES && !(local & 0x00480000), "a local one has neither: %#lx", local);
        f = CreateFileW(L"C:\\sroot\\local.txt", FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE,
                        NULL, OPEN_EXISTING, 0, NULL);
        CHECK(pPin(f, 2, 0, NULL) == S_OK, "unpinned local.txt");
        CloseHandle(f);
        unpinned = GetFileAttributesW(L"C:\\sroot\\local.txt");
        CHECK(unpinned != INVALID_FILE_ATTRIBUTES && (unpinned & 0x00100000) && !(unpinned & 0x00080000),
              "an unpinned file has FILE_ATTRIBUTE_UNPINNED: %#lx", unpinned);
    }
    status_of(root, L"plain.txt", text, &state);
    CHECK(!text[0] && state == 0, "a file that is no placeholder: '%ls' %lu", text, state);

    /* desktop.ini as OneDrive leaves it: hidden and system */
    CloseHandle(make_file(L"C:\\sroot\\desktop.ini", "[.ShellClassInfo]\r\n"));
    SetFileAttributesW(L"C:\\sroot\\desktop.ini", FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
    CloseHandle(make_file(L"C:\\sroot\\hidden.txt", "h"));
    SetFileAttributesW(L"C:\\sroot\\hidden.txt", FILE_ATTRIBUTE_HIDDEN);
    CHECK((GetFileAttributesW(L"C:\\sroot\\desktop.ini") & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) ==
          (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM),
          "desktop.ini keeps hidden+system: %#lx", GetFileAttributesW(L"C:\\sroot\\desktop.ini"));
    CHECK(enum_has(root, SHCONTF_NONFOLDERS | SHCONTF_INCLUDEHIDDEN, L"hidden.txt") == 1, "a hidden file is enumerated with SHCONTF_INCLUDEHIDDEN");
    CHECK(enum_has(root, SHCONTF_NONFOLDERS | SHCONTF_INCLUDEHIDDEN, L"desktop.ini") == 0, "a hidden system file is not, without SHCONTF_INCLUDESUPERHIDDEN");
    CHECK(enum_has(root, SHCONTF_NONFOLDERS | SHCONTF_INCLUDEHIDDEN | SHCONTF_INCLUDESUPERHIDDEN, L"desktop.ini") == 1,
          "it is with SHCONTF_INCLUDESUPERHIDDEN");

    /* the real file view, details, with hidden items shown */
    set_advanced(L"Hidden", 1);
    set_advanced(L"ShowSuperHidden", 0);
    {
        struct view v;
        WCHAR headers[256], cell[128];
        int status = -1, row;
        LVITEMW item;

        view_open(&v, L"C:\\sroot");
        CHECK(v.list != NULL, "the view of the sync root has a list");
        view_headers(&v, headers, L"Status", &status);
        printf("headers: %ls\n", headers);
        CHECK(status > 0, "the view shows the Status column: %ls", headers);
        CHECK(!wcsstr(headers, L"Attributes"), "and not Attributes, which is off by default: %ls", headers);
        row = v.list ? find_row(v.list, L"local.txt") : -1;
        cell[0] = 0;
        if (row >= 0 && status > 0)
        {
            item.iSubItem = status;
            item.pszText = cell;
            item.cchTextMax = 128;
            SendMessageW(v.list, LVM_GETITEMTEXTW, row, (LPARAM)&item);
        }
        CHECK(!lstrcmpW(cell, L"Available on this device"), "local.txt's Status cell: row %d '%ls'", row, cell);
        row = v.list ? find_row(v.list, L"online.bin") : -1;
        cell[0] = 0;
        if (row >= 0 && status > 0)
        {
            item.iSubItem = status;
            item.pszText = cell;
            item.cchTextMax = 128;
            SendMessageW(v.list, LVM_GETITEMTEXTW, row, (LPARAM)&item);
        }
        CHECK(!lstrcmpW(cell, L"Available when online"), "online.bin's Status cell: row %d '%ls'", row, cell);
        {
            int image, blue, green, image2, blue2, green2, image3, blue3, green3;
            status_glyph(v.list, find_row(v.list, L"online.bin"), status, &image, &blue, &green);
            status_glyph(v.list, find_row(v.list, L"local.txt"), status, &image2, &blue2, &green2);
            status_glyph(v.list, find_row(v.list, L"pinned.txt"), status, &image3, &blue3, &green3);
            printf("glyphs: online %d (blue %d green %d), local %d (%d %d), pinned %d (%d %d)\n",
                   image, blue, green, image2, blue2, green2, image3, blue3, green3);
            CHECK(image >= 0 && image2 >= 0 && image3 >= 0 && image != image2 && image2 != image3,
                  "each state has its own Status glyph: %d %d %d", image, image2, image3);
            CHECK(blue > 8 && green == 0, "online only: a blue cloud is drawn (%d blue, %d green)", blue, green);
            CHECK(green2 > 8 && green3 > green2, "on this device: a green check; always: a filled one (%d, %d)", green2, green3);
        }
        CHECK(v.list && find_row(v.list, L"hidden.txt") >= 0, "a hidden file is listed (hidden items shown)");
        CHECK(v.list && find_row(v.list, L"desktop.ini") < 0, "desktop.ini is not (protected files hidden)");
        view_close(&v);

        view_open(&v, L"C:\\windows");
        view_headers(&v, headers, L"Status", &status);
        CHECK(v.list && status < 0 && wcsstr(headers, L"Size"), "outside a sync root no Status column: %ls", headers);
        view_close(&v);

        set_advanced(L"ShowSuperHidden", 1);
        view_open(&v, L"C:\\sroot");
        CHECK(v.list && find_row(v.list, L"desktop.ini") >= 0, "with protected files shown desktop.ini is listed");
        view_close(&v);
        set_advanced(L"ShowSuperHidden", 0);
    }

    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
