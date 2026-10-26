/* vccorlib-probe.c: Wine's vccorlib140.dll (the C++/CX runtime, patches/sg/7009) as a C++/CX
 * program uses it.  The raisers must throw a C++ exception a "catch (Platform::Exception^ e)" block
 * takes: the catch matches by decorated type name, copies the handle (one more reference), reads
 * e->Message and e->HResult, and the thrown handle is released after the catch.  A vectored
 * handler sees each throw and does what the C++ runtime's catch does, then ends the thread.
 * Prints key=value lines. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <winstring.h>
#include <roapi.h>

typedef HRESULT (__cdecl *init_fn)(int);
typedef HRESULT (__cdecl *factory_fn)(void *, GUID *, void **);
#ifdef _WIN64
#define MEMBER __cdecl
#define NAME(n64, n32) n64
#else
#define MEMBER __thiscall   /* member functions and the C++ runtime's copy/destroy calls */
#define NAME(n64, n32) n32
#endif
typedef HSTRING (MEMBER *message_fn)(void *);
typedef HRESULT (MEMBER *hresult_fn)(void *);
typedef void (__cdecl *raise_fn)(void);
typedef void (__cdecl *raise_hr_fn)(HRESULT);
typedef void * (MEMBER *copy_fn)(void **, void **);
typedef void (MEMBER *dtor_fn)(void **);
typedef ULONG (__stdcall *addref_fn)(void *);

static message_fn get_message;
static hresult_fn get_hresult;

struct type_info { const void *vtable; char *name; char mangled[64]; };
struct catchable { UINT flags; int type_info; int off[3]; UINT size; int copy_ctor; };
struct table { UINT count; int info[8]; };
struct throw_info { UINT flags; int dtor; int handler; int table; };

/* what the vectored handler saw */
static struct
{
    int seen, magic_ok;
    char first[64];
    int caught_by_exception, caught_by_comexception, caught_by_object, caught_by_specific;
    HRESULT hr;
    WCHAR message[256];
    ULONG ref_in_catch, ref_after_catch;
    const char *want;
} r;

static ULONG refcount(void *obj)
{
    void **vtbl = *(void ***)obj;
    ((addref_fn)vtbl[1])(obj);
    return ((addref_fn)vtbl[2])(obj);
}

static int catches(const struct table *t, char *base, const char *name, void *obj, void **copy)
{
    UINT i;
    for (i = 0; i < t->count; i++)
    {
        const struct catchable *c = (const struct catchable *)(base + t->info[i]);
        const struct type_info *ti = (const struct type_info *)(base + c->type_info);
        if (strcmp(ti->mangled, name)) continue;
        if (copy)
        {
            if (c->flags & 1) memcpy(copy, obj, c->size);
            else ((copy_fn)(base + c->copy_ctor))(copy, obj);
        }
        return 1;
    }
    return 0;
}

static LONG CALLBACK handler(EXCEPTION_POINTERS *ptrs)
{
    EXCEPTION_RECORD *rec = ptrs->ExceptionRecord;
    const struct throw_info *info;
    const struct table *t;
    char *base;
    void **object, *handle = NULL;
    HSTRING msg;
    UINT32 len;

    if (rec->ExceptionCode != 0xe06d7363 || rec->NumberParameters < 3) return EXCEPTION_CONTINUE_SEARCH;
    r.seen = 1;
    r.magic_ok = rec->ExceptionInformation[0] == 0x19930520;
    object = (void **)rec->ExceptionInformation[1];
    info = (const struct throw_info *)rec->ExceptionInformation[2];
    base = rec->NumberParameters >= 4 ? (char *)rec->ExceptionInformation[3] : NULL;   /* 32-bit: pointers */
    t = (const struct table *)(base + info->table);
    if (t->count)
    {
        const struct catchable *c = (const struct catchable *)(base + t->info[0]);
        lstrcpynA(r.first, ((const struct type_info *)(base + c->type_info))->mangled, sizeof(r.first));
    }
    r.caught_by_specific = r.want && catches(t, base, r.want, object, NULL);
    r.caught_by_comexception = catches(t, base, ".PE$AAVCOMException@Platform@@", object, NULL);
    r.caught_by_object = catches(t, base, ".PE$AAVObject@Platform@@", object, NULL);
    /* catch (Platform::Exception^ e) { e->Message; e->HResult; } */
    r.caught_by_exception = catches(t, base, ".PE$AAVException@Platform@@", object, &handle);
    if (r.caught_by_exception && handle)
    {
        r.ref_in_catch = refcount(handle);
        r.hr = get_hresult(handle);
        msg = get_message(handle);
        lstrcpynW(r.message, WindowsGetStringRawBuffer(msg, &len), ARRAYSIZE(r.message));
        WindowsDeleteString(msg);
        ((addref_fn)(*(void ***)handle)[2])(handle);   /* the catch's copy goes out of scope */
        ((addref_fn)(*(void ***)handle)[1])(handle);   /* keep it alive to look at it */
        if (info->dtor) ((dtor_fn)(base + info->dtor))(object);   /* the exception object is destroyed */
        r.ref_after_catch = ((addref_fn)(*(void ***)handle)[2])(handle);
    }
    ExitThread(0);
    return EXCEPTION_CONTINUE_SEARCH;
}

