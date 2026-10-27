/* Windows.Services.Store.StoreContext (patches/sg/6150): a machine with no Store account and nothing
 * bought -- GetDefault; GetStoreProductsAsync, GetAssociatedStoreProductsAsync, GetUserCollectionAsync
 * give no products (ExtendedError S_OK); GetAppLicenseAsync an unlicensed, non-trial licence;
 * RequestPurchaseAsync NotPurchased; no updates; each a completed operation whose Completed handler
 * runs. Interface identity per windows-rs (MIT). */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <winstring.h>
#include <roapi.h>
#include <inspectable.h>
#include <stdio.h>

DEFINE_GUID(IID_Statics,  0x9c06ee5f, 0x15c0, 0x4e72, 0x93, 0x30, 0xd6, 0x19, 0x1c, 0xeb, 0xd1, 0x9c);
DEFINE_GUID(IID_Context,  0xac98b6be, 0xf4fd, 0x4912, 0xba, 0xbd, 0x50, 0x35, 0xe5, 0xe8, 0xbc, 0xab);
DEFINE_GUID(IID_License2, 0xb4666e91, 0x4443, 0x40b3, 0x99, 0x3f, 0x28, 0x90, 0x44, 0x35, 0xbd, 0xc6);
DEFINE_GUID(IID_Info,     0x00000036, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
typedef HRESULT (WINAPI *qi_fn)(obj *, REFIID, void **);
static HSTRING hs(const WCHAR *s) { HSTRING h; WindowsCreateString(s, lstrlenW(s), &h); return h; }
static obj *as(void *o, REFIID iid) { obj *r = NULL; if (o) ((qi_fn)SLOT((obj *)o, 0))(o, iid, (void **)&r); return r; }

/* a Completed handler: records the status */
struct handler { void **vtbl; LONG ref; int calls, status; };
static HRESULT WINAPI h_qi(struct handler *h, REFIID iid, void **out) { *out = h; return S_OK; }
static ULONG WINAPI h_addref(struct handler *h) { return ++h->ref; }
static ULONG WINAPI h_release(struct handler *h) { return --h->ref; }
static HRESULT WINAPI h_invoke(struct handler *h, void *op, int status) { h->calls++; h->status = status; return S_OK; }
static void *h_vtbl[] = { h_qi, h_addref, h_release, h_invoke };

/* the result of a completed operation, its handler having run */
static obj *result(obj *op, int *ok)
{
    struct handler h = { h_vtbl, 1 };
    obj *r = NULL;
    int status = -1;
    if (!op) { *ok = 0; return NULL; }
    ((HRESULT (WINAPI *)(obj *, void *))SLOT(op, 6))(op, &h);
    ((HRESULT (WINAPI *)(obj *, int *))SLOT(as(op, &IID_Info), 7))(as(op, &IID_Info), &status);
    ((HRESULT (WINAPI *)(obj *, obj **))SLOT(op, 8))(op, &r);
    *ok = h.calls == 1 && h.status == 1 && status == 1;
    return r;
}
static UINT32 size_of(obj *coll) { UINT32 n = 99; if (coll) ((HRESULT (WINAPI *)(obj *, UINT32 *))SLOT(coll, 7))(coll, &n); return n; }

int main(void)
{
    obj *st = NULL, *ctx = NULL, *op, *res, *products;
    HRESULT hr, err;
    int ok;

    RoInitialize(RO_INIT_MULTITHREADED);
    hr = RoGetActivationFactory(hs(L"Windows.Services.Store.StoreContext"), &IID_Statics, (void **)&st);
    printf("factory=%08lx\n", hr);
    if (FAILED(hr)) return 1;
    hr = ((HRESULT (WINAPI *)(obj *, obj **))SLOT(st, 6))(st, &ctx);
    printf("default=%08lx context=%d\n", hr, as(ctx, &IID_Context) != NULL);

    /* GetStoreProductsAsync(kinds, ids) */
    op = NULL; ((HRESULT (WINAPI *)(obj *, void *, void *, obj **))SLOT(ctx, 13))(ctx, NULL, NULL, &op);
    res = result(op, &ok); products = NULL; err = E_FAIL;
    if (res) { ((HRESULT (WINAPI *)(obj *, obj **))SLOT(res, 6))(res, &products); ((HRESULT (WINAPI *)(obj *, HRESULT *))SLOT(res, 7))(res, &err); }
    printf("products=%d,%u,%08lx\n", ok, size_of(products), err);
    /* GetAssociatedStoreProductsAsync(kinds) */
    op = NULL; ((HRESULT (WINAPI *)(obj *, void *, obj **))SLOT(ctx, 14))(ctx, NULL, &op);
    res = result(op, &ok); products = NULL;
    if (res) ((HRESULT (WINAPI *)(obj *, obj **))SLOT(res, 6))(res, &products);
    printf("addons=%d,%u\n", ok, size_of(products));
    /* GetUserCollectionAsync(kinds) */
    op = NULL; ((HRESULT (WINAPI *)(obj *, void *, obj **))SLOT(ctx, 16))(ctx, NULL, &op);
    res = result(op, &ok); products = NULL;
    if (res) ((HRESULT (WINAPI *)(obj *, obj **))SLOT(res, 6))(res, &products);
    printf("collection=%d,%u\n", ok, size_of(products));
    /* GetAppLicenseAsync */
    op = NULL; ((HRESULT (WINAPI *)(obj *, obj **))SLOT(ctx, 11))(ctx, &op);
    res = result(op, &ok);
    {
        boolean active = 2, trial = 2, disc = 2;
        obj *addons = NULL;
        if (res)
        {
            ((HRESULT (WINAPI *)(obj *, boolean *))SLOT(res, 7))(res, &active);
            ((HRESULT (WINAPI *)(obj *, boolean *))SLOT(res, 8))(res, &trial);
            ((HRESULT (WINAPI *)(obj *, obj **))SLOT(res, 11))(res, &addons);
            ((HRESULT (WINAPI *)(obj *, boolean *))SLOT(as(res, &IID_License2), 6))(as(res, &IID_License2), &disc);
        }
        printf("license=%d,%d,%d,%u,%d\n", ok, active, trial, size_of(addons), disc);
    }
    /* RequestPurchaseAsync(id) */
    op = NULL; ((HRESULT (WINAPI *)(obj *, HSTRING, obj **))SLOT(ctx, 21))(ctx, hs(L"9N9DNPT6D6Z9"), &op);
    res = result(op, &ok);
    {
        int status = -1;
        if (res) ((HRESULT (WINAPI *)(obj *, int *))SLOT(res, 6))(res, &status);
        printf("purchase=%d,%d\n", ok, status);
    }
    /* GetAppAndOptionalStorePackageUpdatesAsync */
    op = NULL; ((HRESULT (WINAPI *)(obj *, obj **))SLOT(ctx, 23))(ctx, &op);
    res = result(op, &ok);
    printf("updates=%d,%u\n", ok, size_of(res));
    return 0;
}
