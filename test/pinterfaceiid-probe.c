/* IIDs of parameterized WinRT type instances (patches/sg/1638).
 *
 * RoGetParameterizedTypeInstanceIID was a stub that returned E_NOTIMPL and
 * GUID_NULL, and RoFreeParameterizedTypeExtra and
 * RoParameterizedTypeExtraGetTypeSignature were missing.  The probe asks for
 * the IIDs of instances such as IAsyncOperation<Boolean> or
 * IAsyncOperation<IVectorView<StorageFile>>, describing the types through
 * its own IRoMetaDataLocator, and compares them with the IIDs in the SDK
 * headers.  Language projections (C++/WinRT, .NET CsWinRT, Python and
 * JavaScript bridges) call this to find the interface to QueryInterface for.
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef struct builder builder;
typedef struct
{
    HRESULT (WINAPI *SetWinRtInterface)(builder *, GUID);
    HRESULT (WINAPI *SetDelegate)(builder *, GUID);
    HRESULT (WINAPI *SetInterfaceGroupSimpleDefault)(builder *, const WCHAR *, const WCHAR *, const GUID *);
    HRESULT (WINAPI *SetInterfaceGroupParameterizedDefault)(builder *, const WCHAR *, UINT32, const WCHAR **);
    HRESULT (WINAPI *SetRuntimeClassSimpleDefault)(builder *, const WCHAR *, const WCHAR *, const GUID *);
    HRESULT (WINAPI *SetRuntimeClassParameterizedDefault)(builder *, const WCHAR *, UINT32, const WCHAR **);
    HRESULT (WINAPI *SetStruct)(builder *, const WCHAR *, UINT32, const WCHAR **);
    HRESULT (WINAPI *SetEnum)(builder *, const WCHAR *, const WCHAR *);
    HRESULT (WINAPI *SetParameterizedInterface)(builder *, GUID, UINT32);
    HRESULT (WINAPI *SetParameterizedDelegate)(builder *, GUID, UINT32);
} builder_vtbl;
struct builder { const builder_vtbl *vtbl; };

typedef struct locator locator;
typedef struct { HRESULT (WINAPI *Locate)(locator *, const WCHAR *, builder *); } locator_vtbl;
struct locator { const locator_vtbl *vtbl; int calls; };

typedef HRESULT (WINAPI *get_iid_t)(UINT32, const WCHAR **, const locator *, GUID *, void **);
typedef void (WINAPI *free_extra_t)(void *);
typedef const char *(WINAPI *get_signature_t)(void *);

static const GUID piid_async_operation = {0x9fc2b0bb,0xe446,0x44e2,{0xaa,0x61,0x9c,0xab,0x8f,0x63,0x6a,0xf2}};
static const GUID piid_completed_handler = {0xfcdcf02c,0xe5d8,0x4478,{0x91,0x5a,0x4d,0x90,0xb7,0x4b,0x83,0xa5}};
static const GUID piid_typed_handler = {0x9de1c534,0x6ae1,0x11e0,{0x84,0xe1,0x18,0xa9,0x05,0xbc,0xc5,0x3f}};
static const GUID piid_iterable = {0xfaa585ea,0x6214,0x4217,{0xaf,0xda,0x7f,0x46,0xde,0x58,0x69,0xb3}};
static const GUID piid_vector_view = {0xbbe1fa4c,0xb0e3,0x4583,{0xba,0xef,0x1f,0x1b,0x2e,0x48,0x3e,0x56}};
static const GUID piid_map = {0x3c2925fe,0x8519,0x45c1,{0xaa,0x79,0x19,0x7b,0x67,0x18,0xc1,0xc1}};
static const GUID piid_reference = {0x61c17706,0x2d65,0x11e0,{0x9a,0xe8,0xd4,0x85,0x64,0x01,0x54,0x72}};
static const GUID iid_uri = {0x9e365e57,0x48b2,0x4160,{0x95,0x6f,0xc7,0x38,0x51,0x20,0xbb,0xfc}};
static const GUID iid_storage_file = {0xfa3f6186,0x4214,0x428c,{0xa6,0x4c,0x14,0xc9,0xac,0x73,0x15,0xea}};
static const GUID iid_dispatcher_queue = {0x603e88e4,0xa338,0x4ffe,{0xa4,0x57,0xa5,0xcf,0xb9,0xce,0xb8,0x99}};

static HRESULT WINAPI locate(locator *This, const WCHAR *name, builder *b)
{
    static const WCHAR *point_fields[] = { L"Single", L"Single" };
    static const WCHAR *timespan_fields[] = { L"Int64" };
    static const WCHAR *color_fields[] = { L"UInt8", L"UInt8", L"UInt8", L"UInt8" };

    This->calls++;
    if (!wcscmp(name, L"Windows.Foundation.IAsyncOperation`1"))
        return b->vtbl->SetParameterizedInterface(b, piid_async_operation, 1);
    if (!wcscmp(name, L"Windows.Foundation.AsyncOperationCompletedHandler`1"))
        return b->vtbl->SetParameterizedDelegate(b, piid_completed_handler, 1);
    if (!wcscmp(name, L"Windows.Foundation.TypedEventHandler`2"))
        return b->vtbl->SetParameterizedDelegate(b, piid_typed_handler, 2);
    if (!wcscmp(name, L"Windows.Foundation.Collections.IIterable`1"))
        return b->vtbl->SetParameterizedInterface(b, piid_iterable, 1);
    if (!wcscmp(name, L"Windows.Foundation.Collections.IVectorView`1"))
        return b->vtbl->SetParameterizedInterface(b, piid_vector_view, 1);
    if (!wcscmp(name, L"Windows.Foundation.Collections.IMap`2"))
        return b->vtbl->SetParameterizedInterface(b, piid_map, 2);
    if (!wcscmp(name, L"Windows.Foundation.IReference`1"))
        return b->vtbl->SetParameterizedInterface(b, piid_reference, 1);
    if (!wcscmp(name, L"Windows.Foundation.Uri"))
        return b->vtbl->SetRuntimeClassSimpleDefault(b, name, L"Windows.Foundation.IUriRuntimeClass", &iid_uri);
    if (!wcscmp(name, L"Windows.Storage.StorageFile"))  /* default interface by name only */
        return b->vtbl->SetRuntimeClassSimpleDefault(b, name, L"Windows.Storage.IStorageFile", NULL);
    if (!wcscmp(name, L"Windows.Storage.IStorageFile"))
        return b->vtbl->SetWinRtInterface(b, iid_storage_file);
    if (!wcscmp(name, L"Windows.System.DispatcherQueue"))
        return b->vtbl->SetRuntimeClassSimpleDefault(b, name, L"Windows.System.IDispatcherQueue", &iid_dispatcher_queue);
    if (!wcscmp(name, L"Windows.Foundation.Point"))
        return b->vtbl->SetStruct(b, name, 2, point_fields);
    if (!wcscmp(name, L"Windows.Foundation.TimeSpan"))
        return b->vtbl->SetStruct(b, name, 1, timespan_fields);
    if (!wcscmp(name, L"Windows.UI.Color"))
        return b->vtbl->SetStruct(b, name, 4, color_fields);
    if (!wcscmp(name, L"Windows.Security.Authorization.AppCapabilityAccess.AppCapabilityAccessStatus"))
        return b->vtbl->SetEnum(b, name, L"Int32");
    if (!wcscmp(name, L"Bogus.Twice"))
    {
        b->vtbl->SetWinRtInterface(b, iid_uri);
        return b->vtbl->SetWinRtInterface(b, iid_uri) == S_OK ? S_OK : E_FAIL;
    }
    return 0x8000000f;  /* RO_E_METADATA_NAME_NOT_FOUND */
}

