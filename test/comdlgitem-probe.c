/* comdlg32 common item dialog batch (patches/sg/2006), run by
 * test/comdlgitem-gate.sh.  Families: IOleWindow::ContextSensitiveHelp,
 * ClearClientData, SetFilter (+ IncludeObject), AddPlace, SetNavigationRoot,
 * file type index, IFileSaveDialog properties / SetSaveAsItem,
 * ICommDlgBrowser3 callbacks, IFileDialogCustomize item text and errors.
 *
 *   comdlgitem-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <propsys.h>
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
#define CHECK_HR(expr, want) do { HRESULT hr_ = (expr); \
    char buf_[200]; snprintf(buf_, sizeof(buf_), "%s == 0x%08lx (got 0x%08lx)", #expr, (unsigned long)(want), (unsigned long)hr_); \
    check(hr_ == (want), buf_); } while (0)

/* ---- mock objects --------------------------------------------------- */

typedef struct { IShellItemFilter iface; LONG ref; HRESULT result; int calls; } MockFilter;
static MockFilter *impl_filter(IShellItemFilter *i) { return (MockFilter *)i; }
static HRESULT WINAPI mf_QI(IShellItemFilter *i, REFIID riid, void **out)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IShellItemFilter))
    { *out = i; IShellItemFilter_AddRef(i); return S_OK; }
    *out = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI mf_AddRef(IShellItemFilter *i) { return InterlockedIncrement(&impl_filter(i)->ref); }
static ULONG WINAPI mf_Release(IShellItemFilter *i) { return InterlockedDecrement(&impl_filter(i)->ref); }
static HRESULT WINAPI mf_Include(IShellItemFilter *i, IShellItem *psi)
{ MockFilter *f = impl_filter(i); f->calls++; return f->result; }
static HRESULT WINAPI mf_Flags(IShellItemFilter *i, IShellItem *psi, SFGAOF *f) { return E_NOTIMPL; }
static IShellItemFilterVtbl mf_vtbl = { mf_QI, mf_AddRef, mf_Release, mf_Include, mf_Flags };

typedef struct { IPropertyStore iface; LONG ref; } MockStore;
static MockStore *impl_store(IPropertyStore *i) { return (MockStore *)i; }
static HRESULT WINAPI ms_QI(IPropertyStore *i, REFIID riid, void **out)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IPropertyStore))
    { *out = i; IPropertyStore_AddRef(i); return S_OK; }
    *out = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI ms_AddRef(IPropertyStore *i) { return InterlockedIncrement(&impl_store(i)->ref); }
static ULONG WINAPI ms_Release(IPropertyStore *i) { return InterlockedDecrement(&impl_store(i)->ref); }
static HRESULT WINAPI ms_GetCount(IPropertyStore *i, DWORD *c) { *c = 0; return S_OK; }
static HRESULT WINAPI ms_GetAt(IPropertyStore *i, DWORD n, PROPERTYKEY *k) { return E_INVALIDARG; }
static HRESULT WINAPI ms_GetValue(IPropertyStore *i, REFPROPERTYKEY k, PROPVARIANT *v) { return E_FAIL; }
static HRESULT WINAPI ms_SetValue(IPropertyStore *i, REFPROPERTYKEY k, REFPROPVARIANT v) { return E_FAIL; }
static HRESULT WINAPI ms_Commit(IPropertyStore *i) { return S_OK; }
static IPropertyStoreVtbl ms_vtbl = { ms_QI, ms_AddRef, ms_Release, ms_GetCount, ms_GetAt,
                                            ms_GetValue, ms_SetValue, ms_Commit };

/* A stand-in for the shell view: only IPersistFolder2::GetCurFolder, which is
 * what SHGetIDListFromObject uses to find the folder being listed. */
typedef struct { IPersistFolder2 iface; LPITEMIDLIST folder; } MockView;
static MockView *impl_view(IPersistFolder2 *i) { return (MockView *)i; }
static HRESULT WINAPI mv_QI(IPersistFolder2 *i, REFIID riid, void **out)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IPersistFolder2))
    { *out = i; return S_OK; }
    *out = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI mv_AddRef(IPersistFolder2 *i) { return 2; }
