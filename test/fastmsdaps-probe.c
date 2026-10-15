/* msdaps: OLE DB proxies and stubs for interfaces whose methods are not remotable as written
 * (run by fastmsdaps-gate.sh). A provider living in an MTA thread is called through proxies from the
 * main thread. Prints "KEY value" lines. */
#define COBJMACROS
#define CONST_VTABLE
#include <windows.h>
#include <ole2.h>
#include <oleauto.h>
#include <oledb.h>
#include <oledberr.h>
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#include <stdio.h>
#include <wchar.h>

static const GUID my_dbinit = {0xc8b522bc,0x5cf3,0x11ce,{0xad,0xe5,0x00,0xaa,0x00,0x44,0x77,0x3d}};
static const GUID my_rowset = {0xc8b522be,0x5cf3,0x11ce,{0xad,0xe5,0x00,0xaa,0x00,0x44,0x77,0x3d}};
static const IID my_null_iid;

#define P(...) do { printf(__VA_ARGS__); fflush(stdout); } while (0)

/* the objects are static; one generic IUnknown */
struct obj { const void *vtbl; const IID *iid; };
static HRESULT WINAPI u_qi(void *iface, REFIID riid, void **out)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, ((struct obj *)iface)->iid)) { *out = iface; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI u_addref(void *iface) { return 2; }
static ULONG WINAPI u_release(void *iface) { return 1; }

static const WCHAR name_id[] = L"id", name_name[] = L"name";

/* IColumnsInfo */
static HRESULT WINAPI ci_GetColumnInfo(IColumnsInfo *i, DBORDINAL *n, DBCOLUMNINFO **info, OLECHAR **strings)
{
    DBCOLUMNINFO *c = CoTaskMemAlloc(2 * sizeof(*c));
    OLECHAR *buf = CoTaskMemAlloc(64 * sizeof(OLECHAR));
    memset(c, 0, 2 * sizeof(*c));
    wcscpy(buf, L"id");                 /* offset 0: "id" (3 chars) */
    wcscpy(buf + 3, L"name");           /* offset 6 bytes */
    wcscpy(buf + 8, L"idcol");          /* column id name, offset 16 bytes */
    c[0].pwszName = buf; c[0].iOrdinal = 1; c[0].wType = DBTYPE_I4;
    c[0].columnid.eKind = DBKIND_NAME; c[0].columnid.uName.pwszName = buf + 8;
    c[1].pwszName = buf + 3; c[1].iOrdinal = 2; c[1].wType = DBTYPE_WSTR;
    c[1].columnid.eKind = DBKIND_PROPID; c[1].columnid.uName.ulPropid = 77;
    *n = 2; *info = c; *strings = buf;
    return S_OK;
}
static HRESULT WINAPI ci_MapColumnIDs(IColumnsInfo *i, DBORDINAL n, const DBID *ids, DBORDINAL *cols)
{
    DBORDINAL k;
    for (k = 0; k < n; k++) cols[k] = ids[k].eKind == DBKIND_PROPID ? ids[k].uName.ulPropid : 0;
    return n == 3 ? DB_S_ERRORSOCCURRED : S_OK;
}
static const IColumnsInfoVtbl ci_vtbl = { (void *)u_qi, (void *)u_addref, (void *)u_release, ci_GetColumnInfo, ci_MapColumnIDs };
static struct obj ci_obj = { &ci_vtbl, &IID_IColumnsInfo };

/* IAccessor: only GetBindings */
static HRESULT WINAPI ac_nope(void) { return E_NOTIMPL; }
static HRESULT WINAPI ac_GetBindings(IAccessor *i, HACCESSOR h, DBACCESSORFLAGS *flags, DBCOUNTITEM *n, DBBINDING **b)
{
    DBBINDING *x;
    if (h != (HACCESSOR)0x42) return DB_E_BADACCESSORHANDLE;
    x = CoTaskMemAlloc(2 * sizeof(*x));
    memset(x, 0, 2 * sizeof(*x));
    x[0].iOrdinal = 1; x[0].obValue = 8; x[0].wType = DBTYPE_I4; x[0].dwPart = DBPART_VALUE;
    x[1].iOrdinal = 2; x[1].obValue = 16; x[1].cbMaxLen = 20; x[1].wType = DBTYPE_WSTR; x[1].dwPart = DBPART_VALUE | DBPART_LENGTH;
    *flags = DBACCESSOR_ROWDATA; *n = 2; *b = x;
    return S_OK;
}
static const IAccessorVtbl ac_vtbl = { (void *)u_qi, (void *)u_addref, (void *)u_release, (void *)ac_nope, (void *)ac_nope, ac_GetBindings, (void *)ac_nope };
static struct obj ac_obj = { &ac_vtbl, &IID_IAccessor };

/* IDBProperties: GetPropertyInfo (descriptions in one buffer) */
static HRESULT WINAPI dp_nope(void) { return E_NOTIMPL; }
static HRESULT WINAPI dp_GetPropertyInfo(IDBProperties *i, ULONG n, const DBPROPIDSET *ids, ULONG *sets, DBPROPINFOSET **info, OLECHAR **desc)
{
    DBPROPINFOSET *s = CoTaskMemAlloc(sizeof(*s));
    DBPROPINFO *p = CoTaskMemAlloc(3 * sizeof(*p));
    OLECHAR *buf = CoTaskMemAlloc(40 * sizeof(OLECHAR));
    memset(p, 0, 3 * sizeof(*p));
    wcscpy(buf, L"Data Source");        /* 12 chars */
    wcscpy(buf + 12, L"Password");      /* 9 chars */
    p[0].pwszDescription = buf; p[0].dwPropertyID = 1; p[0].vtType = VT_BSTR;
    p[1].pwszDescription = NULL; p[1].dwPropertyID = 2; p[1].vtType = VT_I4;
    p[2].pwszDescription = buf + 12; p[2].dwPropertyID = 3; p[2].vtType = VT_BSTR;
    s->rgPropertyInfos = p; s->cPropertyInfos = 3; s->guidPropertySet = my_dbinit;
    *sets = 1; *info = s; *desc = buf;
    return S_OK;
}
static const IDBPropertiesVtbl dp_vtbl = { (void *)u_qi, (void *)u_addref, (void *)u_release, (void *)dp_nope, dp_GetPropertyInfo, (void *)dp_nope };
static struct obj dp_obj = { &dp_vtbl, &IID_IDBProperties };

/* ICommandWithParameters */
static HRESULT WINAPI cp_GetParameterInfo(ICommandWithParameters *i, DB_UPARAMS *n, DBPARAMINFO **info, OLECHAR **names)
{
    DBPARAMINFO *p = CoTaskMemAlloc(2 * sizeof(*p));
    OLECHAR *buf = CoTaskMemAlloc(30 * sizeof(OLECHAR));
    memset(p, 0, 2 * sizeof(*p));
    wcscpy(buf, L"@first"); wcscpy(buf + 7, L"@second");
    p[0].pwszName = buf; p[0].iOrdinal = 1; p[0].wType = DBTYPE_I4; p[0].dwFlags = DBPARAMFLAGS_ISINPUT;
    p[1].pwszName = buf + 7; p[1].iOrdinal = 2; p[1].wType = DBTYPE_WSTR; p[1].ulParamSize = 40;
    *n = 2; *info = p; *names = buf;
    return S_OK;
}
static HRESULT WINAPI cp_MapParameterNames(ICommandWithParameters *i, DB_UPARAMS n, LPCWSTR names[], DB_LPARAMS ord[])
{
    DB_UPARAMS k;
    for (k = 0; k < n; k++) ord[k] = !wcscmp(names[k], L"@first") ? 1 : !wcscmp(names[k], L"@second") ? 2 : 0;
    return S_OK;
}
static HRESULT WINAPI cp_SetParameterInfo(ICommandWithParameters *i, DB_UPARAMS n, const DB_UPARAMS ord[], const DBPARAMBINDINFO b[])
{
    return n == 2 && ord[1] == 9 && b[1].ulParamSize == 33 ? S_OK : E_INVALIDARG;
}
static const ICommandWithParametersVtbl cp_vtbl = { (void *)u_qi, (void *)u_addref, (void *)u_release, cp_GetParameterInfo, cp_MapParameterNames, cp_SetParameterInfo };
static struct obj cp_obj = { &cp_vtbl, &IID_ICommandWithParameters };

/* ICommandProperties */
static HRESULT WINAPI cprop_GetProperties(ICommandProperties *i, const ULONG n, const DBPROPIDSET ids[], ULONG *sets, DBPROPSET **ps)
{
    DBPROPSET *s = CoTaskMemAlloc(sizeof(*s));
    DBPROP *p = CoTaskMemAlloc(sizeof(*p));
    memset(p, 0, sizeof(*p));
    p->dwPropertyID = 5; p->dwStatus = DBPROPSTATUS_OK; V_VT(&p->vValue) = VT_I4; V_I4(&p->vValue) = 1234;
    s->rgProperties = p; s->cProperties = 1; s->guidPropertySet = my_rowset;
    *sets = 1; *ps = s;
    return S_OK;
}
static HRESULT WINAPI cprop_SetProperties(ICommandProperties *i, ULONG n, DBPROPSET ps[])
{
    ULONG s, k;
    for (s = 0; s < n; s++)
        for (k = 0; k < ps[s].cProperties; k++)
            ps[s].rgProperties[k].dwStatus = ps[s].rgProperties[k].dwPropertyID == 99 ? DBPROPSTATUS_NOTSUPPORTED : DBPROPSTATUS_OK;
    return DB_S_ERRORSOCCURRED;
}
static const ICommandPropertiesVtbl cprop_vtbl = { (void *)u_qi, (void *)u_addref, (void *)u_release, cprop_GetProperties, cprop_SetProperties };
static struct obj cprop_obj = { &cprop_vtbl, &IID_ICommandProperties };

/* IRowsetIdentity / IConvertType / ICommandPrepare / ITransactionLocal / IRowPosition / IMultipleResults / IOpenRowset */
static HRESULT WINAPI ri_IsSameRow(IRowsetIdentity *i, HROW a, HROW b) { return a == b ? S_OK : S_FALSE; }
static const IRowsetIdentityVtbl ri_vtbl = { (void *)u_qi, (void *)u_addref, (void *)u_release, ri_IsSameRow };
static struct obj ri_obj = { &ri_vtbl, &IID_IRowsetIdentity };

static HRESULT WINAPI ct_CanConvert(IConvertType *i, DBTYPE from, DBTYPE to, DBCONVERTFLAGS f)
{
    return from == DBTYPE_I4 && to == DBTYPE_WSTR && f == DBCONVERTFLAGS_COLUMN ? S_OK : S_FALSE;
}
static const IConvertTypeVtbl ct_vtbl = { (void *)u_qi, (void *)u_addref, (void *)u_release, ct_CanConvert };
static struct obj ct_obj = { &ct_vtbl, &IID_IConvertType };

static HRESULT WINAPI cprep_Prepare(ICommandPrepare *i, ULONG runs)
{
    ICreateErrorInfo *cei;
    IErrorInfo *ei;
    if (runs != 7) return S_OK;
    CreateErrorInfo(&cei);
    ICreateErrorInfo_SetDescription(cei, (LPOLESTR)L"prepare failed");
    ICreateErrorInfo_SetSource(cei, (LPOLESTR)L"fakeprov");
    ICreateErrorInfo_QueryInterface(cei, &IID_IErrorInfo, (void **)&ei);
    SetErrorInfo(0, ei);
    IErrorInfo_Release(ei);
    ICreateErrorInfo_Release(cei);
    return E_FAIL;
}
static HRESULT WINAPI cprep_Unprepare(ICommandPrepare *i) { return DB_S_NORESULT; }
static const ICommandPrepareVtbl cprep_vtbl = { (void *)u_qi, (void *)u_addref, (void *)u_release, cprep_Prepare, cprep_Unprepare };
static struct obj cprep_obj = { &cprep_vtbl, &IID_ICommandPrepare };

static HRESULT WINAPI tl_nope(void) { return E_NOTIMPL; }
static HRESULT WINAPI tl_Start(ITransactionLocal *i, ISOLEVEL level, ULONG flags, ITransactionOptions *o, ULONG *tl)
{
    if (tl) *tl = 3;
    return level == ISOLATIONLEVEL_READCOMMITTED ? S_OK : XACT_E_ISOLATIONLEVEL;
}
static const ITransactionLocalVtbl tl_vtbl = { (void *)u_qi, (void *)u_addref, (void *)u_release, (void *)tl_nope, (void *)tl_nope, (void *)tl_nope, (void *)tl_nope, tl_Start };
static struct obj tl_obj = { &tl_vtbl, &IID_ITransactionLocal };

static HRESULT WINAPI rp_nope(void) { return E_NOTIMPL; }
static HRESULT WINAPI rp_GetRowPosition(IRowPosition *i, HCHAPTER *c, HROW *r, DBPOSITIONFLAGS *f) { *c = 5; *r = 9; *f = DBPOSITION_OK; return S_OK; }
static HRESULT WINAPI rp_SetRowPosition(IRowPosition *i, HCHAPTER c, HROW r, DBPOSITIONFLAGS f) { return c == 6 && r == 10 && f == DBPOSITION_EOF ? S_OK : E_INVALIDARG; }
static HRESULT WINAPI rp_Clear(IRowPosition *i) { return DB_S_NORESULT; }
static const IRowPositionVtbl rp_vtbl = { (void *)u_qi, (void *)u_addref, (void *)u_release, rp_Clear, rp_GetRowPosition, (void *)rp_nope, (void *)rp_nope, rp_SetRowPosition };
static struct obj rp_obj = { &rp_vtbl, &IID_IRowPosition };

static HRESULT WINAPI mr_GetResult(IMultipleResults *i, IUnknown *outer, DBRESULTFLAG f, REFIID riid, DBROWCOUNT *affected, IUnknown **rs)
{
    if (rs) *rs = NULL;
    if (affected) *affected = 41 + f;
    return S_OK;
}
static const IMultipleResultsVtbl mr_vtbl = { (void *)u_qi, (void *)u_addref, (void *)u_release, mr_GetResult };
static struct obj mr_obj = { &mr_vtbl, &IID_IMultipleResults };

static HRESULT WINAPI or_OpenRowset(IOpenRowset *i, IUnknown *outer, DBID *table, DBID *index, REFIID riid, ULONG n, DBPROPSET ps[], IUnknown **rs)
{
    ULONG s, k;
    for (s = 0; s < n; s++)
        for (k = 0; k < ps[s].cProperties; k++)
            ps[s].rgProperties[k].dwStatus = ps[s].rgProperties[k].dwPropertyID == 99 ? DBPROPSTATUS_NOTSUPPORTED : DBPROPSTATUS_OK;
    if (!table || table->eKind != DBKIND_NAME || wcscmp(table->uName.pwszName, L"tbl")) return DB_E_NOTABLE;
    *rs = (IUnknown *)&ri_obj;
    return n ? DB_S_ERRORSOCCURRED : S_OK;
}
static const IOpenRowsetVtbl or_vtbl = { (void *)u_qi, (void *)u_addref, (void *)u_release, or_OpenRowset };
static struct obj or_obj = { &or_vtbl, &IID_IOpenRowset };

static const struct { const IID *iid; void *obj; } provided[] =
{
    { &IID_IColumnsInfo, &ci_obj }, { &IID_IAccessor, &ac_obj }, { &IID_IDBProperties, &dp_obj },
    { &IID_ICommandWithParameters, &cp_obj }, { &IID_ICommandProperties, &cprop_obj }, { &IID_IRowsetIdentity, &ri_obj },
    { &IID_IConvertType, &ct_obj }, { &IID_ICommandPrepare, &cprep_obj }, { &IID_ITransactionLocal, &tl_obj },
    { &IID_IRowPosition, &rp_obj }, { &IID_IMultipleResults, &mr_obj }, { &IID_IOpenRowset, &or_obj },
};
static IStream *streams[ARRAY_SIZE(provided)];
static HANDLE ready, done;

static DWORD WINAPI provider_thread(void *arg)
{
    unsigned int i;
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    for (i = 0; i < ARRAY_SIZE(provided); i++)
    {
        HRESULT hr = CoMarshalInterThreadInterfaceInStream(provided[i].iid, (IUnknown *)provided[i].obj, &streams[i]);
        if (FAILED(hr)) P("MARSHAL%u %08lx\n", i, hr);
    }
    SetEvent(ready);
    WaitForSingleObject(done, 60000);
    CoUninitialize();
    return 0;
}

static void *get(unsigned int i)
{
    void *p = NULL;
    HRESULT hr;
    if (!streams[i]) return NULL;
    hr = CoGetInterfaceAndReleaseStream(streams[i], provided[i].iid, &p);
    if (FAILED(hr)) P("UNMARSHAL%u %08lx\n", i, hr);
    return p;
}

static const char *u8(const WCHAR *w, char *buf)
{
    if (!w) return "(null)";
    WideCharToMultiByte(CP_UTF8, 0, w, -1, buf, 100, NULL, NULL);
    return buf;
}

int main(void)
{
    HANDLE th;
    char b1[128], b2[128];
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    ready = CreateEventW(NULL, TRUE, FALSE, NULL);
    done = CreateEventW(NULL, TRUE, FALSE, NULL);
    th = CreateThread(NULL, 0, provider_thread, NULL, 0, NULL);
    WaitForSingleObject(ready, 30000);

    {   /* IColumnsInfo: names travel as offsets and come back as pointers into one buffer */
        IColumnsInfo *ci = get(0);
        DBORDINAL n = 0, cols[3] = {0};
        DBCOLUMNINFO *info = NULL;
        OLECHAR *strings = NULL;
        DBID ids[3];
        if (ci)
        {
            hr = IColumnsInfo_GetColumnInfo(ci, &n, &info, &strings);
            P("CI_GET %08lx %Iu\n", hr, n);
            if (SUCCEEDED(hr) && n == 2)
            {
                P("CI_NAMES %s %s\n", u8(info[0].pwszName, b1), u8(info[1].pwszName, b2));
                P("CI_IDNAME %d %s\n", info[0].columnid.eKind, u8(info[0].columnid.uName.pwszName, b1));
                P("CI_IDPROP %d %lu\n", info[1].columnid.eKind, info[1].columnid.uName.ulPropid);
                P("CI_SHARED %d\n", info[0].pwszName == strings && info[1].pwszName == strings + 3);
                P("CI_TYPES %u %u %Iu\n", info[0].wType, info[1].wType, info[1].iOrdinal);
            }
            ids[0].eKind = DBKIND_PROPID; ids[0].uName.ulPropid = 11;
            ids[1].eKind = DBKIND_PROPID; ids[1].uName.ulPropid = 22;
            ids[2].eKind = DBKIND_PROPID; ids[2].uName.ulPropid = 33;
            hr = IColumnsInfo_MapColumnIDs(ci, 2, ids, cols);
            P("CI_MAP %08lx %Iu %Iu\n", hr, cols[0], cols[1]);
            hr = IColumnsInfo_MapColumnIDs(ci, 3, ids, cols);
            P("CI_MAP3 %08lx\n", hr);
        }
    }
    {   /* IAccessor::GetBindings */
        IAccessor *ac = get(1);
        DBACCESSORFLAGS flags = 0;
        DBCOUNTITEM n = 0;
        DBBINDING *b = NULL;
        if (ac)
        {
            hr = IAccessor_GetBindings(ac, (HACCESSOR)0x42, &flags, &n, &b);
            P("AC_GET %08lx %lu %Iu\n", hr, flags, n);
            if (SUCCEEDED(hr) && n == 2)
                P("AC_B %Iu %Iu %u|%Iu %Iu %u %lu\n", b[0].iOrdinal, b[0].obValue, b[0].wType, b[1].iOrdinal, b[1].obValue, b[1].wType, b[1].dwPart);
            hr = IAccessor_GetBindings(ac, (HACCESSOR)0x43, &flags, &n, &b);
            P("AC_BAD %08lx\n", hr);
        }
    }
    {   /* IDBProperties::GetPropertyInfo: descriptions as offsets, a NULL one stays NULL */
        IDBProperties *dp = get(2);
        ULONG sets = 0;
        DBPROPINFOSET *info = NULL;
        OLECHAR *desc = NULL;
        if (dp)
        {
            hr = IDBProperties_GetPropertyInfo(dp, 0, NULL, &sets, &info, &desc);
            P("DP_GET %08lx %lu\n", hr, sets);
            if (SUCCEEDED(hr) && sets == 1 && info[0].cPropertyInfos == 3)
            {
                P("DP_DESC %s|%s|%s\n", u8(info[0].rgPropertyInfos[0].pwszDescription, b1),
                  info[0].rgPropertyInfos[1].pwszDescription ? "set" : "(null)", u8(info[0].rgPropertyInfos[2].pwszDescription, b2));
                P("DP_IDS %lu %lu %lu\n", info[0].rgPropertyInfos[0].dwPropertyID, info[0].rgPropertyInfos[1].dwPropertyID, info[0].rgPropertyInfos[2].dwPropertyID);
                P("DP_SHARED %d\n", info[0].rgPropertyInfos[0].pwszDescription == desc && info[0].rgPropertyInfos[2].pwszDescription == desc + 12);
            }
        }
    }
    {   /* ICommandWithParameters */
        ICommandWithParameters *cp = get(3);
        DB_UPARAMS n = 0, ords[2] = { 4, 9 };
        DBPARAMINFO *info = NULL;
        OLECHAR *names = NULL;
        LPCWSTR nm[3] = { L"@second", L"@first", L"@zzz" };
        DB_LPARAMS mapped[3] = {0};
        DBPARAMBINDINFO bi[2];
        if (cp)
        {
            hr = ICommandWithParameters_GetParameterInfo(cp, &n, &info, &names);
            P("CP_GET %08lx %Iu\n", hr, n);
            if (SUCCEEDED(hr) && n == 2)
                P("CP_NAMES %s %s %u %Iu\n", u8(info[0].pwszName, b1), u8(info[1].pwszName, b2), info[1].wType, info[1].ulParamSize);
            hr = ICommandWithParameters_MapParameterNames(cp, 3, nm, mapped);
            P("CP_MAP %08lx %Id %Id %Id\n", hr, mapped[0], mapped[1], mapped[2]);
            memset(bi, 0, sizeof(bi));
            bi[1].ulParamSize = 33;
            hr = ICommandWithParameters_SetParameterInfo(cp, 2, ords, bi);
            P("CP_SET %08lx\n", hr);
        }
    }
    {   /* ICommandProperties: the property statuses come back */
        ICommandProperties *cp = get(4);
        DBPROP props[2];
        DBPROPSET set;
        ULONG n = 0;
        DBPROPSET *got = NULL;
        if (cp)
        {
            memset(props, 0, sizeof(props));
            props[0].dwPropertyID = 5; props[1].dwPropertyID = 99;
            V_VT(&props[0].vValue) = VT_I4; V_VT(&props[1].vValue) = VT_I4;
            set.rgProperties = props; set.cProperties = 2; set.guidPropertySet = my_rowset;
            props[0].dwStatus = 777; props[1].dwStatus = 777;
            hr = ICommandProperties_SetProperties(cp, 1, &set);
            P("CPR_SET %08lx %lu %lu\n", hr, props[0].dwStatus, props[1].dwStatus);
            hr = ICommandProperties_GetProperties(cp, 0, NULL, &n, &got);
            P("CPR_GET %08lx %lu\n", hr, n);
            if (SUCCEEDED(hr) && n == 1) P("CPR_VAL %lu %ld\n", got[0].rgProperties[0].dwPropertyID, V_I4(&got[0].rgProperties[0].vValue));
        }
    }
    {
        IRowsetIdentity *ri = get(5);
        IConvertType *ct = get(6);
        ICommandPrepare *pr = get(7);
        ITransactionLocal *tl = get(8);
        IRowPosition *rp = get(9);
        IMultipleResults *mr = get(10);
        IOpenRowset *orow = get(11);
        if (ri) P("RI %08lx %08lx\n", IRowsetIdentity_IsSameRow(ri, 3, 3), IRowsetIdentity_IsSameRow(ri, 3, 4));
        if (ct) P("CT %08lx %08lx\n", IConvertType_CanConvert(ct, DBTYPE_I4, DBTYPE_WSTR, DBCONVERTFLAGS_COLUMN), IConvertType_CanConvert(ct, DBTYPE_I4, DBTYPE_R8, 0));
        if (pr)
        {
            IErrorInfo *ei = NULL;
            BSTR d = NULL, src = NULL;
            SetErrorInfo(0, NULL);
            P("PR_OK %08lx %08lx\n", ICommandPrepare_Prepare(pr, 1), ICommandPrepare_Unprepare(pr));
            hr = ICommandPrepare_Prepare(pr, 7);
            GetErrorInfo(0, &ei);
            if (ei) { IErrorInfo_GetDescription(ei, &d); IErrorInfo_GetSource(ei, &src); IErrorInfo_Release(ei); }
            P("PR_FAIL %08lx %s|%s\n", hr, d ? u8(d, b1) : "(none)", src ? u8(src, b2) : "(none)");
        }
        if (tl)
        {
            ULONG lvl = 0;
            hr = ITransactionLocal_StartTransaction(tl, ISOLATIONLEVEL_READCOMMITTED, 0, NULL, &lvl);
            P("TL_START %08lx %lu\n", hr, lvl);
            P("TL_NOLVL %08lx\n", ITransactionLocal_StartTransaction(tl, ISOLATIONLEVEL_READCOMMITTED, 0, NULL, NULL));
            P("TL_BAD %08lx\n", ITransactionLocal_StartTransaction(tl, ISOLATIONLEVEL_CHAOS, 0, NULL, &lvl));
        }
        if (rp)
        {
            HCHAPTER c = 0; HROW r = 0; DBPOSITIONFLAGS f = 99;
            hr = IRowPosition_GetRowPosition(rp, &c, &r, &f);
            P("RP_GET %08lx %Iu %Iu %lu\n", hr, c, r, f);
            P("RP_SET %08lx\n", IRowPosition_SetRowPosition(rp, 6, 10, DBPOSITION_EOF));
            P("RP_CLEAR %08lx\n", IRowPosition_ClearRowPosition(rp));
        }
        if (orow)
        {
            DBID table;
            DBPROP props[2];
            DBPROPSET set;
            IUnknown *rs = NULL;
            memset(props, 0, sizeof(props));
            props[0].dwPropertyID = 5; props[1].dwPropertyID = 99;
            props[0].dwStatus = props[1].dwStatus = 777;
            set.rgProperties = props; set.cProperties = 2; set.guidPropertySet = my_rowset;
            table.eKind = DBKIND_NAME; table.uName.pwszName = (LPOLESTR)L"tbl";
            hr = IOpenRowset_OpenRowset(orow, NULL, &table, NULL, &IID_IUnknown, 1, &set, &rs);
            P("OR_OPEN %08lx %d %lu %lu\n", hr, rs != NULL, props[0].dwStatus, props[1].dwStatus);
            if (rs) IUnknown_Release(rs);
            table.uName.pwszName = (LPOLESTR)L"nope";
            rs = NULL;
            hr = IOpenRowset_OpenRowset(orow, NULL, &table, NULL, &IID_IUnknown, 0, NULL, &rs);
            P("OR_BAD %08lx %d\n", hr, rs != NULL);
        }
        if (mr)
        {
            DBROWCOUNT aff = 0;
            IUnknown *rs = (IUnknown *)1;
            hr = IMultipleResults_GetResult(mr, NULL, DBRESULTFLAG_DEFAULT, &my_null_iid, &aff, NULL);
            P("MR_COUNT %08lx %Id\n", hr, aff);
            hr = IMultipleResults_GetResult(mr, NULL, DBRESULTFLAG_ROWSET, &IID_IUnknown, &aff, &rs);
            P("MR_ROWSET %08lx %Id %d\n", hr, aff, rs != NULL);
        }
    }
    SetEvent(done);
    WaitForSingleObject(th, 10000);
    P("DONE\n");
    return 0;
}
