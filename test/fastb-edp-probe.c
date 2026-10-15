/* windows.security.enterprisedata: the asynchronous requests, GetIids/GetRuntimeClassName/GetTrustLevel (patch 3717)
 * Calls by vtable slot. IIDs of the parameterized types: SHA-1 of the signature in the WinRT way (checked by a script). */
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include <initguid.h>

#ifndef E_ILLEGAL_METHOD_CALL
#define E_ILLEGAL_METHOD_CALL ((HRESULT)0x8000000EL)
#endif
typedef struct HSTRING__ *HSTRING;
typedef struct { void **vt; } OBJ;
#define SLOT(o, n) ((OBJ *)(o))->vt[n]
static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

DEFINE_GUID(IID_Statics, 0xc0bffc66,0x8c3d,0x4d56,0x88,0x04,0xc6,0x8f,0x0a,0xd3,0x2e,0xc5);
DEFINE_GUID(IID_Statics2, 0xb68f9a8c,0x39e0,0x4649,0xb2,0xe4,0x07,0x0a,0xb8,0xa5,0x79,0xb3);
DEFINE_GUID(IID_Statics3, 0x48ff9e8c,0x6a6f,0x4d9f,0xbc,0xed,0x18,0xab,0x53,0x7a,0xa0,0x15);
DEFINE_GUID(IID_Statics4, 0x20b794db,0xccbd,0x490f,0x8c,0x83,0x49,0xcc,0xb7,0x7a,0xea,0x6c);
DEFINE_GUID(IID_Interop, 0x4652651d,0xc1fe,0x4ba1,0x9f,0x0a,0xc0,0xf5,0x65,0x96,0xf7,0x21);
DEFINE_GUID(IID_Manager, 0xd5703e18,0xa08d,0x47e6,0xa2,0x40,0x99,0x34,0xd7,0x16,0x5e,0xb5);
DEFINE_GUID(IID_Manager2, 0xabf7527a,0x8435,0x417f,0x99,0xb6,0x51,0xbe,0xaf,0x36,0x58,0x88);
DEFINE_GUID(IID_Closable, 0x30d5a829,0x7fa4,0x4026,0x83,0xbb,0xd7,0x5b,0xae,0x4e,0xa9,0x9e);
DEFINE_GUID(IID_ThreadCtx, 0xfa4ea8e9,0xef13,0x405a,0xb1,0x2c,0xd7,0x34,0x8c,0x6f,0x41,0xfc);
DEFINE_GUID(IID_ActFactory, 0x00000035, 0x0000, 0x0000, 0xc0,0x00, 0x00,0x00,0x00,0x00,0x00,0x46);
DEFINE_GUID(IID_AsyncInfo, 0x00000036, 0x0000, 0x0000, 0xc0,0x00, 0x00,0x00,0x00,0x00,0x00,0x46);
DEFINE_GUID(IID_Agile, 0x94ea2b94, 0xe9cc, 0x49e0, 0xc0,0xff, 0xee,0x64,0xca,0x8f,0x5b,0x90);
DEFINE_GUID(IID_OpEval, 0xe8d81715,0xc56c,0x5a6b,0xb7,0x38,0x5d,0xf6,0xc2,0x77,0x5b,0x7b);
DEFINE_GUID(IID_HdlEval, 0x2833ba54,0xa4e1,0x5c2d,0x8a,0x7a,0x13,0x6e,0x85,0x10,0xc7,0x8b);
DEFINE_GUID(IID_OpBool, 0xcdb5efb3,0x5788,0x509d,0x9b,0xe1,0x71,0xcc,0xb8,0xa3,0x36,0x2a);
DEFINE_GUID(IID_HdlBool, 0xc1d3d1a2,0xae17,0x5a5f,0xb5,0xa2,0xbd,0xcc,0x88,0x44,0x88,0x9a);
DEFINE_GUID(IID_OpStr, 0x3e1fe603,0xf897,0x5263,0xb3,0x28,0x08,0x06,0x42,0x6b,0x8a,0x79);
DEFINE_GUID(IID_HdlStr, 0xb79a741f,0x7fb5,0x50ae,0x9e,0x99,0x91,0x12,0x01,0xec,0x3d,0x41);

