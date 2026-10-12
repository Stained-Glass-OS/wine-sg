/* windows.media.mediacontrol's audiostatemonitorapi exports (patches/sg/2982-2984),
 * run by test/mc-audiostate-gate.sh. Every one of the eight Create*AudioStateMonitor*
 * functions is called through a table; each monitor must be a real object: IUnknown
 * and IAudioStateMonitor answer QueryInterface with the same pointer, a bogus IID is
 * E_NOINTERFACE, the reference count moves, GetSoundLevel is Full (2), callbacks
 * register with distinct non-zero handles and unregister quietly, and the argument
 * checks hold (NULL out pointer, NULL callback, NULL device id).
 *
 *   mc-audiostate-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <stdio.h>
#include <stdint.h>

DEFINE_GUID(IID_IAudioStateMonitor, 0x63bd8738, 0xe30d, 0x4c77, 0xbf, 0x5c, 0x83, 0x4e, 0x87, 0xc6, 0x57, 0xe2);
DEFINE_GUID(IID_IUnknown, 0x00000000, 0x0000, 0x0000, 0xc0, 0, 0, 0, 0, 0, 0, 0x46);
DEFINE_GUID(IID_Bogus, 0x12345678, 0x1234, 0x1234, 0x12, 0x34, 0x12, 0x34, 0x12, 0x34, 0x12, 0x34);

typedef struct IASM IASM;
typedef struct
{
    HRESULT (WINAPI *QueryInterface)(IASM *, REFIID, void **);
    ULONG (WINAPI *AddRef)(IASM *);
    ULONG (WINAPI *Release)(IASM *);
    int (WINAPI *GetSoundLevel)(IASM *);
    HRESULT (WINAPI *RegisterCallback)(IASM *, void *, void *, INT64 *);
    void (WINAPI *UnregisterCallback)(IASM *, INT64);
} IASMVtbl;
struct IASM { const IASMVtbl *lpVtbl; };

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[256]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

static int callbacks_called;
static void CALLBACK callback(IASM *m, void *ctx) { callbacks_called++; }

enum { A_NONE, A_CAT, A_CAT_ID, A_CAT_ROLE };
static const struct
{
    const char *name;
    int args;
} cases[] =
{
    { "CreateCaptureAudioStateMonitor", A_NONE },
    { "CreateCaptureAudioStateMonitorForCategory", A_CAT },
    { "CreateCaptureAudioStateMonitorForCategoryAndDeviceId", A_CAT_ID },
    { "CreateCaptureAudioStateMonitorForCategoryAndDeviceRole", A_CAT_ROLE },
    { "CreateRenderAudioStateMonitor", A_NONE },
    { "CreateRenderAudioStateMonitorForCategory", A_CAT },
    { "CreateRenderAudioStateMonitorForCategoryAndDeviceId", A_CAT_ID },
    { "CreateRenderAudioStateMonitorForCategoryAndDeviceRole", A_CAT_ROLE },
};

static HRESULT call(FARPROC f, int args, int category, const WCHAR *id, int role, IASM **out)
{
    switch (args)
    {
    case A_NONE: return ((HRESULT (WINAPI *)(IASM **))f)(out);
    case A_CAT: return ((HRESULT (WINAPI *)(int, IASM **))f)(category, out);
    case A_CAT_ID: return ((HRESULT (WINAPI *)(int, const WCHAR *, IASM **))f)(category, id, out);
    default: return ((HRESULT (WINAPI *)(int, int, IASM **))f)(category, role, out);
    }
}

int main(void)
{
    HMODULE mod = LoadLibraryA("windows.media.mediacontrol.dll");
    unsigned i, k;

    check(mod != NULL, "windows.media.mediacontrol.dll loads");
    if (!mod) { printf("RESULT: FAIL\n"); return 1; }

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        const char *n = cases[i].name;
        FARPROC f = GetProcAddress(mod, n);
        int cats[] = { 0, 3, 11 }; /* Other, Media, ... any value is accepted */
        IASM *m = NULL, *m2 = NULL, *u = NULL;
        INT64 h1 = 0, h2 = 0, h3 = 0;
        HRESULT hr;
        ULONG ref;

        CHECKF(f != NULL, "%s is exported", n);
        if (!f) continue;

        for (k = 0; k < 3; k++)
        {
            m = NULL;
            hr = call(f, cases[i].args, cats[k], L"{0.0.0.00000000}.{deadbeef}", k, &m);
            CHECKF(hr == S_OK && m != NULL, "%s(category %d) -> %#lx", n, cats[k], hr);
            if (m) m->lpVtbl->Release(m);
        }
        hr = call(f, cases[i].args, 0, L"x", 0, &m);
        if (hr != S_OK || !m) { CHECKF(0, "%s creates", n); continue; }

        hr = m->lpVtbl->QueryInterface(m, &IID_IUnknown, (void **)&u);
        CHECKF(hr == S_OK && u == m, "%s QI IUnknown %#lx same pointer", n, hr);
        if (u) u->lpVtbl->Release(u);
        hr = m->lpVtbl->QueryInterface(m, &IID_IAudioStateMonitor, (void **)&m2);
        CHECKF(hr == S_OK && m2 == m, "%s QI IAudioStateMonitor %#lx", n, hr);
        if (m2) m2->lpVtbl->Release(m2);
        u = (IASM *)(uintptr_t)1;
        hr = m->lpVtbl->QueryInterface(m, &IID_Bogus, (void **)&u);
        CHECKF(hr == E_NOINTERFACE && u == NULL, "%s QI bogus -> %#lx, out %p", n, hr, u);

        ref = m->lpVtbl->AddRef(m);
        CHECKF(ref == 2, "%s AddRef -> %lu (want 2)", n, ref);
        ref = m->lpVtbl->Release(m);
        CHECKF(ref == 1, "%s Release -> %lu (want 1)", n, ref);

        CHECKF(m->lpVtbl->GetSoundLevel(m) == 2, "%s GetSoundLevel -> %d (want Full = 2)", n, m->lpVtbl->GetSoundLevel(m));

        hr = m->lpVtbl->RegisterCallback(m, callback, NULL, &h1);
        CHECKF(hr == S_OK && h1 != 0, "%s RegisterCallback -> %#lx handle %I64d", n, hr, h1);
        hr = m->lpVtbl->RegisterCallback(m, callback, (void *)0x1234, &h2);
        CHECKF(hr == S_OK && h2 != 0 && h2 != h1, "%s second handle %I64d differs from %I64d", n, h2, h1);
        m->lpVtbl->UnregisterCallback(m, h1);
        hr = m->lpVtbl->RegisterCallback(m, callback, NULL, &h3);
        CHECKF(hr == S_OK && h3 != 0 && h3 != h2 && h3 != h1, "%s handle after unregister %I64d is fresh", n, h3);
        m->lpVtbl->UnregisterCallback(m, h1);   /* already gone: harmless */
        m->lpVtbl->UnregisterCallback(m, 0x7777);
        m->lpVtbl->UnregisterCallback(m, h2);
        CHECKF(m->lpVtbl->GetSoundLevel(m) == 2, "%s still Full after registrations", n);

        h1 = 99;
        hr = m->lpVtbl->RegisterCallback(m, NULL, NULL, &h1);
        CHECKF(hr == E_INVALIDARG && h1 == 0, "%s NULL callback -> %#lx (assumed E_INVALIDARG)", n, hr);
        hr = m->lpVtbl->RegisterCallback(m, callback, NULL, NULL);
        CHECKF(hr == E_POINTER, "%s NULL registration -> %#lx (assumed E_POINTER)", n, hr);
        CHECKF(callbacks_called == 0, "%s callbacks never fired", n);

        ref = m->lpVtbl->Release(m);   /* h3 still registered: freed with the object */
        CHECKF(ref == 0, "%s final Release -> %lu", n, ref);

        /* argument checks */
        switch (cases[i].args)
        {
        case A_NONE: hr = ((HRESULT (WINAPI *)(IASM **))f)(NULL); break;
        case A_CAT: hr = ((HRESULT (WINAPI *)(int, IASM **))f)(0, NULL); break;
        case A_CAT_ID: hr = ((HRESULT (WINAPI *)(int, const WCHAR *, IASM **))f)(0, L"x", NULL); break;
        default: hr = ((HRESULT (WINAPI *)(int, int, IASM **))f)(0, 0, NULL); break;
        }
        CHECKF(hr == E_POINTER, "%s NULL output -> %#lx (assumed E_POINTER)", n, hr);
        if (cases[i].args == A_CAT_ID)
        {
            m = (IASM *)(uintptr_t)1;
            hr = call(f, A_CAT_ID, 0, NULL, 0, &m);
            CHECKF(hr == E_INVALIDARG && m == NULL, "%s NULL device id -> %#lx (assumed E_INVALIDARG)", n, hr);
            if (hr == S_OK && m) m->lpVtbl->Release(m);
        }
    }
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
