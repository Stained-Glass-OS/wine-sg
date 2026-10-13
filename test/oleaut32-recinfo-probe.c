/* Probe for patches/sg/5038: IRecordInfo field access on a record type built
 * with CreateTypeLib2 (BSTR, I4, VARIANT and IUnknown fields).
 * GetField "places a copy of the field's value in the variant" (a new BSTR, an
 * AddRef'd interface, the VARIANT's value); PutField and PUTREF assign "using
 * normal coercion rules", releasing the old value; PutFieldNoCopy "passes
 * ownership ... does not copy the data". Table driven; lines "ok"/"FAIL",
 * last "RESULT: PASS|FAIL". */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static LONG refs = 1;
static HRESULT WINAPI u_QueryInterface(IUnknown *iface, REFIID riid, void **obj) { *obj = iface; IUnknown_AddRef(iface); return S_OK; }
static ULONG WINAPI u_AddRef(IUnknown *iface) { return InterlockedIncrement(&refs); }
static ULONG WINAPI u_Release(IUnknown *iface) { return InterlockedDecrement(&refs); }
static IUnknownVtbl u_vtbl = {u_QueryInterface, u_AddRef, u_Release};
static IUnknown unk = {&u_vtbl};

static IRecordInfo *make_record_info(void)
{
    static const struct { const WCHAR *name; VARTYPE vt; } fields[] =
        {{L"name", VT_BSTR}, {L"count", VT_I4}, {L"value", VT_VARIANT}, {L"obj", VT_UNKNOWN}};
    ICreateTypeLib2 *ctl;
    ICreateTypeInfo *cti;
    ITypeInfo *ti;
    IRecordInfo *ri = NULL;
    unsigned int i;
    WCHAR path[MAX_PATH];

    GetTempPathW(MAX_PATH, path);
    lstrcatW(path, L"sg-recinfo.tlb");
    if (FAILED(CreateTypeLib2(sizeof(void *) == 8 ? SYS_WIN64 : SYS_WIN32, path, &ctl))) return NULL;
    ICreateTypeLib2_CreateTypeInfo(ctl, (LPOLESTR)L"rec", TKIND_RECORD, &cti);
    for (i = 0; i < ARRAYSIZE(fields); i++)
    {
        VARDESC vd;
        memset(&vd, 0, sizeof(vd));
        vd.memid = 0x40000000 + i;
        vd.varkind = VAR_PERINSTANCE;
        vd.elemdescVar.tdesc.vt = fields[i].vt;
        ICreateTypeInfo_AddVarDesc(cti, i, &vd);
        ICreateTypeInfo_SetVarName(cti, i, (LPOLESTR)fields[i].name);
    }
    ICreateTypeInfo_LayOut(cti);
    ICreateTypeInfo_QueryInterface(cti, &IID_ITypeInfo, (void **)&ti);
    GetRecordInfoFromTypeInfo(ti, &ri);
    ITypeInfo_Release(ti);
    ICreateTypeInfo_Release(cti);
    ICreateTypeLib2_Release(ctl);
    return ri;
}

