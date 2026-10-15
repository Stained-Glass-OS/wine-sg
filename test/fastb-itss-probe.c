#define COBJMACROS
/* itss gate: IITStorage on an ILockBytes (open/IsStorage*) compared with the file based path */
#include <windows.h>
#include <objbase.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(c, ...) do { if (c) printf("PASS  %s\n", #c); else { failures++; printf("FAIL  %s: ", #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef struct IITStorageX IITStorageX;
typedef struct
{
    HRESULT (WINAPI *QueryInterface)(IITStorageX *, REFIID, void **);
    ULONG (WINAPI *AddRef)(IITStorageX *);
    ULONG (WINAPI *Release)(IITStorageX *);
    HRESULT (WINAPI *StgCreateDocfile)(IITStorageX *, const WCHAR *, DWORD, DWORD, IStorage **);
    HRESULT (WINAPI *StgCreateDocfileOnILockBytes)(IITStorageX *, ILockBytes *, DWORD, DWORD, IStorage **);
    HRESULT (WINAPI *StgIsStorageFile)(IITStorageX *, const WCHAR *);
    HRESULT (WINAPI *StgIsStorageILockBytes)(IITStorageX *, ILockBytes *);
    HRESULT (WINAPI *StgOpenStorage)(IITStorageX *, const WCHAR *, IStorage *, DWORD, SNB, DWORD, IStorage **);
    HRESULT (WINAPI *StgOpenStorageOnILockBytes)(IITStorageX *, ILockBytes *, IStorage *, DWORD, SNB, DWORD, IStorage **);
} ITVtbl;
struct IITStorageX { const ITVtbl *v; };

static const GUID CLSID_ITStorageX = { 0x5d02926a, 0x212e, 0x11d0, { 0x9d, 0xf9, 0x00, 0xa0, 0xc9, 0x22, 0xe6, 0xec } };
static const GUID IID_IITStorageX = { 0x88cc31de, 0x27ab, 0x11d0, { 0x9d, 0xf9, 0x00, 0xa0, 0xc9, 0x22, 0xe6, 0xec } };

static ILockBytes *make_lockbytes(const void *data, DWORD len)
{
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, len);
    ILockBytes *lb = NULL;
    void *p = GlobalLock(h);
    memcpy(p, data, len);
    GlobalUnlock(h);
    CreateILockBytesOnHGlobal(h, TRUE, &lb);
    return lb;
}

static ULONG refcount(IUnknown *u) { IUnknown_AddRef(u); return IUnknown_Release(u); }

static ULONG read_stream(IStorage *stg, const WCHAR *name, BYTE *buf, ULONG max, HRESULT *hr)
{
    IStream *s = NULL;
    ULONG got = 0;
    *hr = IStorage_OpenStream(stg, name, NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &s);
    if (FAILED(*hr)) return 0;
    IStream_Read(s, buf, max, &got);
    IStream_Release(s);
    return got;
}

static int count_elements(IStorage *stg)
{
    IEnumSTATSTG *en = NULL;
    STATSTG st;
    ULONG n;
    int count = 0;
    if (FAILED(IStorage_EnumElements(stg, 0, NULL, 0, &en))) return -1;
    while (IEnumSTATSTG_Next(en, 1, &st, &n) == S_OK && n)
    {
        count++;
        CoTaskMemFree(st.pwcsName);
    }
    IEnumSTATSTG_Release(en);
    return count;
}

