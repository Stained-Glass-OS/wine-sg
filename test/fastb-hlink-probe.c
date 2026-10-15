#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <hlink.h>
#include <stdio.h>

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)
#define OK(e) do { HRESULT h_ = (e); checks++; if (FAILED(h_)) { fails++; printf("FAIL line %d: %s -> %08lx\n", __LINE__, #e, (long)h_); } } while (0)

static HMODULE mod;
#define FN(name) ((void *)GetProcAddress(mod, name))

typedef HRESULT (WINAPI *create_string_t)(LPCWSTR, LPCWSTR, LPCWSTR, IHlinkSite *, DWORD, IUnknown *, REFIID, void **);
typedef HRESULT (WINAPI *create_bc_t)(IUnknown *, REFIID, void **);

/* a data object holding one text format */
struct textobj { IDataObject iface; WCHAR *text; };
static HRESULT WINAPI t_qi(IDataObject *i, REFIID r, void **o) { *o = i; return S_OK; }
static ULONG WINAPI t_ref(IDataObject *i) { return 2; }
static HRESULT WINAPI t_get(IDataObject *i, FORMATETC *f, STGMEDIUM *m)
{
    struct textobj *t = (struct textobj *)i;
    HGLOBAL h;
    if (f->cfFormat != CF_UNICODETEXT) return DV_E_FORMATETC;
    h = GlobalAlloc(GMEM_MOVEABLE, (lstrlenW(t->text) + 1) * sizeof(WCHAR));
    lstrcpyW(GlobalLock(h), t->text);
    GlobalUnlock(h);
    m->tymed = TYMED_HGLOBAL;
    m->hGlobal = h;
    m->pUnkForRelease = NULL;
    return S_OK;
}
static HRESULT WINAPI t_here(IDataObject *i, FORMATETC *f, STGMEDIUM *m) { return E_NOTIMPL; }
static HRESULT WINAPI t_query(IDataObject *i, FORMATETC *f) { return f->cfFormat == CF_UNICODETEXT ? S_OK : DV_E_FORMATETC; }
static HRESULT WINAPI t_canon(IDataObject *i, FORMATETC *a, FORMATETC *b) { return E_NOTIMPL; }
static HRESULT WINAPI t_set(IDataObject *i, FORMATETC *f, STGMEDIUM *m, BOOL r) { return E_NOTIMPL; }
static HRESULT WINAPI t_enum(IDataObject *i, DWORD d, IEnumFORMATETC **e) { return E_NOTIMPL; }
static HRESULT WINAPI t_adv(IDataObject *i, FORMATETC *f, DWORD a, IAdviseSink *s, DWORD *c) { return E_NOTIMPL; }
static HRESULT WINAPI t_unadv(IDataObject *i, DWORD c) { return E_NOTIMPL; }
static HRESULT WINAPI t_enumadv(IDataObject *i, IEnumSTATDATA **e) { return E_NOTIMPL; }
static IDataObjectVtbl tvtbl = { t_qi, t_ref, t_ref, t_get, t_here, t_query, t_canon, t_set, t_enum, t_adv, t_unadv, t_enumadv };

