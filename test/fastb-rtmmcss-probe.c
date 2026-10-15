/* rtworkq: the MMCSS association of work queues (patch 3743) */
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
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
struct Cb { const CbVtbl *v; HANDLE event; Res *result; IUnk *state; };

static HRESULT WINAPI cb_qi(IUnk *i, const GUID *g, void **o) { *o = i; return S_OK; }
static ULONG WINAPI cb_addref(IUnk *i) { return 2; }
static ULONG WINAPI cb_release(IUnk *i) { return 1; }
static HRESULT WINAPI cb_params(Cb *c, DWORD *flags, DWORD *queue) { return E_NOTIMPL; }
static HRESULT WINAPI cb_invoke(Cb *c, Res *r)
{
    r->v->unk.AddRef((IUnk *)r);
    c->result = r;
    SetEvent(c->event);
    return S_OK;
}
static const CbVtbl cb_vtbl = { { cb_qi, cb_addref, cb_release }, cb_params, cb_invoke };

/* an object that is not one of ours, to pass to the End calls */
static HRESULT WINAPI st_qi(IUnk *i, const GUID *g, void **o) { *o = i; return S_OK; }
static const UnkVtbl st_vtbl = { st_qi, cb_addref, cb_release };

int main(void)
{
    HMODULE m = LoadLibraryA("rtworkq.dll");
    HRESULT (WINAPI *pStartup)(void) = (void *)GetProcAddress(m, "RtwqStartup");
    HRESULT (WINAPI *pShutdown)(void) = (void *)GetProcAddress(m, "RtwqShutdown");
    HRESULT (WINAPI *pAlloc)(int, DWORD *) = (void *)GetProcAddress(m, "RtwqAllocateWorkQueue");
    HRESULT (WINAPI *pUnlock)(DWORD) = (void *)GetProcAddress(m, "RtwqUnlockWorkQueue");
    HRESULT (WINAPI *pBeginReg)(DWORD, const WCHAR *, DWORD, LONG, Cb *, IUnk *) = (void *)GetProcAddress(m, "RtwqBeginRegisterWorkQueueWithMMCSS");
    HRESULT (WINAPI *pEndReg)(Res *, DWORD *) = (void *)GetProcAddress(m, "RtwqEndRegisterWorkQueueWithMMCSS");
    HRESULT (WINAPI *pBeginUnreg)(DWORD, Cb *, IUnk *) = (void *)GetProcAddress(m, "RtwqBeginUnregisterWorkQueueWithMMCSS");
    HRESULT (WINAPI *pEndUnreg)(Res *) = (void *)GetProcAddress(m, "RtwqEndUnregisterWorkQueueWithMMCSS");
    HRESULT (WINAPI *pGetClass)(DWORD, WCHAR *, DWORD *) = (void *)GetProcAddress(m, "RtwqGetWorkQueueMMCSSClass");
    HRESULT (WINAPI *pGetTask)(DWORD, DWORD *) = (void *)GetProcAddress(m, "RtwqGetWorkQueueMMCSSTaskId");
    HRESULT (WINAPI *pGetPrio)(DWORD, LONG *) = (void *)GetProcAddress(m, "RtwqGetWorkQueueMMCSSPriority");
    HRESULT (WINAPI *pRegPlat)(const WCHAR *, DWORD *, LONG) = (void *)GetProcAddress(m, "RtwqRegisterPlatformWithMMCSS");
    HRESULT (WINAPI *pUnregPlat)(void) = (void *)GetProcAddress(m, "RtwqUnregisterPlatformFromMMCSS");
    HRESULT (WINAPI *pCreateResult)(IUnk *, Cb *, IUnk *, Res **) = (void *)GetProcAddress(m, "RtwqCreateAsyncResult");
    HRESULT hr;
    DWORD queue = 0, task = 0, task2 = 0, len;
    LONG prio;
    WCHAR buf[64];
    Cb cb = { &cb_vtbl };
    IUnk state_obj = { &st_vtbl };
    IUnk *foreign_obj;
    IUnk *got_state;
    Res *foreign;

    if (!pBeginReg || !pCreateResult) { printf("FAIL exports\nRESULT: FAIL\n"); return 1; }
    cb.event = CreateEventW(NULL, FALSE, FALSE, NULL);
    hr = pStartup(); CHECK(hr == S_OK, "startup %#lx", hr);
    hr = pAlloc(0, &queue); CHECK(hr == S_OK && queue, "alloc %#lx", hr);

    /* nothing registered */
    len = 7; wcscpy(buf, L"junk");
    hr = pGetClass(queue, buf, &len); CHECK(hr == S_OK && len == 0, "class of an unregistered queue: %#lx %lu", hr, len);
    task = 99; hr = pGetTask(queue, &task); CHECK(hr == S_OK && task == 0, "task id: %#lx %lu", hr, task);
    prio = 99; hr = pGetPrio(queue, &prio); CHECK(hr == S_OK && prio == 0, "priority: %#lx %ld", hr, prio);

    /* argument checks */
    hr = pBeginReg(queue, NULL, 0, 0, &cb, &state_obj); CHECK(hr == E_INVALIDARG, "NULL class: %#lx", hr);
    hr = pBeginReg(queue, L"", 0, 0, &cb, &state_obj); CHECK(hr == E_INVALIDARG, "empty class: %#lx", hr);
    hr = pBeginReg(queue, L"Audio", 0, 0, NULL, &state_obj); CHECK(hr == E_INVALIDARG, "NULL callback: %#lx", hr);
    hr = pBeginReg(0xdead0000, L"Audio", 0, 0, &cb, &state_obj); CHECK(hr == E_INVALID_WORKQUEUE, "unknown queue: %#lx", hr);
    hr = pGetClass(0xdead0000, buf, &len); CHECK(hr == E_INVALID_WORKQUEUE, "get on an unknown queue: %#lx", hr);
    hr = pGetTask(queue, NULL); CHECK(hr == E_POINTER, "NULL task out: %#lx", hr);
    hr = pGetPrio(queue, NULL); CHECK(hr == E_POINTER, "NULL priority out: %#lx", hr);
    hr = pGetClass(queue, buf, NULL); CHECK(hr == E_POINTER, "NULL length: %#lx", hr);

    /* register: the callback is called with a result that End accepts */
    hr = pBeginReg(queue, L"Audio", 0, 1, &cb, &state_obj);
    CHECK(hr == S_OK, "begin register %#lx", hr);
    CHECK(WaitForSingleObject(cb.event, 5000) == WAIT_OBJECT_0, "the callback was not called");
    CHECK(cb.result != NULL, "no result");
    if (cb.result)
    {
        hr = cb.result->v->GetState(cb.result, &got_state);
        CHECK(hr == S_OK && got_state == &state_obj, "the state travels with the result: %#lx %p", hr, got_state);
        hr = pEndReg(cb.result, &task);
        CHECK(hr == S_OK && task != 0, "end register: %#lx task %lu", hr, task);
        hr = pEndUnreg(cb.result);
        CHECK(hr == E_INVALIDARG, "a register result is no unregister result: %#lx", hr);
        hr = pEndReg(cb.result, NULL);
        CHECK(hr == E_POINTER, "NULL task out for End: %#lx", hr);
        cb.result->v->unk.Release((IUnk *)cb.result);
        cb.result = NULL;
    }
    len = 64;
    hr = pGetClass(queue, buf, &len); CHECK(hr == S_OK && len == 5 && !wcscmp(buf, L"Audio"), "class: %#lx %lu %ls", hr, len, buf);
    task2 = 0; hr = pGetTask(queue, &task2); CHECK(hr == S_OK && task2 == task && task2, "task id %lu / %lu", task2, task);
    prio = 99; hr = pGetPrio(queue, &prio); CHECK(hr == S_OK && prio == 1, "priority: %#lx %ld", hr, prio);
    len = 3;
    hr = pGetClass(queue, buf, &len); CHECK(hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER) && len == 6, "small class buffer: %#lx %lu", hr, len);

    /* a task id that is given is kept */
    hr = pBeginReg(queue, L"Games", 77, -1, &cb, NULL);
    CHECK(hr == S_OK && WaitForSingleObject(cb.event, 5000) == WAIT_OBJECT_0, "re-register %#lx", hr);
    if (cb.result)
    {
        hr = pEndReg(cb.result, &task2); CHECK(hr == S_OK && task2 == 77, "given task id: %#lx %lu", hr, task2);
        cb.result->v->unk.Release((IUnk *)cb.result);
        cb.result = NULL;
    }
    len = 64; hr = pGetClass(queue, buf, &len); CHECK(hr == S_OK && !wcscmp(buf, L"Games"), "replaced class %ls", buf);
    prio = 99; pGetPrio(queue, &prio); CHECK(prio == -1, "replaced priority %ld", prio);

    /* a result that is not ours */
    foreign_obj = calloc(1, 128);   /* big and zeroed: what the End calls would read of an object that is not theirs */
    foreign_obj->v = &st_vtbl;
    hr = pCreateResult(foreign_obj, &cb, NULL, &foreign);
    CHECK(hr == S_OK, "create result %#lx", hr);
    if (hr == S_OK)
    {
        hr = pEndReg(foreign, &task2); CHECK(hr == E_INVALIDARG, "foreign result for End: %#lx", hr);
        hr = pEndUnreg(foreign); CHECK(hr == E_INVALIDARG, "foreign result for End unregister: %#lx", hr);
        foreign->v->unk.Release((IUnk *)foreign);
    }
    hr = pEndReg(NULL, &task2); CHECK(hr == E_INVALIDARG, "NULL result: %#lx", hr);

    /* unregister */
    hr = pBeginUnreg(queue, &cb, &state_obj); CHECK(hr == S_OK, "begin unregister %#lx", hr);
    CHECK(WaitForSingleObject(cb.event, 5000) == WAIT_OBJECT_0, "no unregister callback");
    if (cb.result)
    {
        hr = pEndUnreg(cb.result); CHECK(hr == S_OK, "end unregister %#lx", hr);
        hr = pEndReg(cb.result, &task2); CHECK(hr == E_INVALIDARG, "an unregister result is no register result: %#lx", hr);
        cb.result->v->unk.Release((IUnk *)cb.result);
        cb.result = NULL;
    }
    len = 9; hr = pGetClass(queue, buf, &len); CHECK(hr == S_OK && len == 0, "class after unregister: %#lx %lu", hr, len);
    task2 = 5; pGetTask(queue, &task2); CHECK(task2 == 0, "task id after unregister %lu", task2);

    /* the platform queues */
    task = 0;
    hr = pRegPlat(L"Playback", &task, 2); CHECK(hr == S_OK && task != 0, "register platform: %#lx %lu", hr, task);
    len = 64; hr = pGetClass(1 /* the standard queue */, buf, &len);
    CHECK(hr == S_OK && !wcscmp(buf, L"Playback"), "platform class: %#lx %ls", hr, buf);
    task2 = 0; pGetTask(1, &task2); CHECK(task2 == task, "platform task id %lu / %lu", task2, task);
    prio = 0; pGetPrio(1, &prio); CHECK(prio == 2, "platform priority %ld", prio);
    hr = pRegPlat(NULL, &task, 0); CHECK(hr == E_INVALIDARG, "platform NULL class: %#lx", hr);
    hr = pRegPlat(L"Playback", NULL, 0); CHECK(hr == E_INVALIDARG, "platform NULL task: %#lx", hr);
    hr = pUnregPlat(); CHECK(hr == S_OK, "unregister platform %#lx", hr);
    len = 9; hr = pGetClass(1, buf, &len); CHECK(hr == S_OK && len == 0, "platform class after unregister: %#lx %lu", hr, len);

    pUnlock(queue);
    pShutdown();
    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    return f != 0;
}
