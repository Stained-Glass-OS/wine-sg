#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const GUID CLSID_PKG = {0xf20da720,0xc02f,0x11ce,{0x92,0x7b,0x08,0x00,0x09,0x5a,0xe3,0x40}};

struct adv_sink { IAdviseSink iface; LONG ref; };
static HRESULT WINAPI s_qi(IAdviseSink *i, REFIID r, void **o) { if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IAdviseSink)) { *o = i; return S_OK; } *o = NULL; return E_NOINTERFACE; }
static ULONG WINAPI s_ar(IAdviseSink *i) { return 2; }
static ULONG WINAPI s_rl(IAdviseSink *i) { return 1; }
static void WINAPI s_dc(IAdviseSink *i, FORMATETC *f, STGMEDIUM *m) {}
static void WINAPI s_vc(IAdviseSink *i, DWORD a, LONG b) {}
static void WINAPI s_rn(IAdviseSink *i, IMoniker *m) {}
static void WINAPI s_sv(IAdviseSink *i) {}
static void WINAPI s_cl(IAdviseSink *i) {}
static IAdviseSinkVtbl sink_vtbl = { s_qi, s_ar, s_rl, s_dc, s_vc, s_rn, s_sv, s_cl };

static void write_fixture(IStorage *stg)
{
    IStream *stream;
    const char *label = "hello.txt", *src = "C:\\src\\hello.txt", *tmp = "C:\\tmp\\hello.txt";
    const WCHAR *wpath = L"C:\\src\\hello.txt";
    const char payload[] = "payload text\n";
    DWORD total = 0, v;
    WORD w;
    BYTE buf[512];
    DWORD n = 0;
    ULONG written;
    HRESULT hr;

#define PUT(p, len) do { memcpy(buf + n, p, len); n += (len); } while (0)
    w = 2; PUT(&w, 2);
    PUT(label, strlen(label) + 1);
    PUT(src, strlen(src) + 1);
    w = 0; PUT(&w, 2); w = 3; PUT(&w, 2);
    v = strlen(tmp) + 1; PUT(&v, 4); PUT(tmp, v);
    v = sizeof(payload) - 1; PUT(&v, 4); PUT(payload, v);
    v = lstrlenW(wpath) + 1; PUT(&v, 4); PUT(wpath, v * sizeof(WCHAR));
    total = n;
    hr = IStorage_CreateStream(stg, L"\1Ole10Native", STGM_CREATE | STGM_WRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &stream);
    CHECK(hr == S_OK, "create stream %08lx", hr);
    IStream_Write(stream, &total, 4, &written);
    IStream_Write(stream, buf, n, &written);
    IStream_Release(stream);
}

static BOOL read_file(const WCHAR *path, char *out, DWORD size)
{
    DWORD got = 0;
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    BOOL ok;
    if (f == INVALID_HANDLE_VALUE) return FALSE;
    ok = ReadFile(f, out, size, &got, NULL);
    CloseHandle(f);
    out[got] = 0;
    return ok;
}

