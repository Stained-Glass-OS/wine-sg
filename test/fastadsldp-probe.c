#define COBJMACROS
#include "fastadsldp-server.h"
#include <ole2.h>
#include <iads.h>
#include <adshlp.h>
#include <adserr.h>

static const CLSID CLSID_LDAPNamespace_ = {0x228d9a82,0xc302,0x11cf,{0x9a,0xa4,0x00,0xaa,0x00,0x4a,0x56,0x91}};
static const CLSID CLSID_ADSystemInfo_ = {0x50b6327f,0xafd1,0x11d2,{0x9c,0xb9,0x00,0x00,0xf8,0x7a,0x36,0x9e}};
static const IID IID_IADs_ = {0xfd8256d0,0xfd15,0x11ce,{0xab,0xc4,0x02,0x60,0x8c,0x9e,0x75,0x53}};
static const IID IID_IADsOpenDSObject_ = {0xddf2891e,0x0f9c,0x11d0,{0x8a,0xd4,0x00,0xc0,0x4f,0xd8,0xd5,0x03}};
static const IID IID_IDirectoryObject_ = {0xe798de2c,0x22e4,0x11d0,{0x84,0xfe,0x00,0xc0,0x4f,0xd8,0xd5,0x03}};
static const IID IID_IDirectorySearch_ = {0x109ba8ec,0x92f0,0x11d0,{0xa7,0x90,0x00,0xc0,0x4f,0xd8,0xd5,0xa8}};
static const IID IID_IADsADSystemInfo_ = {0x5bb11929,0xafd1,0x11d2,{0x9c,0xb9,0x00,0x00,0xf8,0x7a,0x36,0x9e}};
#define P(...) printf(__VA_ARGS__)

#define BASE "CN=Jeff Smith,CN=Users,DC=wine,DC=test"

/* text with the port of the server replaced */
static void pr(const char *name, const WCHAR *w)
{
    char buf[512]; char port[16]; char *p, *q;
    WideCharToMultiByte(CP_UTF8, 0, w, -1, buf, sizeof(buf), NULL, NULL);
    sprintf(port, ":%d", g_port);
    for (p = buf; (q = strstr(p, port)); p = q + 5) { memmove(q + 5, q + strlen(port), strlen(q + strlen(port)) + 1); memcpy(q, ":PORT", 5); }
    P("%s=%s\n", name, buf);
}

static const char *sv_val(const char *dn, const char *attr, int idx)
{
    LEntry *e; LAttr *a; static char bufs[8][256]; static int next; char *buf = bufs[next++ & 7];
    EnterCriticalSection(&g_cs);
    e = sv_find(dn); a = e ? sv_attr(e, attr) : NULL;
    if (!a || idx >= a->n) { LeaveCriticalSection(&g_cs); return NULL; }
    memcpy(buf, a->v[idx].d, a->v[idx].len); buf[a->v[idx].len] = 0;
    LeaveCriticalSection(&g_cs);
    return buf;
}
static int sv_count(const char *dn, const char *attr)
{
    LEntry *e; LAttr *a; int n;
    EnterCriticalSection(&g_cs);
    e = sv_find(dn); a = e ? sv_attr(e, attr) : NULL; n = a ? a->n : -1;
    LeaveCriticalSection(&g_cs);
    return n;
}
static void svshow(const char *key, const char *dn, const char *attr)
{
    char out[512] = ""; int i, n = sv_count(dn, attr);
    for (i = 0; i < n; i++) { if (i) strcat(out, ";"); strcat(out, sv_val(dn, attr, i)); }
    P("%s=%d:%s\n", key, n, out);
}

static IADs *open_ads(const char *dn, IDispatch **disp_out)
{
    IADsOpenDSObject *open; IDispatch *disp = NULL; IADs *ads = NULL; WCHAR path[400]; HRESULT hr;
    swprintf(path, 400, L"LDAP://127.0.0.1:%d/%hs", g_port, dn);
    hr = CoCreateInstance(&CLSID_LDAPNamespace_, NULL, CLSCTX_INPROC_SERVER, &IID_IADsOpenDSObject_, (void**)&open);
    if (hr) { P("OPEN_CREATE=%08lx\n", hr); return NULL; }
    hr = IADsOpenDSObject_OpenDSObject(open, path, NULL, NULL, 0, &disp);
    IADsOpenDSObject_Release(open);
    if (hr) { P("OPEN=%08lx\n", hr); return NULL; }
    IDispatch_QueryInterface(disp, &IID_IADs_, (void**)&ads);
    if (disp_out) *disp_out = disp; else IDispatch_Release(disp);
    return ads;
}

