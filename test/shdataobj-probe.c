/* shell32 batch (patches/sg/2049), run by test/shdataobj-gate.sh. Families:
 * the formats of the shell's data object (files and items that are not
 * files), its format enumerator, SHGetItemFromDataObject on such objects,
 * SHGetSetFolderCustomSettings reading and writing desktop.ini, and
 * SHLimitInputEdit on an edit control.
 *
 *   shdataobj-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECK(expr) check((expr), #expr)

static int has_format(IDataObject *obj, UINT cf)
{
    FORMATETC fmt = { cf, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    return IDataObject_QueryGetData(obj, &fmt) == S_OK;
}

static int get_format(IDataObject *obj, UINT cf, HRESULT *hr)
{
    FORMATETC fmt = { cf, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM medium;
    *hr = IDataObject_GetData(obj, &fmt, &medium);
    if (*hr == S_OK) ReleaseStgMedium(&medium);
    return *hr == S_OK;
}

static ULONG count_formats(IDataObject *obj)
{
    IEnumFORMATETC *e;
    FORMATETC f;
    ULONG n = 0;
    if (FAILED(IDataObject_EnumFormatEtc(obj, DATADIR_GET, &e))) return 99;
    while (IEnumFORMATETC_Next(e, 1, &f, NULL) == S_OK) n++;
    IEnumFORMATETC_Release(e);
    return n;
}

int main(void)
{
    UINT cf_idl, cf_drop_effect, cf_name_a, cf_name_w;
    IShellFolder *desktop;
    IDataObject *obj;
    LPITEMIDLIST drives, personal, filepid;
    LPCITEMIDLIST pidls[2];
    HRESULT hr;
    WCHAR tmp[MAX_PATH], file[MAX_PATH], dir[MAX_PATH];
    HWND edit;

    HRESULT (WINAPI *SHGetItemFromDataObject_)(IDataObject *, int, REFIID, void **);
    HRESULT (WINAPI *SHGetSetFolderCustomSettings_)(SHFOLDERCUSTOMSETTINGS *, const WCHAR *, DWORD);
    HRESULT (WINAPI *SHLimitInputEdit_)(HWND, IShellFolder *);
    HMODULE sh;

    CoInitialize(NULL);
    sh = LoadLibraryA("shell32.dll");
    SHGetItemFromDataObject_ = (void *)GetProcAddress(sh, "SHGetItemFromDataObject");
    SHGetSetFolderCustomSettings_ = (void *)GetProcAddress(sh, MAKEINTRESOURCEA(709));
    SHLimitInputEdit_ = (void *)GetProcAddress(sh, MAKEINTRESOURCEA(747));
    cf_idl = RegisterClipboardFormatW(L"Shell IDList Array");
    cf_drop_effect = RegisterClipboardFormatW(L"Preferred DropEffect");
    cf_name_a = RegisterClipboardFormatA("FileName");
    cf_name_w = RegisterClipboardFormatW(L"FileNameW");

    SHGetDesktopFolder(&desktop);
    SHGetSpecialFolderLocation(NULL, CSIDL_DRIVES, &drives);
    SHGetSpecialFolderLocation(NULL, CSIDL_PERSONAL, &personal);

    /* an item that is not a file: the list of items, and linking */
    pidls[0] = drives;
    hr = IShellFolder_GetUIObjectOf(desktop, NULL, 1, pidls, &IID_IDataObject, NULL, (void **)&obj);
    if (hr != S_OK) { check(0, "GetUIObjectOf drives"); printf("RESULT: FAIL\n"); return 1; }
    pidls[0] = ILFindLastID(drives);
    IDataObject_Release(obj);
    hr = IShellFolder_GetUIObjectOf(desktop, NULL, 1, pidls, &IID_IDataObject, NULL, (void **)&obj);
    CHECK(hr == S_OK);
    CHECK(has_format(obj, cf_idl));
    CHECK(has_format(obj, cf_drop_effect));
    CHECK(!has_format(obj, CF_HDROP));
    CHECK(!has_format(obj, cf_name_a));
    CHECK(!has_format(obj, cf_name_w));
    {
        FORMATETC fmt = { cf_drop_effect, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        STGMEDIUM medium;
        CHECK(IDataObject_GetData(obj, &fmt, &medium) == S_OK && medium.tymed == TYMED_HGLOBAL);
        if (medium.tymed == TYMED_HGLOBAL)
        {
            DWORD *effect = GlobalLock(medium.hGlobal);
            CHECK(effect && *effect == DROPEFFECT_LINK);
            GlobalUnlock(medium.hGlobal);
            ReleaseStgMedium(&medium);
        }
        fmt.cfFormat = CF_HDROP;
        CHECK(IDataObject_GetData(obj, &fmt, &medium) == DV_E_FORMATETC);
    }
    CHECK(count_formats(obj) == 2);

    /* the enumerator */
    {
        IEnumFORMATETC *e, *clone;
        FORMATETC f[4];
        ULONG n = 99;

        IDataObject_EnumFormatEtc(obj, DATADIR_GET, &e);
        CHECK(IEnumFORMATETC_Next(e, 4, f, &n) == S_FALSE && n == 2);
        IEnumFORMATETC_Reset(e);
        CHECK(IEnumFORMATETC_Skip(e, 2) == S_OK);                      /* all of them is not too many */
        CHECK(IEnumFORMATETC_Skip(e, 1) == S_FALSE);
        CHECK(IEnumFORMATETC_Next(e, 1, f, NULL) == S_FALSE);
        IEnumFORMATETC_Reset(e);
        CHECK(IEnumFORMATETC_Skip(e, 3) == S_FALSE);                   /* past the end leaves it at the end */
        CHECK(IEnumFORMATETC_Skip(e, 1) == S_FALSE);
        IEnumFORMATETC_Reset(e);
        CHECK(IEnumFORMATETC_Clone(e, &clone) == S_OK && clone != e);
        n = 0;
        CHECK(IEnumFORMATETC_Next(clone, 1, f, &n) == S_OK && n == 1);
        IEnumFORMATETC_Release(clone);
        IEnumFORMATETC_Release(e);
    }
    IDataObject_Release(obj);

    /* two such items are not one item */
    pidls[0] = ILFindLastID(drives);
    pidls[1] = ILFindLastID(personal);
    hr = IShellFolder_GetUIObjectOf(desktop, NULL, 2, pidls, &IID_IDataObject, NULL, (void **)&obj);
    if (hr == S_OK && SHGetItemFromDataObject_)
    {
        IShellItem *item = NULL;
        CHECK(SHGetItemFromDataObject_(obj, 8 /* DOGIF_ONLY_IF_ONE */, &IID_IShellItem, (void **)&item) == E_FAIL);
        CHECK(item == NULL);
        CHECK(SHGetItemFromDataObject_(obj, 0, &IID_IShellItem, (void **)&item) == S_OK);
        if (item) IShellItem_Release(item);
    }
    if (hr == S_OK) IDataObject_Release(obj);

    /* a file: its path as well */
    GetTempPathW(MAX_PATH, tmp);
    swprintf(dir, MAX_PATH, L"%lsdataobj-probe", tmp);
    CreateDirectoryW(dir, NULL);
    swprintf(file, MAX_PATH, L"%ls\\one.txt", dir);
    {
        HANDLE h = CreateFileW(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        CloseHandle(h);
    }
    filepid = ILCreateFromPathW(file);
    if (filepid)
    {
        IShellFolder *parent;
        LPCITEMIDLIST child;

        SHBindToParent(filepid, &IID_IShellFolder, (void **)&parent, &child);
        pidls[0] = child;
        hr = IShellFolder_GetUIObjectOf(parent, NULL, 1, pidls, &IID_IDataObject, NULL, (void **)&obj);
        CHECK(hr == S_OK);
        if (hr == S_OK)
        {
            HRESULT h2;
            CHECK(has_format(obj, cf_idl) && has_format(obj, CF_HDROP) && has_format(obj, cf_name_a) && has_format(obj, cf_name_w));
            CHECK(!has_format(obj, cf_drop_effect));
            CHECK(get_format(obj, CF_HDROP, &h2));
            CHECK(count_formats(obj) == 4);
            IDataObject_Release(obj);
        }
        IShellFolder_Release(parent);
        ILFree(filepid);
    }
    ILFree(drives);
    ILFree(personal);

    /* desktop.ini settings */
    if (SHGetSetFolderCustomSettings_)
    {
        SHFOLDERCUSTOMSETTINGS fcs;
        WCHAR icon[MAX_PATH], tip[100], logo[100];
        CLSID clsid = { 0x11223344, 0x5566, 0x7788, { 1, 2, 3, 4, 5, 6, 7, 8 } }, back;
        WCHAR ini[MAX_PATH];

        swprintf(ini, MAX_PATH, L"%ls\\desktop.ini", dir);
        memset(&fcs, 0, sizeof(fcs));
        fcs.dwSize = sizeof(fcs);
        fcs.dwMask = FCSM_ICONFILE;
        fcs.pszIconFile = icon;
        fcs.cchIconFile = MAX_PATH;
        CHECK(SHGetSetFolderCustomSettings_(&fcs, dir, FCS_READ) == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        memset(&fcs, 0, sizeof(fcs));
        fcs.dwSize = sizeof(fcs);
        fcs.dwMask = FCSM_ICONFILE | FCSM_INFOTIP | FCSM_LOGO | FCSM_CLSID;
        fcs.pszIconFile = (WCHAR *)L"C:\\foo\\x.ico";
        fcs.iIconIndex = 3;
        fcs.pszInfoTip = (WCHAR *)L"a tip";
        fcs.pszLogo = (WCHAR *)L"logo.bmp";
        fcs.pclsid = &clsid;
        CHECK(SHGetSetFolderCustomSettings_(&fcs, dir, FCS_FORCEWRITE) == S_OK);

        memset(&fcs, 0, sizeof(fcs));
        fcs.dwSize = sizeof(fcs);
        fcs.dwMask = FCSM_ICONFILE | FCSM_INFOTIP | FCSM_LOGO | FCSM_CLSID;
        fcs.pszIconFile = icon; fcs.cchIconFile = MAX_PATH; fcs.iIconIndex = 77;
        fcs.pszInfoTip = tip; fcs.cchInfoTip = 100;
        fcs.pszLogo = logo; fcs.cchLogo = 100;
        fcs.pclsid = &back;
        memset(&back, 0, sizeof(back));
        CHECK(SHGetSetFolderCustomSettings_(&fcs, dir, FCS_READ) == S_OK);
        CHECK(!wcscmp(icon, L"C:\\foo\\x.ico") && fcs.iIconIndex == 3);
        CHECK(!wcscmp(tip, L"a tip") && !wcscmp(logo, L"logo.bmp") && IsEqualGUID(&back, &clsid));
        CHECK(fcs.dwMask == (FCSM_ICONFILE | FCSM_INFOTIP | FCSM_LOGO | FCSM_CLSID));

        /* the older form, and entries the file lacks */
        DeleteFileW(ini);
        WritePrivateProfileStringW(L".ShellClassInfo", L"IconFile", L"old.ico", ini);
        WritePrivateProfileStringW(L".ShellClassInfo", L"IconIndex", L"2", ini);
        fcs.iIconIndex = 77;
        CHECK(SHGetSetFolderCustomSettings_(&fcs, dir, FCS_READ) == S_OK);
        CHECK(!wcscmp(icon, L"old.ico") && fcs.iIconIndex == 2);
        CHECK(fcs.dwMask == FCSM_ICONFILE && !tip[0]);

        fcs.dwMask = FCSM_INFOTIP;
        fcs.pszInfoTip = NULL;
        CHECK(SHGetSetFolderCustomSettings_(&fcs, dir, FCS_READ) == E_INVALIDARG);
        CHECK(FAILED(SHGetSetFolderCustomSettings_(NULL, dir, FCS_READ)));
        CHECK(FAILED(SHGetSetFolderCustomSettings_(&fcs, NULL, FCS_READ)));
        DeleteFileW(ini);
    }

    /* an edit control that refuses the characters a file name cannot have */
    if (SHLimitInputEdit_)
    {
        WCHAR text[64];
        HGLOBAL mem;
        WCHAR *p;
        const WCHAR *paste = L"a:b*c\\d?e";

        CHECK(SHLimitInputEdit_(NULL, desktop) == E_FAIL);
        edit = CreateWindowW(L"EDIT", NULL, WS_VISIBLE | WS_POPUP, 0, 0, 200, 30, NULL, NULL, NULL, NULL);
        CHECK(SHLimitInputEdit_(edit, desktop) == S_OK);
        CHECK(SHLimitInputEdit_(edit, desktop) == S_OK);
        SendMessageW(edit, WM_CHAR, 'a', 0);
        SendMessageW(edit, WM_CHAR, ':', 0);
        SendMessageW(edit, WM_CHAR, '*', 0);
        SendMessageW(edit, WM_CHAR, 'b', 0);
        SendMessageW(edit, WM_CHAR, '"', 0);
        SendMessageW(edit, WM_CHAR, '-', 0);
        GetWindowTextW(edit, text, ARRAY_SIZE(text));
        CHECK(!wcscmp(text, L"ab-"));

        SetWindowTextW(edit, L"");
        mem = GlobalAlloc(GMEM_MOVEABLE, (wcslen(paste) + 1) * sizeof(WCHAR));
        p = GlobalLock(mem);
        wcscpy(p, paste);
        GlobalUnlock(mem);
        if (OpenClipboard(edit))
        {
            EmptyClipboard();
            SetClipboardData(CF_UNICODETEXT, mem);
            CloseClipboard();
            SendMessageW(edit, WM_PASTE, 0, 0);
            GetWindowTextW(edit, text, ARRAY_SIZE(text));
            CHECK(!wcscmp(text, L"abcde"));
        }
        else check(0, "clipboard");
        DestroyWindow(edit);

        edit = CreateWindowW(L"EDIT", NULL, WS_POPUP, 0, 0, 200, 30, NULL, NULL, NULL, NULL);
        SendMessageW(edit, WM_CHAR, ':', 0);
        GetWindowTextW(edit, text, ARRAY_SIZE(text));
        CHECK(!wcscmp(text, L":"));                       /* an edit control no one limited */
        DestroyWindow(edit);
    }

    IShellFolder_Release(desktop);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
