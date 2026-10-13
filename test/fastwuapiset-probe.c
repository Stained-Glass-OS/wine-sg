/* wuapi IAutomaticUpdates::Settings / EnableService (patch 3174), run by test/fastwuapiset-gate.sh.
 * Hand-made vtables: mingw has no wuapi.h. */
#include <windows.h>
#include <objbase.h>
#include <stdio.h>

typedef struct AU AU;
typedef struct { void *qi, *addref; ULONG (WINAPI *Release)(void *); void *d[4];
    HRESULT (WINAPI *DetectNow)(AU *); void *pause, *resume, *show;
    HRESULT (WINAPI *get_Settings)(AU *, void **);
    HRESULT (WINAPI *get_ServiceEnabled)(AU *, VARIANT_BOOL *);
    HRESULT (WINAPI *EnableService)(AU *); } AUVT;
struct AU { const AUVT *vt; };
typedef struct SET SET;
typedef struct { void *qi, *addref; ULONG (WINAPI *Release)(SET *); void *d[4];
    HRESULT (WINAPI *get_Level)(SET *, int *); HRESULT (WINAPI *put_Level)(SET *, int);
    HRESULT (WINAPI *get_ReadOnly)(SET *, VARIANT_BOOL *); HRESULT (WINAPI *get_Required)(SET *, VARIANT_BOOL *);
    HRESULT (WINAPI *get_Day)(SET *, int *); HRESULT (WINAPI *put_Day)(SET *, int);
    HRESULT (WINAPI *get_Time)(SET *, LONG *); HRESULT (WINAPI *put_Time)(SET *, LONG);
    HRESULT (WINAPI *Refresh)(SET *); HRESULT (WINAPI *Save)(SET *); } SETVT;
struct SET { const SETVT *vt; };

static const GUID CLSID_AU = {0xbfe18e9c, 0x6d87, 0x4450, {0xb3, 0x7c, 0xe0, 0x2f, 0x0b, 0x37, 0x38, 0x03}};
static const GUID IID_AU = {0x673425bf, 0xc082, 0x4c7c, {0xbd, 0xfd, 0x56, 0x94, 0x64, 0xb8, 0xe0, 0xce}};

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

int main(void)
{
    AU *au = NULL;
    SET *set = NULL, *set2 = NULL;
    HRESULT hr;
    VARIANT_BOOL b;
    int v;
    LONG t;

    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_AU, NULL, CLSCTX_INPROC_SERVER, &IID_AU, (void **)&au);
    check(hr == S_OK, "create AutomaticUpdates");
    if (hr) return 1;

    check(au->vt->get_Settings(au, NULL) == E_POINTER, "Settings NULL");
    hr = au->vt->get_Settings(au, (void **)&set);
    check(hr == S_OK && set, "Settings");
    if (hr) return 1;
    check(set->vt->get_Level(set, &v) == S_OK && v == 0, "level not configured");
    check(set->vt->get_ReadOnly(set, &b) == S_OK && b == VARIANT_TRUE, "read only (no update service)");
    check(set->vt->get_Required(set, &b) == S_OK && b == VARIANT_FALSE, "not required");
    check(set->vt->get_Day(set, &v) == S_OK && v == 0, "day every day");
    check(set->vt->get_Time(set, &t) == S_OK && t >= 0 && t <= 23, "time in range");
    check(set->vt->get_Level(set, NULL) == E_POINTER, "level NULL");
    check(set->vt->put_Level(set, 99) == E_INVALIDARG, "level out of range");
    check(set->vt->put_Level(set, 4) == E_ACCESSDENIED, "level change refused");
    check(set->vt->put_Day(set, 99) == E_INVALIDARG, "day out of range");
    check(set->vt->put_Day(set, 3) == E_ACCESSDENIED, "day change refused");
    check(set->vt->put_Time(set, 24) == E_INVALIDARG, "time out of range");
    check(set->vt->put_Time(set, 5) == E_ACCESSDENIED, "time change refused");
    check(set->vt->Refresh(set) == S_OK, "Refresh");
    check(set->vt->Save(set) == E_ACCESSDENIED, "Save refused");
    check(au->vt->get_Settings(au, (void **)&set2) == S_OK && set2 != set, "a new object each time");
    set2->vt->Release(set2);
    set->vt->Release(set);

    check(au->vt->get_ServiceEnabled(au, &b) == S_OK && b == VARIANT_FALSE, "service not enabled");
    check(au->vt->EnableService(au) == HRESULT_FROM_WIN32(ERROR_SERVICE_DOES_NOT_EXIST), "EnableService: no service");

    au->vt->Release(au);
    printf("failures: %d\nRESULT: %s\n", failures, failures ? "FAIL" : "PASS");
    return failures != 0;
}