static void show_bstr(const char *key, HRESULT hr, BSTR b)
{
    if (hr) P("%s=ERR %08lx\n", key, hr); else { pr(key, b); SysFreeString(b); }
}
static void show_ex(const char *key, IADs *ads, const WCHAR *name)
{
    VARIANT v; HRESULT hr; BSTR b = SysAllocString(name); char out[400] = ""; LONG lb, ub, i;
    VariantInit(&v);
    hr = IADs_GetEx(ads, b, &v); SysFreeString(b);
    if (hr) { P("%s=ERR %08lx\n", key, hr); return; }
    SafeArrayGetLBound(V_ARRAY(&v), 1, &lb); SafeArrayGetUBound(V_ARRAY(&v), 1, &ub);
    for (i = lb; i <= ub; i++) {
        VARIANT item; char tmp[128];
        VariantInit(&item); SafeArrayGetElement(V_ARRAY(&v), &i, &item);
        WideCharToMultiByte(CP_UTF8, 0, V_BSTR(&item), -1, tmp, sizeof(tmp), NULL, NULL);
        if (i > lb) strcat(out, ";");
        strcat(out, tmp); VariantClear(&item);
    }
    P("%s=%d:%s\n", key, (int)(ub - lb + 1), out);
    VariantClear(&v);
}
static void show_get(const char *key, IADs *ads, const WCHAR *name)
{
    VARIANT v; HRESULT hr; BSTR b = SysAllocString(name); char tmp[128];
    VariantInit(&v);
    hr = IADs_Get(ads, b, &v); SysFreeString(b);
    if (hr) { P("%s=ERR %08lx\n", key, hr); return; }
    if (V_VT(&v) == VT_BSTR) { WideCharToMultiByte(CP_UTF8, 0, V_BSTR(&v), -1, tmp, sizeof(tmp), NULL, NULL); P("%s=%s\n", key, tmp); }
    else P("%s=vt%d\n", key, V_VT(&v));
    VariantClear(&v);
}
static VARIANT vstr(const WCHAR *s) { VARIANT v; VariantInit(&v); V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(s); return v; }
static VARIANT vstrs(const WCHAR **s, int n)
{
    VARIANT v; SAFEARRAY *sa = SafeArrayCreateVector(VT_VARIANT, 0, n); LONG i;
    for (i = 0; i < n; i++) { VARIANT it = vstr(s[i]); SafeArrayPutElement(sa, &i, &it); VariantClear(&it); }
    VariantInit(&v); V_VT(&v) = VT_ARRAY | VT_VARIANT; V_ARRAY(&v) = sa; return v;
}

static void typeinfo_name(const char *key, IDispatch *d)
{
    ITypeInfo *ti; BSTR name = NULL; UINT cnt = 99; HRESULT hr;
    hr = IDispatch_GetTypeInfoCount(d, &cnt);
    P("%s_COUNT=%08lx %u\n", key, hr, cnt);
    hr = IDispatch_GetTypeInfo(d, 0, 0, &ti);
    if (hr) { P("%s=ERR %08lx\n", key, hr); return; }
    hr = ITypeInfo_GetDocumentation(ti, MEMBERID_NIL, &name, NULL, NULL, NULL);
    P("%s=%ls\n", key, hr ? L"?" : name); SysFreeString(name);
    ITypeInfo_Release(ti);
    hr = IDispatch_GetTypeInfo(d, 1, 0, &ti); P("%s_BAD=%08lx\n", key, hr);
    hr = IDispatch_GetTypeInfo(d, 0, 0, NULL); P("%s_NULL=%08lx\n", key, hr);
}

