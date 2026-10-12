/* Probe for patches 2931-2935: IMediaControl::AddSourceFilter, FilterCollection,
 * RegFilterCollection and the IAMCollection / IFilterInfo / IRegFilterInfo /
 * IPinInfo / IMediaTypeInfo automation objects. Values are compared against the
 * same information read through the plain IBaseFilter/IPin/IFilterMapper interfaces. */
#define COBJMACROS
#include <windows.h>
#include <dshow.h>
#include <stdio.h>
#include <wchar.h>

#define ARRAY_SIZE_(x) (sizeof(x)/sizeof((x)[0]))
static int failures, checks;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; printf("FAIL  line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define CHECK_HR(hr, exp, what) CHECK((hr) == (exp), "%s: got %#lx, expected %#lx", what, (long)(hr), (long)(exp))

static IGraphBuilder *new_graph(void)
{
    IGraphBuilder *g = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, &IID_IGraphBuilder, (void **)&g);
    CHECK_HR(hr, S_OK, "create graph");
    return g;
}

static IAMCollection *get_collection(IDispatch *d)
{
    IAMCollection *c = NULL;
    HRESULT hr;
    CHECK(d != NULL, "collection NULL");
    if (!d) return NULL;
    hr = IDispatch_QueryInterface(d, &IID_IAMCollection, (void **)&c);
    CHECK_HR(hr, S_OK, "QI IAMCollection");
    IDispatch_Release(d);
    return c;
}

/* Table-driven checks common to all collections: argument handling, bounds, enumeration, IDispatch. */
static void check_collection_generic(IAMCollection *c, LONG expect_count, const char *what)
{
    static const struct { LONG off; HRESULT hr; } bounds[] = {
        { -1, E_INVALIDARG }, { 0, S_OK }, { -1000, E_INVALIDARG }, { 100000, E_INVALIDARG },
    };
    IUnknown *unk;
    IEnumVARIANT *ev, *clone;
    VARIANT v[2];
    ULONG got;
    LONG count = -77, i;
    HRESULT hr;
    DISPID id;
    DISPPARAMS dp = {0};
    VARIANT res;
    OLECHAR *name;
    UINT n;

    hr = IAMCollection_get_Count(c, NULL);
    CHECK_HR(hr, E_POINTER, "get_Count(NULL)");
    hr = IAMCollection_get_Count(c, &count);
    CHECK_HR(hr, S_OK, "get_Count");
    CHECK(count == expect_count, "%s: Count %ld, expected %ld", what, count, expect_count);

    hr = IAMCollection_Item(c, 0, NULL);
    CHECK_HR(hr, E_POINTER, "Item(NULL)");
    unk = (IUnknown *)0x1234;
    hr = IAMCollection_Item(c, count, &unk);
    CHECK_HR(hr, E_INVALIDARG, "Item(count)");
    CHECK(unk == NULL, "Item(count) left %p", unk);
    for (i = 0; i < (LONG)ARRAY_SIZE_(bounds); i++)
    {
        HRESULT exp = bounds[i].hr;
        if (bounds[i].off == 0 && !count) exp = E_INVALIDARG;
        hr = IAMCollection_Item(c, bounds[i].off, &unk);
        CHECK(hr == exp, "Item(%ld) hr %#lx expected %#lx", bounds[i].off, (long)hr, (long)exp);
        if (hr == S_OK) IUnknown_Release(unk);
    }

    /* IDispatch through the type library: Count. */
    name = (OLECHAR *)L"Count";
    hr = IAMCollection_GetIDsOfNames(c, &IID_NULL, &name, 1, LOCALE_USER_DEFAULT, &id);
    CHECK_HR(hr, S_OK, "GetIDsOfNames(Count)");
    if (hr == S_OK)
    {
        VariantInit(&res);
        hr = IAMCollection_Invoke(c, id, &IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET, &dp, &res, NULL, NULL);
        CHECK_HR(hr, S_OK, "Invoke(Count)");
        CHECK(V_VT(&res) == VT_I4 && V_I4(&res) == expect_count, "Invoke Count vt %d value %ld", V_VT(&res), V_I4(&res));
    }
    hr = IAMCollection_GetTypeInfoCount(c, &n);
    CHECK(hr == S_OK && n == 1, "GetTypeInfoCount hr %#lx n %u", (long)hr, n);

    /* _NewEnum */
    hr = IAMCollection_get__NewEnum(c, NULL);
    CHECK_HR(hr, E_POINTER, "_NewEnum(NULL)");
    hr = IAMCollection_get__NewEnum(c, &unk);
    CHECK_HR(hr, S_OK, "_NewEnum");
    if (hr != S_OK) return;
    hr = IUnknown_QueryInterface(unk, &IID_IEnumVARIANT, (void **)&ev);
    CHECK_HR(hr, S_OK, "QI IEnumVARIANT");
    IUnknown_Release(unk);
    if (hr != S_OK) return;
    for (i = 0; i < count; i++)
    {
        got = 99;
        hr = IEnumVARIANT_Next(ev, 1, v, &got);
        CHECK(hr == S_OK && got == 1 && V_VT(&v[0]) == VT_DISPATCH && V_DISPATCH(&v[0]), "enum Next %ld hr %#lx got %lu", i, (long)hr, got);
        if (hr == S_OK) VariantClear(&v[0]);
    }
    got = 99;
    hr = IEnumVARIANT_Next(ev, 1, v, &got);
    CHECK(hr == S_FALSE && got == 0, "enum Next at end hr %#lx got %lu", (long)hr, got);
    hr = IEnumVARIANT_Reset(ev);
    CHECK_HR(hr, S_OK, "enum Reset");
    if (count >= 1)
    {
        hr = IEnumVARIANT_Next(ev, 2, v, &got);
        CHECK(got == (ULONG)(count >= 2 ? 2 : 1), "enum Next(2) got %lu", got);
        CHECK(hr == (count >= 2 ? S_OK : S_FALSE), "enum Next(2) hr %#lx", (long)hr);
        for (i = 0; i < (LONG)got; i++) VariantClear(&v[i]);
        hr = IEnumVARIANT_Reset(ev);
        hr = IEnumVARIANT_Skip(ev, 1);
        CHECK_HR(hr, S_OK, "enum Skip(1)");
        hr = IEnumVARIANT_Clone(ev, &clone);
        CHECK_HR(hr, S_OK, "enum Clone");
        if (hr == S_OK)
        {
            hr = IEnumVARIANT_Next(clone, 1, v, &got);
            CHECK(got == (ULONG)(count >= 2 ? 1 : 0), "clone keeps position: got %lu", got);
            if (hr == S_OK) VariantClear(&v[0]);
            IEnumVARIANT_Release(clone);
        }
    }
    IEnumVARIANT_Release(ev);
}


