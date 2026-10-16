/* rtworkq: RtwqJoinWorkQueue / RtwqUnjoinWorkQueue (patch 3761) */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

#define E_INVALID_WORKQUEUE ((HRESULT)0xc00d36ff)  /* RTWQ_E_INVALID_WORKQUEUE */

typedef struct IUnk IUnk;
typedef struct { HRESULT (WINAPI *QI)(IUnk *, const GUID *, void **); ULONG (WINAPI *AddRef)(IUnk *); ULONG (WINAPI *Release)(IUnk *); } UnkVtbl;
struct IUnk { const UnkVtbl *v; };

typedef struct Res Res;
typedef struct
{
    UnkVtbl unk;
    HRESULT (WINAPI *GetState)(Res *, IUnk **);
    HRESULT (WINAPI *GetStatus)(Res *);
    HRESULT (WINAPI *SetStatus)(Res *, HRESULT);
    HRESULT (WINAPI *GetObject)(Res *, IUnk **);
    IUnk *(WINAPI *GetStateNoAddRef)(Res *);
} ResVtbl;
struct Res { const ResVtbl *v; };

typedef struct Cb Cb;
typedef struct
{
    UnkVtbl unk;
    HRESULT (WINAPI *GetParameters)(Cb *, DWORD *, DWORD *);
    HRESULT (WINAPI *Invoke)(Cb *, Res *);
} CbVtbl;
struct Cb
{
    const CbVtbl *v;
    HANDLE event;
    DWORD sleep_ms;
    volatile LONG calls, running, overlapped_runs;
    DWORD tid, first_start, last_end;
    HRESULT status;
    DWORD bytes;
};

/* RTWQASYNCRESULT */
typedef struct
{
    Res iface;
    OVERLAPPED overlapped;
    Cb *pCallback;
    HRESULT hrStatusResult;
    DWORD dwBytesTransferred;
    HANDLE hEvent;
} RESULT;

static HRESULT WINAPI cb_qi(IUnk *i, const GUID *g, void **o) { *o = i; return S_OK; }
static ULONG WINAPI cb_addref(IUnk *i) { return 2; }
static ULONG WINAPI cb_release(IUnk *i) { return 1; }
static HRESULT WINAPI cb_params(Cb *c, DWORD *flags, DWORD *queue) { return E_NOTIMPL; }
static HRESULT WINAPI cb_invoke(Cb *c, Res *r)
{
    RESULT *res = (RESULT *)r;
    DWORD start = GetTickCount();

    if (InterlockedIncrement(&c->running) > 1) InterlockedIncrement(&c->overlapped_runs);
    if (!c->calls) c->first_start = start;
    c->tid = GetCurrentThreadId();
    c->status = r->v->GetStatus(r);
    c->bytes = res->dwBytesTransferred;
    if (c->sleep_ms) Sleep(c->sleep_ms);
    c->last_end = GetTickCount();
    InterlockedDecrement(&c->running);
    InterlockedIncrement(&c->calls);
    SetEvent(c->event);
    return S_OK;
}
static const CbVtbl cb_vtbl = { { cb_qi, cb_addref, cb_release }, cb_params, cb_invoke };

static HRESULT (WINAPI *pStartup)(void), (WINAPI *pShutdown)(void);
static HRESULT (WINAPI *pAlloc)(int, DWORD *), (WINAPI *pLock)(DWORD), (WINAPI *pUnlock)(DWORD);
static HRESULT (WINAPI *pSerial)(DWORD, DWORD *);
static HRESULT (WINAPI *pJoin)(DWORD, HANDLE, HANDLE *), (WINAPI *pUnjoin)(DWORD, HANDLE);
static HRESULT (WINAPI *pCreateResult)(IUnk *, Cb *, IUnk *, Res **);

static HANDLE open_overlapped(const WCHAR *name)
{
    return CreateFileW(name, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,
                       FILE_FLAG_OVERLAPPED | FILE_ATTRIBUTE_NORMAL, NULL);
}

/* starts a write whose completion is the result's */
static BOOL write_with(HANDLE file, Cb *cb, DWORD offset, const void *data, DWORD len, Res **keep)
{
    Res *r = NULL;
    RESULT *res;
    BOOL ok;

    if (pCreateResult(NULL, cb, NULL, &r)) return FALSE;
    res = (RESULT *)r;
    memset(&res->overlapped, 0, sizeof(res->overlapped));
    res->overlapped.Offset = offset;
    ok = WriteFile(file, data, len, NULL, &res->overlapped);
    if (!ok && GetLastError() != ERROR_IO_PENDING) { printf("note: WriteFile error %lu\n", GetLastError()); return FALSE; }
    if (keep) *keep = r;
    return TRUE;
}