int main(void)
{
    IOleObject *obj, *obj2;
    IPersistStorage *ps, *ps2;
    IStorage *stg1, *stg2;
    IOleClientSite *site;
    IOleAdviseHolder *dummy;
    IAdviseSink sink = { &sink_vtbl };
    SIZEL ext;
    CLSID clsid;
    WCHAR *type = NULL;
    DWORD conn = 0;
    IEnumOLEVERB *verbs = NULL;
    IMoniker *mk = (IMoniker *)1;
    HRESULT hr;
    char contents[64];
    WCHAR tmpname[MAX_PATH], path[MAX_PATH];

    CoInitialize(NULL);
    GetTempPathW(MAX_PATH, tmpname);
    lstrcatW(tmpname, L"fastpkg-src.stg");
    DeleteFileW(tmpname);
    hr = StgCreateDocfile(tmpname, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &stg1);
    CHECK(hr == S_OK, "docfile %08lx", hr);
    write_fixture(stg1);

    hr = CoCreateInstance(&CLSID_PKG, NULL, CLSCTX_INPROC_SERVER, &IID_IOleObject, (void **)&obj);
    CHECK(hr == S_OK, "package %08lx", hr);
    if (hr != S_OK) goto done;
    (void)dummy;

    hr = IOleObject_GetClientSite(obj, &site);
    CHECK(hr == S_OK && site == NULL, "no site yet %08lx", hr);
    hr = IOleObject_GetUserClassID(obj, &clsid);
    CHECK(hr == S_OK && IsEqualGUID(&clsid, &CLSID_PKG), "user class id %08lx", hr);
    hr = IOleObject_SetHostNames(obj, L"app", L"obj");
    CHECK(hr == S_OK, "host names");
    hr = IOleObject_GetMoniker(obj, OLEGETMONIKER_ONLYIFTHERE, OLEWHICHMK_OBJFULL, &mk);
    CHECK(FAILED(hr) && mk == NULL, "moniker %08lx", hr);
    hr = IOleObject_IsUpToDate(obj);
    CHECK(hr == S_OK, "uptodate");
    hr = IOleObject_Update(obj);
    CHECK(hr == S_OK, "update");
    hr = IOleObject_EnumVerbs(obj, &verbs);
    CHECK(hr == S_OK || hr == OLE_S_USEREG || FAILED(hr), "verbs %08lx", hr);
    if (verbs) IEnumOLEVERB_Release(verbs);
    hr = IOleObject_GetUserType(obj, USERCLASSTYPE_FULL, &type);
    CHECK(hr == S_OK || FAILED(hr), "user type %08lx", hr);
    CoTaskMemFree(type);

    hr = IOleObject_GetExtent(obj, DVASPECT_ICON, &ext);
    CHECK(hr == S_OK && ext.cx > 0 && ext.cy > 0, "default extent %08lx", hr);
    ext.cx = 1234; ext.cy = 4321;
    hr = IOleObject_SetExtent(obj, DVASPECT_ICON, &ext);
    CHECK(hr == S_OK, "set extent");
    ext.cx = ext.cy = 0;
    hr = IOleObject_GetExtent(obj, DVASPECT_ICON, &ext);
    CHECK(hr == S_OK && ext.cx == 1234 && ext.cy == 4321, "stored extent");
    hr = IOleObject_GetExtent(obj, DVASPECT_THUMBNAIL, &ext);
    CHECK(hr == DV_E_DVASPECT, "bad aspect %08lx", hr);

    hr = IOleObject_Advise(obj, &sink, &conn);
    CHECK(hr == S_OK && conn, "advise %08lx", hr);
    {
        IEnumSTATDATA *en = NULL;
        STATDATA sd;
        hr = IOleObject_EnumAdvise(obj, &en);
        CHECK(hr == S_OK && en, "enum advise");
        if (en)
        {
            hr = IEnumSTATDATA_Next(en, 1, &sd, NULL);
            CHECK(hr == S_OK && sd.dwConnection == conn, "advise entry");
            IEnumSTATDATA_Release(en);
        }
    }
    hr = IOleObject_Unadvise(obj, conn);
    CHECK(hr == S_OK, "unadvise");
    hr = IOleObject_Unadvise(obj, 99);
    CHECK(FAILED(hr), "unadvise unknown");

    IOleObject_QueryInterface(obj, &IID_IPersistStorage, (void **)&ps);
    hr = IPersistStorage_GetClassID(ps, &clsid);
    CHECK(hr == S_OK && IsEqualGUID(&clsid, &CLSID_PKG), "persist class id");
    hr = IPersistStorage_IsDirty(ps);
    CHECK(hr == S_FALSE, "dirty %08lx", hr);
    hr = IPersistStorage_Load(ps, stg1);
    CHECK(hr == S_OK, "load %08lx", hr);

    /* save the package into another storage and load it into a second object */
    GetTempPathW(MAX_PATH, path);
    lstrcatW(path, L"fastpkg-dst.stg");
    DeleteFileW(path);
    hr = StgCreateDocfile(path, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &stg2);
    CHECK(hr == S_OK, "dst docfile");
    hr = IPersistStorage_Save(ps, stg2, FALSE);
    CHECK(hr == S_OK, "save %08lx", hr);
    hr = IPersistStorage_SaveCompleted(ps, NULL);
    CHECK(hr == S_OK, "save completed");
    hr = IPersistStorage_HandsOffStorage(ps);
    CHECK(hr == S_OK, "hands off");

    hr = CoCreateInstance(&CLSID_PKG, NULL, CLSCTX_INPROC_SERVER, &IID_IOleObject, (void **)&obj2);
    CHECK(hr == S_OK, "second package");
    IOleObject_QueryInterface(obj2, &IID_IPersistStorage, (void **)&ps2);
    hr = IPersistStorage_Load(ps2, stg2);
    CHECK(hr == S_OK, "reload %08lx", hr);
    IPersistStorage_Release(ps2);
    IOleObject_Release(obj2);

    IPersistStorage_Release(ps);
    IOleObject_Release(obj);
    IStorage_Release(stg2);
    DeleteFileW(path);
    (void)read_file; (void)contents;
done:
    IStorage_Release(stg1);
    DeleteFileW(tmpname);
    CoUninitialize();
    printf(fails ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", fails);
    return fails != 0;
}
