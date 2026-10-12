/* Windows.Devices.Geolocation over GeoClue: one phase per run (argv[1]) */
#define COBJMACROS
#include <windows.h>
#include <roapi.h>
#include <winstring.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL  %s:%d  ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef void *OBJ;
#define SLOT(o, n) ((*(void ***)(o))[n])
#define CALL(o, n, ret, ...) ((ret (WINAPI *)())SLOT(o, n))(o, ##__VA_ARGS__)

static const GUID IID_IActivationFactory_ = {0x00000035,0x0000,0x0000,{0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};
static const GUID IID_IGeolocator_ = {0xa9c3bf62,0x4524,0x4989,{0x8a,0xa9,0xde,0x01,0x9d,0x2e,0x55,0x1f}};
static const GUID IID_IGeolocatorStatics_ = {0x9a8e7571,0x2df5,0x4591,{0x9f,0x87,0xeb,0x5f,0xd8,0x94,0xe9,0xb7}};
static const GUID IID_IAsyncInfo_ = {0x00000036,0x0000,0x0000,{0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};

enum { G_DesiredAccuracy = 6, G_put_DesiredAccuracy, G_MovementThreshold, G_put_MovementThreshold, G_ReportInterval,
       G_put_ReportInterval, G_LocationStatus, G_GetGeopositionAsync, G_GetGeopositionAsyncAgeTimeout,
       G_add_PositionChanged, G_remove_PositionChanged, G_add_StatusChanged, G_remove_StatusChanged };

static OBJ geolocator_new(void)
{
    static const WCHAR name[] = L"Windows.Devices.Geolocation.Geolocator";
    HSTRING s;
    OBJ factory = NULL, inst = NULL, geo = NULL;
    HRESULT hr;

    WindowsCreateString(name, wcslen(name), &s);
    hr = RoGetActivationFactory(s, &IID_IActivationFactory_, (void **)&factory);
    WindowsDeleteString(s);
    CHECK(hr == S_OK, "factory %08lx", hr);
    if (hr != S_OK) return NULL;
    hr = CALL(factory, 6, HRESULT, &inst);   /* ActivateInstance */
    CHECK(hr == S_OK, "activate %08lx", hr);
    hr = CALL(inst, 0, HRESULT, &IID_IGeolocator_, &geo);
    CHECK(hr == S_OK, "IGeolocator %08lx", hr);
    CALL(inst, 2, ULONG);
    CALL(factory, 2, ULONG);
    return geo;
}

static OBJ statics_get(void)
{
    static const WCHAR name[] = L"Windows.Devices.Geolocation.Geolocator";
    HSTRING s;
    OBJ st = NULL;

    WindowsCreateString(name, wcslen(name), &s);
    RoGetActivationFactory(s, &IID_IGeolocatorStatics_, (void **)&st);
    WindowsDeleteString(s);
    return st;
}

/* waits for an operation; returns AsyncStatus and the error code */
static int wait_op(OBJ op, HRESULT *errcode)
{
    OBJ info = NULL;
    int status = 0, i;
    HRESULT hr = CALL(op, 0, HRESULT, &IID_IAsyncInfo_, &info);
    CHECK(hr == S_OK, "IAsyncInfo %08lx", hr);
    if (hr != S_OK) return -1;
    for (i = 0; i < 400; i++)
    {
        CALL(info, 7, HRESULT, &status);
        if (status != 0) break;
        Sleep(100);
    }
    *errcode = 0;
    CALL(info, 8, HRESULT, errcode);
    CALL(info, 2, ULONG);
    return status;
}

static int status_of(OBJ geo)
{
    int st = -99;
    CALL(geo, G_LocationStatus, HRESULT, &st);
    return st;
}

static const WCHAR *consent_path = L"Software\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\location";

static void set_consent(HKEY root, const WCHAR *sub, const WCHAR *value)
{
    WCHAR path[300];
    HKEY key;
    wcscpy(path, consent_path);
    if (sub) { wcscat(path, L"\\"); wcscat(path, sub); }
    if (RegCreateKeyExW(root, path, 0, NULL, 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, NULL, &key, NULL)) { CHECK(0, "create consent key"); return; }
    RegSetValueExW(key, L"Value", 0, REG_SZ, (const BYTE *)value, (wcslen(value) + 1) * 2);
    RegCloseKey(key);
}

/* event handlers: Invoke(this, sender, args) */
static volatile LONG position_calls, status_calls;
static double last_lat;
static int last_status = -1;

static HRESULT WINAPI h_qi(void *This, const GUID *iid, void **out) { *out = This; return S_OK; }
static ULONG WINAPI h_addref(void *This) { return 2; }
static ULONG WINAPI h_release(void *This) { return 1; }

static HRESULT WINAPI h_position(void *This, void *sender, void *args)
{
    OBJ pos = NULL, coord = NULL;
    double lat = 0;

    if (CALL(args, 6, HRESULT, &pos) == S_OK && CALL(pos, 6, HRESULT, &coord) == S_OK)
    {
        CALL(coord, 6, HRESULT, &lat);
        last_lat = lat;
        CALL(coord, 2, ULONG);
        CALL(pos, 2, ULONG);
    }
    InterlockedIncrement(&position_calls);
    return S_OK;
}

static HRESULT WINAPI h_status(void *This, void *sender, void *args)
{
    int st = -1;
    CALL(args, 6, HRESULT, &st);
    last_status = st;
    InterlockedIncrement(&status_calls);
    return S_OK;
}

static void *vt_position[4] = { h_qi, h_addref, h_release, h_position };
static void *vt_status[4] = { h_qi, h_addref, h_release, h_status };
static void *obj_position = vt_position, *obj_status = vt_status;

static OBJ get_position(OBJ geo, HRESULT *op_err, int *op_status, HRESULT *results_hr)
{
    OBJ op = NULL, pos = NULL;
    HRESULT hr = CALL(geo, G_GetGeopositionAsync, HRESULT, &op);
    CHECK(hr == S_OK && op, "GetGeopositionAsync %08lx", hr);
    if (hr != S_OK) return NULL;
    *op_status = wait_op(op, op_err);
    *results_hr = CALL(op, 8, HRESULT, &pos);
    CALL(op, 2, ULONG);
    return pos;
}

static void check_position(OBJ pos, int expect_alt)
{
    OBJ coord = NULL, ref = NULL;
    double v = 0;
    LONGLONG ts = 0;
    HRESULT hr = CALL(pos, 6, HRESULT, &coord);
    CHECK(hr == S_OK && coord, "Coordinate %08lx", hr);
    if (hr != S_OK) return;
    CALL(coord, 6, HRESULT, &v);
    CHECK(v > 47.6061 && v < 47.6063, "latitude %f", v);
    CALL(coord, 7, HRESULT, &v);
    CHECK(v > -122.3322 && v < -122.3320, "longitude %f", v);
    CALL(coord, 9, HRESULT, &v);
    CHECK(v == 25.5, "accuracy %f", v);
    ref = (OBJ)1;
    hr = CALL(coord, 8, HRESULT, &ref);
    if (expect_alt) { CHECK(hr == S_OK && ref && ref != (OBJ)1, "altitude present %08lx", hr);
        if (ref && ref != (OBJ)1) { v = 0; CALL(ref, 6, HRESULT, &v); CHECK(v == 56.5, "altitude %f", v); CALL(ref, 2, ULONG); } }
    else CHECK(hr == S_OK && ref == NULL, "altitude absent");
    ref = (OBJ)1;
    CALL(coord, 12, HRESULT, &ref);
    CHECK(ref && ref != (OBJ)1, "speed present");
    if (ref && ref != (OBJ)1) { v = 0; CALL(ref, 6, HRESULT, &v); CHECK(v == 3.25, "speed %f", v); CALL(ref, 2, ULONG); }
    ref = (OBJ)1;
    CALL(coord, 11, HRESULT, &ref);
    CHECK(ref == NULL, "heading unknown is null");
    ref = (OBJ)1;
    CALL(coord, 10, HRESULT, &ref);
    CHECK(ref == NULL, "altitude accuracy null");
    CALL(coord, 13, HRESULT, &ts);
    CHECK(ts == 1700000000LL * 10000000 + 250000 * 10 + 116444736000000000LL, "timestamp %lld", ts);
    CALL(coord, 2, ULONG);
}

int main(int argc, char **argv)
{
    const char *phase = argc > 1 ? argv[1] : "allow";
    OBJ geo, pos, st, op;
    HRESULT err = 0, rhr = 0, hr;
    int ops = 0, i;
    LONGLONG tok1 = 0, tok2 = 0;

    RoInitialize(RO_INIT_MULTITHREADED);
    geo = geolocator_new();
    if (!geo) goto end;

    if (!strcmp(phase, "allow") || !strcmp(phase, "allow-noalt"))
    {
        int s0 = status_of(geo);
        CHECK(s0 == 4, "initial status %d (NotInitialized)", s0);
        pos = get_position(geo, &err, &ops, &rhr);
        CHECK(ops == 1 && rhr == S_OK && pos, "completed ops=%d err=%08lx results=%08lx", ops, err, rhr);
        if (pos) { check_position(pos, !strcmp(phase, "allow")); CALL(pos, 2, ULONG); }
        CHECK(status_of(geo) == 0, "status Ready after a fix: %d", status_of(geo));

        /* a recent enough cached position answers without asking again */
        {
            LONGLONG age = 3600LL * 10000000, timeout = 5LL * 10000000;
            hr = CALL(geo, G_GetGeopositionAsyncAgeTimeout, HRESULT, age, timeout, &op);
            CHECK(hr == S_OK, "age call %08lx", hr);
            ops = wait_op(op, &err);
            rhr = CALL(op, 8, HRESULT, &pos);
            CHECK(ops == 1 && rhr == S_OK, "cached ops=%d %08lx", ops, rhr);
            if (pos) CALL(pos, 2, ULONG);
            CALL(op, 2, ULONG);
        }
        hr = CALL(geo, G_GetGeopositionAsyncAgeTimeout, HRESULT, (LONGLONG)-1, (LONGLONG)10000000, &op);
        CHECK(hr == E_INVALIDARG, "negative age %08lx", hr);

        st = statics_get();
        CHECK(st != NULL, "statics");
        if (st)
        {
            int access = -1;
            hr = CALL(st, 6, HRESULT, &op);
            CHECK(hr == S_OK, "RequestAccessAsync %08lx", hr);
            ops = wait_op(op, &err);
            CALL(op, 8, HRESULT, &access);
            CHECK(ops == 1 && access == 1, "access Allowed: ops=%d access=%d", ops, access);
            CALL(op, 2, ULONG);
            CALL(st, 2, ULONG);
        }
        /* high accuracy asks GeoClue for the exact level */
        CALL(geo, G_put_DesiredAccuracy, HRESULT, 1);
        pos = get_position(geo, &err, &ops, &rhr);
        if (pos) CALL(pos, 2, ULONG);
    }
    else if (!strncmp(phase, "deny-", 5))
    {
        if (!strcmp(phase, "deny-hklm")) set_consent(HKEY_LOCAL_MACHINE, NULL, L"Deny");
        else if (!strcmp(phase, "deny-hkcu")) set_consent(HKEY_CURRENT_USER, NULL, L"Deny");
        else set_consent(HKEY_CURRENT_USER, L"NonPackaged", L"Deny");

        CHECK(status_of(geo) == 3, "status Disabled: %d", status_of(geo));
        pos = get_position(geo, &err, &ops, &rhr);
        CHECK(ops == 3 && err == E_ACCESSDENIED && rhr == E_ACCESSDENIED && !pos,
              "denied ops=%d err=%08lx results=%08lx", ops, err, rhr);
        st = statics_get();
        if (st)
        {
            int access = -1;
            CALL(st, 6, HRESULT, &op);
            ops = wait_op(op, &err);
            CALL(op, 8, HRESULT, &access);
            CHECK(ops == 1 && access == 2, "access Denied: ops=%d access=%d", ops, access);
            CALL(op, 2, ULONG);
            CALL(st, 2, ULONG);
        }
        CALL(geo, G_add_PositionChanged, HRESULT, &obj_position, &tok1);
        CALL(geo, G_add_StatusChanged, HRESULT, &obj_status, &tok2);
        Sleep(3000);
        CHECK(position_calls == 0, "no position while denied: %ld", position_calls);
        CALL(geo, G_remove_PositionChanged, HRESULT, tok1);
        CALL(geo, G_remove_StatusChanged, HRESULT, tok2);
        /* consent is read at each request: allowing again works without recreating anything */
        if (!strcmp(phase, "deny-hklm")) set_consent(HKEY_LOCAL_MACHINE, NULL, L"Allow");
        else if (!strcmp(phase, "deny-hkcu")) set_consent(HKEY_CURRENT_USER, NULL, L"Allow");
        else set_consent(HKEY_CURRENT_USER, L"NonPackaged", L"Allow");
        CHECK(status_of(geo) != 3, "status not Disabled after Allow: %d", status_of(geo));
    }
    else if (!strcmp(phase, "absent"))
    {
        pos = get_position(geo, &err, &ops, &rhr);
        CHECK(ops == 3 && err == HRESULT_FROM_WIN32(ERROR_TIMEOUT) && !pos, "absent ops=%d err=%08lx", ops, err);
        CHECK(status_of(geo) == 5, "status NotAvailable: %d", status_of(geo));
    }
    else if (!strcmp(phase, "events"))
    {
        hr = CALL(geo, G_add_PositionChanged, HRESULT, &obj_position, &tok1);
        CHECK(hr == S_OK && tok1, "add PositionChanged %08lx", hr);
        hr = CALL(geo, G_add_StatusChanged, HRESULT, &obj_status, &tok2);
        CHECK(hr == S_OK && tok2 && tok2 != tok1, "add StatusChanged %08lx", hr);
        for (i = 0; i < 300 && (!position_calls || !status_calls || last_status != 0); i++) Sleep(100);
        CHECK(position_calls >= 1, "PositionChanged fired: %ld", position_calls);
        CHECK(last_lat > 47.6061 && last_lat < 47.6063, "event latitude %f", last_lat);
        CHECK(status_calls >= 1 && last_status == 0, "StatusChanged fired: %ld last=%d", status_calls, last_status);
        CALL(geo, G_remove_PositionChanged, HRESULT, tok1);
        CALL(geo, G_remove_StatusChanged, HRESULT, tok2);
    }
    CALL(geo, 2, ULONG);
end:
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
