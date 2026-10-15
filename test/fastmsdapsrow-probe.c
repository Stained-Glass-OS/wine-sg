/* msdaps row_server.c (patch 3191): the row and rowset proxies for the methods that were stubs (RestartPosition,
 * Compare, GetRowsByBookmark, Hash, GetReferencedRowset, GetSpecification, AddRefAccessor, IRow::GetSourceRowset,
 * IRow::Open / IBindResource::Bind for rows and for other objects). A provider in an MTA thread, honouring
 * aggregation, called from the main STA. Prints "KEY value" lines. */
#define COBJMACROS
#define CONST_VTABLE
#include <windows.h>
#include <ole2.h>
#include <oledb.h>
#include <oledberr.h>
#include <stdio.h>
#include <stddef.h>
#include <wchar.h>

#define P(...) do { printf(__VA_ARGS__); fflush(stdout); } while (0)
static const GUID g_rowset = {0xc8b522f6,0x5cf3,0x11ce,{0xad,0xe5,0x00,0xaa,0x00,0x44,0x77,0x3d}};
static const GUID g_row = {0xc8b522f7,0x5cf3,0x11ce,{0xad,0xe5,0x00,0xaa,0x00,0x44,0x77,0x3d}};
static const GUID g_stream = {0xc8b522f9,0x5cf3,0x11ce,{0xad,0xe5,0x00,0xaa,0x00,0x44,0x77,0x3d}};
static const GUID g_other = {0x12345678,0x1111,0x2222,{0x33,0x33,0x44,0x44,0x44,0x44,0x44,0x44}};

static volatile LONG last_cb1, last_cb2, last_rs, saw_outer;

/* ---- a rowset: IRowsetLocate + IRowsetInfo + IAccessor on one object ---- */
struct rs { IRowsetLocate loc; IRowsetInfo info; IAccessor acc; IUnknown *outer; int id; };
static struct rs rs_a = { .id = 1 }, rs_ref = { .id = 2 }, rs_src = { .id = 3 };
static IOpenRowset or_iface;

static HRESULT WINAPI rs_qi(struct rs *s, REFIID riid, void **out)
{
    *out = NULL;
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IRowset) || IsEqualGUID(riid, &IID_IRowsetLocate)) *out = &s->loc;
    else if (IsEqualGUID(riid, &IID_IRowsetInfo)) *out = &s->info;
    else if (IsEqualGUID(riid, &IID_IAccessor)) *out = &s->acc;
    else if (IsEqualGUID(riid, &IID_IMarshal) && s->outer) return IUnknown_QueryInterface(s->outer, riid, out);
    else return E_NOINTERFACE;
    return S_OK;
}
static ULONG WINAPI rs_addref(void *i) { return 2; }
static ULONG WINAPI rs_release(void *i) { return 1; }
static HRESULT WINAPI l_qi(IRowsetLocate *i, REFIID r, void **o) { return rs_qi(CONTAINING_RECORD(i, struct rs, loc), r, o); }
static HRESULT WINAPI i_qi(IRowsetInfo *i, REFIID r, void **o) { return rs_qi(CONTAINING_RECORD(i, struct rs, info), r, o); }
static HRESULT WINAPI a_qi(IAccessor *i, REFIID r, void **o) { return rs_qi(CONTAINING_RECORD(i, struct rs, acc), r, o); }
static HRESULT WINAPI l_nope(void) { return E_NOTIMPL; }
static HRESULT WINAPI l_Restart(IRowsetLocate *i, HCHAPTER c) { return DB_S_COMMANDREEXECUTED; }
static unsigned sum(const BYTE *p, ULONG n) { unsigned s = 0; while (n--) s += *p++; return s; }
static HRESULT WINAPI l_Compare(IRowsetLocate *i, HCHAPTER c, DBBKMARK cb1, const BYTE *b1, DBBKMARK cb2, const BYTE *b2, DBCOMPARE *r)
{
    struct rs *s = CONTAINING_RECORD(i, struct rs, loc);
    unsigned x = sum(b1, cb1), y = sum(b2, cb2);
    last_cb1 = cb1; last_cb2 = cb2; last_rs = s->id;
    *r = x < y ? DBCOMPARE_LT : x == y ? DBCOMPARE_EQ : DBCOMPARE_GT;
    return S_OK;
}
static HRESULT WINAPI l_ByBm(IRowsetLocate *i, HCHAPTER c, DBCOUNTITEM n, const DBBKMARK *cbs, const BYTE **bms, HROW *rows, DBROWSTATUS *st)
{
    DBCOUNTITEM k;
    HRESULT hr = S_OK;
    for (k = 0; k < n; k++)
    {
        if (cbs[k] && bms[k][0] == 0xff) { rows[k] = 0; if (st) st[k] = DBROWSTATUS_E_INVALID; hr = DB_S_ERRORSOCCURRED; continue; }
        rows[k] = sum(bms[k], cbs[k]) * 10 + cbs[k];
        if (st) st[k] = DBROWSTATUS_S_OK;
    }
    return hr;
}
static HRESULT WINAPI l_Hash(IRowsetLocate *i, HCHAPTER c, DBBKMARK n, const DBBKMARK *cbs, const BYTE **bms, DBHASHVALUE *h, DBROWSTATUS *st)
{
    DBBKMARK k;
    for (k = 0; k < n; k++) { h[k] = sum(bms[k], cbs[k]) * 31 + cbs[k]; if (st) st[k] = DBROWSTATUS_S_OK; }
    return S_OK;
}
static const IRowsetLocateVtbl loc_vtbl = { l_qi, (void *)rs_addref, (void *)rs_release, (void *)l_nope, (void *)l_nope, (void *)l_nope, (void *)l_nope,
    l_Restart, l_Compare, (void *)l_nope, l_ByBm, l_Hash };

