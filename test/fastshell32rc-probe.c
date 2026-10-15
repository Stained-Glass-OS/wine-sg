#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <propsys.h>
#include <shimgdata.h>
#include <stdio.h>

static int bad, total;
#define EXPECT_HR(e, c) do { HRESULT h_ = (c); total++; if (h_ != (e)) { bad++; printf("MISMATCH line %d: %s = %lx, expected %lx\n", __LINE__, #c, h_, (HRESULT)(e)); } } while (0)
#define EXPECT(c) do { total++; if (!(c)) { bad++; printf("MISMATCH line %d: %s\n", __LINE__, #c); } } while (0)

static const GUID VID_Details = {0x137e7700, 0x3573, 0x11cf, {0xae,0x69,0x08,0x00,0x2b,0x2e,0x12,0x62}};
static const GUID FOLDERID_Windows = {0xF38BF404, 0x1D43, 0x42F2, {0x93,0x05,0x67,0xDE,0x0B,0x28,0xFC,0x23}};
static const GUID CLSID_ShellImageDataFactory = {0x66e4e4fb, 0xf385, 0x4dd0, {0x8d,0x74,0xa2,0xef,0xd1,0xbc,0x61,0x78}};
static const PROPERTYKEY test_key = {{0x12345678, 0x1234, 0x1234, {1,2,3,4,5,6,7,8}}, 7};

