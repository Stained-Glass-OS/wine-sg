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
static const GUID IID_IGeocoordinate_ = {0xee21a3aa,0x976a,0x4c70,{0x80,0x3d,0x08,0x3e,0xa5,0x5b,0xcb,0xc4}};
static const GUID IID_IGeocoordinateWithPoint_ = {0xfeea0525,0xd22c,0x4d46,{0xb5,0x27,0x0b,0x96,0x06,0x6f,0xc7,0xdb}};
static const GUID IID_IGeocoordinateWithPositionData_ = {0x95e634be,0xdbd6,0x40ac,{0xb8,0xf2,0xa6,0x5c,0x03,0x40,0xd9,0xa6}};
static const GUID IID_IGeocoordinateWithPositionSourceTimestamp_ = {0x8543fc02,0xc9f1,0x4610,{0xaf,0xe0,0x8b,0xc3,0xa6,0xa8,0x70,0x36}};
static const GUID IID_IGeocoordinateWithRemoteSource_ = {0x397cebd7,0xee38,0x5f3b,{0x89,0x00,0xc4,0xa7,0xbc,0x9c,0xf9,0x53}};
static const GUID IID_IGeoposition2_ = {0x7f62f697,0x8671,0x4b0d,{0x86,0xf8,0x47,0x4a,0x84,0x96,0x18,0x7c}};
static const GUID IID_IGeoshape_ = {0xc99ca2af,0xc729,0x43c1,{0x8f,0xab,0xd6,0xde,0xc9,0x14,0xdf,0x7e}};
static const GUID IID_IGeopoint_ = {0x6bfa00eb,0xe56e,0x49bb,{0x9c,0xaf,0xcb,0xaa,0x78,0xa8,0xbc,0xef}};
static const GUID IID_IGeopointFactory_ = {0xdb6b8d33,0x76bd,0x4e30,{0x8a,0xf7,0xa8,0x44,0xdc,0x37,0xb7,0xa0}};
static const GUID IID_IGeolocatorWithScalarAccuracy_ = {0x96f5d3c1,0xb80f,0x460a,{0x99,0x4d,0xa9,0x6c,0x47,0xa5,0x1a,0xa4}};
static const GUID IID_IGeolocatorStatics2_ = {0x993011a2,0xfa1c,0x4631,{0xa7,0x1d,0x0d,0xbe,0xb1,0x25,0x0d,0x9c}};
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

typedef struct { double lat, lon, alt; } BGP;
static BGP def_value;
static unsigned meters_value;
static HRESULT WINAPI bgp_value(void *This, BGP *v) { *v = def_value; return S_OK; }
static HRESULT WINAPI uint_value(void *This, unsigned *v) { *v = meters_value; return S_OK; }
static void *vt_bgp[7] = { h_qi, h_addref, h_release, NULL, NULL, NULL, bgp_value };
static void *vt_uint[7] = { h_qi, h_addref, h_release, NULL, NULL, NULL, uint_value };
static void *obj_bgp = vt_bgp, *obj_uint = vt_uint;
static HSTRING hs(const WCHAR *text) { HSTRING s; WindowsCreateString(text, wcslen(text), &s); return s; }
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

typedef HRESULT (WINAPI *geopoint_create_fn)(OBJ, BGP, OBJ *);
typedef HRESULT (WINAPI *geopoint_create_sys_fn)(OBJ, BGP, int, OBJ *);
typedef HRESULT (WINAPI *geopoint_create_full_fn)(OBJ, BGP, int, unsigned, OBJ *);