static HRESULT WINAPI i_GetProps(IRowsetInfo *i, ULONG n, const DBPROPIDSET *ids, ULONG *np, DBPROPSET **ps) { return E_NOTIMPL; }
static HRESULT WINAPI i_GetRef(IRowsetInfo *i, DBORDINAL ord, REFIID riid, IUnknown **out)
{
    *out = NULL;
    if (ord != 1) return DB_E_NOTAREFERENCECOLUMN;
    return rs_qi(&rs_ref, riid, (void **)out);
}
static HRESULT WINAPI i_GetSpec(IRowsetInfo *i, REFIID riid, IUnknown **out)
{
    *out = NULL;
    if (!IsEqualGUID(riid, &IID_IOpenRowset)) return E_NOINTERFACE;
    *out = (IUnknown *)&or_iface;
    return S_OK;
}
static const IRowsetInfoVtbl info_vtbl = { i_qi, (void *)rs_addref, (void *)rs_release, i_GetProps, i_GetRef, i_GetSpec };

static HRESULT WINAPI a_AddRef(IAccessor *i, HACCESSOR h, DBREFCOUNT *c) { if (!h) return DB_E_BADACCESSORHANDLE; if (c) *c = (DBREFCOUNT)h + 1; return S_OK; }
static const IAccessorVtbl acc_vtbl = { a_qi, (void *)rs_addref, (void *)rs_release, a_AddRef, (void *)l_nope, (void *)l_nope, (void *)l_nope };

/* ---- a row ---- */
struct row { IRow row; IUnknown *outer; };
static struct row row_a;
static IBindResource br_iface;
static HRESULT WINAPI row_qi(IRow *i, REFIID riid, void **out)
{
    struct row *r = CONTAINING_RECORD(i, struct row, row);
    *out = NULL;
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IRow)) *out = i;
    else if (IsEqualGUID(riid, &IID_IMarshal) && r->outer) return IUnknown_QueryInterface(r->outer, riid, out);
    else return E_NOINTERFACE;
    return S_OK;
}
static HRESULT WINAPI row_GetCols(IRow *i, DBORDINAL n, DBCOLUMNACCESS *c) { return E_NOTIMPL; }
static HRESULT WINAPI row_GetSrc(IRow *i, REFIID riid, IUnknown **out, HROW *hr)
{
    *hr = 7;
    if (out) { *out = NULL; return rs_qi(&rs_src, riid, (void **)out); }
    return S_OK;
}
static HRESULT WINAPI row_Open(IRow *i, IUnknown *outer, DBID *col, REFGUID type, DWORD flags, REFIID riid, IUnknown **out)
{
    struct row *r = &row_a;
    *out = NULL;
    if (IsEqualGUID(type, &g_row)) { r->outer = outer; return row_qi(&r->row, riid, (void **)out); }
    if (IsEqualGUID(type, &g_rowset)) { rs_a.outer = outer; return rs_qi(&rs_a, riid, (void **)out); }
    saw_outer = outer != NULL;
    if (!col || !col->uName.pwszName || wcscmp(col->uName.pwszName, L"col")) return E_INVALIDARG;
    return IUnknown_QueryInterface((IUnknown *)&or_iface, riid, (void **)out);
}
static const IRowVtbl row_vtbl = { row_qi, (void *)rs_addref, (void *)rs_release, row_GetCols, row_GetSrc, row_Open };

/* ---- IOpenRowset and IBindResource ---- */
static HRESULT WINAPI or_qi(IOpenRowset *i, REFIID riid, void **out)
{
    *out = NULL;
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IOpenRowset)) { *out = i; return S_OK; }
    return E_NOINTERFACE;
}
static HRESULT WINAPI or_Open(IOpenRowset *i, IUnknown *outer, DBID *t, DBID *x, REFIID riid, ULONG n, DBPROPSET *ps, IUnknown **rs)
{
    *rs = NULL;
    rs_a.outer = outer;
    return rs_qi(&rs_a, riid, (void **)rs);
}
static const IOpenRowsetVtbl or_vtbl = { or_qi, (void *)rs_addref, (void *)rs_release, or_Open };

