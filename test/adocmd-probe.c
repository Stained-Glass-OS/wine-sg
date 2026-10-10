/* msado15 command and parameter batch (patches/sg/2014), run by
 * test/adocmd-gate.sh. Families: Command properties (CommandTimeout, Prepared,
 * Name, State, Dialect, NamedParameters, ActiveConnection), the Parameters
 * collection (Item by position and name, Delete, _NewEnum, Refresh) and
 * Parameter properties (Precision, NumericScale, Attributes, AppendChunk),
 * all through IDispatch.
 *
 *   adocmd-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <oleauto.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
static void checkhr(HRESULT hr, HRESULT want, const char *what)
{
    char buf[200];
    snprintf(buf, sizeof(buf), "%s (hr %08lx, want %08lx)", what, (unsigned long)hr, (unsigned long)want);
    check(hr == want, buf);
}

#define ADO_HR(n) ((HRESULT)(0x800a0000 | (n)))
static const GUID SG_CLSID_Command = { 0x00000507, 0, 0x0010, { 0x80, 0, 0, 0xaa, 0, 0x6d, 0x2e, 0xa4 } };
static const GUID SG_CLSID_Connection = { 0x00000514, 0, 0x0010, { 0x80, 0, 0, 0xaa, 0, 0x6d, 0x2e, 0xa4 } };

/* a call by name or id; the HRESULT of a failing method comes back as the exception's scode */
static HRESULT invoke(IDispatch *d, DISPID id, const WCHAR *name, WORD flags, VARIANT *res, int n, VARIANT *args)
{
    DISPPARAMS dp;
    DISPID named = DISPID_PROPERTYPUT;
    EXCEPINFO ei;
    VARIANT rev[8];
    HRESULT hr;
    int i;

    if (name)
    {
        OLECHAR *nm = (OLECHAR *)name;
        if (FAILED(hr = IDispatch_GetIDsOfNames(d, &IID_NULL, &nm, 1, 0, &id))) return hr;
    }
    for (i = 0; i < n; i++) rev[i] = args[n - 1 - i];
    dp.rgvarg = n ? rev : NULL;
    dp.cArgs = n;
    dp.rgdispidNamedArgs = (flags & (DISPATCH_PROPERTYPUT | DISPATCH_PROPERTYPUTREF)) ? &named : NULL;
    dp.cNamedArgs = dp.rgdispidNamedArgs ? 1 : 0;
    memset(&ei, 0, sizeof(ei));
    if (res) VariantInit(res);
    hr = IDispatch_Invoke(d, id, &IID_NULL, 0, flags, &dp, res, &ei, NULL);
    if (hr == DISP_E_EXCEPTION)
    {
        hr = ei.scode ? ei.scode : E_FAIL;
        SysFreeString(ei.bstrSource); SysFreeString(ei.bstrDescription); SysFreeString(ei.bstrHelpFile);
    }
    return hr;
}

static HRESULT get(IDispatch *d, const WCHAR *name, VARIANT *res)
{
    return invoke(d, 0, name, DISPATCH_PROPERTYGET, res, 0, NULL);
}
static HRESULT put(IDispatch *d, const WCHAR *name, VARIANT *v)
{
    return invoke(d, 0, name, DISPATCH_PROPERTYPUT, NULL, 1, v);
}
static HRESULT call(IDispatch *d, const WCHAR *name, VARIANT *res, int n, VARIANT *args)
{
    return invoke(d, 0, name, DISPATCH_METHOD, res, n, args);
}

static VARIANT vi4(LONG l) { VARIANT v; VariantInit(&v); V_VT(&v) = VT_I4; V_I4(&v) = l; return v; }
static VARIANT vi2(short l) { VARIANT v; VariantInit(&v); V_VT(&v) = VT_I2; V_I2(&v) = l; return v; }
static VARIANT vbool(VARIANT_BOOL b) { VARIANT v; VariantInit(&v); V_VT(&v) = VT_BOOL; V_BOOL(&v) = b; return v; }
static VARIANT vstr(const WCHAR *s) { VARIANT v; VariantInit(&v); V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(s); return v; }
static VARIANT vdisp(IDispatch *d) { VARIANT v; VariantInit(&v); V_VT(&v) = VT_DISPATCH; V_DISPATCH(&v) = d; return v; }

