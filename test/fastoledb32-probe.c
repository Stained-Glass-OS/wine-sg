#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <ocidl.h>
#include <oledb.h>
#include <oledberr.h>
#include <msdasc.h>
#include <msdadc.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const GUID CLSID_DI = {0x2206CDB0,0x19C1,0x11D1,{0x89,0xE0,0x00,0xC0,0x4F,0xD7,0xA8,0x29}};
static const GUID CLSID_DL = {0x2206CDB2,0x19C1,0x11D1,{0x89,0xE0,0x00,0xC0,0x4F,0xD7,0xA8,0x29}};
static const GUID IID_DI = {0x2206CCB1,0x19C1,0x11D1,{0x89,0xE0,0x00,0xC0,0x4F,0xD7,0xA8,0x29}};
static const GUID IID_DSL = {0x2206CCB2,0x19C1,0x11D1,{0x89,0xE0,0x00,0xC0,0x4F,0xD7,0xA8,0x29}};
static const GUID CLSID_CONV = {0xc8b522d1,0x5cf3,0x11ce,{0xad,0xe5,0x00,0xaa,0x00,0x44,0x77,0x3d}};
static const GUID IID_CONV = {0x0c733a8d,0x2a1c,0x11ce,{0xad,0xe5,0x00,0xaa,0x00,0x44,0x77,0x3d}};
static const GUID CLSID_ROWPOS = {0x2048eee6,0x7fa2,0x11d0,{0x9e,0x6a,0x00,0xa0,0xc9,0x13,0x8c,0x29}};
static const GUID IID_ROWPOS = {0x0c733a94,0x2a1c,0x11ce,{0xad,0xe5,0x00,0xaa,0x00,0x44,0x77,0x3d}};

static void test_udl(IDataInitialize *di)
{
    WCHAR *str = NULL;
    HANDLE f;
    WORD bom = 0;
    DWORD n;
    HRESULT hr;

    DeleteFileW(L"t.udl");
    hr = IDataInitialize_WriteStringToStorage(di, L"t.udl", L"Provider=MSDASQL;Data Source=x", CREATE_NEW);
    CHECK(hr == S_OK, "write %08lx", hr);
    f = CreateFileW(L"t.udl", GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(f != INVALID_HANDLE_VALUE, "file exists");
    ReadFile(f, &bom, 2, &n, NULL);
    CloseHandle(f);
    CHECK(bom == 0xfeff, "bom %04x", bom);
    hr = IDataInitialize_WriteStringToStorage(di, L"t.udl", L"x", CREATE_NEW);
    CHECK(FAILED(hr), "CREATE_NEW on existing file %08lx", hr);
    hr = IDataInitialize_LoadStringFromStorage(di, L"t.udl", &str);
    CHECK(hr == S_OK && str && !lstrcmpW(str, L"Provider=MSDASQL;Data Source=x"), "load %08lx %ls", hr, str ? str : L"(null)");
    CoTaskMemFree(str);
    hr = IDataInitialize_WriteStringToStorage(di, L"t.udl", L"Provider=Other", CREATE_ALWAYS);
    CHECK(hr == S_OK, "overwrite %08lx", hr);
    str = NULL;
    hr = IDataInitialize_LoadStringFromStorage(di, L"t.udl", &str);
    CHECK(hr == S_OK && str && !lstrcmpW(str, L"Provider=Other"), "load2 %08lx", hr);
    CoTaskMemFree(str);
    str = NULL;
    hr = IDataInitialize_LoadStringFromStorage(di, L"missing.udl", &str);
    CHECK(FAILED(hr) && !str, "missing %08lx", hr);
    DeleteFileW(L"t.udl");
}

struct sink { IRowPositionChange iface; LONG ref; };
static HRESULT WINAPI s_qi(IRowPositionChange *i, REFIID r, void **o)
{ if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IRowPositionChange)) { *o = i; IRowPositionChange_AddRef(i); return S_OK; } *o = NULL; return E_NOINTERFACE; }
static ULONG WINAPI s_ar(IRowPositionChange *i) { return InterlockedIncrement(&((struct sink *)i)->ref); }
static ULONG WINAPI s_rl(IRowPositionChange *i) { return InterlockedDecrement(&((struct sink *)i)->ref); }
static HRESULT WINAPI s_ch(IRowPositionChange *i, DBREASON a, DBEVENTPHASE b, BOOL c) { return S_OK; }
static IRowPositionChangeVtbl sink_vtbl = { s_qi, s_ar, s_rl, s_ch };