static HRESULT WINAPI br_qi(IBindResource *i, REFIID riid, void **out)
{
    *out = NULL;
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IBindResource)) { *out = i; return S_OK; }
    return E_NOINTERFACE;
}
static HRESULT WINAPI br_Bind(IBindResource *i, IUnknown *outer, LPCOLESTR url, DBBINDURLFLAG fl, REFGUID type, REFIID riid, IAuthenticate *au,
                              DBIMPLICITSESSION *sess, DBBINDURLSTATUS *st, IUnknown **out)
{
    *out = NULL;
    *st = 0;
    if (IsEqualGUID(type, &g_row)) { row_a.outer = outer; return row_qi(&row_a.row, riid, (void **)out); }
    if (IsEqualGUID(type, &g_rowset)) { rs_a.outer = outer; return rs_qi(&rs_a, riid, (void **)out); }
    saw_outer = outer != NULL;
    return IUnknown_QueryInterface((IUnknown *)&or_iface, riid, (void **)out);
}
static const IBindResourceVtbl br_vtbl = { br_qi, (void *)rs_addref, (void *)rs_release, br_Bind };

/* ---- a command with a parameter accessor: ICommand + IAccessor ---- */
struct P
{
    DWORD st_i; LONG i;
    DWORD st_b; BSTR b;
    VARIANT v;
    WCHAR *wref; DBLENGTH wlen;
    IUnknown *unk;
    LONG out_i; DWORD out_i_st;
    BSTR out_b;
    WCHAR *out_ref; DBLENGTH out_len;
    LONG io_i;
    BSTR io_b;
    BYTE pad[40];
};
struct cmd { ICommand cmd; IAccessor acc; };
static struct cmd cmd_a;
static DBBINDING acc_binds[4][16];
static DBCOUNTITEM acc_nb[4];
static DBLENGTH acc_size[4];
static volatile LONG exec_seen_sets, exec_in_i[2], exec_in_wlen[2];
static WCHAR exec_in_b[2][32], exec_in_w[2][32], exec_in_v[2][32];
static volatile LONG exec_stream_ok, exec_called;

