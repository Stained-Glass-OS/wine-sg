/* Probe for patches/sg/4583: RtwqRegisterPlatformEvents and
 * RtwqUnregisterPlatformEvents: a registered listener hears
 * InitializationComplete when RtwqStartup starts the platform, and
 * ShutdownStart then ShutdownComplete when the last RtwqShutdown stops it.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

/* IRtwqPlatformEvents: IUnknown, InitializationComplete, ShutdownStart, ShutdownComplete */
struct events;
struct events_vtbl
{
    HRESULT (WINAPI *QueryInterface)(struct events *, REFIID, void **);
    ULONG (WINAPI *AddRef)(struct events *);
    ULONG (WINAPI *Release)(struct events *);
    HRESULT (WINAPI *InitializationComplete)(struct events *);
    HRESULT (WINAPI *ShutdownStart)(struct events *);
    HRESULT (WINAPI *ShutdownComplete)(struct events *);
};
struct events { const struct events_vtbl *vtbl; LONG ref; char log[32]; };

static HRESULT WINAPI ev_qi(struct events *e, REFIID iid, void **out) { *out = e; e->ref++; return S_OK; }
static ULONG WINAPI ev_addref(struct events *e) { return ++e->ref; }
static ULONG WINAPI ev_release(struct events *e) { return --e->ref; }
static HRESULT WINAPI ev_init(struct events *e) { strcat(e->log, "I"); return S_OK; }
static HRESULT WINAPI ev_start(struct events *e) { strcat(e->log, "S"); return S_OK; }
static HRESULT WINAPI ev_complete(struct events *e) { strcat(e->log, "C"); return S_OK; }
static const struct events_vtbl ev_vtbl = {ev_qi, ev_addref, ev_release, ev_init, ev_start, ev_complete};

int main(int argc, char **argv)
{
    HMODULE mod = LoadLibraryA("rtworkq.dll");
    HRESULT (WINAPI *p_reg)(struct events *), (WINAPI *p_unreg)(struct events *), (WINAPI *p_startup)(void), (WINAPI *p_shutdown)(void);
    struct events a = {&ev_vtbl, 1}, b = {&ev_vtbl, 1};
    /* operations: '+'/'-' register/unregister A, 'b'/'B' register/unregister B, 'u' startup, 'd' shutdown */
    static const struct { const char *ops; const char *log_a, *log_b; } cases[] =
    {
        {"+ud-", "ISC", ""},             /* one start and stop */
        {"+uudd-", "ISC", ""},           /* nested: only the outer pair counts */
        {"+-ud", "", ""},                /* unregistered before the start */
        {"u+d-", "SC", ""},              /* registered while running: no InitializationComplete */
        {"+bud-B", "ISC", "ISC"},        /* two listeners */
        {"+bu-dB", "I", "ISC"},          /* one leaves while running */
    };
    unsigned int i;
    HRESULT hr;

    if (!mod) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    p_reg = (void *)GetProcAddress(mod, "RtwqRegisterPlatformEvents");
    p_unreg = (void *)GetProcAddress(mod, "RtwqUnregisterPlatformEvents");
    p_startup = (void *)GetProcAddress(mod, "RtwqStartup");
    p_shutdown = (void *)GetProcAddress(mod, "RtwqShutdown");
    check(p_reg && p_unreg && p_startup && p_shutdown, "exports");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    for (i = 0; i < ARRAYSIZE(cases); i++)
    {
        const char *op;
        int ok = 1;

        a.log[0] = b.log[0] = 0;
        for (op = cases[i].ops; *op; op++)
        {
            switch (*op)
            {
            case '+': hr = p_reg(&a); break;
            case '-': hr = p_unreg(&a); break;
            case 'b': hr = p_reg(&b); break;
            case 'B': hr = p_unreg(&b); break;
            case 'u': hr = p_startup(); break;
            default: hr = p_shutdown(); break;
            }
            if (hr != S_OK) ok = 0;
        }
        check(ok && !strcmp(a.log, cases[i].log_a) && !strcmp(b.log, cases[i].log_b),
              "case %u \"%s\": calls S_OK, A heard \"%s\", B heard \"%s\"", i, cases[i].ops, a.log, b.log);
        check(a.ref == 1 && b.ref == 1, "case %u: references released (%ld %ld)", i, a.ref, b.ref);
    }

    hr = p_unreg(&a);
    check(FAILED(hr), "unregistering what is not registered fails: %#lx", hr);
    check(p_reg(NULL) == E_POINTER, "registering NULL");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