static void test_rowpos(void)
{
    IRowPosition *rp;
    IConnectionPointContainer *cpc;
    IEnumConnectionPoints *ecp, *ecp2;
    IEnumConnections *ec;
    IConnectionPoint *cp;
    struct sink sink = { { &sink_vtbl }, 1 };
    CONNECTDATA cd;
    ULONG got;
    DWORD cookie;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_ROWPOS, NULL, CLSCTX_INPROC_SERVER, &IID_ROWPOS, (void **)&rp);
    CHECK(hr == S_OK, "rowpos %08lx", hr);
    if (hr != S_OK) return;
    IRowPosition_QueryInterface(rp, &IID_IConnectionPointContainer, (void **)&cpc);
    hr = IConnectionPointContainer_EnumConnectionPoints(cpc, &ecp);
    CHECK(hr == S_OK, "enum cps %08lx", hr);
    hr = IEnumConnectionPoints_Next(ecp, 1, &cp, &got);
    CHECK(hr == S_OK && got == 1, "next %08lx", hr);
    hr = IEnumConnectionPoints_Clone(ecp, &ecp2);
    CHECK(hr == S_OK, "clone");
    hr = IEnumConnectionPoints_Next(ecp2, 1, &cp, &got);
    CHECK(hr == S_FALSE && got == 0, "clone keeps position %08lx", hr);
    IEnumConnectionPoints_Reset(ecp2);
    hr = IEnumConnectionPoints_Next(ecp2, 1, &cp, &got);
    CHECK(hr == S_OK, "after reset");
    IConnectionPoint_Release(cp);
    IEnumConnectionPoints_Release(ecp2);
    hr = IEnumConnectionPoints_Next(ecp, 1, &cp, &got);
    CHECK(hr == S_FALSE, "end %08lx", hr);
    IEnumConnectionPoints_Release(ecp);

    hr = IConnectionPointContainer_FindConnectionPoint(cpc, &IID_IRowPositionChange, &cp);
    CHECK(hr == S_OK, "find");
    hr = IConnectionPoint_EnumConnections(cp, &ec);
    CHECK(hr == S_OK, "enum empty %08lx", hr);
    if (hr == S_OK) { hr = IEnumConnections_Next(ec, 1, &cd, &got); CHECK(hr == S_FALSE && got == 0, "empty next"); IEnumConnections_Release(ec); }
    hr = IConnectionPoint_Advise(cp, (IUnknown *)&sink.iface, &cookie);
    CHECK(hr == S_OK, "advise");
    hr = IConnectionPoint_EnumConnections(cp, &ec);
    CHECK(hr == S_OK, "enum %08lx", hr);
    hr = IEnumConnections_Next(ec, 1, &cd, &got);
    CHECK(hr == S_OK && got == 1 && cd.dwCookie == cookie && cd.pUnk == (IUnknown *)&sink.iface, "conn data");
    if (hr == S_OK) IUnknown_Release(cd.pUnk);
    IEnumConnections_Release(ec);
    IConnectionPoint_Unadvise(cp, cookie);
    IConnectionPoint_Release(cp);
    CHECK(sink.ref == 1, "sink leaked refs %ld", sink.ref);
    IConnectionPointContainer_Release(cpc);
    IRowPosition_Release(rp);
}