struct handler { void **vt; const GUID *iid; int calls; int status; void *async; };
static HRESULT WINAPI h_qi(void *t, const GUID *g, void **o)
{
    struct handler *h = t;
    if (IsEqualGUID(g, &IID_IUnknown) || IsEqualGUID(g, &IID_Agile) || IsEqualGUID(g, h->iid)) { *o = t; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI h_ar(void *t) { return 2; }
static HRESULT WINAPI h_invoke(void *t, void *async, int status)
{
    struct handler *h = t; h->calls++; h->status = status; h->async = async; return S_OK;
}
static void *h_vt[4] = { h_qi, h_ar, h_ar, h_invoke };

typedef HRESULT (WINAPI *create_string_t)(const WCHAR *, UINT32, HSTRING *);
typedef const WCHAR *(WINAPI *raw_t)(HSTRING, UINT32 *);
static create_string_t pCreate;
static raw_t pRaw;


static void class_name(void *obj, const WCHAR *want, const char *what)
{
    HSTRING s = NULL; HRESULT hr = ((HRESULT (WINAPI *)(void *, HSTRING *))SLOT(obj, 4))(obj, &s);
    CHECK(hr == S_OK, "%s GetRuntimeClassName %#lx", what, hr);
    if (hr == S_OK) CHECK(!wcscmp(pRaw(s, NULL), want), "%s class name %ls", what, pRaw(s, NULL));
}
static void iids(void *obj, const GUID *const *want, ULONG n, const char *what)
{
    ULONG c = 99, i, j; GUID *g = NULL; int tl = 99;
    HRESULT hr = ((HRESULT (WINAPI *)(void *, ULONG *, GUID **))SLOT(obj, 3))(obj, &c, &g);
    CHECK(hr == S_OK, "%s GetIids %#lx", what, hr);
    if (hr == S_OK)
    {
        CHECK(c == n, "%s iid count %lu", what, c);
        for (i = 0; i < n; i++)
        {
            BOOL found = FALSE;
            for (j = 0; j < c; j++) if (IsEqualGUID(&g[j], want[i])) found = TRUE;
            CHECK(found, "%s missing iid %lu", what, i);
        }
        CoTaskMemFree(g);
    }
    hr = ((HRESULT (WINAPI *)(void *, int *))SLOT(obj, 5))(obj, &tl);
    CHECK(hr == S_OK && tl == 0, "%s GetTrustLevel %#lx %d", what, hr, tl);
    hr = ((HRESULT (WINAPI *)(void *, int *))SLOT(obj, 5))(obj, NULL);
    CHECK(hr == E_POINTER, "%s GetTrustLevel NULL %#lx", what, hr);
}

/* check an operation: completed at once, the handler runs when set, results, Close */
static void check_op(void *op, const GUID *op_iid, const GUID *hdl_iid, int expect_result, const WCHAR *expect_str, const char *what)
{
    struct handler h = { h_vt, hdl_iid, 0, -1, NULL }, h2 = { h_vt, hdl_iid, 0, -1, NULL };
    void *info = NULL, *again = NULL; HRESULT hr; UINT32 id = 0; int st = -1; HRESULT ec = 1;
    const GUID *w[2];
    void *got = NULL;

    CHECK(op != NULL, "%s: no operation", what);
    if (!op) return;
    hr = ((HRESULT (WINAPI *)(void *, const GUID *, void **))SLOT(op, 0))(op, op_iid, &again);
    CHECK(hr == S_OK && again, "%s QI own iid %#lx", what, hr);
    if (again) ((ULONG (WINAPI *)(void *))SLOT(again, 2))(again);
    hr = ((HRESULT (WINAPI *)(void *, const GUID *, void **))SLOT(op, 0))(op, &IID_AsyncInfo, &info);
    CHECK(hr == S_OK && info, "%s QI IAsyncInfo %#lx", what, hr);
    w[0] = op_iid; w[1] = &IID_AsyncInfo;
    iids(op, w, 2, what);

    hr = ((HRESULT (WINAPI *)(void *, void **))SLOT(op, 7))(op, &got);
    CHECK(hr == S_OK && !got, "%s get_Completed before %#lx %p", what, hr, got);
    hr = ((HRESULT (WINAPI *)(void *, void *))SLOT(op, 6))(op, &h);
    CHECK(hr == S_OK, "%s put_Completed %#lx", what, hr);
    CHECK(h.calls == 1 && h.status == 1 && h.async == op, "%s handler calls %d status %d async %p", what, h.calls, h.status, h.async);
    hr = ((HRESULT (WINAPI *)(void *, void *))SLOT(op, 6))(op, &h2);
    CHECK(hr == (HRESULT)0x80000018, "%s second put_Completed %#lx", what, hr);
    CHECK(h2.calls == 0, "%s second handler ran", what);

    if (info)
    {
        hr = ((HRESULT (WINAPI *)(void *, UINT32 *))SLOT(info, 6))(info, &id);
        CHECK(hr == S_OK && id != 0, "%s Id %#lx %u", what, hr, id);
        hr = ((HRESULT (WINAPI *)(void *, int *))SLOT(info, 7))(info, &st);
        CHECK(hr == S_OK && st == 1, "%s Status %#lx %d", what, hr, st);
        hr = ((HRESULT (WINAPI *)(void *, HRESULT *))SLOT(info, 8))(info, &ec);
        CHECK(hr == S_OK && ec == S_OK, "%s ErrorCode %#lx %#lx", what, hr, ec);
        hr = ((HRESULT (WINAPI *)(void *))SLOT(info, 9))(info);
        CHECK(hr == S_OK, "%s Cancel %#lx", what, hr);
    }
    if (op_iid == &IID_OpEval)
    {
        int r = -1; hr = ((HRESULT (WINAPI *)(void *, int *))SLOT(op, 8))(op, &r);
        CHECK(hr == S_OK && r == expect_result, "%s GetResults %#lx %d (want %d)", what, hr, r, expect_result);
    }
    else if (op_iid == &IID_OpBool)
    {
        BYTE r = 9; hr = ((HRESULT (WINAPI *)(void *, BYTE *))SLOT(op, 8))(op, &r);
        CHECK(hr == S_OK && r == expect_result, "%s GetResults %#lx %d (want %d)", what, hr, r, expect_result);
    }
    else
    {
        HSTRING r = (HSTRING)1; hr = ((HRESULT (WINAPI *)(void *, HSTRING *))SLOT(op, 8))(op, &r);
        CHECK(hr == S_OK, "%s GetResults %#lx", what, hr);
        CHECK(r == NULL || !wcscmp(pRaw(r, NULL), expect_str), "%s string result", what);
        if (!expect_str[0]) CHECK(r == NULL || !pRaw(r, NULL)[0], "%s string not empty", what);
    }
    if (info)
    {
        hr = ((HRESULT (WINAPI *)(void *))SLOT(info, 10))(info);
        CHECK(hr == S_OK, "%s Close %#lx", what, hr);
        hr = ((HRESULT (WINAPI *)(void *, UINT32 *))SLOT(info, 6))(info, &id);
        CHECK(hr == E_ILLEGAL_METHOD_CALL, "%s Id after Close %#lx", what, hr);
        hr = ((HRESULT (WINAPI *)(void *, int *))SLOT(info, 7))(info, &st);
        CHECK(hr == E_ILLEGAL_METHOD_CALL, "%s Status after Close %#lx", what, hr);
        ((ULONG (WINAPI *)(void *))SLOT(info, 2))(info);
    }
    {
        int r = 0; BYTE b = 0; void *p = NULL;
        void *out = (op_iid == &IID_OpEval) ? (void *)&r : (op_iid == &IID_OpBool) ? (void *)&b : (void *)&p;
        hr = ((HRESULT (WINAPI *)(void *, void *))SLOT(op, 8))(op, out);
        CHECK(hr == E_ILLEGAL_METHOD_CALL, "%s GetResults after Close %#lx", what, hr);
        hr = ((HRESULT (WINAPI *)(void *, void *))SLOT(op, 6))(op, NULL);
        CHECK(hr == E_ILLEGAL_METHOD_CALL, "%s put_Completed after Close %#lx", what, hr);
    }
    ((ULONG (WINAPI *)(void *))SLOT(op, 2))(op);
}

int main(void)
{
    HMODULE cb, dll; HRESULT hr; HSTRING cls, src, tgt, app, msg, ident;
    void *fac = NULL, *s1 = NULL, *s2 = NULL, *s3 = NULL, *s4 = NULL, *io = NULL, *mgr = NULL, *ctx = NULL, *op = NULL;
    static const WCHAR name[] = L"Windows.Security.EnterpriseData.ProtectionPolicyManager";
    setvbuf(stdout, NULL, _IONBF, 0);
    cb = LoadLibraryA("combase.dll");
    pCreate = (create_string_t)GetProcAddress(cb, "WindowsCreateString");
    pRaw = (raw_t)GetProcAddress(cb, "WindowsGetStringRawBuffer");
    hr = ((HRESULT (WINAPI *)(int))GetProcAddress(cb, "RoInitialize"))(1);
    CHECK(hr == S_OK, "RoInitialize %#lx", hr);
    pCreate(name, wcslen(name), &cls);
    pCreate(L"user@contoso.com", 16, &src); pCreate(L"https://x.example", 17, &tgt); pCreate(L"Contoso.App_1234", 16, &app);
    pCreate(L"why", 3, &msg); pCreate(L"user@contoso.com", 16, &ident);
    for (int try = 0; try < 8; try++)  /* a fresh prefix may still be registering the classes */
    {
        hr = ((HRESULT (WINAPI *)(HSTRING, const GUID *, void **))GetProcAddress(cb, "RoGetActivationFactory"))(cls, &IID_ActFactory, &fac);
        if (hr != (HRESULT)0x80040154) break;
        Sleep(1500);
    }
    CHECK(hr == S_OK && fac, "factory %#lx", hr);
    if (!fac) { printf("RESULT: FAIL\n"); return 1; }

    /* the factory */
    {
        const GUID *w[] = { &IID_Statics, &IID_Statics2, &IID_Statics3, &IID_Statics4, &IID_Interop, &IID_ActFactory };
        iids(fac, w, 6, "factory");
        class_name(fac, name, "factory");
        hr = ((HRESULT (WINAPI *)(void *, void **))SLOT(fac, 6))(fac, &op);
        CHECK(hr == E_NOTIMPL && !op, "ActivateInstance %#lx", hr);
    }
    hr = ((HRESULT (WINAPI *)(void *, const GUID *, void **))SLOT(fac, 0))(fac, &IID_Statics, &s1);
    CHECK(hr == S_OK, "statics %#lx", hr);
    ((HRESULT (WINAPI *)(void *, const GUID *, void **))SLOT(fac, 0))(fac, &IID_Statics2, &s2);
    ((HRESULT (WINAPI *)(void *, const GUID *, void **))SLOT(fac, 0))(fac, &IID_Statics3, &s3);
    ((HRESULT (WINAPI *)(void *, const GUID *, void **))SLOT(fac, 0))(fac, &IID_Statics4, &s4);
    ((HRESULT (WINAPI *)(void *, const GUID *, void **))SLOT(fac, 0))(fac, &IID_Interop, &io);
    CHECK(s1 && s2 && s3 && s4 && io, "interfaces %p %p %p %p %p", s1, s2, s3, s4, io);
    if (!(s1 && s2 && s3 && s4 && io)) { printf("RESULT: FAIL\n"); return 1; }

    /* the objects the statics make */
    hr = ((HRESULT (WINAPI *)(void *, void **))SLOT(s1, 12))(s1, &mgr);
    CHECK(hr == S_OK && mgr, "GetForCurrentView %#lx", hr);
    if (mgr)
    {
        const GUID *w[] = { &IID_Manager, &IID_Manager2 };
        iids(mgr, w, 2, "manager");
        class_name(mgr, name, "manager");
        ((ULONG (WINAPI *)(void *))SLOT(mgr, 2))(mgr);
    }
    hr = ((HRESULT (WINAPI *)(void *, HSTRING, void **))SLOT(s1, 9))(s1, ident, &ctx);
    CHECK(hr == S_OK && ctx, "CreateCurrentThreadNetworkContext %#lx", hr);
    if (ctx)
    {
        const GUID *w[] = { &IID_ThreadCtx, &IID_Closable };
        iids(ctx, w, 2, "context");
        class_name(ctx, L"Windows.Security.EnterpriseData.ThreadNetworkContext", "context");
        ((ULONG (WINAPI *)(void *))SLOT(ctx, 2))(ctx);
    }

    /* the access requests: allowed (0) */
    hr = ((HRESULT (WINAPI *)(void *, HSTRING, HSTRING, void **))SLOT(s1, 20))(s1, src, tgt, &op);
    CHECK(hr == S_OK, "RequestAccessAsync %#lx", hr);
    check_op(op, &IID_OpEval, &IID_HdlEval, 0, NULL, "RequestAccessAsync");
    class_name(NULL ? NULL : fac, name, "factory again");
    hr = ((HRESULT (WINAPI *)(void *, HSTRING, HSTRING, void **))SLOT(s2, 8))(s2, src, app, &op);
    CHECK(hr == S_OK, "RequestAccessForAppAsync %#lx", hr);
    check_op(op, &IID_OpEval, &IID_HdlEval, 0, NULL, "RequestAccessForAppAsync");
    hr = ((HRESULT (WINAPI *)(void *, HSTRING, HSTRING, void *, void **))SLOT(s3, 6))(s3, src, tgt, NULL, &op);
    CHECK(hr == S_OK, "RequestAccessWithAuditingInfoAsync %#lx", hr);
    check_op(op, &IID_OpEval, &IID_HdlEval, 0, NULL, "WithAuditingInfo");
    hr = ((HRESULT (WINAPI *)(void *, HSTRING, HSTRING, void *, HSTRING, void **))SLOT(s3, 7))(s3, src, tgt, NULL, msg, &op);
    CHECK(hr == S_OK, "RequestAccessWithMessageAsync %#lx", hr);
    check_op(op, &IID_OpEval, &IID_HdlEval, 0, NULL, "WithMessage");
    hr = ((HRESULT (WINAPI *)(void *, HSTRING, HSTRING, void *, void **))SLOT(s3, 8))(s3, src, app, NULL, &op);
    CHECK(hr == S_OK, "RequestAccessForAppWithAuditingInfoAsync %#lx", hr);
    check_op(op, &IID_OpEval, &IID_HdlEval, 0, NULL, "ForAppWithAuditingInfo");
    hr = ((HRESULT (WINAPI *)(void *, HSTRING, HSTRING, void *, HSTRING, void **))SLOT(s3, 9))(s3, src, app, NULL, msg, &op);
    CHECK(hr == S_OK, "RequestAccessForAppWithMessageAsync %#lx", hr);
    check_op(op, &IID_OpEval, &IID_HdlEval, 0, NULL, "ForAppWithMessage");
    hr = ((HRESULT (WINAPI *)(void *, HSTRING, HSTRING, void *, HSTRING, UINT32, void **))SLOT(s4, 7))(s4, src, tgt, NULL, msg, 0, &op);
    CHECK(hr == S_OK, "RequestAccessWithBehaviorAsync %#lx", hr);
    check_op(op, &IID_OpEval, &IID_HdlEval, 0, NULL, "WithBehavior");
    hr = ((HRESULT (WINAPI *)(void *, HSTRING, HSTRING, void *, HSTRING, UINT32, void **))SLOT(s4, 8))(s4, src, app, NULL, msg, 0, &op);
    CHECK(hr == S_OK, "RequestAccessForAppWithBehaviorAsync %#lx", hr);
    check_op(op, &IID_OpEval, &IID_HdlEval, 0, NULL, "ForAppWithBehavior");
    hr = ((HRESULT (WINAPI *)(void *, void *, HSTRING, void *, void **))SLOT(s4, 9))(s4, NULL, app, NULL, &op);
    CHECK(hr == S_OK, "RequestAccessToFilesForAppAsync %#lx", hr);
    check_op(op, &IID_OpEval, &IID_HdlEval, 0, NULL, "ToFilesForApp");
    hr = ((HRESULT (WINAPI *)(void *, void *, HSTRING, void *, HSTRING, UINT32, void **))SLOT(s4, 10))(s4, NULL, app, NULL, msg, 0, &op);
    CHECK(hr == S_OK, "RequestAccessToFilesForAppWithMessageAndBehaviorAsync %#lx", hr);
    check_op(op, &IID_OpEval, &IID_HdlEval, 0, NULL, "ToFilesForAppMsgBeh");
    hr = ((HRESULT (WINAPI *)(void *, void *, UINT32, void *, void **))SLOT(s4, 11))(s4, NULL, 4, NULL, &op);
    CHECK(hr == S_OK, "RequestAccessToFilesForProcessAsync %#lx", hr);
    check_op(op, &IID_OpEval, &IID_HdlEval, 0, NULL, "ToFilesForProcess");
    hr = ((HRESULT (WINAPI *)(void *, void *, UINT32, void *, HSTRING, UINT32, void **))SLOT(s4, 12))(s4, NULL, 4, NULL, msg, 0, &op);
    CHECK(hr == S_OK, "RequestAccessToFilesForProcessWithMessageAndBehaviorAsync %#lx", hr);
    check_op(op, &IID_OpEval, &IID_HdlEval, 0, NULL, "ToFilesForProcessMsgBeh");
    /* the window flavour answers through the iid asked for */
    hr = ((HRESULT (WINAPI *)(void *, HWND, HSTRING, HSTRING, const GUID *, void **))SLOT(io, 6))(io, NULL, src, tgt, &IID_OpEval, &op);
    CHECK(hr == S_OK, "RequestAccessForWindowAsync %#lx", hr);
    check_op(op, &IID_OpEval, &IID_HdlEval, 0, NULL, "ForWindow");
    op = (void *)1;
    hr = ((HRESULT (WINAPI *)(void *, HWND, HSTRING, HSTRING, const GUID *, void **))SLOT(io, 6))(io, NULL, src, tgt, &IID_Closable, &op);
    CHECK(FAILED(hr) && !op, "RequestAccessForWindowAsync with a wrong iid %#lx %p", hr, op);
    hr = ((HRESULT (WINAPI *)(void *, HSTRING, HSTRING, void **))SLOT(s1, 20))(s1, src, tgt, NULL);
    CHECK(hr == E_POINTER, "RequestAccessAsync NULL out %#lx", hr);

    /* does a file need protecting: no */
    hr = ((HRESULT (WINAPI *)(void *, void *, HSTRING, void **))SLOT(s4, 13))(s4, NULL, ident, &op);
    CHECK(hr == S_OK, "IsFileProtectionRequiredAsync %#lx", hr);
    check_op(op, &IID_OpBool, &IID_HdlBool, 0, NULL, "IsFileProtectionRequired");
    hr = ((HRESULT (WINAPI *)(void *, void *, HSTRING, HSTRING, void **))SLOT(s4, 14))(s4, NULL, ident, msg, &op);
    CHECK(hr == S_OK, "IsFileProtectionRequiredForNewFileAsync %#lx", hr);
    check_op(op, &IID_OpBool, &IID_HdlBool, 0, NULL, "IsFileProtectionRequiredForNewFile");

    /* the identity for an endpoint: none, an empty string */
    hr = ((HRESULT (WINAPI *)(void *, void *, void **))SLOT(s1, 10))(s1, NULL, &op);
    CHECK(hr == S_OK, "GetPrimaryManagedIdentityForNetworkEndpointAsync %#lx", hr);
    check_op(op, &IID_OpStr, &IID_HdlStr, 0, L"", "PrimaryIdentityForEndpoint");

    /* the COM entry point of the DLL */
    dll = LoadLibraryA("windows.security.enterprisedata.dll");
    if (dll)
    {
        typedef HRESULT (WINAPI *gco_t)(REFCLSID, REFIID, void **);
        gco_t gco = (gco_t)GetProcAddress(dll, "DllGetClassObject");
        void *p = (void *)1;
        if (gco) { hr = gco(&IID_Statics, &IID_IUnknown, &p); CHECK(hr == CLASS_E_CLASSNOTAVAILABLE && !p, "DllGetClassObject %#lx %p", hr, p); }
    }
    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    return f != 0;
}
