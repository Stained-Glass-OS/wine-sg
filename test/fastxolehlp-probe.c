#include <windows.h>
#include <ole2.h>
#include <transact.h>
#include <txdtc.h>
#include <stdio.h>

static const GUID IID_RMF2 = {0x6b369c21,0xfbd2,0x11d1,{0x8f,0x47,0x00,0xc0,0x4f,0x8e,0xe5,0x7d}};
static const GUID IID_TRA = {0x69e971f0,0x23ce,0x11cf,{0xad,0x60,0x00,0xaa,0x00,0xa7,0x4c,0xcd}};
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

struct res
{
    ITransactionResourceAsync iface;
    LONG ref;
    ITransactionEnlistmentAsync *enlist;
    HRESULT vote;      /* answer to the prepare request */
    int prepares, commits, aborts;
    BOOL delay;        /* answer from another thread */
    BOOL mute;         /* never answer a prepare request */
    HANDLE thread;
    enum { PH_PREPARE, PH_COMMIT, PH_ABORT } phase;
};

static struct res *impl(ITransactionResourceAsync *i) { return (struct res *)i; }

static HRESULT WINAPI r_qi(ITransactionResourceAsync *i, REFIID riid, void **o)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_TRA)) { *o = i; i->lpVtbl->AddRef(i); return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI r_ar(ITransactionResourceAsync *i) { return InterlockedIncrement(&impl(i)->ref); }
static ULONG WINAPI r_rl(ITransactionResourceAsync *i) { return InterlockedDecrement(&impl(i)->ref); }

static DWORD WINAPI answer(void *arg)
{
    struct res *r = arg;
    Sleep(50);
    switch (r->phase)
    {
    case PH_PREPARE: r->enlist->lpVtbl->PrepareRequestDone(r->enlist, r->vote, NULL, NULL); break;
    case PH_COMMIT: r->enlist->lpVtbl->CommitRequestDone(r->enlist, S_OK); break;
    case PH_ABORT: r->enlist->lpVtbl->AbortRequestDone(r->enlist, S_OK); break;
    }
    return 0;
}

static void reply(struct res *r)
{
    if (r->delay)
    {
        if (r->thread) { WaitForSingleObject(r->thread, 2000); CloseHandle(r->thread); }
        r->thread = CreateThread(NULL, 0, answer, r, 0, NULL);
        return;
    }
    answer(r);
}
static HRESULT WINAPI r_prepare(ITransactionResourceAsync *i, BOOL ret, DWORD rm, BOOL mk, BOOL single)
{ struct res *r = impl(i); r->prepares++; r->phase = PH_PREPARE; if (!r->mute) reply(r); return S_OK; }
static HRESULT WINAPI r_commit(ITransactionResourceAsync *i, DWORD rm, XACTUOW *u)
{ struct res *r = impl(i); r->commits++; r->phase = PH_COMMIT; reply(r); return S_OK; }
static HRESULT WINAPI r_abort(ITransactionResourceAsync *i, BOID *b, BOOL ret, XACTUOW *u)
{ struct res *r = impl(i); r->aborts++; r->phase = PH_ABORT; reply(r); return S_OK; }
static HRESULT WINAPI r_down(ITransactionResourceAsync *i) { return S_OK; }

static ITransactionResourceAsyncVtbl rvtbl = { r_qi, r_ar, r_rl, r_prepare, r_commit, r_abort, r_down };

static void init_res(struct res *r, HRESULT vote, BOOL delay)
{
    memset(r, 0, sizeof(*r));
    r->iface.lpVtbl = &rvtbl;
    r->ref = 1;
    r->vote = vote;
    r->delay = delay;
}

static void enlist(IResourceManager *rm, ITransaction *tx, struct res *r)
{
    XACTUOW uow;
    LONG iso = 0;
    HRESULT hr = rm->lpVtbl->Enlist(rm, tx, &r->iface, &uow, &iso, &r->enlist);
    CHECK(hr == S_OK && r->enlist, "enlist %08lx", hr);
}

int main(void)
{
    HMODULE mod;
    HRESULT (__cdecl *pGet)(char *, char *, REFIID, DWORD, WORD, void *, void **);
    ITransactionDispenser *disp;
    IResourceManagerFactory2 *factory;
    IResourceManager *rm;
    ITransaction *tx;
    ITransactionOptions *opts;
    XACTTRANSINFO info;
    struct res a, b;
    GUID guid = { 1, 2, 3, { 4 } };
    XACTUOW uow;
    LONG iso;
    ITransactionEnlistmentAsync *e;
    HRESULT hr;
    void *p;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    mod = LoadLibraryA("xolehlp.dll");
    pGet = (void *)GetProcAddress(mod, "DtcGetTransactionManager");
    CHECK(pGet != NULL, "export");
    if (!pGet) goto done;

    hr = pGet((char *)"remote.example", NULL, &IID_ITransactionDispenser, 0, 0, NULL, &p);
    CHECK(hr == XACT_E_TMNOTAVAILABLE, "remote %08lx", hr);

    hr = pGet(NULL, NULL, &IID_ITransactionDispenser, 0, 0, NULL, (void **)&disp);
    CHECK(hr == S_OK, "dispenser %08lx", hr);
    hr = disp->lpVtbl->QueryInterface(disp, &IID_RMF2, (void **)&factory);
    CHECK(hr == S_OK, "factory");
    hr = factory->lpVtbl->Create(factory, &guid, (char *)"sg", NULL, &rm);
    CHECK(hr == S_OK, "rm create");

    /* two phase commit with an asynchronous and a synchronous resource */
    hr = disp->lpVtbl->BeginTransaction(disp, NULL, ISOLATIONLEVEL_READCOMMITTED, 0, NULL, &tx);
    CHECK(hr == S_OK, "begin");
    hr = tx->lpVtbl->GetTransactionInfo(tx, &info);
    CHECK(hr == S_OK && info.isoLevel == ISOLATIONLEVEL_READCOMMITTED, "info");
    init_res(&a, S_OK, TRUE);
    init_res(&b, S_OK, FALSE);
    enlist(rm, tx, &a);
    enlist(rm, tx, &b);
    hr = tx->lpVtbl->Commit(tx, FALSE, 0, 0);
    CHECK(hr == S_OK, "commit %08lx", hr);
    CHECK(a.prepares == 1 && a.commits == 1 && a.aborts == 0, "a %d %d %d", a.prepares, a.commits, a.aborts);
    CHECK(b.prepares == 1 && b.commits == 1 && b.aborts == 0, "b %d %d %d", b.prepares, b.commits, b.aborts);
    hr = tx->lpVtbl->Commit(tx, FALSE, 0, 0);
    CHECK(hr == XACT_E_NOTRANSACTION, "second commit %08lx", hr);
    hr = rm->lpVtbl->Enlist(rm, tx, &a.iface, &uow, &iso, &e);
    CHECK(hr == XACT_E_NOTRANSACTION, "enlist after commit %08lx", hr);
    a.enlist->lpVtbl->Release(a.enlist);
    b.enlist->lpVtbl->Release(b.enlist);
    tx->lpVtbl->Release(tx);

    /* a read-only vote needs no second phase */
    disp->lpVtbl->BeginTransaction(disp, NULL, ISOLATIONLEVEL_READCOMMITTED, 0, NULL, &tx);
    init_res(&a, XACT_S_READONLY, FALSE);
    init_res(&b, S_OK, FALSE);
    enlist(rm, tx, &a);
    enlist(rm, tx, &b);
    hr = tx->lpVtbl->Commit(tx, FALSE, 0, 0);
    CHECK(hr == S_OK && a.commits == 0 && b.commits == 1, "readonly %08lx %d %d", hr, a.commits, b.commits);
    a.enlist->lpVtbl->Release(a.enlist);
    b.enlist->lpVtbl->Release(b.enlist);
    tx->lpVtbl->Release(tx);

    /* a failing vote aborts the transaction */
    disp->lpVtbl->BeginTransaction(disp, NULL, ISOLATIONLEVEL_READCOMMITTED, 0, NULL, &tx);
    init_res(&a, S_OK, FALSE);
    init_res(&b, E_FAIL, FALSE);
    enlist(rm, tx, &a);
    enlist(rm, tx, &b);
    hr = tx->lpVtbl->Commit(tx, FALSE, 0, 0);
    CHECK(hr == XACT_E_ABORTED && a.commits == 0 && a.aborts == 1, "abort %08lx %d %d", hr, a.commits, a.aborts);
    a.enlist->lpVtbl->Release(a.enlist);
    b.enlist->lpVtbl->Release(b.enlist);
    tx->lpVtbl->Release(tx);

    /* explicit abort */
    disp->lpVtbl->BeginTransaction(disp, NULL, ISOLATIONLEVEL_READCOMMITTED, 0, NULL, &tx);
    init_res(&a, S_OK, TRUE);
    enlist(rm, tx, &a);
    hr = tx->lpVtbl->Abort(tx, NULL, FALSE, FALSE);
    CHECK(hr == S_OK && a.aborts == 1 && a.prepares == 0, "explicit abort %08lx", hr);
    hr = tx->lpVtbl->Commit(tx, FALSE, 0, 0);
    CHECK(hr == XACT_E_ABORTED, "commit after abort %08lx", hr);
    a.enlist->lpVtbl->Release(a.enlist);
    tx->lpVtbl->Release(tx);

    /* timeout: a resource that never answers */
    disp->lpVtbl->GetOptionsObject(disp, &opts);
    {
        XACTOPT o = { 300, "t" };
        opts->lpVtbl->SetOptions(opts, &o);
    }
    disp->lpVtbl->BeginTransaction(disp, NULL, ISOLATIONLEVEL_READCOMMITTED, 0, opts, &tx);
    init_res(&a, S_OK, FALSE);
    a.mute = TRUE;
    enlist(rm, tx, &a);
    hr = tx->lpVtbl->Commit(tx, FALSE, 0, 0);
    CHECK(hr == XACT_E_ABORTED && a.prepares == 1 && a.aborts == 1, "timeout %08lx %d %d", hr, a.prepares, a.aborts);
    a.enlist->lpVtbl->Release(a.enlist);
    tx->lpVtbl->Release(tx);
    opts->lpVtbl->Release(opts);

    hr = rm->lpVtbl->GetDistributedTransactionManager(rm, &IID_ITransactionDispenser, &p);
    CHECK(hr == S_OK && p == (void *)disp, "get dtc %08lx", hr);
    if (hr == S_OK) disp->lpVtbl->Release(disp);

    rm->lpVtbl->Release(rm);
    factory->lpVtbl->Release(factory);
    disp->lpVtbl->Release(disp);
done:
    CoUninitialize();
    printf(fails ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", fails);
    return fails != 0;
}
