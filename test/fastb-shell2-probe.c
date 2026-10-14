/* shell32 batch (3684): QueryAppIsDefault(All), ClearUserAssociations, SHAssocEnumHandlers,
 * SHCreateDataObject, SHGetDataFromIDList(DESCRIPTIONID), IEnumIDList::Clone, IActiveDesktop wallpaper,
 * DriveType, PathIsTemporaryW, SHFlushClipboard */
#define COBJMACROS
#include <windows.h>
#include <wininet.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <stdio.h>

static int failed;
#define CHECK(c, ...) do { if (!(c)) { failed++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void setreg(HKEY root, const WCHAR *key, const WCHAR *name, const WCHAR *val)
{
    RegSetKeyValueW(root, key, name, REG_SZ, val, (lstrlenW(val) + 1) * sizeof(WCHAR));
}

/* a tiny data object with CF_TEXT, used as the inner object of SHCreateDataObject */
typedef struct { IDataObject IDataObject_iface; LONG ref; } inner_t;
static inner_t inner;
static HRESULT WINAPI in_QI(IDataObject *i, REFIID r, void **o) { if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IDataObject)) { *o = i; return S_OK; } *o = NULL; return E_NOINTERFACE; }
static ULONG WINAPI in_AddRef(IDataObject *i) { return InterlockedIncrement(&inner.ref); }
static ULONG WINAPI in_Release(IDataObject *i) { return InterlockedDecrement(&inner.ref); }
static HRESULT WINAPI in_GetData(IDataObject *i, FORMATETC *f, STGMEDIUM *m)
{
    HGLOBAL g; char *p;
    if (f->cfFormat != CF_TEXT) return DV_E_FORMATETC;
    g = GlobalAlloc(GMEM_MOVEABLE, 6); p = GlobalLock(g); lstrcpyA(p, "inner"); GlobalUnlock(g);
    m->tymed = TYMED_HGLOBAL; m->hGlobal = g; m->pUnkForRelease = NULL; return S_OK;
}
static HRESULT WINAPI in_GetDataHere(IDataObject *i, FORMATETC *f, STGMEDIUM *m) { return E_NOTIMPL; }
static HRESULT WINAPI in_Query(IDataObject *i, FORMATETC *f) { return f->cfFormat == CF_TEXT ? S_OK : DV_E_FORMATETC; }
static HRESULT WINAPI in_Canon(IDataObject *i, FORMATETC *f, FORMATETC *o) { return E_NOTIMPL; }
static HRESULT WINAPI in_Set(IDataObject *i, FORMATETC *f, STGMEDIUM *m, BOOL r) { return E_NOTIMPL; }
static HRESULT WINAPI in_Enum(IDataObject *i, DWORD d, IEnumFORMATETC **e)
{
    FORMATETC f = { CF_TEXT, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    return SHCreateStdEnumFmtEtc(1, &f, e);
}
static HRESULT WINAPI in_DAdvise(IDataObject *i, FORMATETC *f, DWORD a, IAdviseSink *s, DWORD *c) { return E_NOTIMPL; }
static HRESULT WINAPI in_DUnadvise(IDataObject *i, DWORD c) { return E_NOTIMPL; }
static HRESULT WINAPI in_EnumDAdvise(IDataObject *i, IEnumSTATDATA **e) { return E_NOTIMPL; }
static IDataObjectVtbl invtbl = { in_QI, in_AddRef, in_Release, in_GetData, in_GetDataHere, in_Query, in_Canon, in_Set, in_Enum,
    in_DAdvise, in_DUnadvise, in_EnumDAdvise };

static void test_assoc(void)
{
    IApplicationAssociationRegistration *reg = NULL;
    HRESULT hr;
    BOOL def;
    WCHAR exe[MAX_PATH], cmd[MAX_PATH + 8];

    GetSystemDirectoryW(exe, MAX_PATH);
    lstrcatW(exe, L"\\notepad.exe");
    wsprintfW(cmd, L"\"%s\" \"%%1\"", exe);
    /* two fake applications that declare .sgq1 and .sgq2, and a protocol */
    RegSetKeyValueW(HKEY_CLASSES_ROOT, L"SGQ.File\\shell\\open\\command", NULL, REG_SZ, cmd, (lstrlenW(cmd) + 1) * 2);
    RegSetKeyValueW(HKEY_CLASSES_ROOT, L"SGQ.Other\\shell\\open\\command", NULL, REG_SZ, cmd, (lstrlenW(cmd) + 1) * 2);
    setreg(HKEY_LOCAL_MACHINE, L"Software\\SGQApp\\Capabilities\\FileAssociations", L".sgq1", L"SGQ.File");
    setreg(HKEY_LOCAL_MACHINE, L"Software\\SGQApp\\Capabilities\\FileAssociations", L".sgq2", L"SGQ.File");
    setreg(HKEY_LOCAL_MACHINE, L"Software\\SGQApp\\Capabilities\\URLAssociations", L"sgqp", L"SGQ.File");
    setreg(HKEY_LOCAL_MACHINE, L"Software\\RegisteredApplications", L"SGQApp", L"Software\\SGQApp\\Capabilities");
    setreg(HKEY_LOCAL_MACHINE, L"Software\\SGQOther\\Capabilities\\FileAssociations", L".sgq1", L"SGQ.Other");
    setreg(HKEY_LOCAL_MACHINE, L"Software\\RegisteredApplications", L"SGQOther", L"Software\\SGQOther\\Capabilities");
    RegSetKeyValueW(HKEY_CLASSES_ROOT, L".sgq1", NULL, REG_SZ, L"SGQ.Other", 20);
    RegSetKeyValueW(HKEY_CLASSES_ROOT, L".sgq1\\OpenWithProgids", L"SGQ.File", REG_NONE, NULL, 0);

    hr = CoCreateInstance(&CLSID_ApplicationAssociationRegistration, NULL, CLSCTX_INPROC_SERVER,
            &IID_IApplicationAssociationRegistration, (void **)&reg);
    CHECK(hr == S_OK, "AAR create %#lx", hr);
    if (!reg) return;

    def = TRUE;
    hr = IApplicationAssociationRegistration_QueryAppIsDefault(reg, L".sgq1", AT_FILEEXTENSION, AL_EFFECTIVE, L"SGQApp", &def);
    CHECK(hr == S_OK && !def, "before: hr %#lx def %d", hr, def);
    def = TRUE;
    hr = IApplicationAssociationRegistration_QueryAppIsDefault(reg, L".sgq1", AT_FILEEXTENSION, AL_EFFECTIVE, L"SGQOther", &def);
    CHECK(hr == S_OK && def, "class default is the other app's: hr %#lx def %d", hr, def);
    def = TRUE;
    hr = IApplicationAssociationRegistration_QueryAppIsDefault(reg, L".sgq1", AT_FILEEXTENSION, AL_MACHINE, L"SGQApp", &def);
    CHECK(hr == S_OK && !def, "machine: hr %#lx def %d", hr, def);
    def = TRUE;
    hr = IApplicationAssociationRegistration_QueryAppIsDefault(reg, L".sgq1", AT_FILEEXTENSION, AL_USER, L"SGQOther", &def);
    CHECK(hr == S_OK && !def, "user level has no choice yet: hr %#lx def %d", hr, def);
    hr = IApplicationAssociationRegistration_QueryAppIsDefault(reg, NULL, AT_FILEEXTENSION, AL_EFFECTIVE, L"SGQApp", &def);
    CHECK(hr == E_INVALIDARG, "NULL query %#lx", hr);
    hr = IApplicationAssociationRegistration_QueryAppIsDefault(reg, L"sgq1", AT_FILEEXTENSION, AL_EFFECTIVE, L"SGQApp", &def);
    CHECK(hr == E_INVALIDARG, "extension without dot %#lx", hr);

    hr = IApplicationAssociationRegistration_SetAppAsDefault(reg, L"SGQApp", L".sgq1", AT_FILEEXTENSION);
    CHECK(hr == S_OK || hr == E_ACCESSDENIED, "SetAppAsDefault %#lx", hr);
    if (hr == E_ACCESSDENIED)
    {   /* publisher rule: record the choice directly */
        setreg(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.sgq1\\UserChoice", L"ProgId", L"SGQ.File");
    }
    def = FALSE;
    hr = IApplicationAssociationRegistration_QueryAppIsDefault(reg, L".sgq1", AT_FILEEXTENSION, AL_EFFECTIVE, L"SGQApp", &def);
    CHECK(hr == S_OK && def, "after set, effective: hr %#lx def %d", hr, def);
    def = FALSE;
    hr = IApplicationAssociationRegistration_QueryAppIsDefault(reg, L".sgq1", AT_FILEEXTENSION, AL_USER, L"SGQApp", &def);
    CHECK(hr == S_OK && def, "after set, user: hr %#lx def %d", hr, def);
    def = TRUE;
    hr = IApplicationAssociationRegistration_QueryAppIsDefault(reg, L".sgq1", AT_FILEEXTENSION, AL_EFFECTIVE, L"SGQOther", &def);
    CHECK(hr == S_OK && !def, "after set, the other app: hr %#lx def %d", hr, def);
    def = TRUE;
    hr = IApplicationAssociationRegistration_QueryAppIsDefault(reg, L".sgq2", AT_FILEEXTENSION, AL_EFFECTIVE, L"SGQApp", &def);
    CHECK(hr == S_OK && !def, ".sgq2 not set yet: hr %#lx def %d", hr, def);

    /* all: .sgq1 set, .sgq2 and the protocol not */
    def = TRUE;
    hr = IApplicationAssociationRegistration_QueryAppIsDefaultAll(reg, AL_EFFECTIVE, L"SGQApp", &def);
    CHECK(hr == S_OK && !def, "all with one of three: hr %#lx def %d", hr, def);
    hr = IApplicationAssociationRegistration_SetAppAsDefaultAll(reg, L"SGQApp");
    CHECK(hr == S_OK, "SetAppAsDefaultAll %#lx", hr);
    def = FALSE;
    hr = IApplicationAssociationRegistration_QueryAppIsDefaultAll(reg, AL_EFFECTIVE, L"SGQApp", &def);
    CHECK(hr == S_OK && def, "all after SetAppAsDefaultAll: hr %#lx def %d", hr, def);
    def = FALSE;
    hr = IApplicationAssociationRegistration_QueryAppIsDefault(reg, L"sgqp", AT_URLPROTOCOL, AL_EFFECTIVE, L"SGQApp", &def);
    CHECK(hr == S_OK && def, "protocol: hr %#lx def %d", hr, def);
    def = TRUE;
    hr = IApplicationAssociationRegistration_QueryAppIsDefaultAll(reg, AL_EFFECTIVE, L"NoSuchApp", &def);
    CHECK(hr == S_OK && !def, "unknown app: hr %#lx def %d", hr, def);

    /* start menu clients */
    setreg(HKEY_LOCAL_MACHINE, L"Software\\Clients\\SGQKind\\SGQClient", NULL, L"x");
    setreg(HKEY_CURRENT_USER, L"Software\\Clients\\SGQKind", NULL, L"SGQClient");
    def = FALSE;
    hr = IApplicationAssociationRegistration_QueryAppIsDefault(reg, L"SGQKind", AT_STARTMENUCLIENT, AL_EFFECTIVE, L"SGQClient", &def);
    CHECK(hr == S_OK && def, "client: hr %#lx def %d", hr, def);
    def = TRUE;
    hr = IApplicationAssociationRegistration_QueryAppIsDefault(reg, L"SGQKind", AT_STARTMENUCLIENT, AL_MACHINE, L"SGQClient", &def);
    CHECK(hr == S_OK && !def, "client at machine level: hr %#lx def %d", hr, def);

    hr = IApplicationAssociationRegistration_ClearUserAssociations(reg);
    CHECK(hr == S_OK, "ClearUserAssociations %#lx", hr);
    def = TRUE;
    hr = IApplicationAssociationRegistration_QueryAppIsDefault(reg, L".sgq1", AT_FILEEXTENSION, AL_USER, L"SGQApp", &def);
    CHECK(hr == S_OK && !def, "after clear, user: hr %#lx def %d", hr, def);
    def = TRUE;
    hr = IApplicationAssociationRegistration_QueryAppIsDefaultAll(reg, AL_EFFECTIVE, L"SGQApp", &def);
    CHECK(hr == S_OK && !def, "after clear, all: hr %#lx def %d", hr, def);
    {
        HKEY k;
        CHECK(RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.sgq1", 0, KEY_READ, &k) == 0 ||
              TRUE, "");
        if (!RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.sgq1\\UserChoice", 0, KEY_READ, &k))
        { CHECK(0, "UserChoice key still there"); RegCloseKey(k); }
    }
    IApplicationAssociationRegistration_Release(reg);

    /* the handlers of .sgq1 */
    {
        IEnumAssocHandlers *en = NULL;
        IAssocHandler *h[4];
        ULONG n = 0;
        int found = 0;
        WCHAR *name;
        int idx;

        hr = SHAssocEnumHandlers(L".sgq1", ASSOC_FILTER_RECOMMENDED, &en);
        CHECK(hr == S_OK && en, "SHAssocEnumHandlers %#lx", hr);
        if (en)
        {
            hr = IEnumAssocHandlers_Next(en, 4, h, &n);
            CHECK(n >= 1, "handlers %lu hr %#lx", n, hr);
            CHECK((n == 4) == (hr == S_OK), "Next hr %#lx for %lu", hr, n);
            for (; n; n--)
            {
                name = NULL;
                if (IAssocHandler_GetName(h[n - 1], &name) == S_OK)
                {
                    if (StrStrIW(name, L"notepad")) found++;
                    CoTaskMemFree(name);
                }
                if (found == 1)
                {
                    WCHAR *ui = NULL;
                    CHECK(IAssocHandler_IsRecommended(h[n - 1]) == S_OK, "IsRecommended");
                    CHECK(IAssocHandler_GetUIName(h[n - 1], &ui) == S_OK && ui && ui[0], "UIName");
                    CoTaskMemFree(ui);
                    name = NULL;
                    CHECK(IAssocHandler_GetIconLocation(h[n - 1], &name, &idx) == S_OK && name && idx == 0, "icon");
                    CoTaskMemFree(name);
                    hr = IAssocHandler_MakeDefault(h[n - 1], L"test");
                    CHECK(hr == S_OK, "MakeDefault %#lx", hr);
                    found = 2;
                }
                IAssocHandler_Release(h[n - 1]);
            }
            CHECK(found >= 1, "the registered program is among the handlers");
            IEnumAssocHandlers_Release(en);
        }
        hr = SHAssocEnumHandlers(L"nodot", ASSOC_FILTER_NONE, &en);
        CHECK(hr == E_INVALIDARG, "bad extension %#lx", hr);
        {
            IAssocHandler *hh[1];
            hr = SHAssocEnumHandlersForProtocolByApplication(L"sgqp", &IID_IEnumAssocHandlers, (void **)&en);
            CHECK(hr == S_OK && en, "protocol handlers %#lx", hr);
            if (en)
            {
                n = 0;
                hr = IEnumAssocHandlers_Next(en, 1, hh, &n);
                CHECK(n == 1 && hr == S_OK, "one protocol handler: %lu %#lx", n, hr);
                if (n) IAssocHandler_Release(hh[0]);
                IEnumAssocHandlers_Release(en);
            }
        }
    }
    /* tidy: our keys */
    SHDeleteKeyW(HKEY_CLASSES_ROOT, L"SGQ.File"); SHDeleteKeyW(HKEY_CLASSES_ROOT, L"SGQ.Other");
    SHDeleteKeyW(HKEY_CLASSES_ROOT, L".sgq1");
    SHDeleteKeyW(HKEY_LOCAL_MACHINE, L"Software\\SGQApp"); SHDeleteKeyW(HKEY_LOCAL_MACHINE, L"Software\\SGQOther");
    SHDeleteKeyW(HKEY_LOCAL_MACHINE, L"Software\\Clients\\SGQKind");
    SHDeleteValueW(HKEY_LOCAL_MACHINE, L"Software\\RegisteredApplications", L"SGQApp");
    SHDeleteValueW(HKEY_LOCAL_MACHINE, L"Software\\RegisteredApplications", L"SGQOther");
}

static void test_dataobject(void)
{
    ITEMIDLIST *folder = NULL, *item = NULL, *full = NULL;
    IDataObject *obj = NULL;
    FORMATETC f = { 0, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM m;
    HRESULT hr;
    WCHAR path[MAX_PATH];
    IEnumFORMATETC *en;
    FORMATETC got;
    ULONG n;
    int has_text = 0, has_hdrop = 0;
    UINT cf_ida = RegisterClipboardFormatW(L"Shell IDList Array");

    GetWindowsDirectoryW(path, MAX_PATH);
    SHParseDisplayName(path, NULL, &full, 0, NULL);
    CHECK(full != NULL, "parse windows dir");
    if (!full) return;
    folder = ILClone(full); ILRemoveLastID(folder);
    item = ILFindLastID(full);

    inner.IDataObject_iface.lpVtbl = &invtbl; inner.ref = 1;
    hr = SHCreateDataObject(folder, 1, (PCUITEMID_CHILD_ARRAY)&item, &inner.IDataObject_iface, &IID_IDataObject, (void **)&obj);
    CHECK(hr == S_OK && obj, "SHCreateDataObject %#lx", hr);
    if (obj)
    {
        f.cfFormat = cf_ida;
        CHECK(IDataObject_QueryGetData(obj, &f) == S_OK, "IDList format present");
        f.cfFormat = CF_HDROP;
        CHECK(IDataObject_QueryGetData(obj, &f) == S_OK, "HDROP present for a file system item");
        hr = IDataObject_GetData(obj, &f, &m);
        CHECK(hr == S_OK, "HDROP GetData %#lx", hr);
        if (hr == S_OK)
        {
            WCHAR file[MAX_PATH];
            CHECK(DragQueryFileW(m.hGlobal, ~0u, NULL, 0) == 1, "one file");
            DragQueryFileW(m.hGlobal, 0, file, MAX_PATH);
            CHECK(!lstrcmpiW(file, path), "path %ls vs %ls", file, path);
            ReleaseStgMedium(&m);
        }
        /* inner object */
        f.cfFormat = CF_TEXT;
        CHECK(IDataObject_QueryGetData(obj, &f) == S_OK, "inner format queried");
        hr = IDataObject_GetData(obj, &f, &m);
        CHECK(hr == S_OK && !lstrcmpA(GlobalLock(m.hGlobal), "inner"), "inner GetData %#lx", hr);
        if (hr == S_OK) { GlobalUnlock(m.hGlobal); ReleaseStgMedium(&m); }
        hr = IDataObject_EnumFormatEtc(obj, DATADIR_GET, &en);
        CHECK(hr == S_OK, "EnumFormatEtc %#lx", hr);
        if (hr == S_OK)
        {
            while (IEnumFORMATETC_Next(en, 1, &got, &n) == S_OK) { if (got.cfFormat == CF_TEXT) has_text++; if (got.cfFormat == CF_HDROP) has_hdrop++; }
            CHECK(has_text == 1 && has_hdrop == 1, "formats: text %d hdrop %d", has_text, has_hdrop);
            IEnumFORMATETC_Release(en);
        }
        /* GetDataHere */
        f.cfFormat = cf_ida;
        {
            STGMEDIUM src;
            hr = IDataObject_GetData(obj, &f, &src);
            if (hr == S_OK)
            {
                SIZE_T sz = GlobalSize(src.hGlobal);
                m.tymed = TYMED_HGLOBAL; m.pUnkForRelease = NULL;
                m.hGlobal = GlobalAlloc(GMEM_MOVEABLE, sz);
                hr = IDataObject_GetDataHere(obj, &f, &m);
                CHECK(hr == S_OK, "GetDataHere %#lx", hr);
                GlobalFree(m.hGlobal);
                m.hGlobal = GlobalAlloc(GMEM_MOVEABLE, 4);
                hr = IDataObject_GetDataHere(obj, &f, &m);
                CHECK(hr == STG_E_MEDIUMFULL, "GetDataHere small %#lx", hr);
                GlobalFree(m.hGlobal);
                ReleaseStgMedium(&src);
            }
        }
        {
            DWORD conn = 5;
            FORMATETC cf = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL }, out;
            hr = IDataObject_DAdvise(obj, &cf, 0, NULL, &conn);
            CHECK(hr == OLE_E_ADVISENOTSUPPORTED, "DAdvise %#lx", hr);
            hr = IDataObject_GetCanonicalFormatEtc(obj, &cf, &out);
            CHECK(hr == DATA_S_SAMEFORMATETC && out.cfFormat == CF_HDROP, "GetCanonicalFormatEtc %#lx", hr);
        }
        IDataObject_Release(obj);
    }
    CHECK(inner.ref == 1, "inner object released: %ld", inner.ref);
    /* NULL folder: absolute items */
    obj = NULL;
    hr = SHCreateDataObject(NULL, 1, (PCUITEMID_CHILD_ARRAY)&full, NULL, &IID_IDataObject, (void **)&obj);
    CHECK(hr == S_OK && obj, "absolute SHCreateDataObject %#lx", hr);
    if (obj)
    {
        f.cfFormat = CF_HDROP;
        CHECK(IDataObject_QueryGetData(obj, &f) == S_OK, "HDROP for absolute");
        IDataObject_Release(obj);
    }
    hr = SHCreateDataObject(folder, 1, NULL, NULL, &IID_IDataObject, (void **)&obj);
    CHECK(hr == E_INVALIDARG, "no items %#lx", hr);
    ILFree(folder);
    ILFree(full);
}

static void test_descid(void)
{
    IShellFolder *desktop, *parent;
    ITEMIDLIST *pidl = NULL;
    const ITEMIDLIST *child;
    SHDESCRIPTIONID desc;
    HRESULT hr;
    WCHAR path[MAX_PATH], file[MAX_PATH];

    GetWindowsDirectoryW(path, MAX_PATH);
    lstrcpyW(file, path); lstrcatW(file, L"\\win.ini");
    {
        HANDLE h = CreateFileW(file, GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    }
    SHGetDesktopFolder(&desktop);
    {
        const WCHAR *paths[] = { L"C:\\", path, file };
        DWORD want[] = { SHDID_COMPUTER_FIXED, SHDID_FS_DIRECTORY, SHDID_FS_FILE };
        int i;
        for (i = 0; i < 3; i++)
        {
            pidl = NULL;
            hr = SHParseDisplayName(paths[i], NULL, &pidl, 0, NULL);
            if (FAILED(hr)) { CHECK(0, "parse %ls", paths[i]); continue; }
            hr = SHBindToParent(pidl, &IID_IShellFolder, (void **)&parent, &child);
            memset(&desc, 0xcc, sizeof(desc));
            hr = SHGetDataFromIDListW(parent, child, SHGDFIL_DESCRIPTIONID, &desc, sizeof(desc));
            CHECK(hr == S_OK && desc.dwDescriptionId == want[i], "%ls: hr %#lx id %lu want %lu", paths[i], hr, desc.dwDescriptionId, want[i]);
            hr = SHGetDataFromIDListW(parent, child, SHGDFIL_DESCRIPTIONID, &desc, 4);
            CHECK(hr == E_INVALIDARG, "short buffer %#lx", hr);
            IShellFolder_Release(parent);
            ILFree(pidl);
        }
    }
    pidl = NULL;
    SHGetSpecialFolderLocation(NULL, CSIDL_DRIVES, &pidl);
    SHBindToParent(pidl, &IID_IShellFolder, (void **)&parent, &child);
    memset(&desc, 0, sizeof(desc));
    hr = SHGetDataFromIDListW(parent, child, SHGDFIL_DESCRIPTIONID, &desc, sizeof(desc));
    CHECK(hr == S_OK && desc.dwDescriptionId == SHDID_ROOT_REGITEM && IsEqualGUID(&desc.clsid, &CLSID_MyComputer),
          "my computer: hr %#lx id %lu", hr, desc.dwDescriptionId);
    IShellFolder_Release(parent);
    ILFree(pidl);
    IShellFolder_Release(desktop);
}

static void test_enum_clone(void)
{
    IShellFolder *desktop;
    IEnumIDList *en = NULL, *cl = NULL;
    ITEMIDLIST *a, *b;
    ULONG n1, n2;
    HRESULT hr;
    int cnt1 = 0, cnt2 = 0;

    SHGetDesktopFolder(&desktop);
    hr = IShellFolder_EnumObjects(desktop, NULL, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS, &en);
    CHECK(hr == S_OK, "EnumObjects %#lx", hr);
    if (en)
    {
        hr = IEnumIDList_Next(en, 1, &a, &n1);
        CHECK(hr == S_OK, "Next %#lx", hr);
        if (hr == S_OK) ILFree(a);
        hr = IEnumIDList_Clone(en, &cl);
        CHECK(hr == S_OK && cl, "Clone %#lx", hr);
        if (cl)
        {
            while (IEnumIDList_Next(en, 1, &a, &n1) == S_OK && n1)
            {
                hr = IEnumIDList_Next(cl, 1, &b, &n2);
                if (hr != S_OK || !ILIsEqual(a, b)) { CHECK(0, "clone diverges at %d", cnt1); ILFree(a); if (hr == S_OK) ILFree(b); break; }
                cnt1++; ILFree(a); ILFree(b);
            }
            CHECK(cnt1 > 0, "enumerated %d items after the first", cnt1);
            hr = IEnumIDList_Next(cl, 1, &b, &n2);
            CHECK(hr == S_FALSE, "clone ends with the original %#lx", hr);
            IEnumIDList_Reset(cl);
            while (IEnumIDList_Next(cl, 1, &b, &n2) == S_OK && n2) { cnt2++; ILFree(b); }
            CHECK(cnt2 == cnt1 + 1, "clone reset: %d vs %d", cnt2, cnt1 + 1);
            IEnumIDList_Release(cl);
        }
        IEnumIDList_Release(en);
    }
    IShellFolder_Release(desktop);
}

static void test_activedesktop(void)
{
    IActiveDesktop *ad = NULL;
    HRESULT hr;
    WCHAR buf[MAX_PATH], bmp[MAX_PATH], cur[MAX_PATH];
    WALLPAPEROPT opt;
    COMPONENTSOPT copt;
    int count = 7;
    HANDLE h;
    static const unsigned char bm[] = { 'B','M', 58,0,0,0, 0,0,0,0, 54,0,0,0, 40,0,0,0, 1,0,0,0, 1,0,0,0, 1,0, 24,0, 0,0,0,0, 4,0,0,0,
        0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 255,0,0,0 };
    DWORD w, size;

    GetTempPathW(MAX_PATH, bmp);
    lstrcatW(bmp, L"sgad.bmp");
    h = CreateFileW(bmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(h, bm, sizeof(bm), &w, NULL);
    CloseHandle(h);

    hr = CoCreateInstance(&CLSID_ActiveDesktop, NULL, CLSCTX_INPROC_SERVER, &IID_IActiveDesktop, (void **)&ad);
    CHECK(hr == S_OK && ad, "ActiveDesktop %#lx", hr);
    if (!ad) return;
    hr = ad->lpVtbl->SetWallpaper(ad, bmp, 0);
    CHECK(hr == S_OK, "SetWallpaper %#lx", hr);
    hr = ad->lpVtbl->GetWallpaper(ad, buf, MAX_PATH, 0);
    CHECK(hr == S_OK && !lstrcmpiW(buf, bmp), "GetWallpaper pending: %#lx %ls", hr, buf);
    opt.dwSize = sizeof(opt); opt.dwStyle = WPSTYLE_STRETCH;
    hr = ad->lpVtbl->SetWallpaperOptions(ad, &opt, 0);
    CHECK(hr == S_OK, "SetWallpaperOptions %#lx", hr);
    opt.dwStyle = 99;
    hr = ad->lpVtbl->SetWallpaperOptions(ad, &opt, 0);
    CHECK(hr == E_INVALIDARG, "bad style %#lx", hr);
    opt.dwStyle = 0;
    hr = ad->lpVtbl->GetWallpaperOptions(ad, &opt, 0);
    CHECK(hr == S_OK && opt.dwStyle == WPSTYLE_STRETCH, "GetWallpaperOptions pending: %#lx %lu", hr, opt.dwStyle);
    hr = ad->lpVtbl->SetPattern(ad, L"0 80 40 20 10 8 4 2", 0);
    CHECK(hr == S_OK, "SetPattern %#lx", hr);
    hr = ad->lpVtbl->ApplyChanges(ad, AD_APPLY_ALL);
    CHECK(hr == S_OK, "ApplyChanges %#lx", hr);
    SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH, cur, 0);
    CHECK(!lstrcmpiW(cur, bmp), "applied wallpaper: %ls", cur);
    size = sizeof(buf);
    CHECK(!RegGetValueW(HKEY_CURRENT_USER, L"Control Panel\\Desktop", L"WallpaperStyle", RRF_RT_REG_SZ, NULL, buf, &size) && !lstrcmpW(buf, L"2"),
          "WallpaperStyle %ls", buf);
    size = sizeof(buf);
    CHECK(!RegGetValueW(HKEY_CURRENT_USER, L"Control Panel\\Desktop", L"Pattern", RRF_RT_REG_SZ, NULL, buf, &size) && !lstrcmpW(buf, L"0 80 40 20 10 8 4 2"),
          "Pattern %ls", buf);
    hr = ad->lpVtbl->GetPattern(ad, buf, MAX_PATH, 0);
    CHECK(hr == S_OK && !lstrcmpW(buf, L"0 80 40 20 10 8 4 2"), "GetPattern %#lx %ls", hr, buf);
    opt.dwStyle = 9;
    hr = ad->lpVtbl->GetWallpaperOptions(ad, &opt, 0);
    CHECK(hr == S_OK && opt.dwStyle == WPSTYLE_STRETCH, "options from registry: %lu", opt.dwStyle);
    hr = ad->lpVtbl->GetDesktopItemCount(ad, &count, 0);
    CHECK(hr == S_OK && count == 0, "item count %#lx %d", hr, count);
    copt.dwSize = sizeof(copt); copt.fEnableComponents = TRUE; copt.fActiveDesktop = TRUE;
    hr = ad->lpVtbl->GetDesktopItemOptions(ad, &copt, 0);
    CHECK(hr == S_OK && !copt.fActiveDesktop && !copt.fEnableComponents, "item options %#lx", hr);
    ad->lpVtbl->Release(ad);
    DeleteFileW(bmp);
}

static void test_misc(void)
{
    int (WINAPI *pDriveType)(int) = (void *)GetProcAddress(GetModuleHandleA("shell32.dll"), (LPCSTR)64);
    BOOL (WINAPI *pTemp)(const WCHAR *) = (void *)GetProcAddress(GetModuleHandleA("shell32.dll"), (LPCSTR)714);
    HRESULT (WINAPI *pFlush)(void) = (void *)GetProcAddress(GetModuleHandleA("shell32.dll"), (LPCSTR)121);
    WCHAR t[MAX_PATH], p[MAX_PATH];

    CHECK(pDriveType && pDriveType(2) == DRIVE_FIXED, "DriveType(C:) = %d", pDriveType ? pDriveType(2) : -1);
    CHECK(pDriveType && pDriveType(30) == DRIVE_NO_ROOT_DIR, "DriveType(30)");
    GetTempPathW(MAX_PATH, t);
    lstrcpyW(p, t); lstrcatW(p, L"sub\\x.tmp");
    CHECK(pTemp && pTemp(p), "temp file is temporary");
    GetWindowsDirectoryW(p, MAX_PATH);
    CHECK(pTemp && !pTemp(p), "windows dir is not");
    OleInitialize(NULL);
    CHECK(pFlush && pFlush() == S_OK, "SHFlushClipboard");
    OleUninitialize();
}

int main(void)
{
    CoInitialize(NULL);
    test_assoc();
    test_dataobject();
    test_descid();
    test_enum_clone();
    test_activedesktop();
    test_misc();
    printf(failed ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", failed);
    return failed != 0;
}
