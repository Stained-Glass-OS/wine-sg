/* CNG DPAPI (patch 2436): protection descriptors, NCryptProtectSecret / NCryptUnprotectSecret and the
 * NCryptStream* calls, 64 and 32 bit. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef ULONG_PTR DESC;
typedef void *(WINAPI *PFN_ALLOC)(SIZE_T);
typedef void (WINAPI *PFN_FREE)(void *);
typedef struct { DWORD cbSize; PFN_ALLOC pfnAlloc; PFN_FREE pfnFree; } ALLOC_PARA;
typedef HRESULT (WINAPI *STREAMCB)(void *, const BYTE *, SIZE_T, BOOL);
typedef struct { STREAMCB cb; void *ctx; } STREAM_INFO;

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

#define NTE_INVALID_PARAMETER_ ((LONG)0x80090027)
#define NTE_INVALID_HANDLE_ ((LONG)0x80090026)
#define NTE_BAD_DATA_ ((LONG)0x80090005)
#define NTE_PERM_ ((LONG)0x80090010)

static LONG (WINAPI *pCreate)(const WCHAR *, DWORD, DESC *);
static LONG (WINAPI *pClose)(DESC);
static LONG (WINAPI *pInfo)(DESC, const ALLOC_PARA *, DWORD, void **);
static LONG (WINAPI *pProtect)(DESC, DWORD, const BYTE *, ULONG, const ALLOC_PARA *, HWND, BYTE **, ULONG *);
static LONG (WINAPI *pUnprotect)(DESC *, DWORD, const BYTE *, ULONG, const ALLOC_PARA *, HWND, BYTE **, ULONG *);
static LONG (WINAPI *pOpenProtect)(DESC, DWORD, HWND, STREAM_INFO *, DESC *);
static LONG (WINAPI *pOpenUnprotect)(STREAM_INFO *, DWORD, HWND, DESC *);
static LONG (WINAPI *pUpdate)(DESC, const BYTE *, SIZE_T, BOOL);
static LONG (WINAPI *pStreamClose)(DESC);

static int allocs;
static void *WINAPI my_alloc(SIZE_T n) { allocs++; return HeapAlloc(GetProcessHeap(), 0, n); }
static void WINAPI my_free(void *p) { HeapFree(GetProcessHeap(), 0, p); }

struct sink { BYTE data[512]; SIZE_T size; int calls; BOOL final; };
static HRESULT WINAPI sink_cb(void *ctx, const BYTE *data, SIZE_T size, BOOL final)
{
    struct sink *s = ctx;
    memcpy(s->data + s->size, data, size);
    s->size += size;
    s->calls++;
    s->final = final;
    return S_OK;
}

static const void *memmem_(const BYTE *h, SIZE_T hl, const BYTE *n, SIZE_T nl)
{
    SIZE_T i;
    for (i = 0; i + nl <= hl; i++) if (!memcmp(h + i, n, nl)) return h + i;
    return NULL;
}

static BOOL round_trip(const WCHAR *descriptor, const char *label)
{
    static const BYTE secret[] = "the secret of SG";
    DESC d = 0, d2 = 0;
    BYTE *blob = NULL, *plain = NULL;
    ULONG blob_size = 0, plain_size = 0;
    LONG st;
    char what[128];
    BOOL ok;

    st = pCreate(descriptor, 0, &d);
    snprintf(what, sizeof(what), "%s: create descriptor", label);
    check(st == 0 && d, what);
    if (st) return FALSE;
    st = pProtect(d, 0, secret, sizeof(secret), NULL, NULL, &blob, &blob_size);
    snprintf(what, sizeof(what), "%s: NCryptProtectSecret (%#lx)", label, st);
    check(st == 0 && blob && blob_size > sizeof(secret), what);
    if (st) { pClose(d); return FALSE; }
    check(!memmem_(blob, blob_size, secret, sizeof(secret) - 1), "the blob does not hold the secret in the clear");
    st = pUnprotect(&d2, 0, blob, blob_size, NULL, NULL, &plain, &plain_size);
    ok = st == 0 && plain_size == sizeof(secret) && !memcmp(plain, secret, sizeof(secret));
    snprintf(what, sizeof(what), "%s: NCryptUnprotectSecret gives the secret back (%#lx)", label, st);
    check(ok, what);
    if (d2)
    {
        WCHAR *str = NULL;
        check(pInfo(d2, NULL, 1, (void **)&str) == 0 && str && !lstrcmpiW(str, descriptor),
              "the descriptor handle from Unprotect names the descriptor");
        if (str) LocalFree(str);
        check(pClose(d2) == 0, "and it closes");
    }
    if (blob) LocalFree(blob);
    if (plain) LocalFree(plain);
    pClose(d);
    return ok;
}

int main(void)
{
    HMODULE mod = LoadLibraryA("ncrypt.dll");
    DESC d = 0, d2 = 0, stream = 0;
    BYTE *blob = NULL, *plain = NULL, buf[16] = {0};
    ULONG blob_size = 0, plain_size = 0;
    ALLOC_PARA para = { sizeof(para), my_alloc, my_free };
    STREAM_INFO info;
    struct sink sink = {0};
    WCHAR *str = NULL;
    LONG st;

#define LOAD(n, v) v = (void *)GetProcAddress(mod, n)
    LOAD("NCryptCreateProtectionDescriptor", pCreate); LOAD("NCryptCloseProtectionDescriptor", pClose);
    LOAD("NCryptGetProtectionDescriptorInfo", pInfo); LOAD("NCryptProtectSecret", pProtect);
    LOAD("NCryptUnprotectSecret", pUnprotect); LOAD("NCryptStreamOpenToProtect", pOpenProtect);
    LOAD("NCryptStreamOpenToUnprotect", pOpenUnprotect); LOAD("NCryptStreamUpdate", pUpdate);
    LOAD("NCryptStreamClose", pStreamClose);
    if (!mod || !pCreate) { printf("SKIP\nRESULT: PASS\n"); return 0; }

    /* descriptors */
    check(pCreate(NULL, 0, &d) == NTE_INVALID_PARAMETER_, "NULL descriptor string");
    check(pCreate(L"LOCAL=user", 0, NULL) == NTE_INVALID_PARAMETER_, "NULL handle");
    check(pCreate(L"LOCAL=user", 0x80, &d) == NTE_INVALID_PARAMETER_, "unknown flag");
    check(pCreate(L"", 0, &d) == NTE_INVALID_PARAMETER_, "empty string");
    check(pCreate(L"LOCAL=nobody", 0, &d) == NTE_INVALID_PARAMETER_, "unknown LOCAL value");
    check(pCreate(L"FOO=bar", 0, &d) == NTE_INVALID_PARAMETER_, "unknown rule");
    check(pCreate(L"SID=not-a-sid", 0, &d) == NTE_INVALID_PARAMETER_, "bad SID");
    check(pCreate(L"LOCAL=user AND SID=S-1-1-0 OR LOCAL=machine", 0, &d) == NTE_INVALID_PARAMETER_, "AND and OR mixed");
    check(pCreate(L"LOCAL=user AND", 0, &d) == NTE_INVALID_PARAMETER_, "a dangling AND");
    check(pClose(0) == NTE_INVALID_HANDLE_, "closing no handle");
    check(pCreate(L"LOCAL=user", 0, &d) == 0 && d, "LOCAL=user");
    check(pClose(d) == 0, "closes");
    check(pClose(d) == NTE_INVALID_HANDLE_, "and not twice");

    check(pCreate(L"LOCAL=user", 0, &d) == 0, "descriptor for info");
    check(pInfo(d, NULL, 1, (void **)&str) == 0 && str && !lstrcmpW(str, L"LOCAL=user"), "GetProtectionDescriptorInfo: the string");
    if (str) LocalFree(str);
    str = NULL;
    allocs = 0;
    check(pInfo(d, &para, 1, (void **)&str) == 0 && str && allocs == 1, "it uses the caller's allocator");
    if (str) my_free(str);
    check(pInfo(d, NULL, 7, (void **)&str) == NTE_INVALID_PARAMETER_, "unknown info type");
    check(pInfo(0, NULL, 1, (void **)&str) == NTE_INVALID_HANDLE_, "bad handle");

    /* protecting */
    check(pProtect(d, 0, NULL, 4, NULL, NULL, &blob, &blob_size) == NTE_INVALID_PARAMETER_, "no data");
    check(pProtect(0, 0, buf, 4, NULL, NULL, &blob, &blob_size) == NTE_INVALID_HANDLE_, "bad descriptor");
    allocs = 0;
    st = pProtect(d, 0, (const BYTE *)"abcd", 4, &para, NULL, &blob, &blob_size);
    check(st == 0 && allocs == 1, "the blob comes from the caller's allocator");
    if (blob) my_free(blob);
    pClose(d);

    check(round_trip(L"LOCAL=user", "user"), "round trip: user");
    check(round_trip(L"LOCAL=machine", "machine"), "round trip: machine");
    check(round_trip(L"SID=S-1-1-0", "Everyone"), "round trip: SID Everyone");
    check(round_trip(L"SID=S-1-5-21-111-222-333-4444 OR SID=S-1-1-0", "OR with a SID that is in the token"), "round trip: OR");

    /* rules that are not met */
    pCreate(L"SID=S-1-5-21-111-222-333-4444", 0, &d);
    st = pProtect(d, 0, (const BYTE *)"abcd", 4, NULL, NULL, &blob, &blob_size);
    check(st == 0, "protect for a SID that is not ours");
    st = pUnprotect(NULL, 0, blob, blob_size, NULL, NULL, &plain, &plain_size);
    { char m[80]; snprintf(m, sizeof m, "unprotecting it is refused (NTE_PERM) st=%lx", st); check(st == NTE_PERM_ && !plain, m); }
    if (blob) LocalFree(blob);
    pClose(d);
    pCreate(L"SID=S-1-1-0 AND SID=S-1-5-21-111-222-333-4444", 0, &d);
    pProtect(d, 0, (const BYTE *)"abcd", 4, NULL, NULL, &blob, &blob_size);
    st = pUnprotect(NULL, 0, blob, blob_size, NULL, NULL, &plain, &plain_size);
    check(st == NTE_PERM_, "AND needs every rule");
    if (blob) LocalFree(blob);
    pClose(d);

    /* bad blobs */
    check(pUnprotect(NULL, 0, (const BYTE *)"garbage garbage garbage garbage", 32, NULL, NULL, &plain, &plain_size) == NTE_BAD_DATA_,
          "garbage is NTE_BAD_DATA");
    check(pUnprotect(NULL, 0, NULL, 0, NULL, NULL, &plain, &plain_size) == NTE_INVALID_PARAMETER_, "no blob");
    pCreate(L"LOCAL=user", 0, &d);
    pProtect(d, 0, (const BYTE *)"abcd", 4, NULL, NULL, &blob, &blob_size);
    blob[blob_size - 2] ^= 0x55;
    st = pUnprotect(NULL, 0, blob, blob_size, NULL, NULL, &plain, &plain_size);
    check(st != 0 && !plain, "a damaged blob is refused");
    LocalFree(blob); blob = NULL;

    /* streams */
    info.cb = sink_cb; info.ctx = &sink;
    check(pOpenProtect(d, 0, NULL, &info, &stream) == 0 && stream, "NCryptStreamOpenToProtect");
    check(pUpdate(stream, (const BYTE *)"hello ", 6, FALSE) == 0 && sink.calls == 0, "a piece: nothing out yet");
    check(pUpdate(stream, (const BYTE *)"world", 5, TRUE) == 0 && sink.calls == 1 && sink.final && sink.size > 11, "the last piece: the blob");
    check(pUpdate(stream, (const BYTE *)"x", 1, TRUE) != 0, "no more after the last");
    check(pStreamClose(stream) == 0, "NCryptStreamClose");
    check(pStreamClose(stream) == NTE_INVALID_HANDLE_, "not twice");
    {
        struct sink out = {0};
        STREAM_INFO info2 = { sink_cb, &out };

        check(pOpenUnprotect(&info2, 0, NULL, &stream) == 0, "NCryptStreamOpenToUnprotect");
        pUpdate(stream, sink.data, sink.size / 2, FALSE);
        check(pUpdate(stream, sink.data + sink.size / 2, sink.size - sink.size / 2, TRUE) == 0 &&
              out.size == 11 && !memcmp(out.data, "hello world", 11), "the pieces unprotect to what went in");
        pStreamClose(stream);
    }
    pClose(d);
    (void)d2;
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