static int get_i4_is(IDispatch *d, const WCHAR *name, LONG want)
{
    VARIANT v;
    int ok = get(d, name, &v) == S_OK && V_VT(&v) == VT_I4 && V_I4(&v) == want;
    if (!ok && V_VT(&v) != VT_EMPTY) printf("   %ls: vt %d value %ld\n", name, V_VT(&v), (long)V_I4(&v));
    VariantClear(&v);
    return ok;
}
static int get_str_is(IDispatch *d, const WCHAR *name, const WCHAR *want)
{
    VARIANT v;
    int ok = get(d, name, &v) == S_OK && V_VT(&v) == VT_BSTR && V_BSTR(&v) && !wcscmp(V_BSTR(&v), want);
    VariantClear(&v);
    return ok;
}
static int get_bool_is(IDispatch *d, const WCHAR *name, VARIANT_BOOL want)
{
    VARIANT v;
    int ok = get(d, name, &v) == S_OK && V_VT(&v) == VT_BOOL && V_BOOL(&v) == want;
    VariantClear(&v);
    return ok;
}

static IDispatch *create(const GUID *clsid)
{
    IDispatch *d = NULL;
    CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IDispatch, (void **)&d);
    return d;
}

static IDispatch *newparam(IDispatch *cmd, const WCHAR *name, int type, int dir, int size)
{
    VARIANT args[5], res;
    IDispatch *p = NULL;
    args[0] = vstr(name); args[1] = vi4(type); args[2] = vi4(dir); args[3] = vi4(size);
    VariantInit(&args[4]);
    if (call(cmd, L"CreateParameter", &res, 5, args) == S_OK && V_VT(&res) == VT_DISPATCH)
        p = V_DISPATCH(&res);
    VariantClear(&args[0]);
    return p;
}

static void test_command_props(void)
{
    IDispatch *cmd = create(&SG_CLSID_Command), *conn = create(&SG_CLSID_Connection);
    VARIANT v, r;
    HRESULT hr;

    check(cmd != NULL, "create a Command");
    if (!cmd) return;

    check(get_i4_is(cmd, L"CommandTimeout", 30), "default CommandTimeout is 30");
    v = vi4(90);
    checkhr(put(cmd, L"CommandTimeout", &v), S_OK, "put CommandTimeout");
    check(get_i4_is(cmd, L"CommandTimeout", 90), "CommandTimeout reads back");
    v = vi4(0);
    put(cmd, L"CommandTimeout", &v);
    check(get_i4_is(cmd, L"CommandTimeout", 0), "CommandTimeout of 0");

    check(get_bool_is(cmd, L"Prepared", VARIANT_FALSE), "Prepared is false at first");
    v = vbool(VARIANT_TRUE);
    checkhr(put(cmd, L"Prepared", &v), S_OK, "put Prepared");
    check(get_bool_is(cmd, L"Prepared", VARIANT_TRUE), "Prepared reads back");

    hr = get(cmd, L"Name", &r);
    check(hr == S_OK && (V_VT(&r) == VT_EMPTY || V_VT(&r) == VT_NULL || (V_VT(&r) == VT_BSTR && (!V_BSTR(&r) || !*V_BSTR(&r)))), "Name is empty at first");
    VariantClear(&r);
    v = vstr(L"MyCommand");
    checkhr(put(cmd, L"Name", &v), S_OK, "put Name");
    VariantClear(&v);
    check(get_str_is(cmd, L"Name", L"MyCommand"), "Name reads back");

    check(get_i4_is(cmd, L"State", 0), "State is closed");

    check(get_str_is(cmd, L"Dialect", L"{C8B521FB-5CF3-11CE-ADE5-00AA0044773D}"), "default Dialect");
    v = vstr(L"{AA}");
    checkhr(put(cmd, L"Dialect", &v), S_OK, "put Dialect");
    VariantClear(&v);
    check(get_str_is(cmd, L"Dialect", L"{AA}"), "Dialect reads back");

    check(get_bool_is(cmd, L"NamedParameters", VARIANT_FALSE), "NamedParameters is false at first");
    v = vbool(VARIANT_TRUE);
    checkhr(put(cmd, L"NamedParameters", &v), S_OK, "put NamedParameters");
    check(get_bool_is(cmd, L"NamedParameters", VARIANT_TRUE), "NamedParameters reads back");

    /* the active connection */
    hr = get(cmd, L"ActiveConnection", &r);
    check(hr == S_OK && (V_VT(&r) == VT_EMPTY || V_VT(&r) == VT_NULL || (V_VT(&r) == VT_DISPATCH && !V_DISPATCH(&r))), "no active connection at first");
    VariantClear(&r);
    v = vdisp(conn);
    checkhr(put(cmd, L"ActiveConnection", &v), S_OK, "put ActiveConnection with a Connection");
    hr = get(cmd, L"ActiveConnection", &r);
    {
        IUnknown *u1 = NULL, *u2 = NULL;
        if (hr == S_OK && V_VT(&r) == VT_DISPATCH && V_DISPATCH(&r))
        {
            IDispatch_QueryInterface(V_DISPATCH(&r), &IID_IUnknown, (void **)&u1);
            IDispatch_QueryInterface(conn, &IID_IUnknown, (void **)&u2);
        }
        check(u1 && u1 == u2, "the Connection reads back");
        if (u1) IUnknown_Release(u1);
        if (u2) IUnknown_Release(u2);
    }
    VariantClear(&r);
    v = vi4(4);
    checkhr(put(cmd, L"ActiveConnection", &v), ADO_HR(3001), "put ActiveConnection with a number");
    v = vstr(L"Provider=NoSuchProvider.1");
    hr = put(cmd, L"ActiveConnection", &v);
    check(FAILED(hr), "put ActiveConnection with a bad connection string fails");
    VariantClear(&v);
    hr = get(cmd, L"ActiveConnection", &r);
    check(hr == S_OK && V_VT(&r) == VT_DISPATCH && V_DISPATCH(&r), "a failed put keeps the connection");
    VariantClear(&r);
    VariantInit(&v);
    checkhr(put(cmd, L"ActiveConnection", &v), S_OK, "put ActiveConnection with nothing");
    hr = get(cmd, L"ActiveConnection", &r);
    check(hr == S_OK && (V_VT(&r) == VT_EMPTY || V_VT(&r) == VT_NULL || (V_VT(&r) == VT_DISPATCH && !V_DISPATCH(&r))), "the connection is released");
    VariantClear(&r);
    IDispatch_Release(conn);
    IDispatch_Release(cmd);
}