static void check_bstr(BSTR b, const WCHAR *expect, const char *what)
{
    CHECK(b && !wcscmp(b, expect), "%s: got %ls, expected %ls", what, b ? b : L"(null)", expect);
    SysFreeString(b);
}

static void guid_str(const GUID *g, WCHAR *buf) { StringFromGUID2(g, buf, 40); }

static void check_pin_info(IPinInfo *pi, IPin *pin, IFilterInfo *owner)
{
    PIN_INFO info;
    PIN_DIRECTION dir;
    WCHAR *id;
    BSTR b;
    LONG l;
    IUnknown *unk;
    IDispatch *d;
    IPin *peer;
    HRESULT hr, exp;

    IPin_QueryPinInfo(pin, &info);
    IBaseFilter_Release(info.pFilter);
    IPin_QueryDirection(pin, &dir);
    IPin_QueryId(pin, &id);

    hr = IPinInfo_get_Name(pi, &b);
    CHECK_HR(hr, S_OK, "pin Name");
    check_bstr(b, info.achName, "pin Name");
    hr = IPinInfo_get_PinID(pi, &b);
    CHECK_HR(hr, S_OK, "pin PinID");
    check_bstr(b, id, "pin PinID");
    l = -5;
    hr = IPinInfo_get_Direction(pi, &l);
    CHECK(hr == S_OK && l == (LONG)dir, "pin Direction %ld expected %d", l, dir);
    hr = IPinInfo_get_Direction(pi, NULL);
    CHECK_HR(hr, E_POINTER, "pin Direction NULL");
    hr = IPinInfo_get_Pin(pi, &unk);
    CHECK_HR(hr, S_OK, "pin Pin");
    CHECK(unk == (IUnknown *)pin, "pin Pin %p expected %p", unk, pin);
    if (hr == S_OK) IUnknown_Release(unk);

    /* connection state */
    d = (IDispatch *)0x1;
    exp = IPin_ConnectedTo(pin, &peer);
    if (SUCCEEDED(exp)) IPin_Release(peer);
    hr = IPinInfo_get_ConnectedTo(pi, &d);
    CHECK_HR(hr, exp, "pin ConnectedTo");
    if (FAILED(exp)) CHECK(d == NULL, "ConnectedTo out %p", d);
    else if (d) IDispatch_Release(d);
    d = (IDispatch *)0x1;
    hr = IPinInfo_get_ConnectionMediaType(pi, &d);
    CHECK_HR(hr, exp, "pin ConnectionMediaType");
    if (SUCCEEDED(hr) && d) IDispatch_Release(d);

    /* owner filter */
    hr = IPinInfo_get_FilterInfo(pi, &d);
    CHECK_HR(hr, S_OK, "pin FilterInfo");
    if (hr == S_OK)
    {
        IFilterInfo *fi;
        BSTR n1, n2;
        IDispatch_QueryInterface(d, &IID_IFilterInfo, (void **)&fi);
        IFilterInfo_get_Name(fi, &n1);
        IFilterInfo_get_Name(owner, &n2);
        CHECK(n1 && n2 && !wcscmp(n1, n2), "pin FilterInfo name %ls vs %ls", n1, n2);
        SysFreeString(n1); SysFreeString(n2);
        IFilterInfo_Release(fi);
        IDispatch_Release(d);
    }

    /* media types vs IEnumMediaTypes */
    {
        IEnumMediaTypes *em;
        AM_MEDIA_TYPE *mt;
        AM_MEDIA_TYPE *all[16];
        int n = 0, i;
        IAMCollection *c;
        IDispatch *cd;

        hr = IPin_EnumMediaTypes(pin, &em);
        if (hr == S_OK)
        {
            while (n < 16 && IEnumMediaTypes_Next(em, 1, &mt, NULL) == S_OK) all[n++] = mt;
            IEnumMediaTypes_Release(em);
        }
        hr = IPinInfo_get_MediaTypes(pi, &cd);
        CHECK_HR(hr, S_OK, "pin MediaTypes");
        c = get_collection(cd);
        if (c)
        {
            check_collection_generic(c, n, "MediaTypes");
            for (i = 0; i < n; i++)
            {
                IUnknown *u;
                IMediaTypeInfo *mi;
                WCHAR g1[40], g2[40];
                BSTR t, s;

                hr = IAMCollection_Item(c, i, &u);
                CHECK_HR(hr, S_OK, "MediaTypes Item");
                if (hr != S_OK) continue;
                hr = IUnknown_QueryInterface(u, &IID_IMediaTypeInfo, (void **)&mi);
                CHECK_HR(hr, S_OK, "QI IMediaTypeInfo");
                if (hr == S_OK)
                {
                    guid_str(&all[i]->majortype, g1);
                    guid_str(&all[i]->subtype, g2);
                    IMediaTypeInfo_get_Type(mi, &t);
                    IMediaTypeInfo_get_Subtype(mi, &s);
                    check_bstr(t, g1, "MediaType Type");
                    check_bstr(s, g2, "MediaType Subtype");
                    hr = IMediaTypeInfo_get_Type(mi, NULL);
                    CHECK_HR(hr, E_POINTER, "MediaType Type NULL");
                    IMediaTypeInfo_Release(mi);
                }
                IUnknown_Release(u);
            }
            IAMCollection_Release(c);
        }
        for (i = 0; i < n; i++) { CoTaskMemFree(all[i]->pbFormat); CoTaskMemFree(all[i]); }
    }
    CoTaskMemFree(id);
}