static void test_convert(void)
{
    IDataConvert *c;
    DB_NUMERIC num;
    DBLENGTH len = 0x1234;
    DBSTATUS st;
    int i4 = 4098, o4 = 0;
    signed char i1 = 0;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_CONV, NULL, CLSCTX_INPROC_SERVER, &IID_CONV, (void **)&c);
    CHECK(hr == S_OK, "conv %08lx", hr);
    if (hr != S_OK) return;
    memset(&num, 0xfe, sizeof(num));
    hr = IDataConvert_DataConvert(c, DBTYPE_I4, DBTYPE_NUMERIC, 0, &len, &i4, &num, sizeof(num), 0, &st, 10, 2, 0);
    CHECK(hr == S_OK && st == DBSTATUS_S_OK && len == sizeof(num), "to numeric %08lx %lx", hr, (long)st);
    CHECK(num.precision == 10 && num.scale == 2 && num.sign == 1 && *(DWORD *)num.val == 409800, "numeric val %u %u %u %lu",
          num.precision, num.scale, num.sign, *(DWORD *)num.val);
    hr = IDataConvert_DataConvert(c, DBTYPE_NUMERIC, DBTYPE_I4, sizeof(num), &len, &num, &o4, sizeof(o4), 0, &st, 0, 0, 0);
    CHECK(hr == S_OK && o4 == 4098, "numeric back %08lx %d", hr, o4);
    i4 = 0x4321cafe;
    hr = IDataConvert_DataConvert(c, DBTYPE_I4, DBTYPE_I1, 0, &len, &i4, &i1, sizeof(i1), 0, &st, 0, 0, 0);
    CHECK(hr == DB_E_DATAOVERFLOW && st == DBSTATUS_E_DATAOVERFLOW, "narrow %08lx", hr);
    i4 = -5;
    len = 0x77;
    hr = IDataConvert_DataConvert(c, DBTYPE_I4, DBTYPE_UI8, 0, &len, &i4, &num, sizeof(ULONGLONG), 0, &st, 0, 0, 0);
    CHECK(hr == DB_E_ERRORSOCCURRED && st == DBSTATUS_E_SIGNMISMATCH && len == 0x77, "sign %08lx", hr);
    IDataConvert_Release(c);
}

int main(void)
{
    IDataInitialize *di;
    IDataSourceLocator *dsl;
    IDispatch *disp;
    MULTI_QI mq = { &IID_IUnknown, NULL, 0 };
    UINT cnt = 0;
    DISPID id;
    LPOLESTR name = (LPOLESTR)L"hWnd";
    HRESULT hr;

    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_DI, NULL, CLSCTX_INPROC_SERVER, &IID_DI, (void **)&di);
    CHECK(hr == S_OK, "datainit %08lx", hr);
    if (hr == S_OK)
    {
        test_udl(di);
        hr = IDataInitialize_CreateDBInstanceEx(di, &CLSID_CONV, NULL, CLSCTX_INPROC_SERVER, NULL, NULL, 1, &mq);
        CHECK(hr == S_OK && mq.hr == S_OK && mq.pItf, "createex %08lx %08lx", hr, mq.hr);
        if (mq.pItf) IUnknown_Release(mq.pItf);
        IDataInitialize_Release(di);
    }

    hr = CoCreateInstance(&CLSID_DL, NULL, CLSCTX_INPROC_SERVER, &IID_DSL, (void **)&dsl);
    CHECK(hr == S_OK, "datalinks %08lx", hr);
    if (hr == S_OK)
    {
        hr = IDataSourceLocator_QueryInterface(dsl, &IID_IDispatch, (void **)&disp);
        CHECK(hr == S_OK, "disp");
        hr = IDispatch_GetTypeInfoCount(disp, &cnt);
        CHECK(hr == S_OK && cnt == 1, "typeinfocount %08lx %u", hr, cnt);
        hr = IDispatch_GetIDsOfNames(disp, &IID_NULL, &name, 1, LOCALE_USER_DEFAULT, &id);
        CHECK(hr == S_OK, "ids %08lx", hr);
        {
            DISPPARAMS dp = { 0 };
            VARIANT v;
            VariantInit(&v);
            hr = IDispatch_Invoke(disp, id, &IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET, &dp, &v, NULL, NULL);
            CHECK(hr == S_OK && (V_VT(&v) == VT_I4 || V_VT(&v) == VT_I8) && V_I4(&v) == 0, "invoke hWnd %08lx vt %d", hr, V_VT(&v));
        }
        IDispatch_Release(disp);
        {
            IDataInitialize *di2;
            WCHAR *s = NULL;
            hr = IDataSourceLocator_QueryInterface(dsl, &IID_DI, (void **)&di2);
            CHECK(hr == S_OK, "qi di");
            if (hr == S_OK)
            {
                hr = IDataInitialize_WriteStringToStorage(di2, L"u.udl", L"Provider=P;X=1", CREATE_ALWAYS);
                CHECK(hr == S_OK, "dl write %08lx", hr);
                hr = IDataInitialize_LoadStringFromStorage(di2, L"u.udl", &s);
                CHECK(hr == S_OK && s && !lstrcmpW(s, L"Provider=P;X=1"), "dl load %08lx", hr);
                CoTaskMemFree(s);
                DeleteFileW(L"u.udl");
                IDataInitialize_Release(di2);
            }
        }
        IDataSourceLocator_Release(dsl);
    }
    test_rowpos();
    test_convert();
    CoUninitialize();
    printf(fails ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", fails);
    return fails != 0;
}