static IDispatch *item(IDispatch *params, VARIANT idx, HRESULT *hr)
{
    VARIANT res;
    IDispatch *d = NULL;
    *hr = invoke(params, 0, L"Item", DISPATCH_METHOD | DISPATCH_PROPERTYGET, &res, 1, &idx);
    if (*hr == S_OK && V_VT(&res) == VT_DISPATCH) d = V_DISPATCH(&res);
    return d;
}

static int same(IDispatch *a, IDispatch *b)
{
    IUnknown *u1 = NULL, *u2 = NULL;
    int ok;
    if (!a || !b) return 0;
    IDispatch_QueryInterface(a, &IID_IUnknown, (void **)&u1);
    IDispatch_QueryInterface(b, &IID_IUnknown, (void **)&u2);
    ok = u1 == u2;
    IUnknown_Release(u1); IUnknown_Release(u2);
    return ok;
}

static void test_parameters(void)
{
    IDispatch *cmd = create(&SG_CLSID_Command), *params, *p1, *p2, *p3, *it;
    VARIANT res, arg;
    HRESULT hr;

    check(cmd != NULL, "create a Command");
    if (!cmd) return;
    get(cmd, L"Parameters", &res);
    params = V_DISPATCH(&res);
    p1 = newparam(cmd, L"alpha", 3, 1, 4);
    p2 = newparam(cmd, L"Beta", 202, 1, 20);
    p3 = newparam(cmd, L"gamma", 3, 2, 4);
    check(p1 && p2 && p3, "three parameters created");
    arg = vdisp(p1); call(params, L"Append", NULL, 1, &arg);
    arg = vdisp(p2); call(params, L"Append", NULL, 1, &arg);
    arg = vdisp(p3); call(params, L"Append", NULL, 1, &arg);
    check(get_i4_is(params, L"Count", 3), "three parameters in the collection");

    it = item(params, vi4(1), &hr);
    check(hr == S_OK && same(it, p2), "Item by position is the same object");
    if (it) IDispatch_Release(it);
    it = item(params, vi4(3), &hr);
    checkhr(hr, ADO_HR(3265), "Item past the end");
    check(it == NULL, "no object for a missing item");
    it = item(params, vi4(-1), &hr);
    checkhr(hr, ADO_HR(3265), "Item at -1");
    it = item(params, vi2(2), &hr);
    check(hr == S_OK && same(it, p3), "Item by a short position");
    if (it) IDispatch_Release(it);
    arg = vstr(L"BETA");
    it = item(params, arg, &hr);
    check(hr == S_OK && same(it, p2), "Item by name, any case");
    if (it) IDispatch_Release(it);
    VariantClear(&arg);
    arg = vstr(L"nope");
    it = item(params, arg, &hr);
    checkhr(hr, ADO_HR(3265), "Item by an unknown name");
    VariantClear(&arg);

    /* enumeration */
    {
        IUnknown *unk = NULL;
        IEnumVARIANT *en = NULL;
        VARIANT vars[4];
        ULONG got = 0;
        hr = invoke(params, 0, L"_NewEnum", DISPATCH_PROPERTYGET | DISPATCH_METHOD, &res, 0, NULL);
        checkhr(hr, S_OK, "_NewEnum");
        if (hr == S_OK) { unk = V_UNKNOWN(&res); IUnknown_QueryInterface(unk, &IID_IEnumVARIANT, (void **)&en); }
        check(en != NULL, "the enumerator is an IEnumVARIANT");
        if (en)
        {
            memset(vars, 0, sizeof(vars));
            hr = IEnumVARIANT_Next(en, 2, vars, &got);
            check(hr == S_OK && got == 2 && V_VT(&vars[0]) == VT_DISPATCH && same(V_DISPATCH(&vars[0]), p1) &&
                  same(V_DISPATCH(&vars[1]), p2), "Next gives the parameters in order");
            VariantClear(&vars[0]); VariantClear(&vars[1]);
            hr = IEnumVARIANT_Next(en, 4, vars, &got);
            check(hr == S_FALSE && got == 1 && same(V_DISPATCH(&vars[0]), p3), "Next at the end gives the rest");
            VariantClear(&vars[0]);
            IEnumVARIANT_Reset(en);
            check(IEnumVARIANT_Skip(en, 2) == S_OK, "Skip");
            { IEnumVARIANT *copy = NULL;
              check(IEnumVARIANT_Clone(en, &copy) == S_OK && copy, "Clone");
              if (copy)
              {
                  hr = IEnumVARIANT_Next(copy, 1, vars, &got);
                  check(hr == S_OK && same(V_DISPATCH(&vars[0]), p3), "the clone continues where it was");
                  VariantClear(&vars[0]);
                  IEnumVARIANT_Release(copy);
              } }
            check(IEnumVARIANT_Skip(en, 5) == S_FALSE, "Skip past the end");
            IEnumVARIANT_Release(en);
        }
        VariantClear(&res);
    }

    hr = call(params, L"Refresh", NULL, 0, NULL);
    checkhr(hr, ADO_HR(3709), "Refresh without a connection");

    /* delete */
    arg = vi4(1);
    checkhr(call(params, L"Delete", NULL, 1, &arg), S_OK, "Delete by position");
    check(get_i4_is(params, L"Count", 2), "two parameters left");
    it = item(params, vi4(1), &hr);
    check(hr == S_OK && same(it, p3), "the later parameter moved up");
    if (it) IDispatch_Release(it);
    arg = vstr(L"alpha");
    checkhr(call(params, L"Delete", NULL, 1, &arg), S_OK, "Delete by name");
    VariantClear(&arg);
    check(get_i4_is(params, L"Count", 1), "one parameter left");
    arg = vi4(5);
    checkhr(call(params, L"Delete", NULL, 1, &arg), ADO_HR(3265), "Delete past the end");
    arg = vi4(0);
    checkhr(call(params, L"Delete", NULL, 1, &arg), S_OK, "Delete the last one");
    check(get_i4_is(params, L"Count", 0), "no parameters left");

    IDispatch_Release(p1); IDispatch_Release(p2); IDispatch_Release(p3);
    IDispatch_Release(params);
    IDispatch_Release(cmd);
}