int main(void)
{
    IITStorageX *its = NULL;
    HANDLE f;
    BYTE *data;
    DWORD size, got;
    ILockBytes *lb, *junk;
    IStorage *stg = NULL, *fstg = NULL;
    BYTE buf1[4096], buf2[4096];
    HRESULT hr, hr2;
    ULONG n1, n2, before, after;
    WCHAR cwd[MAX_PATH], path[MAX_PATH];

    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_ITStorageX, NULL, CLSCTX_INPROC_SERVER, &IID_IITStorageX, (void **)&its);
    CHECK(hr == S_OK && its, "CoCreateInstance(ITStorage) %#lx", hr);
    if (!its) return 1;

    f = CreateFileW(L"data.chm", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    size = GetFileSize(f, NULL);
    data = malloc(size);
    ReadFile(f, data, size, &got, NULL);
    CloseHandle(f);
    GetCurrentDirectoryW(MAX_PATH, cwd);
    swprintf(path, MAX_PATH, L"%ls\\data.chm", cwd);

    lb = make_lockbytes(data, size);
    junk = make_lockbytes("this is not a chm archive", 26);

    hr = its->v->StgIsStorageILockBytes(its, lb);
    CHECK(hr == S_OK, "IsStorageILockBytes(chm) %#lx", hr);
    hr = its->v->StgIsStorageILockBytes(its, junk);
    CHECK(hr == S_FALSE, "IsStorageILockBytes(junk) %#lx", hr);
    hr = its->v->StgIsStorageILockBytes(its, NULL);
    CHECK(hr == STG_E_INVALIDPOINTER, "IsStorageILockBytes(NULL) %#lx", hr);
    hr = its->v->StgIsStorageFile(its, path);
    CHECK(hr == S_OK, "IsStorageFile(chm) %#lx", hr);
    hr = its->v->StgIsStorageFile(its, L"C:\\nonexistent\\file.chm");
    CHECK(hr == STG_E_FILENOTFOUND, "IsStorageFile(missing) %#lx", hr);
    CreateDirectoryW(L"C:\\sgitss", NULL);
    f = CreateFileW(L"C:\\sgitss\\junk.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(f, "plain text", 10, &got, NULL);
    CloseHandle(f);
    hr = its->v->StgIsStorageFile(its, L"C:\\sgitss\\junk.txt");
    CHECK(hr == S_FALSE, "IsStorageFile(text) %#lx", hr);

    hr = its->v->StgOpenStorageOnILockBytes(its, junk, NULL, STGM_READ, NULL, 0, &stg);
    CHECK(FAILED(hr) && !stg, "open on junk lockbytes: %#lx", hr);
    hr = its->v->StgOpenStorageOnILockBytes(its, NULL, NULL, STGM_READ, NULL, 0, &stg);
    CHECK(hr == STG_E_INVALIDPOINTER, "open on NULL lockbytes: %#lx", hr);

    before = refcount((IUnknown *)lb);
    hr = its->v->StgOpenStorageOnILockBytes(its, lb, NULL, STGM_READ | STGM_SHARE_DENY_WRITE, NULL, 0, &stg);
    CHECK(hr == S_OK && stg, "open on lockbytes %#lx", hr);
    hr2 = its->v->StgOpenStorage(its, path, NULL, STGM_READ | STGM_SHARE_DENY_WRITE, NULL, 0, &fstg);
    CHECK(hr2 == S_OK && fstg, "open on file %#lx", hr2);
    if (stg && fstg)
    {
        n1 = read_stream(stg, L"/blank.html", buf1, sizeof(buf1), &hr);
        n2 = read_stream(fstg, L"/blank.html", buf2, sizeof(buf2), &hr2);
        CHECK(hr == S_OK && hr2 == S_OK && n1 && n1 == n2 && !memcmp(buf1, buf2, n1), "stream contents differ: %lu/%lu %#lx", n1, n2, hr);
        CHECK(count_elements(stg) == count_elements(fstg) && count_elements(stg) > 0, "element counts %d vs %d", count_elements(stg), count_elements(fstg));
        n1 = read_stream(stg, L"/nosuchfile", buf1, sizeof(buf1), &hr);
        CHECK(FAILED(hr), "missing stream opened");
        IStorage_Release(fstg);
    }
    if (stg)
    {
        IStorage *copy = NULL;
        hr = its->v->StgCreateDocfileOnILockBytes(its, lb, STGM_READ, 0, &copy);
        CHECK(hr == S_OK && copy, "CreateDocfileOnILockBytes on an archive %#lx", hr);
        if (copy) IStorage_Release(copy);
        IStorage_Release(stg);
    }
    after = refcount((IUnknown *)lb);
    CHECK(before == after, "lockbytes reference leaked: %lu -> %lu", before, after);

    free(data);
    IUnknown_Release((IUnknown *)lb);
    IUnknown_Release((IUnknown *)junk);
    its->v->Release(its);
    CoUninitialize();
    printf(failures ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", failures);
    return failures != 0;
}
