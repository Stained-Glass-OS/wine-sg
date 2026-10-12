/* MFCreateMediaBufferFromMediaType for other major types and the legacy media buffer's Lock
 * (patches/sg/2966), run by test/mfplat-mediabuf-gate.sh. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfobjects.h>
#include <mediaobj.h>
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

static HRESULT (WINAPI *pCreateFromType)(IMFMediaType *, LONGLONG, DWORD, DWORD, IMFMediaBuffer **);
static HRESULT (WINAPI *pCreateLegacy)(IMFSample *, IMFMediaBuffer *, DWORD, IMediaBuffer **);

static void test_other_major(void)
{
    static const GUID custom_major = {0x12345678, 0x1111, 0x2222, {1,2,3,4,5,6,7,8}};
    IMFMediaBuffer *buffer;
    IMFMediaType *type;
    DWORD max, len;
    BYTE *data;
    HRESULT hr;
    int i;

    MFCreateMediaType(&type);
    hr = pCreateFromType(type, 0, 0, 0, &buffer);
    CHECKF(hr == MF_E_ATTRIBUTENOTFOUND, "no major type hr %#lx", hr);

    for (i = 0; i < 2; i++)
    {
        IMFMediaType_SetGUID(type, &MF_MT_MAJOR_TYPE, i ? &custom_major : &GUID_NULL);
        hr = pCreateFromType(type, 0, 0, 0, &buffer);
        CHECKF(hr == E_INVALIDARG, "%s major without length hr %#lx", i ? "custom" : "null", hr);
        hr = pCreateFromType(type, 0, 16, 0, &buffer);
        CHECKF(hr == S_OK, "%s major, 16 bytes hr %#lx", i ? "custom" : "null", hr);
        if (hr != S_OK) continue;
        max = len = 0xdead;
        hr = IMFMediaBuffer_Lock(buffer, &data, &max, &len);
        CHECKF(hr == S_OK && max == 16 && len == 0, "%s major lock hr %#lx max %lu len %lu", i ? "custom" : "null", hr, max, len);
        CHECKF(((ULONG_PTR)data & 15) == 0, "%s major data aligned", i ? "custom" : "null");
        IMFMediaBuffer_Unlock(buffer);
        IMFMediaBuffer_Release(buffer);

        hr = pCreateFromType(type, 0, 100, MF_32_BYTE_ALIGNMENT, &buffer);
        hr = hr == S_OK ? IMFMediaBuffer_Lock(buffer, &data, &max, NULL) : hr;
        CHECKF(hr == S_OK && max == 100 && ((ULONG_PTR)data & 31) == 0, "%s major 100 bytes, 32 aligned (hr %#lx)", i ? "custom" : "null", hr);
        IMFMediaBuffer_Unlock(buffer);
        IMFMediaBuffer_Release(buffer);
    }
    IMFMediaType_Release(type);
}

/* a media buffer whose Lock records how it was called */
struct tbuf { IMFMediaBuffer IMFMediaBuffer_iface; LONG ref; int locks, unlocks, len_ptr_given; };
static struct tbuf tb;
static HRESULT WINAPI tb_QI(IMFMediaBuffer *i, REFIID riid, void **o)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IMFMediaBuffer)) { *o = i; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI tb_AddRef(IMFMediaBuffer *i) { return InterlockedIncrement(&tb.ref); }
static ULONG WINAPI tb_Release(IMFMediaBuffer *i) { return InterlockedDecrement(&tb.ref); }
static HRESULT WINAPI tb_Lock(IMFMediaBuffer *i, BYTE **data, DWORD *max, DWORD *len)
{
    tb.locks++;
    if (len) tb.len_ptr_given++;
    *data = (BYTE *)0x1000;
    return S_OK;
}
static HRESULT WINAPI tb_Unlock(IMFMediaBuffer *i) { tb.unlocks++; return S_OK; }
static HRESULT WINAPI tb_GetCurrentLength(IMFMediaBuffer *i, DWORD *len) { *len = 77; return S_OK; }
static HRESULT WINAPI tb_SetCurrentLength(IMFMediaBuffer *i, DWORD len) { return S_OK; }
static HRESULT WINAPI tb_GetMaxLength(IMFMediaBuffer *i, DWORD *len) { *len = 99; return S_OK; }
static const IMFMediaBufferVtbl tb_vtbl = { tb_QI, tb_AddRef, tb_Release, tb_Lock, tb_Unlock, tb_GetCurrentLength,
        tb_SetCurrentLength, tb_GetMaxLength };

static void test_legacy_lock(void)
{
    IMediaBuffer *legacy;
    BYTE *data;
    DWORD len;
    HRESULT hr;

    tb.IMFMediaBuffer_iface.lpVtbl = (IMFMediaBufferVtbl *)&tb_vtbl;
    tb.ref = 1;
    hr = pCreateLegacy(NULL, &tb.IMFMediaBuffer_iface, 0, &legacy);
    CHECKF(hr == S_OK, "create legacy buffer hr %#lx", hr);
    if (hr != S_OK) return;
    len = 0;
    data = NULL;
    hr = IMediaBuffer_GetBufferAndLength(legacy, &data, &len);
    CHECKF(hr == S_OK && data == (BYTE *)0x1000 && len == 77, "GetBufferAndLength hr %#lx data %p len %lu", hr, data, len);
    CHECKF(tb.locks == 1 && tb.len_ptr_given == 0, "Lock called %d times, %d with a length pointer", tb.locks, tb.len_ptr_given);
    len = 0;
    hr = IMediaBuffer_GetBufferAndLength(legacy, &data, &len);
    CHECKF(hr == S_OK && len == 77 && tb.locks == 1, "second call reuses the lock (locks %d len %lu)", tb.locks, len);
    IMediaBuffer_Release(legacy);
    CHECKF(tb.unlocks == 1, "released buffer unlocked it (%d)", tb.unlocks);
}

int main(void)
{
    HMODULE mfplat = LoadLibraryA("mfplat.dll");
    HRESULT hr;

    pCreateFromType = (void *)GetProcAddress(mfplat, "MFCreateMediaBufferFromMediaType");
    pCreateLegacy = (void *)GetProcAddress(mfplat, "MFCreateLegacyMediaBufferOnMFMediaBuffer");
    if (!pCreateFromType || !pCreateLegacy) { printf("FAIL  exports missing\n"); return 1; }
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
    if (hr != S_OK) { printf("FAIL  MFStartup %#lx\n", hr); return 1; }
    test_other_major();
    test_legacy_lock();
    MFShutdown();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