static HRESULT WINAPI c_qi(ICommand *i, REFIID riid, void **out)
{
    *out = NULL;
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_ICommand)) *out = &cmd_a.cmd;
    else if (IsEqualGUID(riid, &IID_IAccessor)) *out = &cmd_a.acc;
    else return E_NOINTERFACE;
    return S_OK;
}
static HRESULT WINAPI ca_qi(IAccessor *i, REFIID riid, void **out) { return c_qi(NULL, riid, out); }
static HRESULT WINAPI c_Cancel(ICommand *i) { return S_OK; }
static HRESULT WINAPI c_GetSession(ICommand *i, REFIID riid, IUnknown **out) { return E_NOTIMPL; }
static HRESULT WINAPI c_Execute(ICommand *i, IUnknown *outer, REFIID riid, DBPARAMS *pp, DBROWCOUNT *aff, IUnknown **rs)
{
    unsigned h, k;
    exec_called++;
    if (rs) *rs = NULL;
    if (!pp) { if (aff) *aff = 11; return S_OK; }
    h = (unsigned)pp->hAccessor;
    if (!pp->pData || h == 0 || h > 3) return DB_E_BADACCESSORHANDLE;
    exec_seen_sets = pp->cParamSets;
    for (k = 0; k < pp->cParamSets && k < 2; k++)
    {
        struct P *p = (struct P *)((BYTE *)pp->pData + k * acc_size[h]);
        char *wl = NULL;
        exec_in_i[k] = p->i;
        wcsncpy((WCHAR *)exec_in_b[k], p->b ? p->b : L"(null)", 31);
        wcsncpy((WCHAR *)exec_in_w[k], p->wref ? p->wref : L"(null)", 31);
        exec_in_wlen[k] = p->wlen;
        swprintf((WCHAR *)exec_in_v[k], 32, L"%d:%d", V_VT(&p->v), V_VT(&p->v) == VT_I4 ? V_I4(&p->v) : V_VT(&p->v) == VT_BSTR ? (int)wcslen(V_BSTR(&p->v)) : -1);
        if (k == 0 && p->unk)
        {
            IStream *st = NULL;
            if (SUCCEEDED(IUnknown_QueryInterface(p->unk, &IID_IStream, (void **)&st)))
            {
                ULONG n;
                if (SUCCEEDED(IStream_Write(st, "hello", 5, &n)) && n == 5) exec_stream_ok = 1;
                IStream_Release(st);
            }
        }
        (void)wl;
        p->out_i = p->i * 2; p->out_i_st = 0;
        p->out_b = SysAllocString(L"out-bstr");
        p->out_ref = CoTaskMemAlloc(32 * sizeof(WCHAR));
        wcscpy(p->out_ref, L"out-ref"); p->out_len = 7 * sizeof(WCHAR);
        p->io_i += 100;
        if (p->io_b) { WCHAR tmp[40]; swprintf(tmp, 40, L"io:%ls", p->io_b); SysFreeString(p->io_b); p->io_b = SysAllocString(tmp); }
    }
    if (aff) *aff = 33;
    return S_OK;
}
static HRESULT WINAPI ca_nope(void) { return E_NOTIMPL; }
static HRESULT WINAPI ca_Create(IAccessor *i, DBACCESSORFLAGS f, DBCOUNTITEM n, const DBBINDING *b, DBLENGTH sz, HACCESSOR *h, DBBINDSTATUS *st)
{
    unsigned k;
    DBCOUNTITEM j;
    for (k = 1; k < 4 && acc_nb[k]; k++) ;
    if (k >= 4 || n > 16) return E_FAIL;
    memcpy(acc_binds[k], b, n * sizeof(*b));
    for (j = 0; j < n; j++) acc_binds[k][j].pObject = NULL;
    acc_nb[k] = n; acc_size[k] = sz;
    *h = k;
    if (st) for (j = 0; j < n; j++) st[j] = DBBINDSTATUS_OK;
    return S_OK;
}
static HRESULT WINAPI ca_Get(IAccessor *i, HACCESSOR h, DBACCESSORFLAGS *f, DBCOUNTITEM *n, DBBINDING **b)
{
    if (h == 0 || h > 3 || !acc_nb[h]) return DB_E_BADACCESSORHANDLE;
    *f = DBACCESSOR_PARAMETERDATA; *n = acc_nb[h];
    *b = CoTaskMemAlloc(acc_nb[h] * sizeof(DBBINDING));
    memcpy(*b, acc_binds[h], acc_nb[h] * sizeof(DBBINDING));
    return S_OK;
}
static HRESULT WINAPI ca_Release(IAccessor *i, HACCESSOR h, DBREFCOUNT *c) { if (h > 0 && h < 4) acc_nb[h] = 0; if (c) *c = 0; return S_OK; }
static const ICommandVtbl cmd_vtbl = { c_qi, (void *)rs_addref, (void *)rs_release, c_Cancel, c_Execute, c_GetSession };
static const IAccessorVtbl cmd_acc_vtbl = { ca_qi, (void *)rs_addref, (void *)rs_release, (void *)ca_nope, ca_Create, ca_Get, ca_Release };

static IStream *streams[4];
static HANDLE ready, done;

static DWORD WINAPI provider_thread(void *arg)
{
    HRESULT hr;
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    rs_a.loc.lpVtbl = rs_ref.loc.lpVtbl = rs_src.loc.lpVtbl = &loc_vtbl;
    rs_a.info.lpVtbl = rs_ref.info.lpVtbl = rs_src.info.lpVtbl = &info_vtbl;
    rs_a.acc.lpVtbl = rs_ref.acc.lpVtbl = rs_src.acc.lpVtbl = &acc_vtbl;
    row_a.row.lpVtbl = &row_vtbl;
    or_iface.lpVtbl = &or_vtbl;
    br_iface.lpVtbl = &br_vtbl;
    cmd_a.cmd.lpVtbl = &cmd_vtbl;
    cmd_a.acc.lpVtbl = &cmd_acc_vtbl;
    hr = CoMarshalInterThreadInterfaceInStream(&IID_IOpenRowset, (IUnknown *)&or_iface, &streams[0]);
    if (FAILED(hr)) P("MARSHAL0 %08lx\n", hr);
    hr = CoMarshalInterThreadInterfaceInStream(&IID_IBindResource, (IUnknown *)&br_iface, &streams[1]);
    if (FAILED(hr)) P("MARSHAL1 %08lx\n", hr);
    hr = CoMarshalInterThreadInterfaceInStream(&IID_ICommand, (IUnknown *)&cmd_a.cmd, &streams[2]);
    if (FAILED(hr)) P("MARSHAL2 %08lx\n", hr);
    hr = CoMarshalInterThreadInterfaceInStream(&IID_IAccessor, (IUnknown *)&cmd_a.acc, &streams[3]);
    if (FAILED(hr)) P("MARSHAL3 %08lx\n", hr);
    SetEvent(ready);
    WaitForSingleObject(done, 60000);
    CoUninitialize();
    return 0;
}