/* the interfaces added to Geocoordinate and Geoposition */
static void check_new_interfaces(OBJ pos, OBJ coord, int expect_alt, double expect_alt_value)
{
    OBJ itf = NULL, pt = NULL, shape = NULL, ref = NULL;
    HRESULT hr;
    BGP bgp = {0};
    int src = -1, ivalue = 7, remote = 1;
    unsigned srid = 0;
    LONGLONG ts = 0;

    hr = CALL(coord, 0, HRESULT, &IID_IGeocoordinate_, &itf);
    CHECK(hr == S_OK && itf, "QI IGeocoordinate by its documented IID %08lx", hr);
    if (itf) CALL(itf, 2, ULONG);

    hr = CALL(coord, 0, HRESULT, &IID_IGeocoordinateWithPoint_, &itf);
    CHECK(hr == S_OK && itf, "QI IGeocoordinateWithPoint %08lx", hr);
    if (itf)
    {
        hr = CALL(itf, 6, HRESULT, &pt);
        CHECK(hr == S_OK && pt, "Point %08lx", hr);
        if (pt)
        {
            hr = CALL(pt, 6, HRESULT, &bgp);
            CHECK(hr == S_OK && bgp.lat > 47.6061 && bgp.lat < 47.6063 && bgp.lon > -122.3322 && bgp.lon < -122.3320,
                  "Point.Position %f %f", bgp.lat, bgp.lon);
            CHECK(bgp.alt == (expect_alt ? expect_alt_value : 0.0), "Point.Position.Altitude %f", bgp.alt);
            hr = CALL(pt, 0, HRESULT, &IID_IGeoshape_, &shape);
            CHECK(hr == S_OK && shape, "QI IGeoshape %08lx", hr);
            if (shape)
            {
                src = -1; CALL(shape, 6, HRESULT, &src);
                CHECK(src == 0, "GeoshapeType %d", src);
                CALL(shape, 7, HRESULT, &srid);
                CHECK(srid == 4326, "SpatialReferenceId %u", srid);
                src = -1; CALL(shape, 8, HRESULT, &src);
                CHECK(src == 3, "AltitudeReferenceSystem %d", src);
                CALL(shape, 2, ULONG);
            }
            CALL(pt, 2, ULONG);
        }
        CALL(itf, 2, ULONG);
    }

    hr = CALL(coord, 0, HRESULT, &IID_IGeocoordinateWithPositionData_, &itf);
    CHECK(hr == S_OK && itf, "QI IGeocoordinateWithPositionData %08lx", hr);
    if (itf)
    {
        src = -1; CALL(itf, 6, HRESULT, &src);
        CHECK(src == 4, "PositionSource Unknown: %d", src);
        ref = (OBJ)1; CALL(itf, 7, HRESULT, &ref);
        CHECK(ref == NULL, "SatelliteData is null");
        CALL(itf, 2, ULONG);
    }
    hr = CALL(coord, 0, HRESULT, &IID_IGeocoordinateWithPositionSourceTimestamp_, &itf);
    CHECK(hr == S_OK && itf, "QI IGeocoordinateWithPositionSourceTimestamp %08lx", hr);
    if (itf)
    {
        ref = NULL; hr = CALL(itf, 6, HRESULT, &ref);
        CHECK(hr == S_OK && ref, "PositionSourceTimestamp %08lx", hr);
        if (ref)
        {
            CALL(ref, 6, HRESULT, &ts);
            CHECK(ts == 1700000000LL * 10000000 + 250000 * 10 + 116444736000000000LL, "PositionSourceTimestamp value %lld", ts);
            CALL(ref, 2, ULONG);
        }
        CALL(itf, 2, ULONG);
    }
    hr = CALL(coord, 0, HRESULT, &IID_IGeocoordinateWithRemoteSource_, &itf);
    CHECK(hr == S_OK && itf, "QI IGeocoordinateWithRemoteSource %08lx", hr);
    if (itf)
    {
        CALL(itf, 6, HRESULT, &remote);
        CHECK((remote & 0xff) == 0, "IsRemoteSource %d", remote);
        CALL(itf, 2, ULONG);
    }
    hr = CALL(pos, 0, HRESULT, &IID_IGeoposition2_, &itf);
    CHECK(hr == S_OK && itf, "QI IGeoposition2 %08lx", hr);
    if (itf)
    {
        ref = (OBJ)1; hr = CALL(itf, 6, HRESULT, &ref);
        CHECK(hr == S_OK && ref == NULL, "VenueData is null");
        CALL(itf, 2, ULONG);
    }
    (void)ivalue;
}