static DWORD WINAPI raise_thread(void *arg)
{
    ((raise_fn)arg)();
    return 1;
}

static HRESULT raise_hr_value;
static raise_hr_fn raise_hr;
static DWORD WINAPI raise_hr_thread(void *arg)
{
    raise_hr(raise_hr_value);
    return 1;
}

static void run(const char *key, LPTHREAD_START_ROUTINE fn, void *arg, const char *want, HRESULT want_hr)
{
    HANDLE th;
    char msg[256];
    memset(&r, 0, sizeof(r));
    r.want = want;
    th = CreateThread(NULL, 0, fn, arg, 0, NULL);
    WaitForSingleObject(th, 10000);
    CloseHandle(th);
    WideCharToMultiByte(CP_ACP, 0, r.message, -1, msg, sizeof(msg), NULL, NULL);
    printf("%s_thrown=%d %s_cxx=%d %s_type=%d %s_catch_exception=%d %s_catch_com=%d %s_catch_object=%d "
           "%s_hr=%d %s_message=%d %s_ref_in_catch=%lu %s_released=%d\n",
           key, r.seen, key, r.magic_ok, key, r.caught_by_specific, key, r.caught_by_exception,
           key, r.caught_by_comexception, key, r.caught_by_object, key, r.hr == want_hr,
           key, r.message[0] != 0, key, r.ref_in_catch, key, r.ref_after_catch == 0);
    printf("%s_first=%s %s_text=%s\n", key, r.first, key, msg);
}

int main(void)
{
    static const GUID IID_IActivationFactory = {0x00000035,0,0,{0xc0,0,0,0,0,0,0,0x46}};
    HMODULE mod = LoadLibraryA("vccorlib140.dll");
    init_fn init;
    factory_fn factory;
    void *f = NULL;
    HRESULT hr;

    printf("loaded=%d\n", mod != NULL);
    if (!mod) return 1;
    init = (void *)GetProcAddress(mod, NAME("?InitializeData@Details@Platform@@YAJH@Z", "?InitializeData@Details@Platform@@YAJH@Z"));
    factory = (void *)GetProcAddress(mod, NAME("?GetActivationFactoryByPCWSTR@@YAJPEAXAEAVGuid@Platform@@PEAPEAX@Z", "?GetActivationFactoryByPCWSTR@@YAJPAXAAVGuid@Platform@@PAPAX@Z"));
    get_message = (void *)GetProcAddress(mod, NAME("?get@Message@Exception@Platform@@QE$AAAPE$AAVString@3@XZ", "?get@Message@Exception@Platform@@Q$AAEP$AAVString@3@XZ"));
    get_hresult = (void *)GetProcAddress(mod, NAME("?get@HResult@Exception@Platform@@QE$AAAHXZ", "?get@HResult@Exception@Platform@@Q$AAEHXZ"));
    raise_hr = (void *)GetProcAddress(mod, "?__abi_WinRTraiseCOMException@@YAXJ@Z");
    printf("exports=%d\n", init && factory && get_message && get_hresult && raise_hr &&
           GetProcAddress(mod, "?UninitializeData@Details@Platform@@YAXH@Z") &&
           NAME(1, GetProcAddress(mod, "?InitializeData@Details@Platform@@YGJH@Z") &&
                   GetProcAddress(mod, "?__abi_WinRTraiseCOMException@@YGXJ@Z") &&
                   GetProcAddress(mod, "?__abi_WinRTraiseOutOfMemoryException@@YGXXZ")));
    if (!(init && factory && get_message && get_hresult && raise_hr)) return 1;
    printf("init=%d\n", init(0) == S_OK);

    RoInitialize(RO_INIT_MULTITHREADED);   /* as the program does before activating */
    hr = factory((void *)L"Windows.System.UserProfile.GlobalizationPreferences", (GUID *)&IID_IActivationFactory, &f);
    printf("factory=%d factory_hr=%#lx\n", hr == S_OK && f, hr);
    if (f) ((addref_fn)(*(void ***)f)[2])(f);
    f = (void *)1;
    hr = factory((void *)L"Sg.No.Such.Class", (GUID *)&IID_IActivationFactory, &f);
    printf("factory_unknown=%d\n", FAILED(hr) && !f);

    AddVectoredExceptionHandler(1, handler);
    run("oom", raise_thread, GetProcAddress(mod, "?__abi_WinRTraiseOutOfMemoryException@@YAXXZ"),
        ".PE$AAVOutOfMemoryException@Platform@@", E_OUTOFMEMORY);
    run("cast", raise_thread, GetProcAddress(mod, "?__abi_WinRTraiseInvalidCastException@@YAXXZ"),
        ".PE$AAVInvalidCastException@Platform@@", E_NOINTERFACE);
    run("wrongthread", raise_thread, GetProcAddress(mod, "?__abi_WinRTraiseWrongThreadException@@YAXXZ"),
        ".PE$AAVWrongThreadException@Platform@@", RPC_E_WRONG_THREAD);
    raise_hr_value = E_ACCESSDENIED;
    run("com_mapped", raise_hr_thread, NULL, ".PE$AAVAccessDeniedException@Platform@@", E_ACCESSDENIED);
    raise_hr_value = 0x80041234;
    run("com_other", raise_hr_thread, NULL, ".PE$AAVCOMException@Platform@@", 0x80041234);
    printf("done=1\n");
    return 0;
}