/* ---- change notification ---- */
static LONG got_event;
static BOOL got_lock;
static HANDLE (WINAPI *pLock)(HANDLE, DWORD, LPITEMIDLIST **, LONG *);
static BOOL (WINAPI *pUnlock)(HANDLE);
#define WM_NOTIFY_TEST (WM_USER + 40)

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_NOTIFY_TEST)
    {
        LPITEMIDLIST *pidls = NULL;
        LONG event = 0;
        HANDLE lock = pLock((HANDLE)wp, (DWORD)lp, &pidls, &event);
        if (lock)
        {
            got_lock = pidls && pidls[0];
            got_event = event;
            pUnlock(lock);
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ---- a fake bag ---- */
typedef struct { IPropertyBag IPropertyBag_iface; LONG ref; } bag;
static HRESULT WINAPI bag_QI(IPropertyBag *i, REFIID r, void **o)
{
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IPropertyBag)) { *o = i; IPropertyBag_AddRef(i); return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI bag_AddRef(IPropertyBag *i) { return InterlockedIncrement(&CONTAINING_RECORD(i, bag, IPropertyBag_iface)->ref); }
static ULONG WINAPI bag_Release(IPropertyBag *i) { return InterlockedDecrement(&CONTAINING_RECORD(i, bag, IPropertyBag_iface)->ref); }
static HRESULT WINAPI bag_Read(IPropertyBag *i, LPCOLESTR n, VARIANT *v, IErrorLog *l) { return E_FAIL; }
static HRESULT WINAPI bag_Write(IPropertyBag *i, LPCOLESTR n, VARIANT *v) { return E_FAIL; }
static IPropertyBagVtbl bagvtbl = { bag_QI, bag_AddRef, bag_Release, bag_Read, bag_Write };

/* ---- a fake shell browser that keeps the view state ---- */
typedef struct { IShellBrowser IShellBrowser_iface; HWND hwnd; IStream *state; } browser;
static browser *impl_sb(IShellBrowser *i) { return CONTAINING_RECORD(i, browser, IShellBrowser_iface); }
static HRESULT WINAPI sb_QI(IShellBrowser *i, REFIID r, void **o)
{
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IShellBrowser) || IsEqualIID(r, &IID_IOleWindow)) { *o = i; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI sb_AddRef(IShellBrowser *i) { return 2; }
static ULONG WINAPI sb_Release(IShellBrowser *i) { return 1; }
static HRESULT WINAPI sb_GetWindow(IShellBrowser *i, HWND *w) { *w = impl_sb(i)->hwnd; return S_OK; }
static HRESULT WINAPI sb_Ctx(IShellBrowser *i, BOOL m) { return E_NOTIMPL; }
static HRESULT WINAPI sb_InsertMenusSB(IShellBrowser *i, HMENU m, LPOLEMENUGROUPWIDTHS w) { return E_NOTIMPL; }
static HRESULT WINAPI sb_SetMenuSB(IShellBrowser *i, HMENU m, HOLEMENU h, HWND w) { return E_NOTIMPL; }
static HRESULT WINAPI sb_RemoveMenusSB(IShellBrowser *i, HMENU m) { return E_NOTIMPL; }
static HRESULT WINAPI sb_SetStatusTextSB(IShellBrowser *i, LPCOLESTR t) { return E_NOTIMPL; }
static HRESULT WINAPI sb_EnableModelessSB(IShellBrowser *i, BOOL e) { return E_NOTIMPL; }
static HRESULT WINAPI sb_TranslateAcceleratorSB(IShellBrowser *i, MSG *m, WORD id) { return E_NOTIMPL; }
static HRESULT WINAPI sb_BrowseObject(IShellBrowser *i, LPCITEMIDLIST p, UINT f) { return E_NOTIMPL; }
static HRESULT WINAPI sb_GetViewStateStream(IShellBrowser *i, DWORD mode, IStream **s)
{
    browser *b = impl_sb(i);
    LARGE_INTEGER zero = { { 0 } };
    if (!b->state) CreateStreamOnHGlobal(NULL, FALSE, &b->state);
    IStream_Seek(b->state, zero, STREAM_SEEK_SET, NULL);
    IStream_AddRef(b->state);
    *s = b->state;
    return S_OK;
}
static HRESULT WINAPI sb_GetControlWindow(IShellBrowser *i, UINT id, HWND *w) { *w = NULL; return E_NOTIMPL; }
static HRESULT WINAPI sb_SendControlMsg(IShellBrowser *i, UINT id, UINT m, WPARAM w, LPARAM l, LRESULT *r) { return E_NOTIMPL; }
static HRESULT WINAPI sb_QueryActiveShellView(IShellBrowser *i, IShellView **v) { *v = NULL; return E_NOTIMPL; }
static HRESULT WINAPI sb_OnViewWindowActive(IShellBrowser *i, IShellView *v) { return E_NOTIMPL; }
static HRESULT WINAPI sb_SetToolbarItems(IShellBrowser *i, LPTBBUTTONSB b, UINT n, UINT f) { return E_NOTIMPL; }
static IShellBrowserVtbl sbvtbl = { sb_QI, sb_AddRef, sb_Release, sb_GetWindow, sb_Ctx, sb_InsertMenusSB, sb_SetMenuSB,
    sb_RemoveMenusSB, sb_SetStatusTextSB, sb_EnableModelessSB, sb_TranslateAcceleratorSB, sb_BrowseObject,
    sb_GetViewStateStream, sb_GetControlWindow, sb_SendControlMsg, sb_QueryActiveShellView, sb_OnViewWindowActive,
    sb_SetToolbarItems };

static IShellView3 *make_view(browser *sb, FOLDERVIEWMODE mode, HWND *view_wnd)
{
    IShellFolder *desktop, *folder;
    LPITEMIDLIST pidl;
    IShellView3 *view = NULL;
    RECT rect = { 0, 0, 300, 200 };
    HRESULT hr;

    SHGetDesktopFolder(&desktop);
    SHParseDisplayName(L"C:\\windows", NULL, &pidl, 0, NULL);
    IShellFolder_BindToObject(desktop, pidl, NULL, &IID_IShellFolder, (void **)&folder);
    {
        IShellView *sv;
        hr = IShellFolder_CreateViewObject(folder, sb->hwnd, &IID_IShellView, (void **)&sv);
        if (FAILED(hr)) { printf("no IShellView: %lx\n", hr); return NULL; }
        hr = IShellView_QueryInterface(sv, &IID_IShellView3, (void **)&view);
        IShellView_Release(sv);
        if (FAILED(hr)) { printf("no IShellView3: %lx\n", hr); return NULL; }
    }
    hr = IShellView3_CreateViewWindow3(view, &sb->IShellBrowser_iface, NULL, SV3CVW3_DEFAULT, 0, 0, mode, NULL, &rect, view_wnd);
    if (FAILED(hr)) { printf("CreateViewWindow3 failed: %lx\n", hr); IShellView3_Release(view); return NULL; }
    IShellFolder_Release(folder);
    IShellFolder_Release(desktop);
    CoTaskMemFree(pidl);
    return view;
}

int main(void)
{
    HMODULE shell32;
    BOOL (WINAPI *pMakeUnique)(LPWSTR, UINT, LPCWSTR, LPCWSTR, LPCWSTR);
    DWORD (WINAPI *pNTRegister)(HWND, LONG, LONG, DWORD, int, SHChangeNotifyEntry *);
    BOOL (WINAPI *pNTDeregister)(ULONG);
    HRESULT (WINAPI *pSetTemp)(IShellItem *, const PROPERTYKEY *, const PROPVARIANT *);
    HRESULT (WINAPI *pGetTemp)(IShellItem *, const PROPERTYKEY *, PROPVARIANT *);
    WCHAR dir[MAX_PATH], result[MAX_PATH], expect[MAX_PATH], file[MAX_PATH];
    char resultA[MAX_PATH];
    HANDLE h;

    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);
    shell32 = LoadLibraryA("shell32.dll");
    pMakeUnique = (void *)GetProcAddress(shell32, (LPCSTR)47);
    pNTRegister = (void *)GetProcAddress(shell32, (LPCSTR)640);
    pNTDeregister = (void *)GetProcAddress(shell32, (LPCSTR)641);
    pLock = (void *)GetProcAddress(shell32, "SHChangeNotification_Lock");
    pUnlock = (void *)GetProcAddress(shell32, "SHChangeNotification_Unlock");
    pSetTemp = (void *)GetProcAddress(shell32, "SHSetTemporaryPropertyForItem");
    pGetTemp = (void *)GetProcAddress(shell32, "SHGetTemporaryPropertyForItem");
    EXPECT(pMakeUnique && pNTRegister && pNTDeregister && pLock && pUnlock && pSetTemp && pGetTemp);

    GetTempPathW(MAX_PATH, dir);
    lstrcatW(dir, L"fastshell32rc");
    CreateDirectoryW(dir, NULL);

    /* ---- PathMakeUniqueName ---- */
    result[0] = 0;
    EXPECT(pMakeUnique(result, MAX_PATH, L"NEW", L"My New Filename", dir));
    wsprintfW(expect, L"%s\\My New Filename (1)", dir);
    EXPECT(!lstrcmpiW(result, expect));
    if (lstrcmpiW(result, expect)) printf("  got %ls\n", result);
    h = CreateFileW(result, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
    CloseHandle(h);
    EXPECT(pMakeUnique(result, MAX_PATH, L"NEW", L"My New Filename", dir));
    wsprintfW(expect, L"%s\\My New Filename (2)", dir);
    EXPECT(!lstrcmpiW(result, expect));
    DeleteFileW(file); wsprintfW(file, L"%s\\My New Filename (1)", dir); DeleteFileW(file);
    EXPECT(!pMakeUnique(result, 10, L"NEW", L"My New Filename", dir));
    EXPECT(!pMakeUnique(result, MAX_PATH, NULL, NULL, dir));
    (void)resultA;

    /* ---- NTSHChangeNotifyRegister delivers with the new delivery ---- */
    {
        WNDCLASSW wc = { 0 };
        HWND hwnd;
        SHChangeNotifyEntry entry;
        DWORD id;
        MSG msg;
        DWORD end;
        LPITEMIDLIST dirpidl;

        wc.lpfnWndProc = wndproc;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.lpszClassName = L"fastshell32rc";
        RegisterClassW(&wc);
        hwnd = CreateWindowW(L"fastshell32rc", L"x", WS_POPUP, 0, 0, 10, 10, NULL, NULL, wc.hInstance, NULL);
        SHParseDisplayName(dir, NULL, &dirpidl, 0, NULL);
        entry.pidl = dirpidl;
        entry.fRecursive = TRUE;
        id = pNTRegister(hwnd, SHCNRF_ShellLevel | SHCNRF_InterruptLevel, SHCNE_CREATE, WM_NOTIFY_TEST, 1, &entry);
        EXPECT(id != 0);
        wsprintfW(file, L"%s\\notify.txt", dir);
        h = CreateFileW(file, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
        CloseHandle(h);
        SHChangeNotify(SHCNE_CREATE, SHCNF_PATHW | SHCNF_FLUSH, file, NULL);
        end = GetTickCount() + 3000;
        while (!got_lock && GetTickCount() < end)
        {
            while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
            Sleep(10);
        }
        EXPECT(got_lock);
        EXPECT(got_event == SHCNE_CREATE);
        EXPECT(pNTDeregister(id));
        DeleteFileW(file);
        CoTaskMemFree(dirpidl);
        DestroyWindow(hwnd);
    }

    /* ---- known folder redirection capabilities ---- */
    {
        IKnownFolderManager *mgr;
        IKnownFolder *folder;
        KF_REDIRECTION_CAPABILITIES caps = 0xdead;
        if (SUCCEEDED(CoCreateInstance(&CLSID_KnownFolderManager, NULL, CLSCTX_INPROC_SERVER, &IID_IKnownFolderManager, (void **)&mgr)))
        {
            EXPECT_HR(S_OK, IKnownFolderManager_GetFolder(mgr, &FOLDERID_Windows, &folder));
            EXPECT_HR(S_OK, IKnownFolder_GetRedirectionCapabilities(folder, &caps));
            EXPECT(caps == 0);
            EXPECT_HR(E_INVALIDARG, IKnownFolder_GetRedirectionCapabilities(folder, NULL));
            IKnownFolder_Release(folder);
            IKnownFolderManager_Release(mgr);
        }
        else EXPECT(!"KnownFolderManager");
    }

    /* ---- temporary properties of an item ---- */
    {
        IShellItem *item, *other;
        IShellItem2 *item2;
        IPropertyStore *store;
        PROPVARIANT pv;

        EXPECT_HR(S_OK, SHCreateItemFromParsingName(L"C:\\windows", NULL, &IID_IShellItem, (void **)&item));
        EXPECT_HR(S_OK, SHCreateItemFromParsingName(L"C:\\windows", NULL, &IID_IShellItem, (void **)&other));
        PropVariantInit(&pv);
        pv.vt = VT_I4;
        pv.lVal = 1234;
        EXPECT_HR(S_OK, pSetTemp(item, &test_key, &pv));
        PropVariantInit(&pv);
        EXPECT_HR(S_OK, pGetTemp(item, &test_key, &pv));
        EXPECT(pv.vt == VT_I4 && pv.lVal == 1234);
        PropVariantClear(&pv);
        EXPECT_HR(S_OK, pGetTemp(other, &test_key, &pv));
        EXPECT(pv.vt == VT_EMPTY);
        EXPECT_HR(S_OK, IShellItem_QueryInterface(item, &IID_IShellItem2, (void **)&item2));
        EXPECT_HR(S_OK, IShellItem2_GetPropertyStore(item2, GPS_TEMPORARY, &IID_IPropertyStore, (void **)&store));
        PropVariantInit(&pv);
        EXPECT_HR(S_OK, IPropertyStore_GetValue(store, &test_key, &pv));
        EXPECT(pv.vt == VT_I4 && pv.lVal == 1234);
        IPropertyStore_Release(store);
        EXPECT_HR(E_INVALIDARG, pSetTemp(NULL, &test_key, &pv));
        IShellItem2_Release(item2);
        IShellItem_Release(item);
        IShellItem_Release(other);
    }

    /* ---- IShellImageData::SetEncoderParams ---- */
    {
        IShellImageDataFactory *factory;
        IShellImageData *data;
        bag b = { { &bagvtbl }, 1 };

        if (SUCCEEDED(CoCreateInstance(&CLSID_ShellImageDataFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IShellImageDataFactory, (void **)&factory)))
        {
            EXPECT_HR(S_OK, IShellImageDataFactory_CreateImageFromFile(factory, L"C:\\windows\\nonexistent.png", &data));
            EXPECT_HR(S_OK, IShellImageData_SetEncoderParams(data, &b.IPropertyBag_iface));
            EXPECT(b.ref == 2);
            EXPECT_HR(S_OK, IShellImageData_SetEncoderParams(data, NULL));
            EXPECT(b.ref == 1);
            EXPECT_HR(S_OK, IShellImageData_SetEncoderParams(data, &b.IPropertyBag_iface));
            IShellImageData_Release(data);
            EXPECT(b.ref == 1);
            IShellImageDataFactory_Release(factory);
        }
        else EXPECT(!"ShellImageDataFactory");
    }

    /* ---- the shell view: GetView, property pages, saved view state ---- */
    {
        WNDCLASSW wc = { 0 };
        browser sb = { { &sbvtbl }, NULL, NULL };
        IShellView3 *view;
        HWND parent, vw;
        SHELLVIEWID vid;

        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.lpszClassName = L"fastshell32rcp";
        RegisterClassW(&wc);
        parent = CreateWindowW(L"fastshell32rcp", L"p", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 400, 300, NULL, NULL, wc.hInstance, NULL);
        sb.hwnd = parent;
        view = make_view(&sb, FVM_DETAILS, &vw);
        EXPECT(view != NULL);
        if (view)
        {
            memset(&vid, 0, sizeof(vid));
            EXPECT_HR(S_OK, IShellView3_GetView(view, &vid, (ULONG)-1));
            EXPECT(IsEqualGUID(&vid, &VID_Details));
            EXPECT_HR(E_INVALIDARG, IShellView3_GetView(view, &vid, 5));
            EXPECT_HR(S_OK, IShellView3_GetView(view, &vid, (ULONG)-2));
            EXPECT_HR(S_OK, IShellView3_AddPropertySheetPages(view, 0, NULL, 0));
            EXPECT_HR(S_OK, IShellView3_SaveViewState(view));
            EXPECT(sb.state != NULL);
            IShellView3_DestroyViewWindow(view);
            IShellView3_Release(view);

            /* the next view of the browser starts in the saved mode, not the one it is asked for */
            view = make_view(&sb, FVM_LIST, &vw);
            EXPECT(view != NULL);
            if (view)
            {
                FOLDERSETTINGS fs;
                EXPECT_HR(S_OK, IShellView3_GetCurrentInfo(view, &fs));
                EXPECT(fs.ViewMode == FVM_DETAILS);
                EXPECT_HR(S_OK, IShellView3_GetView(view, &vid, (ULONG)-1));
                EXPECT(IsEqualGUID(&vid, &VID_Details));
                IShellView3_DestroyViewWindow(view);
                IShellView3_Release(view);
            }
        }
        DestroyWindow(parent);
    }

    RemoveDirectoryW(dir);
    printf("checks %d, mismatches %d\nRESULT: %s\n", total, bad, bad ? "FAIL" : "PASS");
    return bad != 0;
}
