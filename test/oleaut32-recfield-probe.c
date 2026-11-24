/* Probe for patches/sg/9611: IRecordInfo access to a record's record and array fields.
 * The record "outer" { I4 a; inner in; SAFEARRAY(BSTR) names; SAFEARRAY(inner) items } is built
 * with CreateTypeLib2. pywin32's com_record reads a field through GetFieldNoCopy and short-cuts a
 * nested structure when it comes back VT_BYREF|VT_RECORD with its IRecordInfo; arrays come back
 * VT_BYREF|VT_ARRAY|<element>; it writes fields with PutField (VT_RECORD, VT_ARRAY|<element>).
 * GetField copies (VT_RECORD with a new record, VT_ARRAY with a copied array).
 * Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static ULONG off_in, off_names, off_items, inner_size;
static IRecordInfo *inner_ri;

static IRecordInfo *make_outer(void)
{
    ICreateTypeLib2 *ctl;
    ICreateTypeInfo *inner, *outer;
    ITypeInfo *ti_inner, *ti;
    IRecordInfo *ri = NULL;
    HREFTYPE h_inner;
    VARDESC vd, *got;
    TYPEDESC bstr_elem, inner_elem;
    TYPEATTR *ta;
    WCHAR path[MAX_PATH];

    GetTempPathW(MAX_PATH, path);
    lstrcatW(path, L"sg-recfield.tlb");
    if (FAILED(CreateTypeLib2(sizeof(void *) == 8 ? SYS_WIN64 : SYS_WIN32, path, &ctl))) return NULL;
    ICreateTypeLib2_CreateTypeInfo(ctl, (LPOLESTR)L"inner", TKIND_RECORD, &inner);
    memset(&vd, 0, sizeof(vd));
    vd.varkind = VAR_PERINSTANCE;
    vd.memid = 0x40000000; vd.elemdescVar.tdesc.vt = VT_BSTR;
    ICreateTypeInfo_AddVarDesc(inner, 0, &vd); ICreateTypeInfo_SetVarName(inner, 0, (LPOLESTR)L"s");
    vd.memid = 0x40000001; vd.elemdescVar.tdesc.vt = VT_I4;
    ICreateTypeInfo_AddVarDesc(inner, 1, &vd); ICreateTypeInfo_SetVarName(inner, 1, (LPOLESTR)L"n");
    ICreateTypeInfo_LayOut(inner);
    ICreateTypeInfo_QueryInterface(inner, &IID_ITypeInfo, (void **)&ti_inner);

    ICreateTypeLib2_CreateTypeInfo(ctl, (LPOLESTR)L"outer", TKIND_RECORD, &outer);
    ICreateTypeInfo_AddRefTypeInfo(outer, ti_inner, &h_inner);
    memset(&vd, 0, sizeof(vd));
    vd.varkind = VAR_PERINSTANCE;
    vd.memid = 0x40000000; vd.elemdescVar.tdesc.vt = VT_I4;
    ICreateTypeInfo_AddVarDesc(outer, 0, &vd); ICreateTypeInfo_SetVarName(outer, 0, (LPOLESTR)L"a");
    vd.memid = 0x40000001; vd.elemdescVar.tdesc.vt = VT_USERDEFINED; vd.elemdescVar.tdesc.hreftype = h_inner;
    ICreateTypeInfo_AddVarDesc(outer, 1, &vd); ICreateTypeInfo_SetVarName(outer, 1, (LPOLESTR)L"in");
    bstr_elem.vt = VT_BSTR;
    vd.memid = 0x40000002; vd.elemdescVar.tdesc.vt = VT_SAFEARRAY; vd.elemdescVar.tdesc.lptdesc = &bstr_elem;
    ICreateTypeInfo_AddVarDesc(outer, 2, &vd); ICreateTypeInfo_SetVarName(outer, 2, (LPOLESTR)L"names");
    inner_elem.vt = VT_USERDEFINED; inner_elem.hreftype = h_inner;
    vd.memid = 0x40000003; vd.elemdescVar.tdesc.vt = VT_SAFEARRAY; vd.elemdescVar.tdesc.lptdesc = &inner_elem;
    ICreateTypeInfo_AddVarDesc(outer, 3, &vd); ICreateTypeInfo_SetVarName(outer, 3, (LPOLESTR)L"items");
    ICreateTypeInfo_LayOut(outer);
    ICreateTypeInfo_QueryInterface(outer, &IID_ITypeInfo, (void **)&ti);
    if (SUCCEEDED(ITypeInfo_GetVarDesc(ti, 1, &got))) { off_in = got->oInst; ITypeInfo_ReleaseVarDesc(ti, got); }
    if (SUCCEEDED(ITypeInfo_GetVarDesc(ti, 2, &got))) { off_names = got->oInst; ITypeInfo_ReleaseVarDesc(ti, got); }
    if (SUCCEEDED(ITypeInfo_GetVarDesc(ti, 3, &got))) { off_items = got->oInst; ITypeInfo_ReleaseVarDesc(ti, got); }
    if (SUCCEEDED(ITypeInfo_GetTypeAttr(ti_inner, &ta))) { inner_size = ta->cbSizeInstance; ITypeInfo_ReleaseTypeAttr(ti_inner, ta); }
    GetRecordInfoFromTypeInfo(ti, &ri);
    GetRecordInfoFromTypeInfo(ti_inner, &inner_ri);
    ITypeInfo_Release(ti);
    ITypeInfo_Release(ti_inner);
    ICreateTypeInfo_Release(outer);
    ICreateTypeInfo_Release(inner);
    ICreateTypeLib2_Release(ctl);
    return ri;
}

struct inner { BSTR s; LONG n; };

int main(void)
{
    IRecordInfo *ri;
    BYTE *rec, *rec2;
    struct inner *in, *in2;
    VARIANT v, got;
    void *carray = (void *)1;
    SAFEARRAY *sa, **ppsa;
    BSTR b;
    LONG idx;
    HRESULT hr;

    CoInitialize(NULL);
    ri = make_outer();
    check(ri != NULL && inner_ri != NULL, "record infos made");
    if (!ri || !inner_ri) { printf("RESULT: FAIL\n"); return 1; }
    rec = IRecordInfo_RecordCreate(ri);
    in = (struct inner *)(rec + off_in);
    in->s = SysAllocString(L"hello");
    in->n = 42;

    /* GetFieldNoCopy of the record field: the record in place with its record info */
    VariantInit(&got);
    hr = IRecordInfo_GetFieldNoCopy(ri, rec, L"in", &got, &carray);
    check(hr == S_OK, "GetFieldNoCopy(in) hr %#lx", hr);
    check(V_VT(&got) == (VT_BYREF | VT_RECORD), "GetFieldNoCopy(in) is VT_BYREF|VT_RECORD (vt %#x)", V_VT(&got));
    check(V_RECORD(&got) == in, "GetFieldNoCopy(in) points at the field");
    check(V_VT(&got) == (VT_BYREF | VT_RECORD) && V_RECORDINFO(&got) != NULL, "GetFieldNoCopy(in) carries the field's IRecordInfo");
    if (V_VT(&got) == (VT_BYREF | VT_RECORD) && V_RECORDINFO(&got))
    {
        VARIANT n;
        BSTR name = NULL;
        VariantInit(&n);
        hr = IRecordInfo_GetField(V_RECORDINFO(&got), V_RECORD(&got), L"n", &n);
        check(hr == S_OK && V_VT(&n) == VT_I4 && V_I4(&n) == 42, "the nested record reads through its IRecordInfo (hr %#lx)", hr);
        IRecordInfo_GetName(V_RECORDINFO(&got), &name);
        check(name && !lstrcmpW(name, L"inner"), "the nested IRecordInfo is the inner type's");
        SysFreeString(name);
    }

    /* GetField of the record field: a copy */
    VariantInit(&got);
    hr = IRecordInfo_GetField(ri, rec, L"in", &got);
    check(hr == S_OK && V_VT(&got) == VT_RECORD, "GetField(in) is VT_RECORD (hr %#lx vt %#x)", hr, V_VT(&got));
    if (hr == S_OK && V_VT(&got) == VT_RECORD)
    {
        struct inner *c = V_RECORD(&got);
        check(c != in && c->n == 42 && c->s != in->s && c->s && !lstrcmpW(c->s, L"hello"), "GetField(in) copies the record deep");
        VariantClear(&got);
    }

    /* PutField of the record field from a VT_RECORD */
    in2 = IRecordInfo_RecordCreate(inner_ri);
    in2->s = SysAllocString(L"world");
    in2->n = 7;
    V_VT(&v) = VT_RECORD; V_RECORD(&v) = in2; V_RECORDINFO(&v) = inner_ri;
    hr = IRecordInfo_PutField(ri, INVOKE_PROPERTYPUT, rec, L"in", &v);
    check(hr == S_OK, "PutField(in, VT_RECORD) hr %#lx", hr);
    check(in->n == 7 && in->s && in->s != in2->s && !lstrcmpW(in->s, L"world"), "PutField(in) copies the record into the field");
    V_VT(&v) = VT_I4; V_I4(&v) = 3;
    hr = IRecordInfo_PutField(ri, INVOKE_PROPERTYPUT, rec, L"in", &v);
    check(hr == DISP_E_TYPEMISMATCH, "PutField(in, VT_I4) is DISP_E_TYPEMISMATCH (hr %#lx)", hr);

    /* the BSTR array field */
    sa = SafeArrayCreateVector(VT_BSTR, 0, 2);
    idx = 0; b = SysAllocString(L"x"); SafeArrayPutElement(sa, &idx, b); SysFreeString(b);
    idx = 1; b = SysAllocString(L"y"); SafeArrayPutElement(sa, &idx, b); SysFreeString(b);
    V_VT(&v) = VT_ARRAY | VT_BSTR; V_ARRAY(&v) = sa;
    hr = IRecordInfo_PutField(ri, INVOKE_PROPERTYPUT, rec, L"names", &v);
    check(hr == S_OK, "PutField(names, VT_ARRAY|VT_BSTR) hr %#lx", hr);
    ppsa = (SAFEARRAY **)(rec + off_names);
    check(*ppsa && *ppsa != sa, "PutField(names) stores a copy of the array");
    VariantClear(&v);
    VariantInit(&got);
    hr = IRecordInfo_GetFieldNoCopy(ri, rec, L"names", &got, &carray);
    check(hr == S_OK && V_VT(&got) == (VT_BYREF | VT_ARRAY | VT_BSTR), "GetFieldNoCopy(names) is VT_BYREF|VT_ARRAY|VT_BSTR (vt %#x)", V_VT(&got));
    check(V_VT(&got) & VT_BYREF && V_ARRAYREF(&got) == ppsa, "GetFieldNoCopy(names) points at the field");
    VariantInit(&got);
    hr = IRecordInfo_GetField(ri, rec, L"names", &got);
    check(hr == S_OK && V_VT(&got) == (VT_ARRAY | VT_BSTR), "GetField(names) is VT_ARRAY|VT_BSTR (hr %#lx vt %#x)", hr, V_VT(&got));
    if (hr == S_OK && V_VT(&got) == (VT_ARRAY | VT_BSTR))
    {
        idx = 1; b = NULL;
        SafeArrayGetElement(V_ARRAY(&got), &idx, &b);
        check(V_ARRAY(&got) != *ppsa && b && !lstrcmpW(b, L"y"), "GetField(names) copies the array");
        SysFreeString(b);
        VariantClear(&got);
    }
    V_VT(&v) = VT_ARRAY | VT_I4; V_ARRAY(&v) = SafeArrayCreateVector(VT_I4, 0, 1);
    hr = IRecordInfo_PutField(ri, INVOKE_PROPERTYPUT, rec, L"names", &v);
    check(hr == DISP_E_TYPEMISMATCH, "PutField(names, VT_ARRAY|VT_I4) is DISP_E_TYPEMISMATCH (hr %#lx)", hr);
    VariantClear(&v);
    /* PutFieldNoCopy takes the array */
    sa = SafeArrayCreateVector(VT_BSTR, 0, 1);
    V_VT(&v) = VT_ARRAY | VT_BSTR; V_ARRAY(&v) = sa;
    hr = IRecordInfo_PutFieldNoCopy(ri, INVOKE_PROPERTYPUT, rec, L"names", &v);
    check(hr == S_OK && *ppsa == sa, "PutFieldNoCopy(names) takes the array itself (hr %#lx)", hr);

    /* an empty array field of records: the element type comes from the type */
    VariantInit(&got);
    hr = IRecordInfo_GetFieldNoCopy(ri, rec, L"items", &got, &carray);
    check(hr == S_OK && V_VT(&got) == (VT_BYREF | VT_ARRAY | VT_RECORD), "GetFieldNoCopy(items), no array yet, is VT_BYREF|VT_ARRAY|VT_RECORD (vt %#x)", V_VT(&got));

    /* RecordCopy / RecordDestroy still deep (6233/6237) */
    rec2 = IRecordInfo_RecordCreate(ri);
    hr = IRecordInfo_RecordCopy(ri, rec, rec2);
    check(hr == S_OK && *(SAFEARRAY **)(rec2 + off_names) && *(SAFEARRAY **)(rec2 + off_names) != *ppsa, "RecordCopy copies the array field");
    IRecordInfo_RecordDestroy(ri, rec2);
    IRecordInfo_RecordDestroy(inner_ri, in2);
    IRecordInfo_RecordDestroy(ri, rec);
    IRecordInfo_Release(inner_ri);
    IRecordInfo_Release(ri);
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