static void check_filter_info(IFilterInfo *fi, IBaseFilter *filter, int expect_source, const WCHAR *expect_file)
{
    FILTER_INFO info;
    WCHAR *vendor = NULL;
    BSTR b;
    LONG l;
    IUnknown *unk;
    IDispatch *d;
    HRESULT hr, exp;
    IEnumPins *ep;
    IPin *pin;
    IAMCollection *c;
    int npins = 0;
    IPin *pins[8];
    int i;

    IBaseFilter_QueryFilterInfo(filter, &info);
    if (info.pGraph) IFilterGraph_Release(info.pGraph);

    hr = IFilterInfo_get_Name(fi, &b);
    CHECK_HR(hr, S_OK, "filter Name");
    check_bstr(b, info.achName, "filter Name");
    hr = IFilterInfo_get_Name(fi, NULL);
    CHECK_HR(hr, E_POINTER, "filter Name NULL");

    exp = IBaseFilter_QueryVendorInfo(filter, &vendor);
    b = (BSTR)0x1;
    hr = IFilterInfo_get_VendorInfo(fi, &b);
    CHECK_HR(hr, exp, "filter VendorInfo");
    if (hr == S_OK && vendor) check_bstr(b, vendor, "filter VendorInfo");
    else if (hr == S_OK) { CHECK(b == NULL, "VendorInfo expected NULL"); }
    else CHECK(b == NULL, "VendorInfo out %p", b);
    CoTaskMemFree(vendor);

    hr = IFilterInfo_get_Filter(fi, &unk);
    CHECK_HR(hr, S_OK, "filter Filter");
    CHECK(unk == (IUnknown *)filter, "Filter %p expected %p", unk, filter);
    if (hr == S_OK) IUnknown_Release(unk);

    l = 77;
    hr = IFilterInfo_get_IsFileSource(fi, &l);
    CHECK(hr == S_OK && l == (expect_source ? -1 : 0), "IsFileSource hr %#lx value %ld expected %d", (long)hr, l, expect_source ? -1 : 0);
    b = NULL;
    hr = IFilterInfo_get_Filename(fi, &b);
    if (expect_source)
    {
        CHECK_HR(hr, S_OK, "filter Filename");
        check_bstr(b, expect_file, "filter Filename");
    }
    else
        CHECK_HR(hr, E_NOINTERFACE, "non-source Filename");

    /* pins */
    IBaseFilter_EnumPins(filter, &ep);
    while (npins < 8 && IEnumPins_Next(ep, 1, &pin, NULL) == S_OK) pins[npins++] = pin;
    IEnumPins_Release(ep);
    hr = IFilterInfo_get_Pins(fi, &d);
    CHECK_HR(hr, S_OK, "filter Pins");
    c = get_collection(d);
    if (c)
    {
        check_collection_generic(c, npins, "Pins");
        for (i = 0; i < npins; i++)
        {
            IUnknown *u;
            IPinInfo *pi;
            IDispatch *fd;
            WCHAR *id;

            hr = IAMCollection_Item(c, i, &u);
            CHECK_HR(hr, S_OK, "Pins Item");
            if (hr != S_OK) continue;
            IUnknown_QueryInterface(u, &IID_IPinInfo, (void **)&pi);
            check_pin_info(pi, pins[i], fi);
            /* FindPin by id */
            IPin_QueryId(pins[i], &id);
            hr = IFilterInfo_FindPin(fi, id, &fd);
            CHECK_HR(hr, S_OK, "FindPin");
            if (hr == S_OK)
            {
                IPinInfo *p2;
                IUnknown *pu;
                IDispatch_QueryInterface(fd, &IID_IPinInfo, (void **)&p2);
                IPinInfo_get_Pin(p2, &pu);
                CHECK(pu == (IUnknown *)pins[i], "FindPin pin %p expected %p", pu, pins[i]);
                IUnknown_Release(pu);
                IPinInfo_Release(p2);
                IDispatch_Release(fd);
            }
            CoTaskMemFree(id);
            IPinInfo_Release(pi);
            IUnknown_Release(u);
        }
        IAMCollection_Release(c);
    }
    hr = IFilterInfo_FindPin(fi, (BSTR)L"no-such-pin", &d);
    {
        IPin *none = NULL;
        HRESULT exp2 = IBaseFilter_FindPin(filter, L"no-such-pin", &none);
        CHECK_HR(hr, exp2, "FindPin(unknown)");
    }
    for (i = 0; i < npins; i++) IPin_Release(pins[i]);
}