static ULONG WINAPI mv_Release(IPersistFolder2 *i) { return 1; }
static HRESULT WINAPI mv_GetClassID(IPersistFolder2 *i, CLSID *c) { return E_NOTIMPL; }
static HRESULT WINAPI mv_Init(IPersistFolder2 *i, LPCITEMIDLIST p) { return E_NOTIMPL; }
static HRESULT WINAPI mv_GetCur(IPersistFolder2 *i, LPITEMIDLIST *p)
{ *p = ILClone(impl_view(i)->folder); return *p ? S_OK : E_OUTOFMEMORY; }
static IPersistFolder2Vtbl mv_vtbl = { mv_QI, mv_AddRef, mv_Release, mv_GetClassID, mv_Init, mv_GetCur };

/* ---- helpers --------------------------------------------------------- */

static IFileOpenDialog *new_open(void)
{
    IFileOpenDialog *d = NULL;
    CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, &IID_IFileOpenDialog, (void **)&d);
    return d;
}
static IFileSaveDialog *new_save(void)
{
    IFileSaveDialog *d = NULL;
    CoCreateInstance(&CLSID_FileSaveDialog, NULL, CLSCTX_INPROC_SERVER, &IID_IFileSaveDialog, (void **)&d);
    return d;
}
static ULONG refcount(IUnknown *u) { IUnknown_AddRef(u); return IUnknown_Release(u); }

static void write_file(const WCHAR *path)
{
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) { DWORD n; WriteFile(h, "x", 1, &n, NULL); CloseHandle(h); }
}

static const COMDLG_FILTERSPEC specs[] = {
    { L"Text", L"*.txt" },
    { L"Logs", L"*.log;*.out" },
};