static const locator_vtbl my_locator_vtbl = { locate };
static int failures;
static get_iid_t pRoGetParameterizedTypeInstanceIID;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static void check_iid(const char *what, const GUID *expect, UINT32 count, const WCHAR **elements)
{
    locator loc = { &my_locator_vtbl };
    GUID iid;
    HRESULT hr;

    memset(&iid, 0xcc, sizeof(iid));
    hr = pRoGetParameterizedTypeInstanceIID(count, elements, &loc, &iid, NULL);
    printf("%s: hr %#lx iid {%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}\n", what, hr, iid.Data1, iid.Data2,
           iid.Data3, iid.Data4[0], iid.Data4[1], iid.Data4[2], iid.Data4[3], iid.Data4[4], iid.Data4[5],
           iid.Data4[6], iid.Data4[7]);
    check(hr == S_OK && !memcmp(&iid, expect, sizeof(iid)), what);
}

#define ELEMS(...) ARRAYSIZE(((const WCHAR *[]){ __VA_ARGS__ })), ((const WCHAR *[]){ __VA_ARGS__ })

int main(void)
{
    HMODULE combase = LoadLibraryA("combase.dll");
    free_extra_t pRoFreeParameterizedTypeExtra;
    get_signature_t pRoParameterizedTypeExtraGetTypeSignature;
    locator loc = { &my_locator_vtbl };
    const char *signature;
    void *extra;
    GUID iid;
    HRESULT hr;

    pRoGetParameterizedTypeInstanceIID = (void *)GetProcAddress(combase, "RoGetParameterizedTypeInstanceIID");
    pRoFreeParameterizedTypeExtra = (void *)GetProcAddress(combase, "RoFreeParameterizedTypeExtra");
    pRoParameterizedTypeExtraGetTypeSignature = (void *)GetProcAddress(combase, "RoParameterizedTypeExtraGetTypeSignature");
    check(pRoGetParameterizedTypeInstanceIID && pRoFreeParameterizedTypeExtra && pRoParameterizedTypeExtraGetTypeSignature,
          "combase exports the parameterized IID functions");
    if (!pRoGetParameterizedTypeInstanceIID || !pRoFreeParameterizedTypeExtra || !pRoParameterizedTypeExtraGetTypeSignature)
    {
        printf("RESULT: FAIL\n");
        return 1;
    }

    {
        static const GUID e = {0xcdb5efb3,0x5788,0x509d,{0x9b,0xe1,0x71,0xcc,0xb8,0xa3,0x36,0x2a}};
        check_iid("IAsyncOperation<Boolean>", &e, ELEMS(L"Windows.Foundation.IAsyncOperation`1", L"Boolean"));
    }
    {
        static const GUID e = {0x3e1fe603,0xf897,0x5263,{0xb3,0x28,0x08,0x06,0x42,0x6b,0x8a,0x79}};
        check_iid("IAsyncOperation<String>", &e, ELEMS(L"Windows.Foundation.IAsyncOperation`1", L"String"));
    }
    {
        static const GUID e = {0xabf53c57,0xee50,0x5342,{0xb5,0x2a,0x26,0xe3,0xb8,0xcc,0x02,0x4f}};
        check_iid("IAsyncOperation<Object>", &e, ELEMS(L"Windows.Foundation.IAsyncOperation`1", L"Object"));
    }
    {
        static const GUID e = {0xef60385f,0xbe78,0x584b,{0xaa,0xef,0x78,0x29,0xad,0xa2,0xb0,0xde}};
        check_iid("IAsyncOperation<UInt32>", &e, ELEMS(L"Windows.Foundation.IAsyncOperation`1", L"UInt32"));
    }
    {
        static const GUID e = {0xe2fcc7c1,0x3bfc,0x5a0b,{0xb2,0xb0,0x72,0xe7,0x69,0xd1,0xcb,0x7e}};
        check_iid("IIterable<String>", &e, ELEMS(L"Windows.Foundation.Collections.IIterable`1", L"String"));
    }
    {
        static const GUID e = {0x1b0d3570,0x0877,0x5ec2,{0x8a,0x2c,0x3b,0x95,0x39,0x50,0x6a,0xca}};
        check_iid("IMap<String, Object>", &e, ELEMS(L"Windows.Foundation.Collections.IMap`2", L"String", L"Object"));
    }
    {
        static const GUID e = {0x7d50f649,0x632c,0x51f9,{0x84,0x9a,0xee,0x49,0x42,0x89,0x33,0xea}};
        check_iid("IReference<Guid>", &e, ELEMS(L"Windows.Foundation.IReference`1", L"Guid"));
    }
    {
        static const GUID e = {0xe5198cc8,0x2873,0x55f5,{0xb0,0xa1,0x84,0xff,0x9e,0x4a,0xad,0x62}};
        check_iid("IReference<UInt8>", &e, ELEMS(L"Windows.Foundation.IReference`1", L"UInt8"));
    }
    {
        static const GUID e = {0x641cb9dd,0xa28d,0x59e2,{0xb8,0xdb,0xa2,0x27,0xed,0xa6,0xcf,0x2e}};
        check_iid("IAsyncOperation<Uri> (runtime class)", &e, ELEMS(L"Windows.Foundation.IAsyncOperation`1", L"Windows.Foundation.Uri"));
    }
    {
        static const GUID e = {0x84f14c22,0xa00a,0x5272,{0x8d,0x3d,0x82,0x11,0x2e,0x66,0xdf,0x00}};
        check_iid("IReference<Point> (struct)", &e, ELEMS(L"Windows.Foundation.IReference`1", L"Windows.Foundation.Point"));
    }
    {
        static const GUID e = {0x604d0c4c,0x91de,0x5c2a,{0x93,0x5f,0x36,0x2f,0x13,0xea,0xf8,0x00}};
        check_iid("IReference<TimeSpan> (struct)", &e, ELEMS(L"Windows.Foundation.IReference`1", L"Windows.Foundation.TimeSpan"));
    }
    {
        static const GUID e = {0xab8e5d11,0xb0c1,0x5a21,{0x95,0xae,0xf1,0x6b,0xf3,0xa3,0x76,0x24}};
        check_iid("IReference<Color> (struct)", &e, ELEMS(L"Windows.Foundation.IReference`1", L"Windows.UI.Color"));
    }
    {
        static const GUID e = {0x827caf42,0x5fe6,0x5b5b,{0x84,0xce,0xc4,0x48,0x34,0x13,0x4d,0x3d}};
        check_iid("IAsyncOperation<AppCapabilityAccessStatus> (enum)", &e, ELEMS(L"Windows.Foundation.IAsyncOperation`1",
                  L"Windows.Security.Authorization.AppCapabilityAccess.AppCapabilityAccessStatus"));
    }
    {
        static const GUID e = {0xc1d3d1a2,0xae17,0x5a5f,{0xb5,0xa2,0xbd,0xcc,0x88,0x44,0x88,0x9a}};
        check_iid("AsyncOperationCompletedHandler<Boolean> (delegate)", &e,
                  ELEMS(L"Windows.Foundation.AsyncOperationCompletedHandler`1", L"Boolean"));
    }
    {
        static const GUID e = {0xfe79f855,0x2f40,0x5b88,{0xa0,0xc3,0x4c,0x04,0x2a,0x05,0xdd,0x05}};
        check_iid("TypedEventHandler<DispatcherQueue, Object>", &e,
                  ELEMS(L"Windows.Foundation.TypedEventHandler`2", L"Windows.System.DispatcherQueue", L"Object"));
    }
    {
        static const GUID e = {0x03362e33,0xe413,0x5f29,{0x97,0xd0,0x48,0xa4,0x78,0x09,0x35,0xf9}};
        check_iid("IAsyncOperation<IVectorView<StorageFile>> (nested; default interface by name)", &e,
                  ELEMS(L"Windows.Foundation.IAsyncOperation`1", L"Windows.Foundation.Collections.IVectorView`1",
                        L"Windows.Storage.StorageFile"));
    }

    /* the extra handle carries the type signature */
    extra = NULL;
    hr = pRoGetParameterizedTypeInstanceIID(ELEMS(L"Windows.Foundation.IAsyncOperation`1", L"Boolean"), &loc, &iid, &extra);
    signature = extra ? pRoParameterizedTypeExtraGetTypeSignature(extra) : NULL;
    printf("signature: hr %#lx %s\n", hr, signature ? signature : "(null)");
    check(hr == S_OK && signature && !strcmp(signature, "pinterface({9fc2b0bb-e446-44e2-aa61-9cab8f636af2};b1)"),
          "RoParameterizedTypeExtraGetTypeSignature gives the signature the IID was made from");
    pRoFreeParameterizedTypeExtra(extra);

    /* errors */
    memset(&iid, 0xcc, sizeof(iid));
    hr = pRoGetParameterizedTypeInstanceIID(ELEMS(L"Windows.Foundation.IAsyncOperation`1", L"No.Such.Type"), &loc, &iid, NULL);
    printf("unknown type argument: hr %#lx\n", hr);
    check(hr == 0x8000000f && iid.Data1 == 0, "an unknown type argument fails with the locator's error and GUID_NULL");
    hr = pRoGetParameterizedTypeInstanceIID(ELEMS(L"Windows.Foundation.IAsyncOperation`1"), &loc, &iid, NULL);
    printf("missing type argument: hr %#lx\n", hr);
    check(FAILED(hr), "a missing type argument fails");
    hr = pRoGetParameterizedTypeInstanceIID(ELEMS(L"Windows.Foundation.IAsyncOperation`1", L"Boolean", L"Boolean"), &loc, &iid, NULL);
    printf("extra type argument: hr %#lx\n", hr);
    check(FAILED(hr), "a surplus type argument fails");
    hr = pRoGetParameterizedTypeInstanceIID(ELEMS(L"Windows.Foundation.IAsyncOperation`1", L"Bogus.Twice"), &loc, &iid, NULL);
    printf("type described twice: hr %#lx\n", hr);
    check(FAILED(hr), "a locator that describes a type twice is refused");
    hr = pRoGetParameterizedTypeInstanceIID(ELEMS(L"Windows.Foundation.IAsyncOperation`1", L"Boolean"), &loc, NULL, NULL);
    check(hr == E_INVALIDARG, "a NULL iid is refused");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
