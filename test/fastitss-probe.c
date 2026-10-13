/* itss: the storage and stream of a CHM file, its moniker, the protocol info */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <urlmon.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define VT(obj) (*(void ***)(obj))
#define CALL(obj, idx, ret, ...) (((ret (WINAPI *)(void *, ...))VT(obj)[idx])(obj, ##__VA_ARGS__))

static const GUID CLSID_ITStorage_ = {0x5d02926a, 0x212e, 0x11d0, {0x9d, 0xf9, 0x00, 0xa0, 0xc9, 0x22, 0xe6, 0xec}};
static const GUID IID_IITStorage_ = {0x88cc31de, 0x27ab, 0x11d0, {0x9d, 0xf9, 0x00, 0xa0, 0xc9, 0x22, 0xe6, 0xec}};
static const GUID CLSID_MSITStore_ = {0x9d148290, 0xb9c8, 0x11d0, {0xa4, 0xcc, 0x00, 0x00, 0xf8, 0x01, 0x49, 0xf6}};
static const GUID CLSID_ITSProtocol_ = {0x9d148291, 0xb9c8, 0x11d0, {0xa4, 0xcc, 0x00, 0x00, 0xf8, 0x01, 0x49, 0xf6}};

int main(void)
{
    void *its = NULL;
    IStorage *stg = NULL, *dest = NULL;
    IStream *stm = NULL, *clone = NULL, *mem = NULL;
    IEnumSTATSTG *en = NULL;
    IParseDisplayName *parser = NULL;
    IMoniker *mk = NULL, *mk2 = NULL;
    IInternetProtocolInfo *info = NULL;
    STATSTG st[2], one;
    HANDLE f;
    FILETIME ft = { 0x12345678, 0x01d00000 }, got;
    ULONG n, eaten;
    ULARGE_INTEGER big, rd, wr;
    LARGE_INTEGER zero = {{0}};
    WCHAR *name;
    DWORD h1, h2, sys;
    char buf[64];
    HRESULT hr;

    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_ITStorage_, NULL, CLSCTX_INPROC_SERVER, &IID_IITStorage_, &its);
    CHECK(hr == S_OK, "create ITStorage %lx", hr);
    if (!its) return 1;

    /* StgIsStorageFile, Compact, StgSetTimes */
    hr = CALL(its, 5, HRESULT, L"C:\\data.chm");
    CHECK(hr == S_OK, "StgIsStorageFile(chm) %lx", hr);
    hr = CALL(its, 5, HRESULT, L"C:\\notchm.txt");
    CHECK(hr == S_FALSE, "StgIsStorageFile(text) %lx", hr);
    hr = CALL(its, 5, HRESULT, L"C:\\missing.chm");
    CHECK(hr == STG_E_FILENOTFOUND, "StgIsStorageFile(missing) %lx", hr);
    hr = CALL(its, 12, HRESULT, L"C:\\data.chm", 0);
    CHECK(hr == S_OK, "Compact %lx", hr);
    hr = CALL(its, 12, HRESULT, L"C:\\notchm.txt", 0);
    CHECK(hr == STG_E_INVALIDHEADER, "Compact(text) %lx", hr);
    hr = CALL(its, 9, HRESULT, L"C:\\data.chm", NULL, NULL, &ft);
    CHECK(hr == S_OK, "StgSetTimes %lx", hr);
    f = CreateFileW(L"C:\\data.chm", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    GetFileTime(f, NULL, NULL, &got);
    CloseHandle(f);
    CHECK(got.dwLowDateTime == ft.dwLowDateTime && got.dwHighDateTime == ft.dwHighDateTime, "mtime %08lx%08lx", got.dwHighDateTime, got.dwLowDateTime);

    /* the storage */
    hr = CALL(its, 7, HRESULT, L"C:\\data.chm", NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, NULL, 0, &stg);
    CHECK(hr == S_OK && stg, "StgOpenStorage %lx", hr);
    if (!stg) return 1;
    hr = IStorage_Stat(stg, &one, 0);
    CHECK(hr == S_OK && one.type == STGTY_STORAGE && one.pwcsName && !wcscmp(one.pwcsName, L"/"), "storage Stat %lx %ls", hr, one.pwcsName);
    CoTaskMemFree(one.pwcsName);
    hr = IStorage_Stat(stg, &one, STATFLAG_NONAME);
    CHECK(hr == S_OK && !one.pwcsName, "storage Stat noname");
    hr = IStorage_CreateStream(stg, L"new", STGM_WRITE, 0, 0, &stm);
    CHECK(hr == STG_E_ACCESSDENIED && !stm, "CreateStream %lx", hr);
    hr = IStorage_CreateStorage(stg, L"new", STGM_WRITE, 0, 0, &dest);
    CHECK(hr == STG_E_ACCESSDENIED, "CreateStorage %lx", hr);
    hr = IStorage_DestroyElement(stg, L"blank.html");
    CHECK(hr == STG_E_ACCESSDENIED, "DestroyElement %lx", hr);
    hr = IStorage_Commit(stg, 0);
    CHECK(hr == S_OK, "Commit %lx", hr);
    hr = IStorage_EnumElements(stg, 0, NULL, 0, &en);
    CHECK(hr == S_OK && en, "EnumElements %lx", hr);
    if (en)
    {
        IEnumSTATSTG *en2 = NULL;
        n = 0;
        memset(st, 0, sizeof(st));
        hr = IEnumSTATSTG_Next(en, 2, st, &n);
        CHECK(hr == S_OK && n == 2 && st[0].pwcsName && st[1].pwcsName && wcscmp(st[0].pwcsName, st[1].pwcsName), "Next(2) %lx %lu", hr, n);
        CoTaskMemFree(st[0].pwcsName); CoTaskMemFree(st[1].pwcsName);
        hr = IEnumSTATSTG_Clone(en, &en2);
        CHECK(hr == S_OK && en2, "enum Clone %lx", hr);
        if (en2)
        {
            STATSTG a, b;
            memset(&a, 0, sizeof(a)); memset(&b, 0, sizeof(b));
            IEnumSTATSTG_Next(en, 1, &a, &n);
            IEnumSTATSTG_Next(en2, 1, &b, &n);
            CHECK(a.pwcsName && b.pwcsName && !wcscmp(a.pwcsName, b.pwcsName), "clone at the same place");
            CoTaskMemFree(a.pwcsName); CoTaskMemFree(b.pwcsName);
            IEnumSTATSTG_Release(en2);
        }
        IEnumSTATSTG_Release(en);
    }

    /* the stream */
    hr = IStorage_OpenStream(stg, L"/blank.html", NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &stm);
    CHECK(hr == S_OK && stm, "OpenStream %lx", hr);
    if (stm)
    {
        ULONG len, read = 0;
        hr = IStream_Stat(stm, &one, 0);
        CHECK(hr == S_OK && one.pwcsName && !wcscmp(one.pwcsName, L"blank.html"), "stream Stat name %ls", one.pwcsName);
        len = one.cbSize.LowPart;
        CoTaskMemFree(one.pwcsName);
        hr = IStream_Write(stm, "x", 1, &n);
        CHECK(hr == STG_E_ACCESSDENIED, "Write %lx", hr);
        big.QuadPart = 5;
        hr = IStream_SetSize(stm, big);
        CHECK(hr == STG_E_ACCESSDENIED, "SetSize %lx", hr);
        hr = IStream_LockRegion(stm, big, big, LOCK_WRITE);
        CHECK(hr == STG_E_INVALIDFUNCTION, "LockRegion %lx", hr);
        hr = IStream_Commit(stm, 0);
        CHECK(hr == S_OK, "stream Commit %lx", hr);
        IStream_Read(stm, buf, 4, &read);
        hr = IStream_Clone(stm, &clone);
        CHECK(hr == S_OK && clone, "stream Clone %lx", hr);
        if (clone)
        {
            ULARGE_INTEGER pos;
            IStream_Seek(clone, zero, STREAM_SEEK_CUR, &pos);
            CHECK(pos.QuadPart == 4, "clone position %lu", (ULONG)pos.QuadPart);
            IStream_Release(clone);
        }
        CreateStreamOnHGlobal(NULL, TRUE, &mem);
        big.QuadPart = ~0ull;
        hr = IStream_CopyTo(stm, mem, big, &rd, &wr);
        CHECK(hr == S_OK && rd.QuadPart == len - 4 && wr.QuadPart == len - 4, "CopyTo %lx %lu %lu (len %lu)", hr, (ULONG)rd.QuadPart, (ULONG)wr.QuadPart, len);
        IStream_Release(mem);
        IStream_Release(stm);
    }

    /* a copy of the whole storage into a compound file */
    hr = StgCreateDocfile(NULL, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE | STGM_DELETEONRELEASE, 0, &dest);
    CHECK(hr == S_OK, "StgCreateDocfile %lx", hr);
    if (dest)
    {
        hr = IStorage_CopyTo(stg, 0, NULL, NULL, dest);
        CHECK(hr == S_OK, "IStorage::CopyTo %lx", hr);
        hr = IStorage_OpenStream(dest, L"blank.html", NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &stm);
        CHECK(hr == S_OK, "copied stream %lx", hr);
        if (stm) IStream_Release(stm);
        IStorage_Release(dest);
    }
    IStorage_Release(stg);

    /* the moniker */
    hr = CoCreateInstance(&CLSID_MSITStore_, NULL, CLSCTX_INPROC_SERVER, &IID_IParseDisplayName, (void **)&parser);
    CHECK(hr == S_OK, "create MSITStore %lx", hr);
    if (parser)
    {
        hr = IParseDisplayName_ParseDisplayName(parser, NULL, (WCHAR *)L"@MSITStore:C:\\data.chm::/blank.html", &eaten, &mk);
        CHECK(hr == S_OK && mk, "ParseDisplayName %lx", hr);
        hr = IParseDisplayName_ParseDisplayName(parser, NULL, (WCHAR *)L"@MSITStore:C:\\other.chm::/a/b.html", &eaten, &mk2);
        if (mk && mk2)
        {
            IPersistStream *ps = NULL;
            IStream *ms = NULL;
            ULARGE_INTEGER size;

            hr = IMoniker_GetDisplayName(mk, NULL, NULL, &name);
            CHECK(hr == S_OK && !wcscmp(name, L"ms-its:C:\\data.chm::/blank.html"), "GetDisplayName %lx %ls", hr, name);
            CoTaskMemFree(name);
            hr = IMoniker_IsEqual(mk, mk2);
            CHECK(hr == S_FALSE, "IsEqual(different) %lx", hr);
            hr = IMoniker_IsDirty(mk);
            CHECK(hr == S_FALSE, "IsDirty %lx", hr);
            hr = IMoniker_QueryInterface(mk, &IID_IPersistStream, (void **)&ps);
            CHECK(hr == S_OK, "QI IPersistStream %lx", hr);
            if (ps) IPersistStream_Release(ps);
            hr = IMoniker_GetSizeMax(mk, &size);
            CHECK(hr == S_OK && size.QuadPart == 2 * 4 + (wcslen(L"C:\\data.chm") + wcslen(L"/blank.html")) * 2, "GetSizeMax %lx", hr);
            CreateStreamOnHGlobal(NULL, TRUE, &ms);
            hr = IMoniker_Save(mk, ms, FALSE);
            CHECK(hr == S_OK, "Save %lx", hr);
            IStream_Seek(ms, zero, STREAM_SEEK_SET, NULL);
            hr = IMoniker_Load(mk2, ms);
            CHECK(hr == S_OK, "Load %lx", hr);
            IStream_Release(ms);
            hr = IMoniker_IsEqual(mk, mk2);
            CHECK(hr == S_OK, "IsEqual after Load %lx", hr);
            IMoniker_Hash(mk, &h1); IMoniker_Hash(mk2, &h2);
            CHECK(h1 == h2 && h1, "Hash %08lx %08lx", h1, h2);
            hr = IMoniker_IsSystemMoniker(mk, &sys);
            CHECK(hr == S_FALSE && sys == MKSYS_NONE, "IsSystemMoniker %lx", hr);
            {
                IMoniker *inv = (IMoniker *)1, *red = NULL, *left = NULL;
                hr = IMoniker_Inverse(mk, &inv);
                CHECK(hr == MK_E_NOINVERSE && !inv, "Inverse %lx", hr);
                hr = IMoniker_Reduce(mk, NULL, MKRREDUCE_ALL, &left, &red);
                CHECK(hr == MK_S_REDUCED_TO_SELF && red == mk, "Reduce %lx", hr);
                if (red) IMoniker_Release(red);
            }
            hr = IMoniker_BindToStorage(mk, NULL, NULL, &IID_IStream, (void **)&stm);
            CHECK(hr == S_OK && stm, "BindToStorage %lx", hr);
            if (stm) IStream_Release(stm);
            hr = IMoniker_BindToObject(mk, NULL, NULL, &IID_IStream, (void **)&stm);
            CHECK(hr == S_OK && stm, "BindToObject %lx", hr);
            if (stm) IStream_Release(stm);
        }
        if (mk) IMoniker_Release(mk);
        if (mk2) IMoniker_Release(mk2);
        IParseDisplayName_Release(parser);
    }

    /* the protocol */
    hr = CoCreateInstance(&CLSID_ITSProtocol_, NULL, CLSCTX_INPROC_SERVER, &IID_IInternetProtocolInfo, (void **)&info);
    CHECK(hr == S_OK, "create protocol %lx", hr);
    if (info)
    {
        DWORD value = 7, size = 0;
        WCHAR out[64];

        hr = IInternetProtocolInfo_CompareUrl(info, L"its:a.chm::/x.html", L"ITS:A.CHM::/X.HTML", 0);
        CHECK(hr == S_OK, "CompareUrl equal %lx", hr);
        hr = IInternetProtocolInfo_CompareUrl(info, L"its:a.chm::/x.html", L"its:b.chm::/x.html", 0);
        CHECK(hr == S_FALSE, "CompareUrl different %lx", hr);
        hr = IInternetProtocolInfo_QueryInfo(info, L"its:a.chm::/x.html", QUERY_USES_NETWORK, 0, &value, sizeof(value), &size, 0);
        CHECK(hr == S_OK && value == 0 && size == sizeof(DWORD), "QueryInfo %lx %lu", hr, value);
        hr = IInternetProtocolInfo_ParseUrl(info, L"its:a.chm::/x.html", PARSE_CANONICALIZE, 0, out, 64, &size, 0);
        CHECK(hr == S_OK && !wcscmp(out, L"its:a.chm::/x.html") && size == 19, "ParseUrl %lx %ls %lu", hr, out, size);
        hr = IInternetProtocolInfo_ParseUrl(info, L"its:a.chm::/x.html", PARSE_CANONICALIZE, 0, out, 4, &size, 0);
        CHECK(hr == E_OUTOFMEMORY && size == 19, "ParseUrl small %lx", hr);
        IInternetProtocolInfo_Release(info);
    }
    CALL(its, 2, ULONG);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails;
}