int main(void)
{
    IADs *ads, *ads2; IDispatch *disp; HRESULT hr; BSTR b; VARIANT v;
    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);
    if (!sv_start()) { P("SERVER=fail\nDONE\n"); return 1; }

    ads = open_ads(BASE, &disp);
    P("OPEN_OK=%d\n", ads != NULL);
    if (!ads) { P("DONE\n"); return 1; }

    /* automation: type information, names and invoke through the type library */
    typeinfo_name("TI_IADS", disp);
    {
        LPOLESTR names[1] = { (LPOLESTR)L"Name" }; DISPID id = -2; DISPPARAMS dp = {0}; VARIANT r; UINT ae;
        hr = IDispatch_GetIDsOfNames(disp, &IID_NULL, names, 1, 0, &id);
        P("GIDS_NAME=%08lx %d\n", hr, id > 0);
        VariantInit(&r);
        hr = IDispatch_Invoke(disp, id, &IID_NULL, 0, DISPATCH_PROPERTYGET, &dp, &r, NULL, &ae);
        P("INVOKE_NAME=%08lx %ls\n", hr, hr ? L"" : V_BSTR(&r));
        VariantClear(&r);
        hr = IDispatch_GetIDsOfNames(disp, &IID_NULL, names, 1, 0, &id);
        { GUID g = {1}; hr = IDispatch_GetIDsOfNames(disp, &g, names, 1, 0, &id); P("GIDS_BADIID=%08lx\n", hr); }
    }
    {
        IADsOpenDSObject *open; hr = IADs_QueryInterface(ads, &IID_IADsOpenDSObject_, (void**)&open);
        if (!hr) { IDispatch *od; IADsOpenDSObject_QueryInterface(open, &IID_IDispatch, (void**)&od); typeinfo_name("TI_OPEN", od); IDispatch_Release(od); IADsOpenDSObject_Release(open); }
    }

    /* the IADs properties */
    hr = IADs_get_Name(ads, &b); show_bstr("P_NAME", hr, b);
    hr = IADs_get_Class(ads, &b); show_bstr("P_CLASS", hr, b);
    hr = IADs_get_GUID(ads, &b); show_bstr("P_GUID", hr, b);
    hr = IADs_get_ADsPath(ads, &b); show_bstr("P_PATH", hr, b);
    hr = IADs_get_Parent(ads, &b); show_bstr("P_PARENT", hr, b);
    hr = IADs_get_Schema(ads, &b); show_bstr("P_SCHEMA", hr, b);
    hr = IADs_get_Name(ads, NULL); P("P_NAME_NULL=%08lx\n", hr);
    ads2 = open_ads("CN=Users,DC=wine,DC=test", NULL);
    if (ads2) { hr = IADs_get_Parent(ads2, &b); show_bstr("P_PARENT_CONTAINER", hr, b); hr = IADs_get_Class(ads2, &b); show_bstr("P_CLASS_CONTAINER", hr, b); IADs_Release(ads2); }
    ads2 = open_ads("rootDSE", NULL);
    if (ads2) { hr = IADs_get_Name(ads2, &b); show_bstr("P_NAME_DSE", hr, b); hr = IADs_get_Parent(ads2, &b); show_bstr("P_PARENT_DSE", hr, b);
                hr = IADs_get_ADsPath(ads2, &b); show_bstr("P_PATH_DSE", hr, b); hr = IADs_get_GUID(ads2, &b); show_bstr("P_GUID_DSE", hr, b); IADs_Release(ads2); }

    /* GetEx always returns arrays; Get still returns the single value */
    show_ex("GETEX_PHONE", ads, L"telephoneNumber");
    show_ex("GETEX_CN", ads, L"cn");
    show_ex("GETEX_NONE", ads, L"nosuch");
    show_get("GET_SN", ads, L"sn");

    /* Put and PutEx change the cache; SetInfo sends one modify request */
    {
        int m0 = g_modifies; VARIANT n = vstr(L"Smythe"), n2 = vstr(L"Jeff S"); b = SysAllocString(L"sn");
        hr = IADs_Put(ads, b, n); P("PUT_SN=%08lx\n", hr);
        SysFreeString(b); b = SysAllocString(L"description"); IADs_Put(ads, b, n2); SysFreeString(b);
        show_get("PUT_GET", ads, L"sn");
        P("PUT_BEFORE_SETINFO=%s modifies=%d\n", sv_val(BASE, "sn", 0), g_modifies - m0);
        hr = IADs_SetInfo(ads); P("SETINFO=%08lx\n", hr);
        P("PUT_AFTER_SETINFO=%s modifies=%d\n", sv_val(BASE, "sn", 0), g_modifies - m0);
        svshow("PUT_DESC", BASE, "description");
        hr = IADs_SetInfo(ads); P("SETINFO_NOTHING=%08lx modifies=%d\n", hr, g_modifies - m0);
        VariantClear(&n); VariantClear(&n2);
    }
    {
        VARIANT a, d, u; const WCHAR *one[1] = { L"425-707-9792" }, *del[1] = { L"425-707-9790" }, *upd[2] = { L"a@wine.test", L"b@wine.test" };
        b = SysAllocString(L"telephoneNumber");
        a = vstrs(one, 1); hr = IADs_PutEx(ads, 3, b, a); P("PUTEX_APPEND=%08lx\n", hr); VariantClear(&a);
        a = vstrs(one, 1); hr = IADs_PutEx(ads, 3, b, a); P("PUTEX_APPEND_DUP=%08lx\n", hr); VariantClear(&a);
        show_ex("PUTEX_APPEND_CACHE", ads, L"telephoneNumber");
        IADs_SetInfo(ads); svshow("PUTEX_APPEND_SRV", BASE, "telephoneNumber");
        d = vstrs(del, 1); hr = IADs_PutEx(ads, 4, b, d); P("PUTEX_DELETE=%08lx\n", hr); VariantClear(&d);
        d = vstrs(del, 1); IADs_PutEx(ads, 4, b, d); VariantClear(&d);   /* the value is already gone: no error */
        show_ex("PUTEX_DELETE_CACHE", ads, L"telephoneNumber");
        IADs_SetInfo(ads); svshow("PUTEX_DELETE_SRV", BASE, "telephoneNumber");
        {   /* deleting every value removes the attribute; the values come back */
            const WCHAR *both[2] = { L"425-707-9791", L"425-707-9792" };
            d = vstrs(both, 2); hr = IADs_PutEx(ads, 4, b, d); P("PUTEX_DELETEALL=%08lx\n", hr); VariantClear(&d);
            show_ex("PUTEX_DELETEALL_CACHE", ads, L"telephoneNumber");
            IADs_SetInfo(ads); svshow("PUTEX_DELETEALL_SRV", BASE, "telephoneNumber");
            d = vstrs(both, 2); IADs_PutEx(ads, 3, b, d); VariantClear(&d);
            show_ex("PUTEX_RESTORE_CACHE", ads, L"telephoneNumber");
            IADs_SetInfo(ads); svshow("PUTEX_RESTORE_SRV", BASE, "telephoneNumber");
        }
        SysFreeString(b); b = SysAllocString(L"mail");
        u = vstrs(upd, 2); hr = IADs_PutEx(ads, 2, b, u); P("PUTEX_UPDATE=%08lx\n", hr); VariantClear(&u);
        IADs_SetInfo(ads); svshow("PUTEX_UPDATE_SRV", BASE, "mail");
        VariantInit(&v); hr = IADs_PutEx(ads, 1, b, v); P("PUTEX_CLEAR=%08lx\n", hr);
        show_ex("PUTEX_CLEAR_CACHE", ads, L"mail");
        IADs_SetInfo(ads); svshow("PUTEX_CLEAR_SRV", BASE, "mail");
        hr = IADs_PutEx(ads, 9, b, v); P("PUTEX_BADCODE=%08lx\n", hr);
        SysFreeString(b);
    }
    {   /* numbers and booleans */
        VARIANT n, bo; VariantInit(&n); V_VT(&n) = VT_I4; V_I4(&n) = 77; VariantInit(&bo); V_VT(&bo) = VT_BOOL; V_BOOL(&bo) = VARIANT_FALSE;
        b = SysAllocString(L"uidNumber"); IADs_Put(ads, b, n); SysFreeString(b);
        b = SysAllocString(L"enabled"); IADs_Put(ads, b, bo); SysFreeString(b);
        IADs_SetInfo(ads);
        P("PUT_TYPES=%s,%s\n", sv_val(BASE, "uidNumber", 0), sv_val(BASE, "enabled", 0));
    }
    {   /* GetInfo drops the changes that were not sent */
        int m0 = g_modifies; VARIANT n = vstr(L"Dropped");
        b = SysAllocString(L"sn"); IADs_Put(ads, b, n); SysFreeString(b); VariantClear(&n);
        IADs_GetInfo(ads);
        show_get("GETINFO_DROPS", ads, L"sn");
        IADs_SetInfo(ads); P("GETINFO_NOMOD=%d\n", g_modifies - m0);
    }
    {   /* a Put the server refuses */
        VARIANT n = vstr(L"x"); b = SysAllocString(L"cn");
        hr = IADs_Put(ads, b, n); P("PUT_CN=%08lx\n", hr);
        SysFreeString(b); VariantClear(&n);
        hr = IADs_SetInfo(ads); P("SETINFO_REFUSED=%d\n", hr == HRESULT_FROM_WIN32(ERROR_DS_CANT_ON_RDN));
        P("SETINFO_REFUSED_CN=%s\n", sv_val(BASE, "cn", 0));
        VariantInit(&v); V_VT(&v) = VT_R8; V_R8(&v) = 1.5; b = SysAllocString(L"sn");
        hr = IADs_Put(ads, b, v); P("PUT_BADTYPE=%08lx\n", hr); SysFreeString(b);
        IADs_GetInfo(ads);
    }

    /* IDirectoryObject */
    {
        IDirectoryObject *dobj; PADS_OBJECT_INFO info = NULL; PADS_ATTR_INFO attrs = NULL; DWORD n = 0, i, j;
        IADs_QueryInterface(ads, &IID_IDirectoryObject_, (void**)&dobj);
        hr = IDirectoryObject_GetObjectInformation(dobj, &info);
        P("DO_INFO=%08lx\n", hr);
        if (!hr) { pr("DO_RDN", info->pszRDN); pr("DO_DN", info->pszObjectDN); pr("DO_PARENT", info->pszParentDN); pr("DO_CLASS", info->pszClassName); pr("DO_SCHEMA", info->pszSchemaDN); FreeADsMem(info); }
        {
            LPWSTR names[] = { (LPWSTR)L"sn", (LPWSTR)L"telephoneNumber", (LPWSTR)L"uidNumber", (LPWSTR)L"enabled", (LPWSTR)L"objectGUID", (LPWSTR)L"nosuch" };
            hr = IDirectoryObject_GetObjectAttributes(dobj, names, 6, &attrs, &n);
            P("DO_ATTRS=%08lx %lu\n", hr, n);
            for (i = 0; !hr && i < n; i++) {
                char line[300]; sprintf(line, "DO_ATTR_%d=", (int)i);
                P("%s", line); P("%ls type=%d n=%lu", attrs[i].pszAttrName, attrs[i].dwADsType, attrs[i].dwNumValues);
                for (j = 0; j < attrs[i].dwNumValues; j++) {
                    ADSVALUE *v = &attrs[i].pADsValues[j];
                    if (v->dwType == ADSTYPE_CASE_IGNORE_STRING) P(" '%ls'", v->CaseIgnoreString);
                    else if (v->dwType == ADSTYPE_INTEGER) P(" %ld", v->Integer);
                    else if (v->dwType == ADSTYPE_BOOLEAN) P(" %ld", v->Boolean);
                    else if (v->dwType == ADSTYPE_OCTET_STRING) P(" octets%lu:%02x", v->OctetString.dwLength, v->OctetString.lpValue[0]);
                }
                P("\n");
            }
            if (attrs) FreeADsMem(attrs);
        }
        attrs = NULL; n = 0;
        hr = IDirectoryObject_GetObjectAttributes(dobj, NULL, (DWORD)-1, &attrs, &n);
        P("DO_ALLATTRS=%08lx %lu\n", hr, n);
        if (attrs) FreeADsMem(attrs);
        {
            LPWSTR names[] = { (LPWSTR)L"nosuch" };
            attrs = (PADS_ATTR_INFO)0x1; n = 99;
            hr = IDirectoryObject_GetObjectAttributes(dobj, names, 1, &attrs, &n);
            P("DO_NONE=%08lx %lu %d\n", hr, n, attrs == NULL);
        }
        {   /* SetObjectAttributes: update, append, delete, clear in one request */
            ADSVALUE v1, v2, v3, v4; ADS_ATTR_INFO set[4]; DWORD done = 99; int m0 = g_modifies;
            v1.dwType = ADSTYPE_CASE_IGNORE_STRING; v1.CaseIgnoreString = (LPWSTR)L"Smyth";
            v2.dwType = ADSTYPE_CASE_IGNORE_STRING; v2.CaseIgnoreString = (LPWSTR)L"425-707-9799";
            v3.dwType = ADSTYPE_CASE_IGNORE_STRING; v3.CaseIgnoreString = (LPWSTR)L"425-707-9791";
            v4.dwType = ADSTYPE_INTEGER; v4.Integer = 5;
            set[0].pszAttrName = (LPWSTR)L"sn"; set[0].dwControlCode = 2; set[0].dwADsType = ADSTYPE_CASE_IGNORE_STRING; set[0].pADsValues = &v1; set[0].dwNumValues = 1;
            set[1].pszAttrName = (LPWSTR)L"telephoneNumber"; set[1].dwControlCode = 3; set[1].dwADsType = ADSTYPE_CASE_IGNORE_STRING; set[1].pADsValues = &v2; set[1].dwNumValues = 1;
            set[2].pszAttrName = (LPWSTR)L"description"; set[2].dwControlCode = 1; set[2].dwADsType = ADSTYPE_CASE_IGNORE_STRING; set[2].pADsValues = NULL; set[2].dwNumValues = 0;
            set[3].pszAttrName = (LPWSTR)L"uidNumber"; set[3].dwControlCode = 2; set[3].dwADsType = ADSTYPE_INTEGER; set[3].pADsValues = &v4; set[3].dwNumValues = 1;
            hr = IDirectoryObject_SetObjectAttributes(dobj, set, 4, &done);
            P("DO_SET=%08lx %lu modifies=%d\n", hr, done, g_modifies - m0);
            P("DO_SET_SRV=%s|%d|%d|%s\n", sv_val(BASE, "sn", 0), sv_count(BASE, "telephoneNumber"), sv_count(BASE, "description"), sv_val(BASE, "uidNumber", 0));
            set[0].pszAttrName = (LPWSTR)L"telephoneNumber"; set[0].dwControlCode = 4; set[0].pADsValues = &v3; set[0].dwNumValues = 1;
            hr = IDirectoryObject_SetObjectAttributes(dobj, set, 1, &done);
            svshow("DO_SET_DELETE", BASE, "telephoneNumber");
            hr = IDirectoryObject_SetObjectAttributes(dobj, set, 1, NULL); P("DO_SET_NULL=%08lx\n", hr);
            set[0].dwControlCode = 7; hr = IDirectoryObject_SetObjectAttributes(dobj, set, 1, &done); P("DO_SET_BADCODE=%08lx\n", hr);
        }
        IDirectoryObject_Release(dobj);

        /* create and delete a child of the container */
        {
            IADs *users = open_ads("CN=Users,DC=wine,DC=test", NULL); IDirectoryObject *udo; IDispatch *created = NULL;
            ADSVALUE c1, c2, c3; ADS_ATTR_INFO ca[3]; BSTR nm;
            c1.dwType = ADSTYPE_CASE_IGNORE_STRING; c1.CaseIgnoreString = (LPWSTR)L"person";
            c2.dwType = ADSTYPE_CASE_IGNORE_STRING; c2.CaseIgnoreString = (LPWSTR)L"New Person";
            c3.dwType = ADSTYPE_CASE_IGNORE_STRING; c3.CaseIgnoreString = (LPWSTR)L"Person";
            ca[0].pszAttrName = (LPWSTR)L"objectClass"; ca[0].dwControlCode = 2; ca[0].dwADsType = ADSTYPE_CASE_IGNORE_STRING; ca[0].pADsValues = &c1; ca[0].dwNumValues = 1;
            ca[1].pszAttrName = (LPWSTR)L"cn"; ca[1].dwControlCode = 2; ca[1].dwADsType = ADSTYPE_CASE_IGNORE_STRING; ca[1].pADsValues = &c2; ca[1].dwNumValues = 1;
            ca[2].pszAttrName = (LPWSTR)L"sn"; ca[2].dwControlCode = 2; ca[2].dwADsType = ADSTYPE_CASE_IGNORE_STRING; ca[2].pADsValues = &c3; ca[2].dwNumValues = 1;
            IADs_QueryInterface(users, &IID_IDirectoryObject_, (void**)&udo);
            hr = IDirectoryObject_CreateDSObject(udo, (LPWSTR)L"CN=New Person", ca, 3, &created);
            P("DO_CREATE=%08lx adds=%d\n", hr, g_adds);
            if (!hr && created) {
                IADs *cads; IDispatch_QueryInterface(created, &IID_IADs_, (void**)&cads);
                hr = IADs_get_Name(cads, &nm); show_bstr("DO_CREATED_NAME", hr, nm);
                IADs_Release(cads); IDispatch_Release(created);
            }
            svshow("DO_CREATE_SRV", "CN=New Person,CN=Users,DC=wine,DC=test", "sn");
            created = NULL;
            hr = IDirectoryObject_CreateDSObject(udo, (LPWSTR)L"CN=New Person", ca, 3, &created); P("DO_CREATE_AGAIN=%08lx\n", hr);
            hr = IDirectoryObject_CreateDSObject(udo, NULL, ca, 3, &created); P("DO_CREATE_NONAME=%08lx\n", hr);
            hr = IDirectoryObject_DeleteDSObject(udo, (LPWSTR)L"CN=New Person"); P("DO_DELETE=%08lx deletes=%d\n", hr, g_deletes);
            P("DO_DELETE_SRV=%d\n", sv_find("CN=New Person,CN=Users,DC=wine,DC=test") != NULL);
            hr = IDirectoryObject_DeleteDSObject(udo, (LPWSTR)L"CN=New Person"); P("DO_DELETE_AGAIN=%08lx\n", hr);
            hr = IDirectoryObject_DeleteDSObject(udo, NULL); P("DO_DELETE_NULL=%08lx\n", hr);
            IDirectoryObject_Release(udo);
            IADs_Release(users);
        }
    }

    /* IDirectorySearch: scrolling, abandon and the preferences */
    {
        IADs *users = open_ads("CN=Users,DC=wine,DC=test", NULL); IDirectorySearch *ds; ADS_SEARCHPREF_INFO pref[2]; ADS_SEARCH_HANDLE h; LPWSTR attrs[1] = { (LPWSTR)L"cn" };
        char seq[300] = ""; int k;
        IADs_QueryInterface(users, &IID_IDirectorySearch_, (void**)&ds);
        pref[0].dwSearchPref = ADS_SEARCHPREF_SEARCH_SCOPE; pref[0].vValue.dwType = ADSTYPE_INTEGER; pref[0].vValue.Integer = ADS_SCOPE_ONELEVEL;
        hr = IDirectorySearch_SetSearchPreference(ds, pref, 1); P("SP_SCOPE=%08lx %d\n", hr, pref[0].dwStatus);
        hr = IDirectorySearch_ExecuteSearch(ds, (LPWSTR)L"(objectClass=*)", attrs, 1, &h); P("SEARCH=%08lx\n", hr);
#define ROW(tag) do { ADS_SEARCH_COLUMN col; HRESULT r = tag; char t[64]; if (r == S_OK) { if (IDirectorySearch_GetColumn(ds, h, (LPWSTR)L"cn", &col) == S_OK) { WideCharToMultiByte(CP_UTF8, 0, col.pADsValues[0].CaseIgnoreString, -1, t, 64, NULL, NULL); IDirectorySearch_FreeColumn(ds, &col); } else strcpy(t, "?"); } else sprintf(t, "<%lx>", r); if (seq[0]) strcat(seq, ","); strcat(seq, t); } while (0)
        for (k = 0; k < 3; k++) ROW(IDirectorySearch_GetNextRow(ds, h));
        ROW(IDirectorySearch_GetPreviousRow(ds, h));
        ROW(IDirectorySearch_GetPreviousRow(ds, h));
        ROW(IDirectorySearch_GetPreviousRow(ds, h));
        ROW(IDirectorySearch_GetNextRow(ds, h));
        P("SCROLL=%s\n", seq);
        hr = IDirectorySearch_AbandonSearch(ds, h); P("ABANDON=%08lx\n", hr);
        hr = IDirectorySearch_GetNextRow(ds, h); P("ABANDON_NEXT=%08lx\n", hr);
        hr = IDirectorySearch_AbandonSearch(ds, NULL); P("ABANDON_NULL=%08lx\n", hr);
        hr = IDirectorySearch_GetPreviousRow(ds, NULL); P("PREV_NULL=%08lx\n", hr);
        IDirectorySearch_CloseSearchHandle(ds, h);

        pref[0].dwSearchPref = ADS_SEARCHPREF_CACHE_RESULTS; pref[0].vValue.dwType = ADSTYPE_BOOLEAN; pref[0].vValue.Boolean = FALSE;
        IDirectorySearch_SetSearchPreference(ds, pref, 1);
        IDirectorySearch_ExecuteSearch(ds, (LPWSTR)L"(objectClass=*)", attrs, 1, &h);
        IDirectorySearch_GetNextRow(ds, h);
        hr = IDirectorySearch_GetPreviousRow(ds, h); P("PREV_NOCACHE=%08lx\n", hr);
        IDirectorySearch_CloseSearchHandle(ds, h);
        pref[0].vValue.Boolean = TRUE; IDirectorySearch_SetSearchPreference(ds, pref, 1);

#define PREF(key, p, type, field, val) do { pref[0].dwSearchPref = p; memset(&pref[0].vValue, 0, sizeof(pref[0].vValue)); pref[0].vValue.dwType = type; pref[0].vValue.field = val; pref[0].dwStatus = 99; \
        hr = IDirectorySearch_SetSearchPreference(ds, pref, 1); P("%s=%08lx %d\n", key, hr, pref[0].dwStatus); } while (0)
        PREF("SP_ASYNC", ADS_SEARCHPREF_ASYNCHRONOUS, ADSTYPE_BOOLEAN, Boolean, TRUE);
        PREF("SP_DEREF", ADS_SEARCHPREF_DEREF_ALIASES, ADSTYPE_INTEGER, Integer, 1);
        PREF("SP_DEREF_BAD", ADS_SEARCHPREF_DEREF_ALIASES, ADSTYPE_INTEGER, Integer, 9);
        PREF("SP_TIMELIMIT", ADS_SEARCHPREF_TIME_LIMIT, ADSTYPE_INTEGER, Integer, 30);
        PREF("SP_TIMEOUT", ADS_SEARCHPREF_TIMEOUT, ADSTYPE_INTEGER, Integer, 30);
        PREF("SP_PAGEDTIME", ADS_SEARCHPREF_PAGED_TIME_LIMIT, ADSTYPE_INTEGER, Integer, 30);
        PREF("SP_CHASE", ADS_SEARCHPREF_CHASE_REFERRALS, ADSTYPE_INTEGER, Integer, 0);
        PREF("SP_CHASE_WRONGTYPE", ADS_SEARCHPREF_CHASE_REFERRALS, ADSTYPE_BOOLEAN, Boolean, TRUE);
        PREF("SP_EXTDN", ADS_SEARCHPREF_EXTENDED_DN, ADSTYPE_INTEGER, Integer, 1);
        PREF("SP_EXTDN_BAD", ADS_SEARCHPREF_EXTENDED_DN, ADSTYPE_INTEGER, Integer, 5);
        PREF("SP_ASQ", ADS_SEARCHPREF_ATTRIBUTE_QUERY, ADSTYPE_CASE_IGNORE_STRING, CaseIgnoreString, (LPWSTR)L"member");
        PREF("SP_DIRSYNC", ADS_SEARCHPREF_DIRSYNC, ADSTYPE_PROV_SPECIFIC, ProviderSpecific.dwLength, 0);
        PREF("SP_UNKNOWN", (ADS_SEARCHPREF_ENUM)99, ADSTYPE_INTEGER, Integer, 1);
        {
            struct { WCHAR *attr; WCHAR *res; BOOLEAN rev; } key = { (WCHAR *)L"cn", NULL, FALSE };
            memset(&pref[0], 0, sizeof(pref[0])); pref[0].dwSearchPref = ADS_SEARCHPREF_SORT_ON; pref[0].vValue.dwType = ADSTYPE_PROV_SPECIFIC;
            pref[0].vValue.ProviderSpecific.dwLength = sizeof(key); pref[0].vValue.ProviderSpecific.lpValue = (BYTE*)&key; pref[0].dwStatus = 99;
            hr = IDirectorySearch_SetSearchPreference(ds, pref, 1); P("SP_SORT=%08lx %d\n", hr, pref[0].dwStatus);
        }
        /* the search still runs with the controls the preferences added */
        pref[0].dwSearchPref = ADS_SEARCHPREF_ATTRIBUTE_QUERY; pref[0].vValue.dwType = ADSTYPE_CASE_IGNORE_STRING; pref[0].vValue.CaseIgnoreString = NULL;
        hr = IDirectorySearch_ExecuteSearch(ds, (LPWSTR)L"(objectClass=*)", attrs, 1, &h); P("SEARCH_AFTER_PREFS=%08lx\n", hr);
        if (!hr) { hr = IDirectorySearch_GetNextRow(ds, h); P("SEARCH_AFTER_PREFS_ROW=%08lx\n", hr); IDirectorySearch_CloseSearchHandle(ds, h); }
        IDirectorySearch_Release(ds);
        IADs_Release(users);
    }

    /* IADsADSystemInfo */
    {
        IADsADSystemInfo *si; IDispatch *sd; VARIANT_BOOL vb; VARIANT tv;
        hr = CoCreateInstance(&CLSID_ADSystemInfo_, NULL, CLSCTX_INPROC_SERVER, &IID_IADsADSystemInfo_, (void**)&si);
        P("SI_CREATE=%08lx\n", hr);
        if (!hr) {
            IADsADSystemInfo_QueryInterface(si, &IID_IDispatch, (void**)&sd);
            typeinfo_name("TI_SYSINFO", sd);
            {   LPOLESTR names[1] = { (LPOLESTR)L"DomainDNSName" }; DISPID id = -2; hr = IDispatch_GetIDsOfNames(sd, &IID_NULL, names, 1, 0, &id); P("SI_GIDS=%08lx %d\n", hr, id > 0); }
            IDispatch_Release(sd);
#define SIBSTR(key, call) do { b = NULL; hr = call; P("%s=%d\n", key, hr == S_OK || (hr & 0xffff0000) == 0x80070000); if (hr == S_OK) SysFreeString(b); } while (0)
            SIBSTR("SI_USER", IADsADSystemInfo_get_UserName(si, &b));
            SIBSTR("SI_SITE", IADsADSystemInfo_get_SiteName(si, &b));
            SIBSTR("SI_SHORT", IADsADSystemInfo_get_DomainShortName(si, &b));
            SIBSTR("SI_DNS", IADsADSystemInfo_get_DomainDNSName(si, &b));
            SIBSTR("SI_FOREST", IADsADSystemInfo_get_ForestDNSName(si, &b));
            SIBSTR("SI_PDC", IADsADSystemInfo_get_PDCRoleOwner(si, &b));
            SIBSTR("SI_SCHEMAOWNER", IADsADSystemInfo_get_SchemaRoleOwner(si, &b));
            SIBSTR("SI_ANYDC", IADsADSystemInfo_GetAnyDCName(si, &b));
            hr = IADsADSystemInfo_get_IsNativeMode(si, &vb); P("SI_NATIVE=%d\n", hr == S_OK || (hr & 0xffff0000) == 0x80070000);
            VariantInit(&tv); hr = IADsADSystemInfo_GetTrees(si, &tv); P("SI_TREES=%d\n", hr == S_OK || (hr & 0xffff0000) == 0x80070000);
            hr = IADsADSystemInfo_GetDCSiteName(si, NULL, &b); P("SI_DCSITE_NULL=%08lx\n", hr);
            hr = IADsADSystemInfo_RefreshSchemaCache(si); P("SI_REFRESH=%08lx\n", hr);
            hr = IADsADSystemInfo_get_UserName(si, NULL); P("SI_NULLARG=%08lx\n", hr);
            IADsADSystemInfo_Release(si);
        }
    }

    IADs_Release(ads); IDispatch_Release(disp);

    /* server locks and the class objects */
    {
        HMODULE m = LoadLibraryW(L"adsldp.dll"); HRESULT (WINAPI *can)(void) = (void*)GetProcAddress(m, "DllCanUnloadNow");
        HRESULT (WINAPI *getco)(REFCLSID, REFIID, void**) = (void*)GetProcAddress(m, "DllGetClassObject");
        IClassFactory *cf = NULL; GUID bogus = {0x12345678, 1, 2, {3,4,5,6,7,8,9,10}};
        hr = getco(&CLSID_ADSystemInfo_, &IID_IClassFactory, (void**)&cf); P("DLL_GETCO=%08lx\n", hr);
        if (cf) {
            IClassFactory_LockServer(cf, TRUE); P("DLL_LOCKED=%08lx\n", can());
            IClassFactory_LockServer(cf, FALSE); P("DLL_UNLOCKED=%08lx\n", can());
            IClassFactory_Release(cf);
        }
        hr = getco(&bogus, &IID_IClassFactory, (void**)&cf); P("DLL_BOGUS=%08lx\n", hr);
    }
    P("BINDS=%d\n", g_binds > 0);
    P("DONE\n");
    return 0;
}