int main(void)
{
    HMODULE m = LoadLibraryA("rtworkq.dll");
    WCHAR path[MAX_PATH], pathb[MAX_PATH];
    HANDLE fa, fb, cookie = NULL, cookie2 = NULL, plain;
    DWORD q_serial, q_pool, q_target, tid_main = GetCurrentThreadId(), start, elapsed;
    Cb cb = { &cb_vtbl }, cbb = { &cb_vtbl };
    HRESULT hr;
    char data[4096];
    Res *r1 = NULL, *r2 = NULL;

    pStartup = (void *)GetProcAddress(m, "RtwqStartup");
    pShutdown = (void *)GetProcAddress(m, "RtwqShutdown");
    pAlloc = (void *)GetProcAddress(m, "RtwqAllocateWorkQueue");
    pLock = (void *)GetProcAddress(m, "RtwqLockWorkQueue");
    pUnlock = (void *)GetProcAddress(m, "RtwqUnlockWorkQueue");
    pSerial = (void *)GetProcAddress(m, "RtwqAllocateSerialWorkQueue");
    pJoin = (void *)GetProcAddress(m, "RtwqJoinWorkQueue");
    pUnjoin = (void *)GetProcAddress(m, "RtwqUnjoinWorkQueue");
    pCreateResult = (void *)GetProcAddress(m, "RtwqCreateAsyncResult");
    CHECK(pStartup && pJoin && pUnjoin && pCreateResult && pAlloc && pSerial, "exports");
    if (!pJoin || !pCreateResult) { printf("RESULT: FAIL\n"); return 1; }

    memset(data, 'x', sizeof(data));
    GetTempPathW(MAX_PATH, path);
    lstrcpyW(pathb, path);
    lstrcatW(path, L"fastb-rtwqjoin-a.tmp");
    lstrcatW(pathb, L"fastb-rtwqjoin-b.tmp");
    cb.event = CreateEventW(NULL, FALSE, FALSE, NULL);
    cbb.event = CreateEventW(NULL, FALSE, FALSE, NULL);

    hr = pStartup();
    CHECK(hr == S_OK, "startup %lx", hr);
    hr = pAlloc(2 /* RTWQ_MULTITHREADED_WORKQUEUE */, &q_pool);
    CHECK(hr == S_OK, "alloc: %lx", hr);

    /* arguments */
    fa = open_overlapped(path);
    CHECK(fa != INVALID_HANDLE_VALUE, "open");
    CHECK(pJoin(q_pool, fa, NULL) == E_POINTER, "NULL cookie");
    cookie = (HANDLE)1;
    hr = pJoin(q_pool, NULL, &cookie);
    CHECK(hr == E_INVALIDARG && cookie == NULL, "NULL handle: %lx %p", hr, cookie);
    hr = pJoin(q_pool, INVALID_HANDLE_VALUE, &cookie);
    CHECK(hr == E_INVALIDARG, "INVALID_HANDLE_VALUE: %lx", hr);
    hr = pJoin(0x12345678, fa, &cookie);
    CHECK(hr == E_INVALID_WORKQUEUE, "a queue that does not exist: %lx", hr);
    plain = CreateFileW(pathb, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    hr = pJoin(q_pool, plain, &cookie);
    CHECK(FAILED(hr), "a handle not opened for overlapped I/O: %lx", hr);
    CloseHandle(plain);
    CHECK(cookie == NULL, "no cookie on failure");

    /* the queue is kept alive by the join; one use of the join's reference is the unjoin */
    hr = pJoin(q_pool, fa, &cookie);
    CHECK(hr == S_OK && cookie != NULL, "join: %lx %p", hr, cookie);
    hr = pJoin(q_pool, fa, &cookie2);
    CHECK(FAILED(hr), "the same handle twice: %lx", hr);
    pUnlock(q_pool);   /* the allocation's own reference */
    hr = pLock(q_pool);
    CHECK(hr == S_OK, "the joined queue is still there: %lx", hr);
    pUnlock(q_pool);

    /* an I/O completes on the queue */
    CHECK(write_with(fa, &cb, 0, data, 100, &r1), "write");
    CHECK(WaitForSingleObject(cb.event, 10000) == WAIT_OBJECT_0, "the callback was invoked");
    CHECK(cb.status == S_OK && cb.bytes == 100, "status %lx bytes %lu", cb.status, cb.bytes);
    CHECK(cb.tid != tid_main && cb.tid != 0, "on another thread");
    {
        RESULT *res = (RESULT *)r1;
        CHECK(res->hrStatusResult == S_OK && res->dwBytesTransferred == 100, "the result fields: %lx %lu", res->hrStatusResult, res->dwBytesTransferred);
    }
    cb.calls = 0;

    /* wrong unjoins */
    CHECK(pUnjoin(q_pool, (HANDLE)((ULONG_PTR)cookie + 1000)) == E_INVALIDARG, "unknown cookie");
    CHECK(pUnjoin(q_pool + 1, cookie) == E_INVALIDARG, "wrong queue");
    hr = pUnjoin(q_pool, cookie);
    CHECK(hr == S_OK, "unjoin: %lx", hr);
    CHECK(pUnjoin(q_pool, cookie) == E_INVALIDARG, "unjoin twice");
    hr = pLock(q_pool);
    CHECK(hr == E_INVALID_WORKQUEUE, "the queue is gone with the join: %lx", hr);

    /* completions after the unjoin are dropped */
    ResetEvent(cb.event);
    CHECK(write_with(fa, &cb, 100, data, 10, NULL), "write after unjoin");
    CHECK(WaitForSingleObject(cb.event, 700) == WAIT_TIMEOUT && cb.calls == 0, "no callback after the unjoin (%ld)", cb.calls);
    CloseHandle(fa);

    /* a serial queue serializes the callbacks, a pool does not */
    pAlloc(2, &q_target);
    if (pSerial(q_target, &q_serial) != S_OK) { CHECK(0, "serial queue"); q_serial = q_target; }
    fa = open_overlapped(path);
    fb = open_overlapped(pathb);
    CHECK(pJoin(q_serial, fa, &cookie) == S_OK, "join serial");
    CHECK(pJoin(q_target, fb, &cookie2) == S_OK, "join pool");
    cb.sleep_ms = 300; cbb.sleep_ms = 300; cb.calls = cbb.calls = 0; cb.overlapped_runs = cbb.overlapped_runs = 0;
    start = GetTickCount();
    CHECK(write_with(fa, &cb, 0, data, 10, NULL) && write_with(fa, &cb, 10, data, 10, NULL) && write_with(fa, &cb, 20, data, 10, NULL), "writes to the serial queue");
    while (cb.calls < 3 && GetTickCount() - start < 8000) Sleep(20);
    elapsed = GetTickCount() - start;
    CHECK(cb.calls == 3, "three callbacks: %ld", cb.calls);
    CHECK(cb.overlapped_runs == 0, "serial: they did not run at the same time (%ld)", cb.overlapped_runs);
    CHECK(elapsed >= 850, "serial: one after the other, %lu ms", elapsed);
    start = GetTickCount();
    CHECK(write_with(fb, &cbb, 0, data, 10, NULL) && write_with(fb, &cbb, 10, data, 10, NULL) && write_with(fb, &cbb, 20, data, 10, NULL), "writes to the pool queue");
    while (cbb.calls < 3 && GetTickCount() - start < 8000) Sleep(20);
    elapsed = GetTickCount() - start;
    CHECK(cbb.calls == 3, "three callbacks: %ld", cbb.calls);
    CHECK(cbb.overlapped_runs > 0 && elapsed < 850, "pool: they ran at the same time (%ld overlaps, %lu ms)", cbb.overlapped_runs, elapsed);
    CHECK(pUnjoin(q_serial, cookie) == S_OK && pUnjoin(q_target, cookie2) == S_OK, "unjoin both");
    CloseHandle(fa); CloseHandle(fb);

    /* a failed I/O: the status is the error */
    fa = open_overlapped(path);
    CHECK(pJoin(q_target, fa, &cookie) == S_OK, "join for a failing read");
    {
        Res *rr = NULL;
        RESULT *res;
        char buf[16];
        BOOL ok;

        cb.sleep_ms = 0; cb.calls = 0; cb.status = S_OK; cb.bytes = 99;
        /* a completion of the earlier phases may have left the event signalled: the wait below
         * returned before this read's callback ran (seen under load at integration) */
        ResetEvent(cb.event);
        pCreateResult(NULL, &cb, NULL, &rr);
        res = (RESULT *)rr;
        memset(&res->overlapped, 0, sizeof(res->overlapped));
        res->overlapped.Offset = 1000000;   /* far behind the end of the file */
        ok = ReadFile(fa, buf, sizeof(buf), NULL, &res->overlapped);
        if (!ok && GetLastError() == ERROR_IO_PENDING || !ok && GetLastError() == ERROR_HANDLE_EOF)
        {
            if (WaitForSingleObject(cb.event, 3000) == WAIT_OBJECT_0)
            {
                CHECK(cb.status == HRESULT_FROM_WIN32(ERROR_HANDLE_EOF) && cb.bytes == 0, "failed read: status %lx bytes %lu", cb.status, cb.bytes);
            }
            else printf("note: no completion for a read at the end of the file\n");
        }
        else printf("note: read error %lu\n", GetLastError());
    }
    pUnjoin(q_target, cookie);
    CloseHandle(fa);

    /* shutdown with a join left over */
    fa = open_overlapped(path);
    CHECK(pJoin(q_target, fa, &cookie) == S_OK, "join again");
    hr = pShutdown();
    CHECK(hr == S_OK, "shutdown: %lx", hr);
    CloseHandle(fa);
    DeleteFileW(path); DeleteFileW(pathb);

    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    return f != 0;
}