/* IFilterInfo of a filter in the graph, found through FilterCollection. */
static IFilterInfo *info_for(IMediaControl *mc, IBaseFilter *filter)
{
    IDispatch *cd;
    IAMCollection *fc;
    IFilterInfo *found = NULL;
    LONG count = 0, i;

    IMediaControl_get_FilterCollection(mc, &cd);
    fc = get_collection(cd);
    if (!fc) return NULL;
    IAMCollection_get_Count(fc, &count);
    for (i = 0; i < count && !found; i++)
    {
        IUnknown *u, *fu;
        IFilterInfo *fi;

        IAMCollection_Item(fc, i, &u);
        IUnknown_QueryInterface(u, &IID_IFilterInfo, (void **)&fi);
        IFilterInfo_get_Filter(fi, &fu);
        if (fu == (IUnknown *)filter) found = fi; else IFilterInfo_Release(fi);
        IUnknown_Release(fu);
        IUnknown_Release(u);
    }
    IAMCollection_Release(fc);
    CHECK(found != NULL, "filter %p not found in FilterCollection", filter);
    return found;
}

static void test_empty_graph(void)
{
    IGraphBuilder *g = new_graph();
    IMediaControl *mc;
    IDispatch *d = (IDispatch *)0x1;
    IAMCollection *c;
    HRESULT hr;

    if (!g) return;
    IGraphBuilder_QueryInterface(g, &IID_IMediaControl, (void **)&mc);

    hr = IMediaControl_get_FilterCollection(mc, NULL);
    CHECK_HR(hr, E_POINTER, "FilterCollection(NULL)");
    hr = IMediaControl_get_RegFilterCollection(mc, NULL);
    CHECK_HR(hr, E_POINTER, "RegFilterCollection(NULL)");
    hr = IMediaControl_AddSourceFilter(mc, (BSTR)L"x", NULL);
    CHECK_HR(hr, E_POINTER, "AddSourceFilter(NULL out)");
    hr = IMediaControl_AddSourceFilter(mc, (BSTR)L"C:\\no\\such\\file.dat", &d);
    CHECK(FAILED(hr) && d == NULL, "AddSourceFilter(missing file) hr %#lx out %p", (long)hr, d);

    hr = IMediaControl_get_FilterCollection(mc, &d);
    CHECK_HR(hr, S_OK, "FilterCollection");
    c = get_collection(d);
    if (c) { check_collection_generic(c, 0, "empty FilterCollection"); IAMCollection_Release(c); }

    IMediaControl_Release(mc);
    IGraphBuilder_Release(g);
}