int main(void)
{
    IRecordInfo *ri;
    void *rec;
    VARIANT v, got;
    BSTR b;
    HRESULT hr;

    CoInitialize(NULL);
    ri = make_record_info();
    check(ri != NULL, "record info made");
    if (!ri) { printf("RESULT: FAIL\n"); return 1; }
    rec = IRecordInfo_RecordCreate(ri);

    /* BSTR field */
    V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(L"abc");
    hr = IRecordInfo_PutField(ri, INVOKE_PROPERTYPUT, rec, L"name", &v);
    check(hr == S_OK, "PutField(name, BSTR): %#lx", hr);
    VariantInit(&got);
    hr = IRecordInfo_GetField(ri, rec, L"name", &got);
    check(hr == S_OK && V_VT(&got) == VT_BSTR && !lstrcmpW(V_BSTR(&got), L"abc") && V_BSTR(&got) != V_BSTR(&v),
          "GetField(name) is a copy: %#lx %d", hr, V_VT(&got));
    VariantClear(&got);
    hr = IRecordInfo_GetField(ri, rec, L"name", &got);
    check(hr == S_OK && V_VT(&got) == VT_BSTR && !lstrcmpW(V_BSTR(&got), L"abc"), "the field survives clearing the copy");
    VariantClear(&got);
    VariantClear(&v);
    V_VT(&v) = VT_I4; V_I4(&v) = 123;
    hr = IRecordInfo_PutField(ri, INVOKE_PROPERTYPUT, rec, L"name", &v);
    VariantInit(&got);
    IRecordInfo_GetField(ri, rec, L"name", &got);
    check(hr == S_OK && V_VT(&got) == VT_BSTR && !lstrcmpW(V_BSTR(&got), L"123"), "PutField(name, I4) coerces: %#lx", hr);
    VariantClear(&got);

    /* I4 field, through PUTREF too */
    V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(L"7");
    hr = IRecordInfo_PutField(ri, INVOKE_PROPERTYPUTREF, rec, L"count", &v);
    VariantClear(&v);
    VariantInit(&got);
    IRecordInfo_GetField(ri, rec, L"count", &got);
    check(hr == S_OK && V_VT(&got) == VT_I4 && V_I4(&got) == 7, "PutField(count, BSTR \"7\", PUTREF): %#lx %d %ld", hr,
          V_VT(&got), V_I4(&got));

    /* VARIANT field */
    V_VT(&v) = VT_R8; V_R8(&v) = 2.5;
    hr = IRecordInfo_PutField(ri, INVOKE_PROPERTYPUT, rec, L"value", &v);
    VariantInit(&got);
    hr = IRecordInfo_GetField(ri, rec, L"value", &got);
    check(hr == S_OK && V_VT(&got) == VT_R8 && V_R8(&got) == 2.5, "VARIANT field round trip: %#lx %d", hr, V_VT(&got));
    VariantClear(&got);

    /* IUnknown field: references */
    V_VT(&v) = VT_UNKNOWN; V_UNKNOWN(&v) = &unk;
    hr = IRecordInfo_PutField(ri, INVOKE_PROPERTYPUT, rec, L"obj", &v);
    check(hr == S_OK && refs == 2, "PutField(obj) holds a reference: %#lx refs %ld", hr, refs);
    VariantInit(&got);
    hr = IRecordInfo_GetField(ri, rec, L"obj", &got);
    check(hr == S_OK && V_VT(&got) == VT_UNKNOWN && V_UNKNOWN(&got) == &unk && refs == 3, "GetField(obj) adds one: %#lx refs %ld",
          hr, refs);
    VariantClear(&got);
    V_VT(&v) = VT_UNKNOWN; V_UNKNOWN(&v) = NULL;
    hr = IRecordInfo_PutField(ri, INVOKE_PROPERTYPUT, rec, L"obj", &v);
    check(hr == S_OK && refs == 1, "replacing it with NULL releases the old one: %#lx refs %ld", hr, refs);

    /* PutFieldNoCopy takes the BSTR itself */
    b = SysAllocString(L"owned");
    V_VT(&v) = VT_BSTR; V_BSTR(&v) = b;
    hr = IRecordInfo_PutFieldNoCopy(ri, INVOKE_PROPERTYPUT, rec, L"name", &v);
    {
        PVOID array = NULL;
        VariantInit(&got);
        IRecordInfo_GetFieldNoCopy(ri, rec, L"name", &got, &array);
        check(hr == S_OK && V_VT(&got) == (VT_BYREF | VT_BSTR) && *V_BSTRREF(&got) == b, "PutFieldNoCopy stores the BSTR itself: %#lx",
              hr);
    }

    IRecordInfo_RecordDestroy(ri, rec);
    IRecordInfo_Release(ri);
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