int main(void)
{
    create_string_t create_from_string;
    create_bc_t create_bc;
    HRESULT (WINAPI *create_from_data)(IDataObject *, IHlinkSite *, DWORD, IUnknown *, REFIID, void **);
    HRESULT (WINAPI *query_from_data)(IDataObject *);
    HRESULT (WINAPI *translate)(LPCWSTR, DWORD, LPWSTR *);
    HRESULT (WINAPI *set_special)(ULONG, LPCWSTR);
    HRESULT (WINAPI *get_special)(ULONG, LPWSTR *);
    HRESULT (WINAPI *create_shortcut_string)(DWORD, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPWSTR *, DWORD);
    HRESULT (WINAPI *resolve_string)(LPCWSTR, LPWSTR *, LPWSTR *);
    HRESULT (WINAPI *resolve_shortcut)(LPCWSTR, IHlinkSite *, DWORD, IUnknown *, REFIID, void **);
    HRESULT (WINAPI *create_shortcut)(DWORD, IHlink *, LPCWSTR, LPCWSTR, LPWSTR *, DWORD);
    HRESULT (WINAPI *save_ex)(IUnknown *, IStream *, BOOL);
    IHlink *hl = NULL, *hl2 = NULL;
    IDataObject *data = NULL;
    IHlinkBrowseContext *bc = NULL, *bc2 = NULL;
    IMoniker *mk1 = NULL, *mk2 = NULL, *mk3 = NULL;
    WCHAR *tgt, *loc, tmp[MAX_PATH], dir[MAX_PATH], *path = NULL;
    FORMATETC fmt;
    STGMEDIUM med;
    HRESULT hr;
    IEnumHLITEM *eh = NULL;
    HLITEM item;
    ULONG fetched, id1, id2, id3;
    struct textobj to;

    OleInitialize(NULL);
    mod = LoadLibraryA("hlink.dll");
    CHECK(mod != NULL);
    create_from_string = FN("HlinkCreateFromString");
    create_bc = FN("HlinkCreateBrowseContext");
    create_from_data = FN("HlinkCreateFromData");
    query_from_data = FN("HlinkQueryCreateFromData");
    translate = FN("HlinkTranslateURL");
    set_special = FN("HlinkSetSpecialReference");
    get_special = FN("HlinkGetSpecialReference");
    create_shortcut_string = FN("HlinkCreateShortcutFromString");
    resolve_string = FN("HlinkResolveShortcutToString");
    resolve_shortcut = FN("HlinkResolveShortcut");
    create_shortcut = FN("HlinkCreateShortcut");
    save_ex = FN("OleSaveToStreamEx");
    CHECK(create_from_string && create_bc && create_from_data && query_from_data && translate && set_special &&
          create_shortcut_string && resolve_string && resolve_shortcut && create_shortcut && save_ex);

    /* hyperlink as a data object */
    OK(create_from_string(L"http://www.winehq.org/", L"loc", L"Wine HQ", NULL, 0, NULL, &IID_IHlink, (void **)&hl));
    OK(IHlink_QueryInterface(hl, &IID_IDataObject, (void **)&data));
    fmt.cfFormat = RegisterClipboardFormatW(L"Hyperlink");
    fmt.ptd = NULL; fmt.dwAspect = DVASPECT_CONTENT; fmt.lindex = -1; fmt.tymed = TYMED_HGLOBAL;
    OK(IDataObject_QueryGetData(data, &fmt));
    fmt.cfFormat = CF_TEXT;
    CHECK(IDataObject_QueryGetData(data, &fmt) != S_OK);
    fmt.cfFormat = RegisterClipboardFormatW(L"Hyperlink");
    memset(&med, 0, sizeof(med));
    OK(IDataObject_GetData(data, &fmt, &med));
    CHECK(med.tymed == TYMED_HGLOBAL && GlobalSize(med.hGlobal) > 40);
    ReleaseStgMedium(&med);
    {
        IEnumFORMATETC *ef = NULL;
        FORMATETC f2;
        ULONG n = 0;
        OK(IDataObject_EnumFormatEtc(data, DATADIR_GET, &ef));
        OK(IEnumFORMATETC_Next(ef, 1, &f2, &n));
        CHECK(n == 1 && f2.cfFormat == fmt.cfFormat);
        CHECK(IEnumFORMATETC_Next(ef, 1, &f2, &n) == S_FALSE);
        IEnumFORMATETC_Release(ef);
    }
    OK(query_from_data(data));
    OK(create_from_data(data, NULL, 0, NULL, &IID_IHlink, (void **)&hl2));
    OK(IHlink_GetStringReference(hl2, HLINKGETREF_DEFAULT, &tgt, &loc));
    CHECK(tgt && !wcscmp(tgt, L"http://www.winehq.org/"));
    CHECK(loc && !wcscmp(loc, L"loc"));
    CoTaskMemFree(tgt); CoTaskMemFree(loc);
    OK(IHlink_GetFriendlyName(hl2, 0, &tgt));
    CHECK(tgt && !wcscmp(tgt, L"Wine HQ"));
    CoTaskMemFree(tgt);
    IHlink_Release(hl2);
    IDataObject_Release(data);

    /* from a text data object */
    to.iface.lpVtbl = &tvtbl;
    to.text = (WCHAR *)L"http://example.org/page#frag";
    OK(query_from_data(&to.iface));
    OK(create_from_data(&to.iface, NULL, 0, NULL, &IID_IHlink, (void **)&hl2));
    OK(IHlink_GetStringReference(hl2, HLINKGETREF_DEFAULT, &tgt, &loc));
    CHECK(tgt && !wcscmp(tgt, L"http://example.org/page"));
    CHECK(loc && !wcscmp(loc, L"frag"));
    CoTaskMemFree(tgt); CoTaskMemFree(loc);
    IHlink_Release(hl2);
    /* URL translation */
    OK(translate(L"www.winehq.org", 0, &tgt));
    CHECK(tgt && !wcsncmp(tgt, L"http://", 7));
    CoTaskMemFree(tgt);
    hr = translate(L"http://www.winehq.org/", 0, &tgt);
    CHECK(hr == S_FALSE && tgt == NULL);

    /* special reference */
    OK(set_special(HLSR_HOME, L"http://start.example/"));
    OK(get_special(HLSR_HOME, &tgt));
    CHECK(tgt && !wcscmp(tgt, L"http://start.example/"));
    CoTaskMemFree(tgt);
    CHECK(set_special(77, L"x") == E_INVALIDARG);

    /* shortcut files */
    GetTempPathW(MAX_PATH, dir);
    OK(create_shortcut_string(0, L"http://www.winehq.org/index", L"top", dir, L"fastb-test", &path, 0));
    CHECK(path != NULL && GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES);
    OK(resolve_string(path, &tgt, &loc));
    CHECK(tgt && !wcscmp(tgt, L"http://www.winehq.org/index"));
    CHECK(loc && !wcscmp(loc, L"top"));
    CoTaskMemFree(tgt); CoTaskMemFree(loc);
    OK(resolve_shortcut(path, NULL, 0, NULL, &IID_IHlink, (void **)&hl2));
    OK(IHlink_GetStringReference(hl2, HLINKGETREF_DEFAULT, &tgt, &loc));
    CHECK(tgt && !wcscmp(tgt, L"http://www.winehq.org/index"));
    CoTaskMemFree(tgt); CoTaskMemFree(loc);
    IHlink_Release(hl2);
    {
        WCHAR *path2 = NULL;
        /* same name again: a new file */
        OK(create_shortcut_string(0, L"http://www.winehq.org/index", NULL, dir, L"fastb-test", &path2, 0));
        CHECK(path2 && wcscmp(path, path2));
        DeleteFileW(path2);
        CoTaskMemFree(path2);
        path2 = NULL;
        OK(create_shortcut_string(HLSHORTCUTF_DONTACTUALLYCREATE, L"http://x.example/", NULL, dir, L"nofile", &path2, 0));
        CHECK(path2 && GetFileAttributesW(path2) == INVALID_FILE_ATTRIBUTES);
        CoTaskMemFree(path2);
    }
    DeleteFileW(path);
    CoTaskMemFree(path);
    CHECK(FAILED(resolve_string(L"C:\\nonexistent\\x.url", &tgt, &loc)));
    OK(create_shortcut(HLSHORTCUTF_DONTACTUALLYCREATE | HLSHORTCUTF_USEFILENAMEFROMFRIENDLYNAME, hl, dir, NULL, &path, 0));
    CHECK(path && wcsstr(path, L"Wine HQ"));
    CoTaskMemFree(path);

    /* OleSaveToStreamEx */
    {
        IStream *stm;
        ULARGE_INTEGER pos;
        LARGE_INTEGER zero = {{0}};
        OK(CreateStreamOnHGlobal(NULL, TRUE, &stm));
        OK(save_ex((IUnknown *)hl, stm, FALSE));
        IStream_Seek(stm, zero, STREAM_SEEK_CUR, &pos);
        CHECK(pos.QuadPart > 16);
        IStream_Release(stm);
    }
    IHlink_Release(hl);

    /* browse context */
    OK(create_bc(NULL, &IID_IHlinkBrowseContext, (void **)&bc));
    OK(CreateFileMoniker(L"C:\\a.txt", &mk1));
    OK(CreateFileMoniker(L"C:\\b.txt", &mk2));
    OK(CreateFileMoniker(L"C:\\c.txt", &mk3));
    OK(IHlinkBrowseContext_SetInitialHlink(bc, mk1, L"l1", L"First"));
    OK(IHlinkBrowseContext_OnNavigateHlink(bc, 0, mk2, L"l2", L"Second", &id2));
    OK(IHlinkBrowseContext_OnNavigateHlink(bc, 0, mk3, L"l3", L"Third", &id3));
    OK(IHlinkBrowseContext_GetHlink(bc, HLID_STACKBOTTOM, &hl));
    OK(IHlink_GetFriendlyName(hl, 0, &tgt));
    CHECK(!wcscmp(tgt, L"First"));
    CoTaskMemFree(tgt);
    IHlink_Release(hl);
    OK(IHlinkBrowseContext_GetHlink(bc, HLID_CURRENT, &hl));
    OK(IHlink_GetFriendlyName(hl, 0, &tgt));
    CHECK(!wcscmp(tgt, L"Third"));
    CoTaskMemFree(tgt);
    IHlink_Release(hl);
    OK(IHlinkBrowseContext_QueryHlink(bc, HLQF_ISCURRENT, id3));
    CHECK(IHlinkBrowseContext_QueryHlink(bc, HLQF_ISCURRENT, id2) == S_FALSE);
    CHECK(IHlinkBrowseContext_QueryHlink(bc, HLQF_ISVALID, 12345) == S_FALSE);
    OK(IHlinkBrowseContext_EnumNavigationStack(bc, 0, 0, &eh));
    {
        int n = 0;
        while (IEnumHLITEM_Next(eh, 1, &item, &fetched) == S_OK)
        {
            n++;
            CoTaskMemFree(item.pwzFriendlyName);
        }
        CHECK(n == 3);
    }
    IEnumHLITEM_Release(eh);
    /* back: moves the current entry */
    id1 = 0;
    OK(IHlinkBrowseContext_OnNavigateHlink(bc, HLNF_NAVIGATINGBACK, NULL, NULL, NULL, &id1));
    CHECK(id1 == id2);
    OK(IHlinkBrowseContext_QueryHlink(bc, HLQF_ISCURRENT, id2));
    /* a new navigation from the middle drops the forward entries */
    OK(IHlinkBrowseContext_OnNavigateHlink(bc, 0, mk3, L"l4", L"Fourth", &id3));
    OK(IHlinkBrowseContext_EnumNavigationStack(bc, 0, 0, &eh));
    {
        int n = 0;
        while (IEnumHLITEM_Next(eh, 1, &item, &fetched) == S_OK) { n++; CoTaskMemFree(item.pwzFriendlyName); }
        CHECK(n == 3);
    }
    IEnumHLITEM_Release(eh);
    /* clone */
    OK(IHlinkBrowseContext_Clone(bc, NULL, &IID_IHlinkBrowseContext, (IUnknown **)&bc2));
    OK(IHlinkBrowseContext_GetHlink(bc2, HLID_CURRENT, &hl));
    OK(IHlink_GetFriendlyName(hl, 0, &tgt));
    CHECK(!wcscmp(tgt, L"Fourth"));
    CoTaskMemFree(tgt);
    IHlink_Release(hl);
    CHECK(IHlinkBrowseContext_Close(bc2, 1) == E_INVALIDARG);
    OK(IHlinkBrowseContext_Close(bc2, 0));
    CHECK(FAILED(IHlinkBrowseContext_GetHlink(bc2, HLID_CURRENT, &hl)));
    OK(IHlinkBrowseContext_GetHlink(bc, HLID_CURRENT, &hl));    /* the original keeps its stack */
    IHlink_Release(hl);
    IHlinkBrowseContext_Release(bc2);
    IHlinkBrowseContext_Release(bc);
    IMoniker_Release(mk1); IMoniker_Release(mk2); IMoniker_Release(mk3);

    printf("%d checks, %d failures\n", checks, fails);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
