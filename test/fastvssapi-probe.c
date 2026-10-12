#include <windows.h>
#include <oleauto.h>
#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

#define VSS_E_BAD_STATE 0x80042301
#define VSS_E_OBJECT_NOT_FOUND 0x80042308
#define VSS_E_VOLUME_NOT_SUPPORTED 0x8004230C
#define VSS_S_ASYNC_FINISHED 0x0004230A
#define VSS_E_WRITER_ALREADY_SUBSCRIBED 0x8004231A

typedef struct { void **vtbl; } Obj;
#define M(o, idx, type) ((type)((o)->vtbl[idx]))

typedef HRESULT (WINAPI *p0)(void *);
typedef HRESULT (WINAPI *p1)(void *, void *);
typedef HRESULT (WINAPI *p2)(void *, void *, void *);
typedef ULONG (WINAPI *pr)(void *);

int main(void)
{
    HMODULE m = LoadLibraryA("vssapi.dll");
    HRESULT (WINAPI *create)(Obj **) = (void *)GetProcAddress(m, "CreateVssBackupComponentsInternal");
    HRESULT (WINAPI *snapped)(const WCHAR *, BOOL *, LONG *) = (void *)GetProcAddress(m, "IsVolumeSnapshottedInternal");
    HRESULT (WINAPI *snapped2)(const WCHAR *, BOOL *, LONG *) = (void *)GetProcAddress(m, "IsVolumeSnapshotted");
    HRESULT (WINAPI *block)(const WCHAR *, BOOL *) = (void *)GetProcAddress(m, "ShouldBlockRevert");
    Obj *b = NULL, *async = NULL;
    HRESULT hr, res;
    GUID id, snap;
    UINT n;
    BOOL supported = TRUE, flag;
    LONG compat = 5;
    BSTR xml = NULL;
    INT reserved;

    CHECK(create && snapped && snapped2 && block, "exports");
    hr = create(&b);
    CHECK(hr == S_OK && b, "create %lx", hr);

    /* state machine */
    hr = M(b, 6, HRESULT (WINAPI *)(void *, BOOL, BOOL, int, BOOL))(b, FALSE, TRUE, 1, FALSE);
    CHECK(hr == (HRESULT)VSS_E_BAD_STATE, "SetBackupState before init %lx", hr);
    hr = M(b, 5, p1)(b, NULL);
    CHECK(hr == S_OK, "InitializeForBackup %lx", hr);
    hr = M(b, 5, p1)(b, NULL);
    CHECK(hr == (HRESULT)VSS_E_BAD_STATE, "second init %lx", hr);
    hr = M(b, 35, p1)(b, (void *)0x1234);
    CHECK(hr == (HRESULT)0x8004231B, "bad context %lx", hr);
    hr = M(b, 35, p1)(b, (void *)0);
    CHECK(hr == S_OK, "SetContext(backup) %lx", hr);
    hr = M(b, 9, p1)(b, &async);
    CHECK(hr == (HRESULT)VSS_E_BAD_STATE, "gather before SetBackupState %lx", hr);
    hr = M(b, 6, HRESULT (WINAPI *)(void *, BOOL, BOOL, int, BOOL))(b, FALSE, TRUE, 1, FALSE);
    CHECK(hr == S_OK, "SetBackupState %lx", hr);
    hr = M(b, 9, p1)(b, &async);
    CHECK(hr == S_OK && async, "GatherWriterMetadata %lx", hr);
    if (async)
    {
        hr = M(async, 4, HRESULT (WINAPI *)(void *, DWORD))(async, INFINITE);
        CHECK(hr == S_OK, "async wait");
        hr = M(async, 5, HRESULT (WINAPI *)(void *, HRESULT *, INT *))(async, &res, &reserved);
        CHECK(hr == S_OK && res == VSS_S_ASYNC_FINISHED, "async status %lx", res);
        M(async, 2, pr)(async);
    }
    n = 99;
    hr = M(b, 10, p1)(b, &n);
    CHECK(hr == S_OK && n == 0, "no writers: %lx %u", hr, n);

    /* snapshot sets: no provider can handle a volume */
    {
        static const GUID null_guid;
        hr = M(b, 37, HRESULT (WINAPI *)(void *, const WCHAR *, GUID, GUID *))(b, L"C:\\", null_guid, &snap);
        CHECK(hr == (HRESULT)VSS_E_BAD_STATE, "add before StartSnapshotSet %lx", hr);
        hr = M(b, 36, p1)(b, &id);
        CHECK(hr == S_OK && memcmp(&id, &null_guid, sizeof(id)), "StartSnapshotSet %lx", hr);
        hr = M(b, 36, p1)(b, &id);
        CHECK(hr == (HRESULT)0x80042316, "second StartSnapshotSet %lx", hr);
        hr = M(b, 37, HRESULT (WINAPI *)(void *, const WCHAR *, GUID, GUID *))(b, L"C:\\", null_guid, &snap);
        CHECK(hr == (HRESULT)VSS_E_VOLUME_NOT_SUPPORTED, "AddToSnapshotSet %lx", hr);
        hr = M(b, 38, p1)(b, &async);
        CHECK(hr == (HRESULT)VSS_E_BAD_STATE, "DoSnapshotSet of an empty set %lx", hr);
        hr = M(b, 44, HRESULT (WINAPI *)(void *, GUID, const WCHAR *, BOOL *))(b, null_guid, L"C:\\", &supported);
        CHECK(hr == S_OK && !supported, "IsVolumeSupported %lx", hr);
        hr = M(b, 26, p1)(b, &xml);
        CHECK(hr == S_OK && xml && wcsstr(xml, L"BACKUP_COMPONENTS"), "SaveAsXML");
        SysFreeString(xml);
        {
            Obj *en = NULL;
            ULONG fetched = 5;
            char prop[512];
            hr = M(b, 43, HRESULT (WINAPI *)(void *, GUID, int, int, Obj **))(b, null_guid, 0, 3, &en);
            CHECK(hr == S_OK && en, "Query %lx", hr);
            if (en)
            {
                hr = M(en, 3, HRESULT (WINAPI *)(void *, ULONG, void *, ULONG *))(en, 1, prop, &fetched);
                CHECK(hr == S_FALSE && fetched == 0, "empty enumeration %lx", hr);
                M(en, 2, pr)(en);
            }
        }
    }
    CHECK(M(b, 2, pr)(b) == 0, "release");

    /* restore side */
    hr = create(&b);
    CHECK(hr == S_OK, "create 2");
    hr = M(b, 7, p1)(b, NULL);
    CHECK(hr == E_INVALIDARG, "InitializeForRestore(NULL) %lx", hr);
    hr = M(b, 7, p1)(b, (void *)L"<x/>");
    CHECK(hr == S_OK, "InitializeForRestore %lx", hr);
    n = 7;
    hr = M(b, 3, p1)(b, &n);
    CHECK(hr == S_OK && n == 0, "writer components count");
    M(b, 2, pr)(b);

    /* free functions */
    flag = TRUE;
    hr = snapped(L"C:\\", &flag, &compat);
    CHECK(hr == S_OK && !flag && compat == 0, "IsVolumeSnapshotted");
    flag = TRUE;
    CHECK(snapped2(L"C:\\", &flag, NULL) == S_OK && !flag, "IsVolumeSnapshotted alias");
    flag = TRUE;
    CHECK(block(L"C:\\", &flag) == S_OK && !flag, "ShouldBlockRevert");
    CHECK(snapped(NULL, &flag, &compat) == E_INVALIDARG, "IsVolumeSnapshotted(NULL)");

    /* writer object: construct, initialize, subscribe */
    {
        void *(*ctor)(void *) = (void *)GetProcAddress(m, "??0CVssWriter@@QEAA@XZ");
        void (*dtor)(void *) = (void *)GetProcAddress(m, "??1CVssWriter@@UEAA@XZ");
        HRESULT (*init)(void *, GUID, const WCHAR *, int, int, int, DWORD, int, BOOL, const WCHAR *) =
            (void *)GetProcAddress(m, "?Initialize@CVssWriter@@QEAAJU_GUID@@PEBGW4VSS_USAGE_TYPE@@W4VSS_SOURCE_TYPE@@W4_VSS_APPLICATION_LEVEL@@KW4VSS_ALTERNATE_WRITER_STATE@@_N1@Z");
        HRESULT (*sub)(void *, DWORD) = (void *)GetProcAddress(m, "?Subscribe@CVssWriter@@QEAAJK@Z");
        HRESULT (*unsub)(void *) = (void *)GetProcAddress(m, "?Unsubscribe@CVssWriter@@QEAAJXZ");
        char writer[64];
        GUID wid = { 1, 2, 3, { 4, 5, 6, 7, 8, 9, 10, 11 } };

        CHECK(ctor && dtor && init && sub && unsub, "writer exports");
        if (ctor && dtor && init && sub && unsub)
        {
            ctor(writer);
            CHECK(sub(writer, 0) == (HRESULT)VSS_E_BAD_STATE, "subscribe before initialize");
            CHECK(init(writer, wid, NULL, 1, 1, 1, 60000, 0, FALSE, NULL) == E_INVALIDARG, "initialize without name");
            CHECK(init(writer, wid, L"SgWriter", 1, 1, 1, 60000, 0, FALSE, NULL) == S_OK, "initialize");
            CHECK(init(writer, wid, L"SgWriter", 1, 1, 1, 60000, 0, FALSE, NULL) == (HRESULT)VSS_E_BAD_STATE, "initialize twice");
            CHECK(unsub(writer) == (HRESULT)VSS_E_BAD_STATE, "unsubscribe when not subscribed");
            CHECK(sub(writer, 0) == S_OK, "subscribe");
            CHECK(sub(writer, 0) == (HRESULT)VSS_E_WRITER_ALREADY_SUBSCRIBED, "subscribe twice");
            CHECK(unsub(writer) == S_OK, "unsubscribe");
            dtor(writer);
            ctor(writer);
            CHECK(sub(writer, 0) == (HRESULT)VSS_E_BAD_STATE, "state gone after destruction");
            dtor(writer);
        }
    }

    printf(fails ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", fails);
    return fails != 0;
}
