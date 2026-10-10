/* dmloader's streams and CollectGarbage (patches/sg/2951), run by test/dmloader-streams-gate.sh:
 * a probe object registered for the tool graph class captures the stream the loader hands to
 * IPersistStream::Load (the loader's stream around the file stream), and the probe calls the
 * IStream methods that used to be stubs on it and on its clone: Write, SetSize, CopyTo, Commit,
 * Revert, LockRegion, UnlockRegion and Stat, checking the values. It then checks that
 * IDirectMusicLoader8::CollectGarbage releases the cached object only once nothing else holds it.
 *
 *   dmloader-streams-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <dmusici.h>
#include <dmusicf.h>
#include <dmerror.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[256]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

static IStream *captured;
static LONG destroyed;

struct obj
{
    IDirectMusicObject IDirectMusicObject_iface;
    IPersistStream IPersistStream_iface;
    LONG ref;
};
static struct obj *from_obj(IDirectMusicObject *iface) { return (struct obj *)iface; }
static struct obj *from_ps(IPersistStream *iface)
{
    return (struct obj *)((char *)iface - offsetof(struct obj, IPersistStream_iface));
}
static HRESULT WINAPI obj_QueryInterface(IDirectMusicObject *iface, REFIID riid, void **out)
{
    struct obj *o = from_obj(iface);
    *out = NULL;
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IDirectMusicObject)) *out = &o->IDirectMusicObject_iface;
    else if (IsEqualGUID(riid, &IID_IPersistStream)) *out = &o->IPersistStream_iface;
    else return E_NOINTERFACE;
    InterlockedIncrement(&o->ref);
    return S_OK;
}
static ULONG WINAPI obj_AddRef(IDirectMusicObject *iface) { return InterlockedIncrement(&from_obj(iface)->ref); }
static ULONG WINAPI obj_Release(IDirectMusicObject *iface)
{
    struct obj *o = from_obj(iface);
    LONG ref = InterlockedDecrement(&o->ref);
    if (!ref)
    {
        InterlockedIncrement(&destroyed);
        HeapFree(GetProcessHeap(), 0, o);
    }
    return ref;
}
static HRESULT WINAPI obj_GetDescriptor(IDirectMusicObject *iface, DMUS_OBJECTDESC *desc)
{
    memset(desc, 0, sizeof(*desc));
    desc->dwSize = sizeof(*desc);
    desc->dwValidData = DMUS_OBJ_CLASS;
    desc->guidClass = CLSID_DirectMusicGraph;
    return S_OK;
}
static HRESULT WINAPI obj_SetDescriptor(IDirectMusicObject *iface, DMUS_OBJECTDESC *desc) { return E_NOTIMPL; }
static HRESULT WINAPI obj_ParseDescriptor(IDirectMusicObject *iface, IStream *stream, DMUS_OBJECTDESC *desc) { return E_NOTIMPL; }
static const IDirectMusicObjectVtbl obj_vtbl = {obj_QueryInterface, obj_AddRef, obj_Release, obj_GetDescriptor,
        obj_SetDescriptor, obj_ParseDescriptor};
static HRESULT WINAPI ps_QueryInterface(IPersistStream *iface, REFIID riid, void **out)
{
    return obj_QueryInterface(&from_ps(iface)->IDirectMusicObject_iface, riid, out);
}
static ULONG WINAPI ps_AddRef(IPersistStream *iface) { return obj_AddRef(&from_ps(iface)->IDirectMusicObject_iface); }
static ULONG WINAPI ps_Release(IPersistStream *iface) { return obj_Release(&from_ps(iface)->IDirectMusicObject_iface); }
static HRESULT WINAPI ps_GetClassID(IPersistStream *iface, CLSID *id) { *id = CLSID_DirectMusicGraph; return S_OK; }
static HRESULT WINAPI ps_IsDirty(IPersistStream *iface) { return S_FALSE; }
static HRESULT WINAPI ps_Load(IPersistStream *iface, IStream *stream)
{
    if (captured) IStream_Release(captured);
    captured = stream;
    IStream_AddRef(stream);
    return S_OK;
}
static HRESULT WINAPI ps_Save(IPersistStream *iface, IStream *stream, BOOL clear) { return E_NOTIMPL; }
static HRESULT WINAPI ps_GetSizeMax(IPersistStream *iface, ULARGE_INTEGER *size) { return E_NOTIMPL; }
static const IPersistStreamVtbl ps_vtbl = {ps_QueryInterface, ps_AddRef, ps_Release, ps_GetClassID, ps_IsDirty,
        ps_Load, ps_Save, ps_GetSizeMax};

static HRESULT WINAPI cf_QueryInterface(IClassFactory *iface, REFIID riid, void **out)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IClassFactory))
    {
        *out = iface;
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI cf_AddRef(IClassFactory *iface) { return 2; }
static ULONG WINAPI cf_Release(IClassFactory *iface) { return 1; }
static HRESULT WINAPI cf_CreateInstance(IClassFactory *iface, IUnknown *outer, REFIID riid, void **out)
{
    struct obj *o = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*o));
    HRESULT hr;
    o->IDirectMusicObject_iface.lpVtbl = &obj_vtbl;
    o->IPersistStream_iface.lpVtbl = &ps_vtbl;
    o->ref = 1;
    hr = IDirectMusicObject_QueryInterface(&o->IDirectMusicObject_iface, riid, out);
    IDirectMusicObject_Release(&o->IDirectMusicObject_iface);
    return hr;
}
static HRESULT WINAPI cf_LockServer(IClassFactory *iface, BOOL lock) { return S_OK; }
static const IClassFactoryVtbl cf_vtbl = {cf_QueryInterface, cf_AddRef, cf_Release, cf_CreateInstance, cf_LockServer};
static IClassFactory factory = {&cf_vtbl};

static void test_stream_methods(IStream *stream, const WCHAR *path, const char *tag)
{
    static const LARGE_INTEGER zero;
    ULARGE_INTEGER size = {{0}}, pos, read, written;
    STATSTG stat;
    IStream *dest;
    HRESULT hr;
    ULONG count;
    char data[100], expect[100];
    HGLOBAL global;
    unsigned int i;
    void *ptr;

    for (i = 0; i < 100; i++) expect[i] = 'A' + i % 26;

    IStream_Seek(stream, zero, STREAM_SEEK_SET, NULL);
    memset(&stat, 0xcc, sizeof(stat));
    hr = IStream_Stat(stream, &stat, STATFLAG_DEFAULT);
    CHECKF(hr == S_OK, "%s Stat (%#lx)", tag, hr);
    CHECKF(stat.type == STGTY_STREAM && stat.cbSize.QuadPart == 100, "%s Stat reports a stream of 100 bytes (%lu, %llu)", tag, stat.type, stat.cbSize.QuadPart);
    CHECKF(stat.pwcsName && !lstrcmpiW(stat.pwcsName, path), "%s Stat reports the file name", tag);
    CHECKF(stat.mtime.dwLowDateTime || stat.mtime.dwHighDateTime, "%s Stat reports a modification time", tag);
    CoTaskMemFree(stat.pwcsName);
    memset(&stat, 0xcc, sizeof(stat));
    hr = IStream_Stat(stream, &stat, STATFLAG_NONAME);
    CHECKF(hr == S_OK && !stat.pwcsName && stat.cbSize.QuadPart == 100, "%s Stat with STATFLAG_NONAME has no name (%#lx)", tag, hr);
    hr = IStream_Stat(stream, NULL, 0);
    CHECKF(hr == STG_E_INVALIDPOINTER, "%s Stat without buffer is STG_E_INVALIDPOINTER (%#lx)", tag, hr);

    count = 0xdeadbeef;
    hr = IStream_Write(stream, "x", 1, &count);
    CHECKF(hr == STG_E_ACCESSDENIED && !count, "%s Write is STG_E_ACCESSDENIED (%#lx, %lu)", tag, hr, count);
    size.QuadPart = 10;
    hr = IStream_SetSize(stream, size);
    CHECKF(hr == STG_E_ACCESSDENIED, "%s SetSize is STG_E_ACCESSDENIED (%#lx)", tag, hr);
    hr = IStream_Commit(stream, STGC_DEFAULT);
    CHECKF(hr == S_OK, "%s Commit is S_OK (%#lx)", tag, hr);
    hr = IStream_Revert(stream);
    CHECKF(hr == S_OK, "%s Revert is S_OK (%#lx)", tag, hr);
    hr = IStream_LockRegion(stream, pos = size, size, LOCK_WRITE);
    CHECKF(hr == STG_E_INVALIDFUNCTION, "%s LockRegion is STG_E_INVALIDFUNCTION (%#lx)", tag, hr);
    hr = IStream_UnlockRegion(stream, pos, size, LOCK_WRITE);
    CHECKF(hr == STG_E_INVALIDFUNCTION, "%s UnlockRegion is STG_E_INVALIDFUNCTION (%#lx)", tag, hr);

    /* CopyTo moves the read position and fills the destination */
    CreateStreamOnHGlobal(NULL, TRUE, &dest);
    size.QuadPart = 40;
    read.QuadPart = written.QuadPart = 0xdeadbeef;
    hr = IStream_CopyTo(stream, dest, size, &read, &written);
    CHECKF(hr == S_OK && read.QuadPart == 40 && written.QuadPart == 40, "%s CopyTo of 40 bytes (%#lx, %llu, %llu)", tag, hr, read.QuadPart, written.QuadPart);
    IStream_Seek(stream, zero, STREAM_SEEK_CUR, &pos);
    CHECKF(pos.QuadPart == 40, "%s CopyTo advanced the position to 40 (%llu)", tag, pos.QuadPart);
    size.QuadPart = 1000;
    hr = IStream_CopyTo(stream, dest, size, &read, &written);
    CHECKF(hr == S_OK && read.QuadPart == 60 && written.QuadPart == 60, "%s CopyTo of the rest copies 60 bytes (%#lx, %llu, %llu)", tag, hr, read.QuadPart, written.QuadPart);
    GetHGlobalFromStream(dest, &global);
    ptr = GlobalLock(global);
    memcpy(data, ptr, 100);
    GlobalUnlock(global);
    CHECKF(!memcmp(data, expect, 100), "%s CopyTo copied the file contents", tag);
    hr = IStream_CopyTo(stream, NULL, size, NULL, NULL);
    CHECKF(hr == STG_E_INVALIDPOINTER, "%s CopyTo without destination is STG_E_INVALIDPOINTER (%#lx)", tag, hr);
    IStream_Release(dest);
}