static void check_geopoint_factory(void)
{
    static const WCHAR name[] = L"Windows.Devices.Geolocation.Geopoint";
    HSTRING s;
    OBJ f = NULL, pt = NULL, shape = NULL;
    BGP in = { 10.5, -20.25, 30 }, out = {0}, bad = { 91, 0, 0 }, badlon = { 0, 181, 0 };
    int sys = -1;
    unsigned srid = 0;
    HRESULT hr;

    WindowsCreateString(name, wcslen(name), &s);
    hr = RoGetActivationFactory(s, &IID_IGeopointFactory_, (void **)&f);
    WindowsDeleteString(s);
    CHECK(hr == S_OK && f, "Geopoint factory %08lx", hr);
    if (!f) return;
    hr = ((geopoint_create_fn)SLOT(f, 6))(f, in, &pt);
    CHECK(hr == S_OK && pt, "Create %08lx", hr);
    if (pt)
    {
        CALL(pt, 6, HRESULT, &out);
        CHECK(out.lat == 10.5 && out.lon == -20.25 && out.alt == 30, "created position %f %f %f", out.lat, out.lon, out.alt);
        hr = CALL(pt, 0, HRESULT, &IID_IGeoshape_, &shape);
        if (shape)
        {
            CALL(shape, 8, HRESULT, &sys);
            CHECK(sys == 0, "Create: unspecified altitude reference system %d", sys);
            CALL(shape, 7, HRESULT, &srid);
            CHECK(srid == 4326, "Create: srid %u", srid);
            CALL(shape, 2, ULONG);
        }
        CALL(pt, 2, ULONG);
    }
    pt = NULL;
    hr = ((geopoint_create_sys_fn)SLOT(f, 7))(f, in, 4, &pt);
    CHECK(hr == S_OK && pt, "CreateWithAltitudeReferenceSystem %08lx", hr);
    if (pt)
    {
        CALL(pt, 0, HRESULT, &IID_IGeoshape_, &shape);
        if (shape) { CALL(shape, 8, HRESULT, &sys); CHECK(sys == 4, "surface system %d", sys); CALL(shape, 2, ULONG); }
        CALL(pt, 2, ULONG);
    }
    pt = NULL;
    hr = ((geopoint_create_full_fn)SLOT(f, 8))(f, in, 1, 3857, &pt);
    CHECK(hr == S_OK && pt, "CreateWith...SpatialReferenceId %08lx", hr);
    if (pt)
    {
        CALL(pt, 0, HRESULT, &IID_IGeoshape_, &shape);
        if (shape) { CALL(shape, 7, HRESULT, &srid); CHECK(srid == 3857, "srid %u", srid); CALL(shape, 2, ULONG); }
        CALL(pt, 2, ULONG);
    }
    hr = ((geopoint_create_fn)SLOT(f, 6))(f, bad, &pt);
    CHECK(hr == E_INVALIDARG, "latitude 91 %08lx", hr);
    hr = ((geopoint_create_fn)SLOT(f, 6))(f, badlon, &pt);
    CHECK(hr == E_INVALIDARG, "longitude 181 %08lx", hr);
    hr = ((geopoint_create_sys_fn)SLOT(f, 7))(f, in, 9, &pt);
    CHECK(hr == E_INVALIDARG, "altitude system 9 %08lx", hr);
    CALL(f, 2, ULONG);
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
    check_new_interfaces(pos, coord, expect_alt, 56.5);
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
    else if (!strcmp(phase, "default"))
    {
        /* no GeoClue: DefaultGeoposition stands in, tagged as the Default source */
        OBJ s2 = NULL, ref = NULL, coord = NULL, wd = NULL;
        BGP def = { 40.5, -105.25, 1600 }, got = {0};
        unsigned char rec = 0;
        double acc = -1;
        int src = -1;

        RoGetActivationFactory(hs(L"Windows.Devices.Geolocation.Geolocator"), &IID_IGeolocatorStatics2_, (void **)&s2);
        CHECK(s2 != NULL, "statics2");
        if (!s2) goto end;
        hr = CALL(s2, 6, HRESULT, &rec);
        CHECK(hr == S_OK && !(rec & 0xff), "default geoposition not recommended before any failure: %d", rec);
        ref = (OBJ)1; CALL(s2, 8, HRESULT, &ref);
        CHECK(ref == NULL, "DefaultGeoposition starts null");
        pos = get_position(geo, &err, &ops, &rhr);
        CHECK(ops == 3 && !pos, "absent without a default: ops=%d", ops);
        rec = 0; CALL(s2, 6, HRESULT, &rec);
        CHECK(rec & 0xff, "default geoposition recommended once the service is missing");
        def_value = def;
        hr = CALL(s2, 7, HRESULT, &obj_bgp);
        CHECK(hr == S_OK, "put DefaultGeoposition %08lx", hr);
        ref = NULL; CALL(s2, 8, HRESULT, &ref);
        CHECK(ref != NULL, "DefaultGeoposition set");
        if (ref) { CALL(ref, 6, HRESULT, &got); CHECK(got.lat == 40.5 && got.lon == -105.25 && got.alt == 1600, "default value %f %f %f", got.lat, got.lon, got.alt); CALL(ref, 2, ULONG); }
        pos = get_position(geo, &err, &ops, &rhr);
        CHECK(ops == 1 && rhr == S_OK && pos, "default used: ops=%d results=%08lx", ops, rhr);
        if (pos)
        {
            CALL(pos, 6, HRESULT, &coord);
            if (coord)
            {
                CALL(coord, 6, HRESULT, &acc);
                CHECK(acc > 40.4999 && acc < 40.5001, "default latitude %f", acc);
                CALL(coord, 0, HRESULT, &IID_IGeocoordinateWithPositionData_, &wd);
                if (wd) { CALL(wd, 6, HRESULT, &src); CHECK(src == 5, "PositionSource Default: %d", src); CALL(wd, 2, ULONG); }
                CALL(coord, 2, ULONG);
            }
            CALL(pos, 2, ULONG);
        }
        def_value.lat = 91;
        hr = CALL(s2, 7, HRESULT, &obj_bgp);
        CHECK(hr == E_INVALIDARG, "invalid default latitude %08lx", hr);
        hr = CALL(s2, 7, HRESULT, NULL);
        CHECK(hr == S_OK, "put null DefaultGeoposition %08lx", hr);
        pos = get_position(geo, &err, &ops, &rhr);
        CHECK(ops == 3 && !pos, "default cleared: ops=%d", ops);
        CALL(s2, 2, ULONG);
    }
    else if (!strcmp(phase, "scalar"))
    {
        OBJ sc = NULL, ref = NULL;
        unsigned got = 0;

        hr = CALL(geo, 0, HRESULT, &IID_IGeolocatorWithScalarAccuracy_, &sc);
        CHECK(hr == S_OK && sc, "QI IGeolocatorWithScalarAccuracy %08lx", hr);
        if (!sc) goto end;
        ref = (OBJ)1; CALL(sc, 6, HRESULT, &ref);
        CHECK(ref == NULL, "DesiredAccuracyInMeters starts null");
        meters_value = 50;
        hr = CALL(sc, 7, HRESULT, &obj_uint);
        CHECK(hr == S_OK, "put meters %08lx", hr);
        ref = NULL; CALL(sc, 6, HRESULT, &ref);
        CHECK(ref != NULL, "meters set");
        if (ref) { CALL(ref, 6, HRESULT, &got); CHECK(got == 50, "meters %u", got); CALL(ref, 2, ULONG); }
        pos = get_position(geo, &err, &ops, &rhr);     /* 50 m: the exact level */
        if (pos) CALL(pos, 2, ULONG);
        meters_value = 5000;
        CALL(sc, 7, HRESULT, &obj_uint);
        pos = get_position(geo, &err, &ops, &rhr);     /* 5000 m: the default level */
        if (pos) CALL(pos, 2, ULONG);
        /* setting the enum goes back to the enum */
        CALL(geo, G_put_DesiredAccuracy, HRESULT, 0);
        ref = (OBJ)1; CALL(sc, 6, HRESULT, &ref);
        CHECK(ref == NULL, "meters cleared by DesiredAccuracy");
        CALL(sc, 2, ULONG);
        check_geopoint_factory();
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