static int get_byte_is(IDispatch *d, const WCHAR *name, BYTE want)
{
    VARIANT v;
    int ok = get(d, name, &v) == S_OK && V_VT(&v) == VT_UI1 && V_UI1(&v) == want;
    VariantClear(&v);
    return ok;
}

static void test_parameter_props(void)
{
    IDispatch *cmd = create(&SG_CLSID_Command), *p;
    VARIANT v, got;
    HRESULT hr;

    check(cmd != NULL, "create a Command");
    if (!cmd) return;
    p = newparam(cmd, L"big", 203, 1, 100);

    check(get_byte_is(p, L"Precision", 0), "Precision is 0 at first");
    VariantInit(&v); V_VT(&v) = VT_UI1; V_UI1(&v) = 18;
    checkhr(put(p, L"Precision", &v), S_OK, "put Precision");
    check(get_byte_is(p, L"Precision", 18), "Precision reads back");
    check(get_byte_is(p, L"NumericScale", 0), "NumericScale is 0 at first");
    V_UI1(&v) = 4;
    checkhr(put(p, L"NumericScale", &v), S_OK, "put NumericScale");
    check(get_byte_is(p, L"NumericScale", 4), "NumericScale reads back");
    check(get_i4_is(p, L"Attributes", 0x10), "default Attributes is adParamSigned");
    v = vi4(0x90);
    put(p, L"Attributes", &v);
    check(get_i4_is(p, L"Attributes", 0x90), "Attributes reads back");

    /* text chunks */
    v = vstr(L"Hello, ");
    checkhr(call(p, L"AppendChunk", NULL, 1, &v), S_OK, "AppendChunk first text");
    VariantClear(&v);
    v = vstr(L"world");
    checkhr(call(p, L"AppendChunk", NULL, 1, &v), S_OK, "AppendChunk second text");
    VariantClear(&v);
    check(get_str_is(p, L"Value", L"Hello, world"), "the chunks are joined");
    v = vi4(1);
    checkhr(call(p, L"AppendChunk", NULL, 1, &v), ADO_HR(3001), "AppendChunk of a number");
    IDispatch_Release(p);

    /* binary chunks */
    p = newparam(cmd, L"blob", 205, 1, 100);
    {
        SAFEARRAY *a1 = SafeArrayCreateVector(VT_UI1, 0, 3), *a2 = SafeArrayCreateVector(VT_UI1, 0, 2);
        BYTE *d;
        SafeArrayAccessData(a1, (void **)&d); d[0] = 1; d[1] = 2; d[2] = 3; SafeArrayUnaccessData(a1);
        SafeArrayAccessData(a2, (void **)&d); d[0] = 4; d[1] = 5; SafeArrayUnaccessData(a2);
        VariantInit(&v);
        V_VT(&v) = VT_ARRAY | VT_UI1; V_ARRAY(&v) = a1;
        checkhr(call(p, L"AppendChunk", NULL, 1, &v), S_OK, "AppendChunk first bytes");
        V_ARRAY(&v) = a2;
        checkhr(call(p, L"AppendChunk", NULL, 1, &v), S_OK, "AppendChunk second bytes");
        SafeArrayDestroy(a1); SafeArrayDestroy(a2);
        hr = get(p, L"Value", &got);
        check(hr == S_OK && V_VT(&got) == (VT_ARRAY | VT_UI1), "the value is a byte array");
        if (hr == S_OK && V_VT(&got) == (VT_ARRAY | VT_UI1))
        {
            LONG ub = -1;
            SafeArrayGetUBound(V_ARRAY(&got), 1, &ub);
            SafeArrayAccessData(V_ARRAY(&got), (void **)&d);
            check(ub == 4 && d[0] == 1 && d[2] == 3 && d[3] == 4 && d[4] == 5, "the byte chunks are joined");
            SafeArrayUnaccessData(V_ARRAY(&got));
        }
        VariantClear(&got);
    }
    IDispatch_Release(p);
    IDispatch_Release(cmd);
}

int main(void)
{
    CoInitialize(NULL);
    test_command_props();
    test_parameters();
    test_parameter_props();
    printf("%d checks failed\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    CoUninitialize();
    return failures != 0;
}