int main(void)
{
    WCHAR path[MAX_PATH], dir[MAX_PATH];
    IDirectMusicLoader8 *loader;
    IDirectMusicObject *object;
    DMUS_OBJECTDESC desc;
    IStream *clone;
    HANDLE file;
    DWORD cookie, written, i;
    char data[100];
    ULONG ref;
    HRESULT hr;

    CoInitialize(NULL);

    GetTempPathW(MAX_PATH, dir);
    GetTempFileNameW(dir, L"dml", 0, path);
    file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    for (i = 0; i < 100; i++) data[i] = 'A' + i % 26;
    WriteFile(file, data, 100, &written, NULL);
    CloseHandle(file);

    hr = CoRegisterClassObject(&CLSID_DirectMusicGraph, (IUnknown *)&factory, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &cookie);
    CHECKF(hr == S_OK, "register the probe object class (%#lx)", hr);

    hr = CoCreateInstance(&CLSID_DirectMusicLoader, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicLoader8, (void **)&loader);
    if (hr != S_OK)
    {
        printf("SKIP  no DirectMusic loader (%#lx)\n", hr);
        return 77;
    }

    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwValidData = DMUS_OBJ_CLASS | DMUS_OBJ_FILENAME | DMUS_OBJ_FULLPATH;
    desc.guidClass = CLSID_DirectMusicGraph;
    lstrcpyW(desc.wszFileName, path);
    hr = IDirectMusicLoader8_GetObject(loader, &desc, &IID_IDirectMusicObject, (void **)&object);
    CHECKF(hr == S_OK && captured, "GetObject loads the file object and the loader stream reaches it (%#lx)", hr);

    if (captured)
    {
        test_stream_methods(captured, path, "loader stream");
        hr = IStream_Clone(captured, &clone);
        CHECKF(hr == S_OK, "loader stream Clone (%#lx)", hr);
        if (hr == S_OK)
        {
            test_stream_methods(clone, path, "cloned loader stream");
            IStream_Release(clone);
        }
        IStream_Release(captured);
        captured = NULL;
    }

    /* CollectGarbage keeps the cached object while somebody else holds it ... */
    IDirectMusicLoader8_CollectGarbage(loader);
    CHECKF(!destroyed, "CollectGarbage keeps an object that is still referenced (%ld destroyed)", destroyed);
    ref = IDirectMusicObject_AddRef(object);
    IDirectMusicObject_Release(object);
    CHECKF(ref == 3, "the object has the cache, the caller and the probe reference (%lu)", ref);

    /* ... and releases it once only the cache does */
    ref = IDirectMusicObject_Release(object);
    CHECKF(ref == 1 && !destroyed, "after the caller released it only the cache holds it (%lu, %ld destroyed)", ref, destroyed);
    IDirectMusicLoader8_CollectGarbage(loader);
    CHECKF(destroyed == 1, "CollectGarbage releases an object only the cache holds (%ld destroyed)", destroyed);

    IDirectMusicLoader8_Release(loader);
    CoRevokeClassObject(cookie);
    DeleteFileW(path);
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
