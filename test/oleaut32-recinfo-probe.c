/* Probe for patches/sg/5038: IRecordInfo field access on a record type built
 * with CreateTypeLib2 (BSTR, I4, VARIANT and IUnknown fields).
 * GetField "places a copy of the field's value in the variant" (a new BSTR, an
 * AddRef'd interface, the VARIANT's value); PutField and PUTREF assign "using
 * normal coercion rules", releasing the old value; PutFieldNoCopy "passes
 * ownership ... does not copy the data". Table driven; lines "ok"/"FAIL",
 * last "RESULT: PASS|FAIL". 6233: a record with a record field and an enum field:
 * RecordCopy copies the inner record deep, RecordClear clears it. */
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

static ITypeInfo *use_ti;   /* IUse: one method taking a pointer to the outer record (6233) */

/* a record with a record field, an enum field and a BSTR (patches/sg/6233) */
static IRecordInfo *make_nested(ULONG *off_inner, ULONG *off_e, ULONG *off_t)
{
    ICreateTypeLib2 *ctl;
    ICreateTypeInfo *inner, *color, *outer;
    ITypeInfo *ti_inner, *ti_color, *ti;
    IRecordInfo *ri = NULL;
    HREFTYPE h_inner, h_color;
    VARDESC vd, *got;
    VARIANT val;
    WCHAR path[MAX_PATH];
    unsigned int i;

    GetTempPathW(MAX_PATH, path);
    lstrcatW(path, L"sg-recinfo2.tlb");
    if (FAILED(CreateTypeLib2(sizeof(void *) == 8 ? SYS_WIN64 : SYS_WIN32, path, &ctl))) return NULL;
    ICreateTypeLib2_CreateTypeInfo(ctl, (LPOLESTR)L"inner", TKIND_RECORD, &inner);
    memset(&vd, 0, sizeof(vd));
    vd.varkind = VAR_PERINSTANCE;
    vd.memid = 0x40000000; vd.elemdescVar.tdesc.vt = VT_BSTR;
    ICreateTypeInfo_AddVarDesc(inner, 0, &vd); ICreateTypeInfo_SetVarName(inner, 0, (LPOLESTR)L"s");
    vd.memid = 0x40000001; vd.elemdescVar.tdesc.vt = VT_I4;
    ICreateTypeInfo_AddVarDesc(inner, 1, &vd); ICreateTypeInfo_SetVarName(inner, 1, (LPOLESTR)L"n");
    ICreateTypeInfo_LayOut(inner);
    ICreateTypeLib2_CreateTypeInfo(ctl, (LPOLESTR)L"color", TKIND_ENUM, &color);
    memset(&vd, 0, sizeof(vd));
    vd.memid = 0x40000000; vd.varkind = VAR_CONST; vd.elemdescVar.tdesc.vt = VT_I4;
    V_VT(&val) = VT_I4; V_I4(&val) = 2; vd.lpvarValue = &val;
    ICreateTypeInfo_AddVarDesc(color, 0, &vd); ICreateTypeInfo_SetVarName(color, 0, (LPOLESTR)L"blue");
    ICreateTypeInfo_LayOut(color);
    ICreateTypeInfo_QueryInterface(inner, &IID_ITypeInfo, (void **)&ti_inner);
    ICreateTypeInfo_QueryInterface(color, &IID_ITypeInfo, (void **)&ti_color);

    ICreateTypeLib2_CreateTypeInfo(ctl, (LPOLESTR)L"outer", TKIND_RECORD, &outer);
    ICreateTypeInfo_AddRefTypeInfo(outer, ti_inner, &h_inner);
    ICreateTypeInfo_AddRefTypeInfo(outer, ti_color, &h_color);
    memset(&vd, 0, sizeof(vd));
    vd.varkind = VAR_PERINSTANCE;
    vd.memid = 0x40000000; vd.elemdescVar.tdesc.vt = VT_I4;
    ICreateTypeInfo_AddVarDesc(outer, 0, &vd); ICreateTypeInfo_SetVarName(outer, 0, (LPOLESTR)L"a");
    vd.memid = 0x40000001; vd.elemdescVar.tdesc.vt = VT_USERDEFINED; vd.elemdescVar.tdesc.hreftype = h_inner;
    ICreateTypeInfo_AddVarDesc(outer, 1, &vd); ICreateTypeInfo_SetVarName(outer, 1, (LPOLESTR)L"in");
    vd.memid = 0x40000002; vd.elemdescVar.tdesc.hreftype = h_color;
    ICreateTypeInfo_AddVarDesc(outer, 2, &vd); ICreateTypeInfo_SetVarName(outer, 2, (LPOLESTR)L"e");
    vd.memid = 0x40000003; vd.elemdescVar.tdesc.vt = VT_BSTR;
    ICreateTypeInfo_AddVarDesc(outer, 3, &vd); ICreateTypeInfo_SetVarName(outer, 3, (LPOLESTR)L"t");
    ICreateTypeInfo_LayOut(outer);
    ICreateTypeInfo_QueryInterface(outer, &IID_ITypeInfo, (void **)&ti);
    {
        ICreateTypeInfo *use;
        HREFTYPE href;
        FUNCDESC fd;
        static ELEMDESC param;
        static TYPEDESC ptr;

        ICreateTypeLib2_CreateTypeInfo(ctl, (LPOLESTR)L"IUse", TKIND_INTERFACE, &use);
        ICreateTypeInfo_AddRefTypeInfo(use, ti, &href);
        memset(&fd, 0, sizeof(fd));
        memset(&param, 0, sizeof(param));
        ptr.vt = VT_USERDEFINED; ptr.hreftype = href;
        param.tdesc.vt = VT_PTR; param.tdesc.lptdesc = &ptr;
        param.paramdesc.wParamFlags = PARAMFLAG_FIN;
        fd.memid = 1; fd.funckind = FUNC_PUREVIRTUAL; fd.invkind = INVOKE_FUNC; fd.callconv = CC_STDCALL;
        fd.cParams = 1; fd.lprgelemdescParam = &param; fd.elemdescFunc.tdesc.vt = VT_HRESULT;
        ICreateTypeInfo_AddFuncDesc(use, 0, &fd);
        ICreateTypeInfo_LayOut(use);
        ICreateTypeInfo_QueryInterface(use, &IID_ITypeInfo, (void **)&use_ti);
        ICreateTypeInfo_Release(use);
    }
    for (i = 1; i <= 3; i++)
        if (SUCCEEDED(ITypeInfo_GetVarDesc(ti, i, &got)))
        {
            *(i == 1 ? off_inner : i == 2 ? off_e : off_t) = got->oInst;
            ITypeInfo_ReleaseVarDesc(ti, got);
        }
    GetRecordInfoFromTypeInfo(ti, &ri);
    ITypeInfo_Release(ti);
    ITypeInfo_Release(ti_inner);
    ITypeInfo_Release(ti_color);
    ICreateTypeInfo_Release(outer);
    ICreateTypeInfo_Release(inner);
    ICreateTypeInfo_Release(color);
    ICreateTypeLib2_Release(ctl);
    return ri;
}