static void test_source_filter(void)
{
    static const struct { const char *ext; } files[] = { {"dat"}, {"bin"} };
    IGraphBuilder *g = new_graph();
    IMediaControl *mc;
    unsigned f;

    if (!g) return;
    IGraphBuilder_QueryInterface(g, &IID_IMediaControl, (void **)&mc);

    for (f = 0; f < ARRAY_SIZE_(files); f++)
    {
        WCHAR path[MAX_PATH];
        char pathA[MAX_PATH];
        HANDLE h;
        DWORD w;
        IDispatch *d = NULL, *cd = NULL;
        IFilterInfo *fi;
        IBaseFilter *filter;
        IUnknown *unk;
        IAMCollection *c;
        IEnumFilters *ef;
        IBaseFilter *oracle[8];
        int n = 0, i;
        HRESULT hr;

        GetTempPathA(MAX_PATH, pathA);
        strcat(pathA, f ? "sg-probe-b.bin" : "sg-probe-a.dat");
        h = CreateFileA(pathA, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        WriteFile(h, "0123456789abcdef0123456789abcdef", 32, &w, NULL);
        CloseHandle(h);
        MultiByteToWideChar(CP_ACP, 0, pathA, -1, path, MAX_PATH);

        hr = IMediaControl_AddSourceFilter(mc, path, &d);
        CHECK_HR(hr, S_OK, "AddSourceFilter");
        if (hr != S_OK) { DeleteFileA(pathA); continue; }
        hr = IDispatch_QueryInterface(d, &IID_IFilterInfo, (void **)&fi);
        CHECK_HR(hr, S_OK, "QI IFilterInfo");
        IDispatch_Release(d);

        IFilterInfo_get_Filter(fi, &unk);
        IUnknown_QueryInterface(unk, &IID_IBaseFilter, (void **)&filter);
        IUnknown_Release(unk);
        /* the graph holds the filter, named after the file */
        {
            IBaseFilter *found = NULL;
            hr = IGraphBuilder_FindFilterByName(g, path, &found);
            CHECK(hr == S_OK && found == filter, "FindFilterByName(%ls) hr %#lx %p expected %p", path, (long)hr, found, filter);
            if (found) IBaseFilter_Release(found);
        }
        check_filter_info(fi, filter, 1, path);

        /* the filename can be changed through the object */
        {
            BSTR b;
            hr = IFilterInfo_put_Filename(fi, (BSTR)L"C:\\no\\such\\file.dat");
            CHECK(FAILED(hr), "put_Filename(missing) hr %#lx", (long)hr);
            hr = IFilterInfo_put_Filename(fi, path);
            CHECK_HR(hr, S_OK, "put_Filename(path)");
            hr = IFilterInfo_get_Filename(fi, &b);
            CHECK_HR(hr, S_OK, "get_Filename after put");
            if (hr == S_OK) check_bstr(b, path, "Filename after put");
        }

        /* collection vs graph's own enumeration */
        IGraphBuilder_EnumFilters(g, &ef);
        while (n < 8 && IEnumFilters_Next(ef, 1, &oracle[n], NULL) == S_OK) n++;
        IEnumFilters_Release(ef);
        hr = IMediaControl_get_FilterCollection(mc, &cd);
        CHECK_HR(hr, S_OK, "FilterCollection");
        c = get_collection(cd);
        if (c)
        {
            check_collection_generic(c, n, "FilterCollection");
            for (i = 0; i < n; i++)
            {
                IUnknown *u, *fu;
                IFilterInfo *fi2;
                FILTER_INFO info;
                BSTR b;

                hr = IAMCollection_Item(c, i, &u);
                CHECK_HR(hr, S_OK, "FilterCollection Item");
                if (hr != S_OK) continue;
                IUnknown_QueryInterface(u, &IID_IFilterInfo, (void **)&fi2);
                IBaseFilter_QueryFilterInfo(oracle[i], &info);
                if (info.pGraph) IFilterGraph_Release(info.pGraph);
                IFilterInfo_get_Name(fi2, &b);
                check_bstr(b, info.achName, "Item(i) name");
                IFilterInfo_get_Filter(fi2, &fu);
                CHECK(fu == (IUnknown *)oracle[i], "Item(%d) filter %p expected %p", i, fu, oracle[i]);
                IUnknown_Release(fu);
                IFilterInfo_Release(fi2);
                IUnknown_Release(u);
            }
            IAMCollection_Release(c);
        }
        for (i = 0; i < n; i++) IBaseFilter_Release(oracle[i]);
        CHECK(n == (int)(f + 1), "graph has %d filters, expected %u", n, f + 1);

        IBaseFilter_Release(filter);
        IFilterInfo_Release(fi);
        DeleteFileA(pathA);
    }
    IMediaControl_Release(mc);
    IGraphBuilder_Release(g);
}

static void test_reg_collection(void)
{
    IGraphBuilder *g = new_graph();
    IMediaControl *mc;
    IFilterMapper *mapper;
    IEnumRegFilters *er;
    REGFILTER *reg[512];
    int n = 0, i, found_avi = -1;
    IDispatch *d;
    IAMCollection *c;
    HRESULT hr;

    if (!g) return;
    IGraphBuilder_QueryInterface(g, &IID_IMediaControl, (void **)&mc);
    hr = CoCreateInstance(&CLSID_FilterMapper, NULL, CLSCTX_INPROC_SERVER, &IID_IFilterMapper, (void **)&mapper);
    CHECK_HR(hr, S_OK, "create FilterMapper");
    hr = IFilterMapper_EnumMatchingFilters(mapper, &er, 0, FALSE, GUID_NULL, GUID_NULL, FALSE, FALSE, GUID_NULL, GUID_NULL);
    CHECK_HR(hr, S_OK, "oracle enum");
    while (n < 512 && IEnumRegFilters_Next(er, 1, &reg[n], NULL) == S_OK) n++;
    IEnumRegFilters_Release(er);
    CHECK(n > 10, "only %d registered filters", n);

    hr = IMediaControl_get_RegFilterCollection(mc, &d);
    CHECK_HR(hr, S_OK, "RegFilterCollection");
    c = get_collection(d);
    if (c)
    {
        check_collection_generic(c, n, "RegFilterCollection");
        for (i = 0; i < n; i++)
        {
            IUnknown *u;
            IRegFilterInfo *ri;
            BSTR b;

            hr = IAMCollection_Item(c, i, &u);
            CHECK_HR(hr, S_OK, "RegFilterCollection Item");
            if (hr != S_OK) continue;
            hr = IUnknown_QueryInterface(u, &IID_IRegFilterInfo, (void **)&ri);
            CHECK_HR(hr, S_OK, "QI IRegFilterInfo");
            if (hr == S_OK)
            {
                IRegFilterInfo_get_Name(ri, &b);
                check_bstr(b, reg[i]->Name, "reg Item(i) name");
                if (i == 0)
                {
                    hr = IRegFilterInfo_get_Name(ri, NULL);
                    CHECK_HR(hr, E_POINTER, "reg Name NULL");
                    hr = IRegFilterInfo_Filter(ri, NULL);
                    CHECK_HR(hr, E_POINTER, "reg Filter NULL");
                }
                if (!wcscmp(reg[i]->Name, L"AVI Splitter")) found_avi = i;
                IRegFilterInfo_Release(ri);
            }
            IUnknown_Release(u);
        }
        CHECK(found_avi >= 0, "AVI Splitter not in the registered filter collection");

        if (found_avi >= 0)
        {
            IUnknown *u;
            IRegFilterInfo *ri;
            IDispatch *fd, *cd;
            IFilterInfo *fi;
            IBaseFilter *filter, *found = NULL;
            IUnknown *fu;
            IAMCollection *gc;
            LONG count = -1;

            IAMCollection_Item(c, found_avi, &u);
            IUnknown_QueryInterface(u, &IID_IRegFilterInfo, (void **)&ri);
            IUnknown_Release(u);
            hr = IRegFilterInfo_Filter(ri, &fd);
            CHECK_HR(hr, S_OK, "RegFilterInfo::Filter");
            if (hr == S_OK)
            {
                IDispatch_QueryInterface(fd, &IID_IFilterInfo, (void **)&fi);
                IDispatch_Release(fd);
                IFilterInfo_get_Filter(fi, &fu);
                IUnknown_QueryInterface(fu, &IID_IBaseFilter, (void **)&filter);
                IUnknown_Release(fu);
                hr = IGraphBuilder_FindFilterByName(g, L"AVI Splitter", &found);
                CHECK(hr == S_OK && found == filter, "AVI Splitter added to graph under its name: hr %#lx", (long)hr);
                if (found) IBaseFilter_Release(found);
                check_filter_info(fi, filter, 0, NULL);
                IMediaControl_get_FilterCollection(mc, &cd);
                gc = get_collection(cd);
                if (gc) { IAMCollection_get_Count(gc, &count); CHECK(count == 1, "graph count %ld after Filter()", count); IAMCollection_Release(gc); }
                IBaseFilter_Release(filter);
                IFilterInfo_Release(fi);
            }
            IRegFilterInfo_Release(ri);
        }
        IAMCollection_Release(c);
    }
    for (i = 0; i < n; i++) CoTaskMemFree(reg[i]);
    IFilterMapper_Release(mapper);
    IMediaControl_Release(mc);
    IGraphBuilder_Release(g);
}

/* Pin connections through IPinInfo: connect a renderer's pins with a mismatched type. */
static void test_pin_connect(void)
{
    IGraphBuilder *g = new_graph();
    IMediaControl *mc;
    WCHAR path[MAX_PATH];
    char pathA[MAX_PATH];
    HANDLE h;
    DWORD w;
    IDispatch *d, *pd, *rd, *ppd;
    IFilterInfo *src_fi, *rend_fi;
    IBaseFilter *rend;
    IPinInfo *out_pi, *in_pi;
    IAMCollection *pc;
    IUnknown *u;
    HRESULT hr, exp;
    IPin *op, *ip;

    if (!g) return;
    IGraphBuilder_QueryInterface(g, &IID_IMediaControl, (void **)&mc);
    GetTempPathA(MAX_PATH, pathA);
    strcat(pathA, "sg-probe-c.dat");
    h = CreateFileA(pathA, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(h, "0123456789abcdef0123456789abcdef", 32, &w, NULL);
    CloseHandle(h);
    MultiByteToWideChar(CP_ACP, 0, pathA, -1, path, MAX_PATH);

    hr = IMediaControl_AddSourceFilter(mc, path, &d);
    CHECK_HR(hr, S_OK, "AddSourceFilter");
    if (hr != S_OK) goto done;
    IDispatch_QueryInterface(d, &IID_IFilterInfo, (void **)&src_fi);
    IDispatch_Release(d);

    hr = CoCreateInstance(&CLSID_VideoRenderer, NULL, CLSCTX_INPROC_SERVER, &IID_IBaseFilter, (void **)&rend);
    CHECK_HR(hr, S_OK, "create renderer");
    IGraphBuilder_AddFilter(g, rend, L"Rend");
    rend_fi = info_for(mc, rend);
    IFilterInfo_get_Pins(src_fi, &pd);
    pc = get_collection(pd);
    IAMCollection_Item(pc, 0, &u);
    IUnknown_QueryInterface(u, &IID_IPinInfo, (void **)&out_pi);
    IUnknown_Release(u);
    IAMCollection_Release(pc);
    IFilterInfo_get_Pins(rend_fi, &rd);
    pc = get_collection(rd);
    IAMCollection_Item(pc, 0, &u);
    IUnknown_QueryInterface(u, &IID_IPinInfo, (void **)&in_pi);
    IUnknown_Release(u);
    IAMCollection_Release(pc);
    (void)ppd;

    {
        IUnknown *pu;
        IPinInfo_get_Pin(out_pi, &pu); IUnknown_QueryInterface(pu, &IID_IPin, (void **)&op); IUnknown_Release(pu);
        IPinInfo_get_Pin(in_pi, &pu); IUnknown_QueryInterface(pu, &IID_IPin, (void **)&ip); IUnknown_Release(pu);
    }
    check_pin_info(in_pi, ip, rend_fi);

    /* expected result of the same operations done directly on the graph */
    exp = IGraphBuilder_ConnectDirect(g, op, ip, NULL);
    CHECK(FAILED(exp), "baseline ConnectDirect should fail, hr %#lx", (long)exp);
    hr = IPinInfo_ConnectDirect(out_pi, (IUnknown *)ip);
    CHECK_HR(hr, exp, "PinInfo::ConnectDirect");
    hr = IPinInfo_ConnectDirect(in_pi, (IUnknown *)op);
    CHECK_HR(hr, exp, "PinInfo::ConnectDirect reversed");
    hr = IPinInfo_ConnectDirect(out_pi, NULL);
    CHECK_HR(hr, E_POINTER, "ConnectDirect(NULL)");
    hr = IPinInfo_Disconnect(out_pi);
    CHECK_HR(hr, VFW_E_NOT_CONNECTED, "Disconnect unconnected");
    exp = IGraphBuilder_Connect(g, op, ip);
    hr = IPinInfo_Connect(out_pi, (IUnknown *)ip);
    CHECK_HR(hr, exp, "PinInfo::Connect");
    {
        IDispatch *mt;
        IMediaTypeInfo *mi;
        BSTR t;
        WCHAR g1[40];
        hr = IPinInfo_get_MediaTypes(out_pi, &mt);
        pc = get_collection(mt);
        if (pc)
        {
            IUnknown *mu;
            if (IAMCollection_Item(pc, 0, &mu) == S_OK)
            {
                IUnknown_QueryInterface(mu, &IID_IMediaTypeInfo, (void **)&mi);
                IUnknown_Release(mu);
                IMediaTypeInfo_get_Type(mi, &t);
                guid_str(&MEDIATYPE_Stream, g1);
                check_bstr(t, g1, "source output major type is Stream");
                /* ConnectWithType to the renderer: type is not accepted, same as direct */
                {
                    AM_MEDIA_TYPE amt = {0};
                    amt.majortype = MEDIATYPE_Stream;
                    exp = IGraphBuilder_ConnectDirect(g, op, ip, &amt);
                    CHECK(FAILED(exp), "baseline ConnectDirect(type) should fail, hr %#lx", (long)exp);
                }
                hr = IPinInfo_ConnectWithType(out_pi, (IUnknown *)ip, (IDispatch *)mi);
                CHECK_HR(hr, exp, "PinInfo::ConnectWithType");
                IMediaTypeInfo_Release(mi);
            }
            IAMCollection_Release(pc);
        }
    }
    IPin_Release(op); IPin_Release(ip);
    IPinInfo_Release(out_pi); IPinInfo_Release(in_pi);
    IFilterInfo_Release(src_fi); IFilterInfo_Release(rend_fi);
    IBaseFilter_Release(rend);
done:
    DeleteFileA(pathA);
    IMediaControl_Release(mc);
    IGraphBuilder_Release(g);
}

static void put32(unsigned char *p, unsigned v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }

/* Smallest AVI the splitter accepts: one 2x2 24-bit frame. */
static unsigned build_avi(unsigned char *b)
{
    unsigned n = 0, strl_off, hdrl_off, movi_off;
    unsigned char *p = b;
#define TAG(t) do { memcpy(p + n, t, 4); n += 4; } while (0)
#define U32(v) do { put32(p + n, v); n += 4; } while (0)
    TAG("RIFF"); U32(0); TAG("AVI ");
    TAG("LIST"); hdrl_off = n; U32(0); TAG("hdrl");
    TAG("avih"); U32(56);
    U32(100000); U32(0); U32(0); U32(0x10); U32(1); U32(0); U32(1); U32(32); U32(2); U32(2);
    U32(0); U32(0); U32(0); U32(0);
    TAG("LIST"); strl_off = n; U32(0); TAG("strl");
    TAG("strh"); U32(56);
    TAG("vids"); TAG("DIB "); U32(0); U32(0); U32(0); U32(1); U32(10); U32(0); U32(1); U32(32);
    U32(0xffffffff); U32(0); U32(0); U32(0);
    TAG("strf"); U32(40);
    U32(40); U32(2); U32(2); U32(1 | (24 << 16)); U32(0); U32(16); U32(0); U32(0); U32(0); U32(0);
    put32(p + strl_off, n - strl_off - 4);
    put32(p + hdrl_off, n - hdrl_off - 4);
    TAG("LIST"); movi_off = n; U32(0); TAG("movi");
    TAG("00db"); U32(16); memset(p + n, 0x80, 16); n += 16;
    put32(p + movi_off, n - movi_off - 4);
    put32(p + 4, n - 8);
    return n;
}

static IPinInfo *pin_info_of(IFilterInfo *fi, const WCHAR *id)
{
    IDispatch *d;
    IPinInfo *pi = NULL;

    if (IFilterInfo_FindPin(fi, (BSTR)id, &d) == S_OK)
    {
        IDispatch_QueryInterface(d, &IID_IPinInfo, (void **)&pi);
        IDispatch_Release(d);
    }
    return pi;
}

static IPin *raw_pin(IPinInfo *pi)
{
    IUnknown *u;
    IPin *pin = NULL;

    IPinInfo_get_Pin(pi, &u);
    IUnknown_QueryInterface(u, &IID_IPin, (void **)&pin);
    IUnknown_Release(u);
    return pin;
}

/* A real connection between the source filter and an AVI splitter, made and broken from either end. */
static void test_real_connect(void)
{
    static const struct { int from_source; const char *how; } cases[] = {
        { 1, "source->splitter" }, { 0, "splitter->source (reversed)" },
    };
    IGraphBuilder *g = new_graph();
    IMediaControl *mc;
    unsigned char data[512];
    char pathA[MAX_PATH];
    WCHAR path[MAX_PATH];
    HANDLE h;
    DWORD w;
    IDispatch *d, *rd;
    IFilterInfo *src_fi = NULL, *spl_fi = NULL;
    IBaseFilter *spl;
    unsigned c, size;
    HRESULT hr;

    if (!g) return;
    IGraphBuilder_QueryInterface(g, &IID_IMediaControl, (void **)&mc);
    size = build_avi(data);
    GetTempPathA(MAX_PATH, pathA);
    strcat(pathA, "sg-probe-d.avi");
    h = CreateFileA(pathA, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(h, data, size, &w, NULL);
    CloseHandle(h);
    MultiByteToWideChar(CP_ACP, 0, pathA, -1, path, MAX_PATH);

    hr = IMediaControl_AddSourceFilter(mc, path, &d);
    CHECK_HR(hr, S_OK, "AddSourceFilter(avi)");
    if (hr != S_OK) goto done;
    IDispatch_QueryInterface(d, &IID_IFilterInfo, (void **)&src_fi);
    IDispatch_Release(d);
    hr = CoCreateInstance(&CLSID_AviSplitter, NULL, CLSCTX_INPROC_SERVER, &IID_IBaseFilter, (void **)&spl);
    CHECK_HR(hr, S_OK, "create AVI splitter");
    if (hr != S_OK) goto done;
    IGraphBuilder_AddFilter(g, spl, L"Splitter");
    spl_fi = info_for(mc, spl);
    (void)rd;

    for (c = 0; c < ARRAY_SIZE_(cases); c++)
    {
        IPinInfo *out_pi = pin_info_of(src_fi, L"Output"), *in_pi = NULL;
        IPinInfo *self, *peer_pi;
        IPin *op, *ip, *peer;
        IDispatch *x, *mt;
        IMediaTypeInfo *mi;
        BSTR t;
        WCHAR g1[40];
        IAMCollection *pc;
        IDispatch *pd;
        IUnknown *u;

        IFilterInfo_get_Pins(spl_fi, &pd);
        pc = get_collection(pd);
        IAMCollection_Item(pc, 0, &u);
        IUnknown_QueryInterface(u, &IID_IPinInfo, (void **)&in_pi);
        IUnknown_Release(u);
        IAMCollection_Release(pc);
        op = raw_pin(out_pi); ip = raw_pin(in_pi);

        self = cases[c].from_source ? out_pi : in_pi;
        peer_pi = cases[c].from_source ? in_pi : out_pi;
        hr = IPinInfo_ConnectDirect(self, (IUnknown *)(cases[c].from_source ? ip : op));
        CHECK(hr == S_OK, "%s: ConnectDirect hr %#lx", cases[c].how, (long)hr);
        if (hr == S_OK)
        {
            hr = IPin_ConnectedTo(op, &peer);
            CHECK(hr == S_OK && peer == ip, "%s: output peer %p expected %p", cases[c].how, hr == S_OK ? peer : NULL, ip);
            if (hr == S_OK) IPin_Release(peer);
            hr = IPinInfo_get_ConnectedTo(out_pi, &x);
            CHECK_HR(hr, S_OK, "ConnectedTo connected");
            if (hr == S_OK)
            {
                IPinInfo *px;
                IUnknown *pu;
                IDispatch_QueryInterface(x, &IID_IPinInfo, (void **)&px);
                IPinInfo_get_Pin(px, &pu);
                CHECK(pu == (IUnknown *)ip, "ConnectedTo pin %p expected %p", pu, ip);
                IUnknown_Release(pu); IPinInfo_Release(px); IDispatch_Release(x);
            }
            hr = IPinInfo_get_ConnectionMediaType(out_pi, &mt);
            CHECK_HR(hr, S_OK, "ConnectionMediaType connected");
            if (hr == S_OK)
            {
                IDispatch_QueryInterface(mt, &IID_IMediaTypeInfo, (void **)&mi);
                IMediaTypeInfo_get_Type(mi, &t);
                guid_str(&MEDIATYPE_Stream, g1);
                check_bstr(t, g1, "connection major type");
                IMediaTypeInfo_Release(mi);
                IDispatch_Release(mt);
            }
            hr = IPinInfo_Disconnect(peer_pi);
            CHECK_HR(hr, S_OK, "Disconnect from the other end");
            CHECK(IPin_ConnectedTo(op, &peer) == VFW_E_NOT_CONNECTED, "%s: output still connected", cases[c].how);
            CHECK(IPin_ConnectedTo(ip, &peer) == VFW_E_NOT_CONNECTED, "%s: input still connected", cases[c].how);
        }
        IPin_Release(op); IPin_Release(ip);
        IPinInfo_Release(out_pi); IPinInfo_Release(in_pi);
    }
    IBaseFilter_Release(spl);
done:
    if (src_fi) IFilterInfo_Release(src_fi);
    if (spl_fi) IFilterInfo_Release(spl_fi);
    DeleteFileA(pathA);
    IMediaControl_Release(mc);
    IGraphBuilder_Release(g);
}

int main(void)
{
    CoInitialize(NULL);
    test_empty_graph();
    test_source_filter();
    test_reg_collection();
    test_pin_connect();
    test_real_connect();
    printf("%d checks, %d failures\n", checks, failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    CoUninitialize();
    return failures != 0;
}
