/* gitrevoke-probe.c: an object of an STA registered in the global interface table, revoked from another
 * thread: while the STA runs (A), after it uninitialized and ended (B), after it ended without
 * uninitializing (C), while it tears down (D). Prints the result and the thread the object was released on. */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <stdio.h>

typedef struct { IUnknown IUnknown_iface; LONG ref; DWORD owner; volatile DWORD released_on; volatile LONG freed; } obj_t;
static HRESULT WINAPI qi(IUnknown *i, REFIID r, void **p)
{ if (IsEqualIID(r, &IID_IUnknown)) { *p = i; IUnknown_AddRef(i); return S_OK; } *p = NULL; return E_NOINTERFACE; }
static ULONG WINAPI addref(IUnknown *i) { return InterlockedIncrement(&((obj_t *)i)->ref); }
static ULONG WINAPI release(IUnknown *i)
{
    obj_t *o = (obj_t *)i; LONG r = InterlockedDecrement(&o->ref);
    if (!r) { o->released_on = GetCurrentThreadId(); o->freed = 1; }
    return r;
}
static IUnknownVtbl vt = { qi, addref, release };

static IGlobalInterfaceTable *git;
struct sta { obj_t obj; DWORD cookie; HANDLE ready, go, done; int mode; };

static DWORD WINAPI sta_proc(void *arg)
{
    struct sta *s = arg; MSG msg;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    s->obj.IUnknown_iface.lpVtbl = &vt; s->obj.ref = 1; s->obj.owner = GetCurrentThreadId();
    IGlobalInterfaceTable_RegisterInterfaceInGlobal(git, &s->obj.IUnknown_iface, &IID_IUnknown, &s->cookie);
    IUnknown_Release(&s->obj.IUnknown_iface);           /* the table holds it now */
    SetEvent(s->ready);
    if (s->mode == 'A')
    {
        while (MsgWaitForMultipleObjects(1, &s->done, FALSE, INFINITE, QS_ALLINPUT) != WAIT_OBJECT_0)
            while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
    }
    else WaitForSingleObject(s->go, INFINITE);
    if (s->mode != 'C') CoUninitialize();
    return 0;
}

static void run(int mode)
{
    struct sta s = {0};
    HANDLE t; HRESULT hr; DWORD me = GetCurrentThreadId();

    s.mode = mode;
    s.ready = CreateEventA(NULL, TRUE, FALSE, NULL); s.go = CreateEventA(NULL, TRUE, FALSE, NULL);
    s.done = CreateEventA(NULL, TRUE, FALSE, NULL);
    t = CreateThread(NULL, 0, sta_proc, &s, 0, NULL);
    WaitForSingleObject(s.ready, 10000);
    if (mode == 'B' || mode == 'C') { SetEvent(s.go); WaitForSingleObject(t, 10000); }
    if (mode == 'D') SetEvent(s.go);                      /* revoke while it uninitializes */
    hr = IGlobalInterfaceTable_RevokeInterfaceFromGlobal(git, s.cookie);
    if (mode == 'A') { SetEvent(s.done); WaitForSingleObject(t, 10000); }
    if (mode == 'D') WaitForSingleObject(t, 10000);
    printf("%c: revoke %#lx, released %s (%s)\n", mode, hr, s.obj.freed ? "yes" : "no",
           !s.obj.freed ? "-" : s.obj.released_on == s.obj.owner ? "on its STA" : s.obj.released_on == me ? "on the revoking thread" : "elsewhere");
    CloseHandle(t);
}

int main(int argc, char **argv)
{
    const char *modes = argc > 1 ? argv[1] : "ABCD";
    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    CoCreateInstance(&CLSID_StdGlobalInterfaceTable, NULL, CLSCTX_INPROC_SERVER, &IID_IGlobalInterfaceTable, (void **)&git);
    for (; *modes; modes++) run(*modes);
    puts("done");
    return 0;
}