static void *get(unsigned int i, const IID *iid)
{
    void *p = NULL;
    HRESULT hr = CoGetInterfaceAndReleaseStream(streams[i], iid, &p);
    if (FAILED(hr)) P("UNMARSHAL%u %08lx\n", i, hr);
    return p;
}

int main(void)
{
    HANDLE th;
    HRESULT hr;
    IOpenRowset *orow;
    IBindResource *br;
    IUnknown *unk = NULL;
    IRowsetLocate *loc = NULL;
    IRowsetInfo *info = NULL;
    IAccessor *acc = NULL;
    IRow *row = NULL;
    DBID col;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    ready = CreateEventW(NULL, TRUE, FALSE, NULL);
    done = CreateEventW(NULL, TRUE, FALSE, NULL);
    th = CreateThread(NULL, 0, provider_thread, NULL, 0, NULL);
    WaitForSingleObject(ready, 30000);
    orow = get(0, &IID_IOpenRowset);
    br = get(1, &IID_IBindResource);
    col.eKind = DBKIND_NAME; col.uName.pwszName = (LPOLESTR)L"col";

    if (orow)
    {
        DBID table;
        table.eKind = DBKIND_NAME; table.uName.pwszName = (LPOLESTR)L"rs";
        hr = IOpenRowset_OpenRowset(orow, NULL, &table, NULL, &IID_IUnknown, 0, NULL, &unk);
        P("RS_OPEN %08lx %d\n", hr, unk != NULL);
        if (unk)
        {
            IUnknown_QueryInterface(unk, &IID_IRowsetLocate, (void **)&loc);
            IUnknown_QueryInterface(unk, &IID_IRowsetInfo, (void **)&info);
            IUnknown_QueryInterface(unk, &IID_IAccessor, (void **)&acc);
            P("RS_IFACES %d %d %d\n", loc != NULL, info != NULL, acc != NULL);
        }
    }
    if (loc)
    {
        DBCOMPARE cmp = 99;
        BYTE a1[] = {1, 2, 3}, a2[] = {9}, b1[] = {5, 5}, b2[] = {1, 2, 3, 4}, c1[] = {200}, c2[] = {1, 1, 1};
        const DBBKMARK cbs[3] = {1, 4, 2};
        BYTE k1[] = {7}, k2[] = {1, 2, 3, 4}, k3[] = {10, 20};
        const BYTE *bms[3] = {k1, k2, k3};
        HROW rows[3] = {0};
        DBROWSTATUS st[3] = {9, 9, 9};
        DBHASHVALUE hv[3] = {0};
        const BYTE *bad[3] = {k1, k2, k3};
        BYTE kbad[] = {0xff, 1};
        const DBBKMARK cbs_bad[3] = {2, 4, 2};

        P("RS_RESTART %08lx\n", IRowsetLocate_RestartPosition(loc, DB_NULL_HCHAPTER));
        hr = IRowsetLocate_Compare(loc, DB_NULL_HCHAPTER, 3, a1, 1, a2, &cmp);
        P("RS_CMP1 %08lx %lu %ld %ld\n", hr, cmp, last_cb1, last_cb2);
        hr = IRowsetLocate_Compare(loc, DB_NULL_HCHAPTER, 2, b1, 4, b2, &cmp);
        P("RS_CMP2 %08lx %lu %ld %ld\n", hr, cmp, last_cb1, last_cb2);
        hr = IRowsetLocate_Compare(loc, DB_NULL_HCHAPTER, 1, c1, 3, c2, &cmp);
        P("RS_CMP3 %08lx %lu %ld %ld\n", hr, cmp, last_cb1, last_cb2);

        hr = IRowsetLocate_GetRowsByBookmark(loc, DB_NULL_HCHAPTER, 3, cbs, bms, rows, st);
        P("RS_GRBB %08lx %Iu %Iu %Iu %lu %lu %lu\n", hr, rows[0], rows[1], rows[2], st[0], st[1], st[2]);
        memset(rows, 0, sizeof(rows));
        hr = IRowsetLocate_GetRowsByBookmark(loc, DB_NULL_HCHAPTER, 3, cbs, bms, rows, NULL);
        P("RS_GRBB_NOST %08lx %Iu %Iu %Iu\n", hr, rows[0], rows[1], rows[2]);
        bad[0] = kbad;
        memset(st, 0, sizeof(st));
        hr = IRowsetLocate_GetRowsByBookmark(loc, DB_NULL_HCHAPTER, 3, cbs_bad, bad, rows, st);
        P("RS_GRBB_BAD %08lx %Iu %lu %lu\n", hr, rows[0], st[0], st[1]);
        P("RS_GRBB_ZERO %08lx\n", IRowsetLocate_GetRowsByBookmark(loc, DB_NULL_HCHAPTER, 0, NULL, NULL, NULL, NULL));

        hr = IRowsetLocate_Hash(loc, DB_NULL_HCHAPTER, 3, cbs, bms, hv, st);
        P("RS_HASH %08lx %Iu %Iu %Iu %lu\n", hr, hv[0], hv[1], hv[2], st[2]);
        memset(hv, 0, sizeof(hv));
        hr = IRowsetLocate_Hash(loc, DB_NULL_HCHAPTER, 3, cbs, bms, hv, NULL);
        P("RS_HASH_NOST %08lx %Iu %Iu %Iu\n", hr, hv[0], hv[1], hv[2]);
    }
    if (acc)
    {
        DBREFCOUNT c = 0;
        hr = IAccessor_AddRefAccessor(acc, 5, &c);
        P("RS_ACC %08lx %lu\n", hr, c);
        c = 77;
        hr = IAccessor_AddRefAccessor(acc, 0, &c);
        P("RS_ACC_BAD %08lx %lu\n", hr, c);
        P("RS_ACC_NULL %08lx\n", IAccessor_AddRefAccessor(acc, 8, NULL));
    }
    if (info)
    {
        IUnknown *u = NULL;
        IRowsetLocate *l2 = NULL;
        DBCOMPARE cmp = 99;
        BYTE x[] = {4};

        hr = IRowsetInfo_GetReferencedRowset(info, 1, &IID_IRowsetLocate, &u);
        P("RS_REF %08lx %d\n", hr, u != NULL);
        if (u)
        {
            l2 = (IRowsetLocate *)u;
            hr = IRowsetLocate_Compare(l2, DB_NULL_HCHAPTER, 1, x, 1, x, &cmp);
            P("RS_REF_CMP %08lx %lu %ld\n", hr, cmp, last_rs);
            IRowsetLocate_Release(l2);
        }
        u = (IUnknown *)1;
        hr = IRowsetInfo_GetReferencedRowset(info, 2, &IID_IRowsetLocate, &u);
        P("RS_REF_BAD %08lx %d\n", hr, u != NULL);
        u = NULL;
        hr = IRowsetInfo_GetSpecification(info, &IID_IOpenRowset, &u);
        P("RS_SPEC %08lx %d\n", hr, u != NULL);
        if (u)
        {
            IUnknown *rs2 = NULL;
            DBID t;
            t.eKind = DBKIND_NAME; t.uName.pwszName = (LPOLESTR)L"rs";
            hr = IOpenRowset_OpenRowset((IOpenRowset *)u, NULL, &t, NULL, &IID_IUnknown, 0, NULL, &rs2);
            P("RS_SPEC_USE %08lx %d\n", hr, rs2 != NULL);
            if (rs2) IUnknown_Release(rs2);
            IUnknown_Release(u);
        }
    }
    if (br)
    {
        DBBINDURLSTATUS bst = 0;
        hr = IBindResource_Bind(br, NULL, L"x:row", 0, &g_row, &IID_IRow, NULL, NULL, &bst, &unk);
        P("BIND_ROW %08lx %d\n", hr, unk != NULL);
        if (unk) { row = (IRow *)unk; }
    }
    if (row)
    {
        IUnknown *u = NULL;
        HROW h = 0;
        IRowsetLocate *l = NULL;
        IRow *row2 = NULL;
        DBCOMPARE cmp = 99;
        BYTE x[] = {4};

        hr = IRow_GetSourceRowset(row, &IID_IRowsetLocate, &u, &h);
        P("ROW_SRC %08lx %d %Iu\n", hr, u != NULL, h);
        if (u)
        {
            l = (IRowsetLocate *)u;
            hr = IRowsetLocate_Compare(l, DB_NULL_HCHAPTER, 1, x, 1, x, &cmp);
            P("ROW_SRC_CMP %08lx %lu %ld\n", hr, cmp, last_rs);
            {
                BYTE k1[] = {7}, k2[] = {1, 2, 3, 4};
                const BYTE *bms[2] = {k1, k2};
                const DBBKMARK cbs[2] = {1, 4};
                HROW rows[2] = {0};
                hr = IRowsetLocate_GetRowsByBookmark(l, DB_NULL_HCHAPTER, 2, cbs, bms, rows, NULL);
                P("ROW_SRC_GRBB %08lx %Iu %Iu\n", hr, rows[0], rows[1]);
            }
            IRowsetLocate_Release(l);
        }
        h = 0;
        hr = IRow_GetSourceRowset(row, &IID_IRowsetLocate, NULL, &h);
        P("ROW_SRC_NOPTR %08lx %Iu\n", hr, h);
        u = NULL;
        hr = IRow_GetSourceRowset(row, &IID_IRowsetLocate, &u, NULL);
        P("ROW_SRC_NOHROW %08lx %d\n", hr, u != NULL);
        if (u) IRowsetLocate_Release((IRowsetLocate *)u);
        u = (IUnknown *)1;
        hr = IRow_GetSourceRowset(row, &IID_IRowsetInfo, &u, &h);
        P("ROW_SRC_INFO %08lx %d\n", hr, u != NULL && u != (IUnknown *)1);
        if (u && u != (IUnknown *)1) IUnknown_Release(u);

        hr = IRow_Open(row, NULL, &col, &g_row, 0, &IID_IRow, (IUnknown **)&row2);
        P("ROW_OPEN_ROW %08lx %d\n", hr, row2 != NULL);
        if (row2)
        {
            u = NULL;
            hr = IRow_GetSourceRowset(row2, &IID_IRowsetLocate, &u, &h);
            P("ROW_OPEN_ROW_SRC %08lx %d\n", hr, u != NULL);
            if (u) IUnknown_Release(u);
            IRow_Release(row2);
        }
        u = NULL;
        hr = IRow_Open(row, NULL, &col, &g_rowset, 0, &IID_IRowsetLocate, &u);
        P("ROW_OPEN_RS %08lx %d\n", hr, u != NULL);
        if (u) IUnknown_Release(u);
        u = NULL;
        saw_outer = 7;
        hr = IRow_Open(row, NULL, &col, &g_stream, 0, &IID_IOpenRowset, &u);
        P("ROW_OPEN_STREAM %08lx %d %ld\n", hr, u != NULL, saw_outer);
        if (u)
        {
            IUnknown *r2 = NULL;
            DBID t;
            t.eKind = DBKIND_NAME; t.uName.pwszName = (LPOLESTR)L"rs";
            hr = IOpenRowset_OpenRowset((IOpenRowset *)u, NULL, &t, NULL, &IID_IUnknown, 0, NULL, &r2);
            P("ROW_OPEN_STREAM_USE %08lx %d\n", hr, r2 != NULL);
            if (r2) IUnknown_Release(r2);
            IUnknown_Release(u);
        }
    }
    if (br)
    {
        DBBINDURLSTATUS bst = 0;
        IUnknown *u = NULL;
        saw_outer = 7;
        hr = IBindResource_Bind(br, NULL, L"x:other", 0, &g_other, &IID_IOpenRowset, NULL, NULL, &bst, &u);
        P("BIND_OTHER %08lx %d %ld\n", hr, u != NULL, saw_outer);
        if (u) IUnknown_Release(u);
        u = NULL;
        hr = IBindResource_Bind(br, NULL, L"x:rs", 0, &g_rowset, &IID_IRowsetLocate, NULL, NULL, &bst, &u);
        P("BIND_ROWSET %08lx %d\n", hr, u != NULL);
        if (u) IUnknown_Release(u);
    }
    {   /* ICommand::Execute with parameters */
        ICommand *cmd = get(2, &IID_ICommand);
        IAccessor *cacc = get(3, &IID_IAccessor);
        if (cmd && cacc)
        {
            struct P rows[2];
            DBBINDING bd[10];
            HACCESSOR h = 0;
            DBROWCOUNT aff = 0;
            DBPARAMS pp;
            IStream *st = NULL;
            DBCOUNTITEM n = 0;
            LARGE_INTEGER zero = {{0}};
            char hello[8] = {0};
            ULONG got = 0;
            int k;
            static WCHAR ref0[] = L"byref-zero", ref1[] = L"byref-one!";

#define BIND(idx, ord, member, part, io, type, maxlen) do { memset(&bd[idx], 0, sizeof(bd[idx])); bd[idx].iOrdinal = ord; \
    bd[idx].obValue = offsetof(struct P, member); bd[idx].dwPart = part; bd[idx].eParamIO = io; bd[idx].wType = type; \
    bd[idx].cbMaxLen = maxlen; bd[idx].dwMemOwner = DBMEMOWNER_CLIENTOWNED; } while (0)
            BIND(0, 1, i, DBPART_VALUE, DBPARAMIO_INPUT, DBTYPE_I4, 4);
            BIND(1, 2, b, DBPART_VALUE, DBPARAMIO_INPUT, DBTYPE_BSTR, sizeof(BSTR));
            BIND(2, 3, v, DBPART_VALUE, DBPARAMIO_INPUT, DBTYPE_VARIANT, sizeof(VARIANT));
            BIND(3, 4, wref, DBPART_VALUE | DBPART_LENGTH, DBPARAMIO_INPUT, DBTYPE_WSTR | DBTYPE_BYREF, sizeof(void *));
            bd[3].obLength = offsetof(struct P, wlen);
            BIND(4, 5, unk, DBPART_VALUE, DBPARAMIO_INPUT, DBTYPE_IUNKNOWN, sizeof(void *));
            BIND(5, 6, out_i, DBPART_VALUE | DBPART_STATUS, DBPARAMIO_OUTPUT, DBTYPE_I4, 4);
            bd[5].obStatus = offsetof(struct P, out_i_st);
            BIND(6, 7, out_b, DBPART_VALUE, DBPARAMIO_OUTPUT, DBTYPE_BSTR, sizeof(BSTR));
            BIND(7, 8, out_ref, DBPART_VALUE | DBPART_LENGTH, DBPARAMIO_OUTPUT, DBTYPE_WSTR | DBTYPE_BYREF, sizeof(void *));
            bd[7].obLength = offsetof(struct P, out_len);
            BIND(8, 9, io_i, DBPART_VALUE, DBPARAMIO_INPUT | DBPARAMIO_OUTPUT, DBTYPE_I4, 4);
            BIND(9, 10, io_b, DBPART_VALUE, DBPARAMIO_INPUT | DBPARAMIO_OUTPUT, DBTYPE_BSTR, sizeof(BSTR));

            CreateStreamOnHGlobal(NULL, TRUE, &st);
            memset(rows, 0, sizeof(rows));
            rows[0].i = 21; rows[0].b = SysAllocString(L"first");
            V_VT(&rows[0].v) = VT_I4; V_I4(&rows[0].v) = 77;
            rows[0].wref = ref0; rows[0].wlen = wcslen(ref0) * sizeof(WCHAR);
            rows[0].unk = (IUnknown *)st;
            rows[0].io_i = 5; rows[0].io_b = SysAllocString(L"inout");
            rows[1].i = 8; rows[1].b = SysAllocString(L"second-bstr");
            V_VT(&rows[1].v) = VT_BSTR; V_BSTR(&rows[1].v) = SysAllocString(L"vbstr");
            rows[1].wref = ref1; rows[1].wlen = wcslen(ref1) * sizeof(WCHAR);
            rows[1].io_i = 6; rows[1].io_b = SysAllocString(L"two");

            hr = IAccessor_CreateAccessor(cacc, DBACCESSOR_PARAMETERDATA, 10, bd, sizeof(struct P), &h, NULL);
            P("CMD_ACC %08lx %Iu\n", hr, h);
            pp.pData = rows; pp.cParamSets = 2; pp.hAccessor = h;
            hr = ICommand_Execute(cmd, NULL, &IID_IUnknown, &pp, &aff, NULL);
            P("CMD_EXEC %08lx %Id sets %ld\n", hr, aff, exec_seen_sets);
            for (k = 0; k < 2; k++)
            {
                char a[40], b[40], c[40];
                WideCharToMultiByte(CP_UTF8, 0, exec_in_b[k], -1, a, sizeof(a), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, exec_in_w[k], -1, b, sizeof(b), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, exec_in_v[k], -1, c, sizeof(c), NULL, NULL);
                P("CMD_IN%d %ld %s %s %ld %s\n", k, exec_in_i[k], a, b, exec_in_wlen[k], c);
            }
            for (k = 0; k < 2; k++)
            {
                char a[40], b[40];
                WideCharToMultiByte(CP_UTF8, 0, rows[k].out_b ? rows[k].out_b : L"(null)", -1, a, sizeof(a), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, rows[k].out_ref ? rows[k].out_ref : L"(null)", -1, b, sizeof(b), NULL, NULL);
                P("CMD_OUT%d %ld %lu %s %s %Iu %ld\n", k, rows[k].out_i, rows[k].out_i_st, a, b, rows[k].out_len, rows[k].io_i);
            }
            {
                char a[40];
                WideCharToMultiByte(CP_UTF8, 0, rows[0].io_b ? rows[0].io_b : L"(null)", -1, a, sizeof(a), NULL, NULL);
                P("CMD_IO0 %s\n", a);
                WideCharToMultiByte(CP_UTF8, 0, rows[1].io_b ? rows[1].io_b : L"(null)", -1, a, sizeof(a), NULL, NULL);
                P("CMD_IO1 %s\n", a);
            }
            P("CMD_STREAM %ld\n", exec_stream_ok);
            IStream_Seek(st, zero, STREAM_SEEK_SET, NULL);
            IStream_Read(st, hello, 5, &got);
            P("CMD_STREAM_DATA %lu %s\n", got, hello);
            /* no parameters, and no rowset requested */
            exec_called = 0;
            hr = ICommand_Execute(cmd, NULL, &IID_IUnknown, NULL, &aff, NULL);
            P("CMD_NOPARAM %08lx %ld\n", hr, exec_called);
            pp.hAccessor = 55;
            hr = ICommand_Execute(cmd, NULL, &IID_IUnknown, &pp, &aff, NULL);
            P("CMD_BADACC %08lx\n", hr);
            (void)n;
        }
    }
    SetEvent(done);
    WaitForSingleObject(th, 10000);
    P("DONE\n");
    return 0;
}