/* ITypeInfo::Invoke with a record for a pointer-to-record parameter (6233) */
static void *got_record;
static HRESULT WINAPI use_record(void *this, void *rec) { got_record = rec; return S_OK; }
static void *use_vtbl[] = { use_record };
static struct { void **vtbl; } use_obj = { use_vtbl };

static void invoke_record(IRecordInfo *ri)
{
    DISPPARAMS dp;
    VARIANT arg, result;
    EXCEPINFO ei;
    UINT err = 0;
    HRESULT hr;
    void *rec = IRecordInfo_RecordCreate(ri);

    if (!use_ti) { check(0, "IUse made"); return; }
    V_VT(&arg) = VT_RECORD;
    V_RECORD(&arg) = rec;
    V_RECORDINFO(&arg) = ri;
    dp.rgvarg = &arg; dp.cArgs = 1; dp.rgdispidNamedArgs = NULL; dp.cNamedArgs = 0;
    VariantInit(&result);
    got_record = NULL;
    hr = ITypeInfo_Invoke(use_ti, &use_obj, 1, DISPATCH_METHOD, &dp, &result, &ei, &err);
    check(hr == S_OK && got_record == rec, "Invoke passes a record to a pointer-to-record parameter: %#lx %d", hr, got_record == rec);
    ITypeInfo_Release(use_ti);
    IRecordInfo_RecordDestroy(ri, rec);
}