int main(void)
{
    IFileOpenDialog *open, *open2;
    IFileSaveDialog *save;
    IFileDialog2 *fd2;
    IOleWindow *ow;
    ICommDlgBrowser3 *cdb;
    IFileDialogCustomize *cust;
    IShellItem *desktop, *file_item;
    LPITEMIDLIST dir_pidl, a_full, b_full, a_child, b_child;
    MockFilter filt = { { &mf_vtbl }, 1, S_OK, 0 };
    MockStore store = { { &ms_vtbl }, 1 };
    MockView view;
    IPropertyStore *got;
    WCHAR dir[MAX_PATH], path[MAX_PATH], spec[64];
    DWORD opts, flags;
    UINT idx;
    ULONG before;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    SHGetKnownFolderItem(&FOLDERID_Desktop, 0, NULL, &IID_IShellItem, (void **)&desktop);
    CHECK(desktop != NULL);

    GetTempPathW(MAX_PATH, dir);
    lstrcatW(dir, L"sgitem");
    CreateDirectoryW(dir, NULL);
    wsprintfW(path, L"%s\\a.txt", dir); write_file(path);
    a_full = ILCreateFromPathW(path);
    wsprintfW(path, L"%s\\b.log", dir); write_file(path);
    b_full = ILCreateFromPathW(path);
    dir_pidl = ILCreateFromPathW(dir);
    CHECK(a_full && b_full && dir_pidl);
    a_child = ILFindLastID(a_full);
    b_child = ILFindLastID(b_full);
    view.iface.lpVtbl = &mv_vtbl;
    view.folder = dir_pidl;
    wsprintfW(path, L"%s\\a.txt", dir);
    SHCreateItemFromParsingName(path, NULL, &IID_IShellItem, (void **)&file_item);
    CHECK(file_item != NULL);

    open = new_open();
    save = new_save();
    CHECK(open && save);

    /* ---- IOleWindow::ContextSensitiveHelp ---------------------------- */
    CHECK_HR(IFileOpenDialog_QueryInterface(open, &IID_IOleWindow, (void **)&ow), S_OK);
    CHECK_HR(IOleWindow_ContextSensitiveHelp(ow, TRUE), S_OK);
    CHECK_HR(IOleWindow_ContextSensitiveHelp(ow, FALSE), S_OK);
    IOleWindow_Release(ow);
    CHECK_HR(IFileSaveDialog_QueryInterface(save, &IID_IOleWindow, (void **)&ow), S_OK);
    CHECK_HR(IOleWindow_ContextSensitiveHelp(ow, TRUE), S_OK);
    IOleWindow_Release(ow);

    /* ---- ClearClientData ---------------------------------------------- */
    CHECK_HR(IFileOpenDialog_ClearClientData(open), E_FAIL);
    CHECK_HR(IFileSaveDialog_ClearClientData(save), E_FAIL);
    CHECK_HR(IFileOpenDialog_SetClientGuid(open, &IID_IFileOpenDialog), S_OK);
    CHECK_HR(IFileOpenDialog_ClearClientData(open), S_OK);

    /* ---- file type index ---------------------------------------------- */
    CHECK_HR(IFileOpenDialog_SetFileTypes(open, 2, specs), S_OK);
    idx = 99;
    CHECK_HR(IFileOpenDialog_GetFileTypeIndex(open, &idx), S_OK);
    CHECK(idx == 0);
    CHECK_HR(IFileOpenDialog_SetFileTypeIndex(open, 2), S_OK);
    idx = 99;
    CHECK_HR(IFileOpenDialog_GetFileTypeIndex(open, &idx), S_OK);
    CHECK(idx == 2);
    CHECK_HR(IFileSaveDialog_SetFileTypes(save, 2, specs), S_OK);
    idx = 99;
    CHECK_HR(IFileSaveDialog_GetFileTypeIndex(save, &idx), S_OK);
    CHECK(idx == 0);

    /* ---- ICommDlgBrowser3 --------------------------------------------- */
    open2 = new_open();
    CHECK_HR(IFileOpenDialog_QueryInterface(open2, &IID_ICommDlgBrowser3, (void **)&cdb), S_OK);
    CHECK_HR(ICommDlgBrowser3_Notify(cdb, NULL, 1), S_OK);
    CHECK_HR(ICommDlgBrowser3_GetDefaultMenuText(cdb, NULL, spec, 64), S_FALSE);
    flags = 0xdeadbeef;
    CHECK_HR(ICommDlgBrowser3_GetViewFlags(cdb, &flags), S_OK);
    CHECK(flags == 0);
    CHECK_HR(ICommDlgBrowser3_GetViewFlags(cdb, NULL), E_INVALIDARG);
    CHECK_HR(IFileOpenDialog_GetOptions(open2, &opts), S_OK);
    CHECK_HR(IFileOpenDialog_SetOptions(open2, opts | FOS_FORCESHOWHIDDEN), S_OK);
    flags = 0xdeadbeef;
    CHECK_HR(ICommDlgBrowser3_GetViewFlags(cdb, &flags), S_OK);
    CHECK(flags == 1);
    CHECK_HR(ICommDlgBrowser3_OnColumnClicked(cdb, NULL, 2), S_FALSE);
    CHECK_HR(ICommDlgBrowser3_OnPreViewCreated(cdb, NULL), S_OK);
    spec[0] = L'x';
    CHECK_HR(ICommDlgBrowser3_GetCurrentFilter(cdb, spec, 64), S_OK);
    CHECK(!wcscmp(spec, L"*.*"));
    CHECK_HR(ICommDlgBrowser3_GetCurrentFilter(cdb, NULL, 64), E_INVALIDARG);
    CHECK_HR(ICommDlgBrowser3_GetCurrentFilter(cdb, spec, 0), E_INVALIDARG);
    ICommDlgBrowser3_Release(cdb);

    CHECK_HR(IFileOpenDialog_QueryInterface(open, &IID_ICommDlgBrowser3, (void **)&cdb), S_OK);
    CHECK_HR(ICommDlgBrowser3_GetCurrentFilter(cdb, spec, 64), S_OK);
    CHECK(!wcscmp(spec, L"*.log;*.out"));
    spec[0] = L'x';
    CHECK_HR(ICommDlgBrowser3_GetCurrentFilter(cdb, spec, 4), HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER));
    CHECK(spec[0] == 0);

    /* IncludeObject applies the current type filter ... */
    CHECK_HR(IFileOpenDialog_SetFileTypeIndex(open, 1), S_OK);
    CHECK_HR(ICommDlgBrowser3_IncludeObject(cdb, (IShellView *)&view.iface, a_child), S_OK);
    CHECK_HR(ICommDlgBrowser3_IncludeObject(cdb, (IShellView *)&view.iface, b_child), S_FALSE);

    /* ---- SetFilter ----------------------------------------------------- */
    CHECK_HR(IFileOpenDialog_SetFilter(open, NULL), S_OK);
    filt.result = S_FALSE;
    before = filt.ref;
    CHECK_HR(IFileOpenDialog_SetFilter(open, &filt.iface), S_OK);
    CHECK(filt.ref == before + 1);
    CHECK_HR(ICommDlgBrowser3_IncludeObject(cdb, (IShellView *)&view.iface, a_child), S_FALSE);
    CHECK(filt.calls > 0);
    filt.result = S_OK;
    CHECK_HR(ICommDlgBrowser3_IncludeObject(cdb, (IShellView *)&view.iface, a_child), S_OK);
    CHECK_HR(ICommDlgBrowser3_IncludeObject(cdb, (IShellView *)&view.iface, b_child), S_FALSE);
    CHECK_HR(IFileOpenDialog_SetFilter(open, NULL), S_OK);
    CHECK(filt.ref == before);
    CHECK_HR(IFileOpenDialog_SetFilter(open, &filt.iface), S_OK);
    ICommDlgBrowser3_Release(cdb);
    IFileOpenDialog_Release(open);
    CHECK(filt.ref == before);
    IFileOpenDialog_Release(open2);

    /* ---- AddPlace ------------------------------------------------------ */
    open = new_open();
    before = refcount((IUnknown *)desktop);
    CHECK_HR(IFileOpenDialog_AddPlace(open, desktop, FDAP_TOP + 1), E_INVALIDARG);
    CHECK(refcount((IUnknown *)desktop) == before);
    CHECK_HR(IFileOpenDialog_AddPlace(open, desktop, FDAP_BOTTOM), S_OK);
    CHECK_HR(IFileOpenDialog_AddPlace(open, desktop, FDAP_TOP), S_OK);
    CHECK_HR(IFileOpenDialog_AddPlace(open, file_item, FDAP_TOP), S_OK);
    CHECK(refcount((IUnknown *)desktop) == before + 2);
    CHECK_HR(IFileSaveDialog_AddPlace(save, desktop, FDAP_TOP + 1), E_INVALIDARG);
    CHECK_HR(IFileSaveDialog_AddPlace(save, desktop, FDAP_BOTTOM), S_OK);

    /* ---- SetNavigationRoot --------------------------------------------- */
    CHECK_HR(IFileOpenDialog_QueryInterface(open, &IID_IFileDialog2, (void **)&fd2), S_OK);
    CHECK_HR(IFileDialog2_SetNavigationRoot(fd2, NULL), E_INVALIDARG);
    CHECK_HR(IFileDialog2_SetNavigationRoot(fd2, desktop), S_OK);
    CHECK(refcount((IUnknown *)desktop) == before + 4);
    CHECK_HR(IFileDialog2_SetNavigationRoot(fd2, desktop), S_OK);
    CHECK(refcount((IUnknown *)desktop) == before + 4);
    IFileDialog2_Release(fd2);
    IFileOpenDialog_Release(open);
    CHECK(refcount((IUnknown *)desktop) == before + 1);   /* the save dialog's place */

    /* ---- IFileSaveDialog properties and SetSaveAsItem ------------------- */
    got = (IPropertyStore *)(INT_PTR)0xdeadbeef;
    CHECK_HR(IFileSaveDialog_GetProperties(save, NULL), E_UNEXPECTED);
    CHECK_HR(IFileSaveDialog_GetProperties(save, &got), E_UNEXPECTED);
    CHECK(got == (IPropertyStore *)(INT_PTR)0xdeadbeef);
    CHECK_HR(IFileSaveDialog_SetCollectedProperties(save, NULL, TRUE), S_OK);
    CHECK_HR(IFileSaveDialog_SetCollectedProperties(save, NULL, FALSE), S_OK);
    CHECK_HR(IFileSaveDialog_SetSaveAsItem(save, NULL), S_OK);
    CHECK_HR(IFileSaveDialog_SetSaveAsItem(save, desktop), MK_E_NOOBJECT);
    before = refcount((IUnknown *)file_item);
    CHECK_HR(IFileSaveDialog_SetSaveAsItem(save, file_item), S_OK);
    CHECK(refcount((IUnknown *)file_item) == before + 1);
    CHECK_HR(IFileSaveDialog_SetSaveAsItem(save, NULL), S_OK);
    CHECK(refcount((IUnknown *)file_item) == before);
    CHECK_HR(IFileSaveDialog_SetProperties(save, &store.iface), S_OK);
    CHECK(store.ref == 2);
    got = NULL;
    CHECK_HR(IFileSaveDialog_GetProperties(save, &got), S_OK);
    CHECK(got == &store.iface);
    if (got) IPropertyStore_Release(got);
    CHECK_HR(IFileSaveDialog_ApplyProperties(save, desktop, NULL, NULL, NULL), S_OK);
    CHECK_HR(IFileSaveDialog_ApplyProperties(save, NULL, &store.iface, NULL, NULL), E_INVALIDARG);

    /* ---- IFileDialogCustomize ------------------------------------------- */
    CHECK_HR(IFileSaveDialog_QueryInterface(save, &IID_IFileDialogCustomize, (void **)&cust), S_OK);
    CHECK_HR(IFileDialogCustomize_AddControlItem(cust, 900, 1, L"x"), E_INVALIDARG);
    {
        CDCONTROLSTATEF st = 0xdeadbeef;
        CHECK_HR(IFileDialogCustomize_GetControlState(cust, 900, &st), E_INVALIDARG);
        CHECK(st == 0xdeadbeef);
    }
    CHECK_HR(IFileDialogCustomize_SetControlItemText(cust, 900, 1, L"x"), E_INVALIDARG);

    CHECK_HR(IFileDialogCustomize_AddComboBox(cust, 10), S_OK);
    CHECK_HR(IFileDialogCustomize_AddControlItem(cust, 10, 1, L"one"), S_OK);
    CHECK_HR(IFileDialogCustomize_AddControlItem(cust, 10, 2, L"two"), S_OK);
    CHECK_HR(IFileDialogCustomize_SetSelectedControlItem(cust, 10, 2), S_OK);
    CHECK_HR(IFileDialogCustomize_SetControlItemText(cust, 10, 1, L"uno"), S_OK);
    CHECK_HR(IFileDialogCustomize_SetControlItemText(cust, 10, 7, L"x"), E_INVALIDARG);
    CHECK_HR(IFileDialogCustomize_SetControlItemText(cust, 10, 1, NULL), E_INVALIDARG);
    {
        DWORD sel = 0;
        CHECK_HR(IFileDialogCustomize_GetSelectedControlItem(cust, 10, &sel), S_OK);
        CHECK(sel == 2);   /* the selection survives re-labelling another item */
        CHECK_HR(IFileDialogCustomize_SetControlItemText(cust, 10, 2, L"deux"), S_OK);
        sel = 0;
        CHECK_HR(IFileDialogCustomize_GetSelectedControlItem(cust, 10, &sel), S_OK);
        CHECK(sel == 2);   /* ... and re-labelling the selected one */
    }

    CHECK_HR(IFileDialogCustomize_AddRadioButtonList(cust, 11), S_OK);
    CHECK_HR(IFileDialogCustomize_AddControlItem(cust, 11, 1, L"r1"), S_OK);
    CHECK_HR(IFileDialogCustomize_SetControlItemText(cust, 11, 1, L"radio one"), S_OK);

    CHECK_HR(IFileDialogCustomize_AddMenu(cust, 12, L"menu"), S_OK);
    CHECK_HR(IFileDialogCustomize_AddControlItem(cust, 12, 1, L"m1"), S_OK);
    CHECK_HR(IFileDialogCustomize_SetControlItemText(cust, 12, 1, L"menu one"), S_OK);

    CHECK_HR(IFileDialogCustomize_AddPushButton(cust, 13, L"push"), S_OK);
    CHECK_HR(IFileDialogCustomize_SetControlItemText(cust, 13, 1, L"x"), E_NOINTERFACE);
    CHECK_HR(IFileDialogCustomize_MakeProminent(cust, 13), S_OK);

    CHECK_HR(IFileDialogCustomize_EnableOpenDropDown(cust, 14), S_OK);
    CHECK_HR(IFileDialogCustomize_AddControlItem(cust, 14, 1, L"od"), S_OK);
    CHECK_HR(IFileDialogCustomize_SetSelectedControlItem(cust, 14, 1), E_NOTIMPL);
    CHECK_HR(IFileDialogCustomize_SetControlItemText(cust, 14, 1, L"open as"), S_OK);
    IFileDialogCustomize_Release(cust);

    IFileSaveDialog_Release(save);
    CHECK(store.ref == 1);

    IShellItem_Release(file_item);
    IShellItem_Release(desktop);
    ILFree(a_full); ILFree(b_full); ILFree(dir_pidl);
    CoUninitialize();

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