/* a record with a SAFEARRAY field after a pointer field: copied deep, cleared to NULL (6237) */
static void arrfield(void)
{
    ICreateTypeLib2 *ctl;
    ICreateTypeInfo *cti;
    ITypeInfo *ti;
    IRecordInfo *ri = NULL;
    VARDESC vd, *got;
    static TYPEDESC elem;
    WCHAR path[MAX_PATH];
    ULONG off_a = 0;
    SAFEARRAYBOUND b = {2, 0};
    SAFEARRAY *sa, *copy;
    BYTE *src, *dst, *recs;
    BSTR str;
    LONG i;

    GetTempPathW(MAX_PATH, path);
    lstrcatW(path, L"sg-recinfo3.tlb");
    if (FAILED(CreateTypeLib2(sizeof(void *) == 8 ? SYS_WIN64 : SYS_WIN32, path, &ctl))) { check(0, "typelib made"); return; }
    ICreateTypeLib2_CreateTypeInfo(ctl, (LPOLESTR)L"witharr", TKIND_RECORD, &cti);
    memset(&vd, 0, sizeof(vd));
    vd.varkind = VAR_PERINSTANCE;
    vd.memid = 0x40000000; vd.elemdescVar.tdesc.vt = VT_BSTR;
    ICreateTypeInfo_AddVarDesc(cti, 0, &vd); ICreateTypeInfo_SetVarName(cti, 0, (LPOLESTR)L"s");
    elem.vt = VT_BSTR;
    vd.memid = 0x40000001; vd.elemdescVar.tdesc.vt = VT_SAFEARRAY; vd.elemdescVar.tdesc.lptdesc = &elem;
    ICreateTypeInfo_AddVarDesc(cti, 1, &vd); ICreateTypeInfo_SetVarName(cti, 1, (LPOLESTR)L"a");
    ICreateTypeInfo_LayOut(cti);
    ICreateTypeInfo_QueryInterface(cti, &IID_ITypeInfo, (void **)&ti);
    if (SUCCEEDED(ITypeInfo_GetVarDesc(ti, 1, &got))) { off_a = got->oInst; ITypeInfo_ReleaseVarDesc(ti, got); }
    GetRecordInfoFromTypeInfo(ti, &ri);
    ITypeInfo_Release(ti);
    ICreateTypeInfo_Release(cti);
    ICreateTypeLib2_Release(ctl);
    check(ri != NULL && off_a == sizeof(void *), "a record with a SAFEARRAY field made (offset %lu)", off_a);
    if (!ri) return;

    src = IRecordInfo_RecordCreate(ri);
    dst = IRecordInfo_RecordCreate(ri);
    sa = SafeArrayCreate(VT_BSTR, 1, &b);
    for (i = 0; i < 2; i++) { str = SysAllocString(i ? L"two" : L"one"); SafeArrayPutElement(sa, &i, str); SysFreeString(str); }
    *(BSTR *)src = SysAllocString(L"head");
    *(SAFEARRAY **)(src + off_a) = sa;
    check(IRecordInfo_RecordCopy(ri, src, dst) == S_OK, "RecordCopy of a record with an array");
    copy = *(SAFEARRAY **)(dst + off_a);
    check(copy && copy != sa, "the array field copied deep");
    if (copy && copy != sa)
    {
        i = 1; str = NULL;
        check(SafeArrayGetElement(copy, &i, &str) == S_OK && str && !lstrcmpW(str, L"two"), "the copy holds the elements");
        SysFreeString(str);
    }
    check(IRecordInfo_RecordClear(ri, dst) == S_OK && !*(SAFEARRAY **)(dst + off_a) && !*(BSTR *)dst,
          "RecordClear destroys the array and clears the field");
    check(*(SAFEARRAY **)(src + off_a) == sa, "the source array untouched");
    IRecordInfo_RecordDestroy(ri, src);
    IRecordInfo_RecordDestroy(ri, dst);

    /* an array of such records: destroying it clears each record */
    recs = NULL;
    sa = SafeArrayCreateEx(VT_RECORD, 1, &b, ri);
    check(sa != NULL, "an array of records made");
    if (sa && SUCCEEDED(SafeArrayAccessData(sa, (void **)&recs)))
    {
        SAFEARRAYBOUND b1 = {1, 0};
        ULONG size = 0;

        IRecordInfo_GetSize(ri, &size);
        *(SAFEARRAY **)(recs + off_a) = SafeArrayCreate(VT_BSTR, 1, &b1); /* the first record's array */
        *(BSTR *)(recs + size) = SysAllocString(L"second");             /* the second record's string */
        SafeArrayUnaccessData(sa);
        check(SafeArrayDestroy(sa) == S_OK, "an array of records with array fields destroyed");
    }
    IRecordInfo_Release(ri);
}

static void nested(void)
{
    ULONG off_inner = 0, off_e = 0, off_t = 0;
    IRecordInfo *ri = make_nested(&off_inner, &off_e, &off_t);
    BYTE *src, *dst;
    BSTR s;

    check(ri != NULL, "a record with a record field made");
    if (!ri) return;
    src = IRecordInfo_RecordCreate(ri);
    dst = IRecordInfo_RecordCreate(ri);
    s = SysAllocString(L"deep");
    *(BSTR *)(src + off_inner) = s;
    *(LONG *)(src + off_inner + sizeof(void *)) = 7;
    *(LONG *)(src + off_e) = 2;
    *(BSTR *)(src + off_t) = SysAllocString(L"tail");
    check(IRecordInfo_RecordCopy(ri, src, dst) == S_OK, "RecordCopy of the outer record");
    check(*(BSTR *)(dst + off_inner) && *(BSTR *)(dst + off_inner) != s && !lstrcmpW(*(BSTR *)(dst + off_inner), L"deep"),
          "the inner record's BSTR copied deep");
    check(*(LONG *)(dst + off_inner + sizeof(void *)) == 7, "the inner record's number copied");
    check(*(LONG *)(dst + off_e) == 2, "the enum field copied");
    check(*(BSTR *)(dst + off_t) && !lstrcmpW(*(BSTR *)(dst + off_t), L"tail"), "the field after them copied");
    check(IRecordInfo_RecordClear(ri, dst) == S_OK && !*(BSTR *)(dst + off_inner), "RecordClear clears the inner record");
    IRecordInfo_RecordDestroy(ri, src);
    IRecordInfo_RecordDestroy(ri, dst);
    invoke_record(ri);
    IRecordInfo_Release(ri);
    arrfield();
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
    nested();
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
