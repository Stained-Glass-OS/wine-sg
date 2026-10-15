/* hnetcfg: services, remote administration, ICMP settings, service restriction, rule interfaces, NAT events
 * (run by fasthnetcfg2-gate.sh). Prints "KEY value" lines. */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <oleauto.h>
#include <netfw.h>
#include <stdio.h>
#include <wchar.h>

static const CLSID CLSID_Mgr  = {0x304ce942,0x6e39,0x40d8,{0x94,0x3a,0xb9,0x13,0xc4,0x0c,0x9c,0xd4}};
static const IID IID_Mgr = {0xf7898af5,0xcac4,0x4632,{0xa2,0xec,0xda,0x06,0xe5,0x11,0x1a,0xf2}};
static const CLSID CLSID_Pol2 = {0xe2b3c97f,0x6ae1,0x41ac,{0x81,0x7a,0xf6,0xf9,0x21,0x66,0xd7,0xdd}};
static const IID IID_Pol2 = {0x98325047,0xc671,0x4174,{0x8d,0x81,0xde,0xfc,0xd3,0xf0,0x31,0x86}};
static const CLSID CLSID_Rule = {0x2c5bc43e,0x3369,0x4c33,{0xab,0x0c,0xbe,0x94,0x69,0x67,0x7a,0xf4}};
static const IID IID_Rule = {0xaf230d27,0xbaba,0x4e42,{0xac,0xed,0xf5,0x24,0xf2,0x2c,0xfc,0xe2}};
static const CLSID CLSID_NAT = {0xae1e00aa,0x3fd5,0x403c,{0x8a,0x27,0x2b,0xbd,0xc3,0x0c,0xd0,0xe1}};
static const IID IID_NAT = {0xb171c812,0xcc76,0x485a,{0x94,0xd8,0xb6,0xb3,0xa2,0x79,0x4e,0x99}};

/* mingw has no natupnp.h: the two interfaces by their vtable slots */
typedef struct nat_vtbl { HRESULT (WINAPI *qi)(void *, REFIID, void **); ULONG (WINAPI *addref)(void *); ULONG (WINAPI *release)(void *);
    void *disp[4]; HRESULT (WINAPI *slot1)(void *, void **); HRESULT (WINAPI *slot2)(void *, void **); HRESULT (WINAPI *slot3)(void *, void **); } nat_vtbl;
typedef struct ev_vtbl { HRESULT (WINAPI *qi)(void *, REFIID, void **); ULONG (WINAPI *addref)(void *); ULONG (WINAPI *release)(void *);
    void *disp[4]; HRESULT (WINAPI *put_ip)(void *, IUnknown *); HRESULT (WINAPI *put_n)(void *, IUnknown *); } ev_vtbl;
typedef struct { const nat_vtbl *v; } IUPnPNAT;
typedef struct { const ev_vtbl *v; } INATEventManager;

#define P(...) do { printf(__VA_ARGS__); fflush(stdout); } while (0)

static const char *bs(BSTR b, char *buf)
{
    if (!b) return "(null)";
    WideCharToMultiByte(CP_UTF8, 0, b, -1, buf, 200, NULL, NULL);
    return buf;
}

static LONG enum_count(IUnknown *unk)
{
    IEnumVARIANT *e;
    VARIANT v;
    ULONG got;
    LONG n = 0;
    if (!unk || FAILED(IUnknown_QueryInterface(unk, &IID_IEnumVARIANT, (void **)&e))) return -1;
    VariantInit(&v);
    while (IEnumVARIANT_Next(e, 1, &v, &got) == S_OK && got) { n++; VariantClear(&v); }
    IEnumVARIANT_Release(e);
    return n;
}

struct cb { IUnknown iface; LONG ref; };
static HRESULT WINAPI cb_QI(IUnknown *i, REFIID r, void **o) { if (IsEqualGUID(r, &IID_IUnknown)) { *o = i; IUnknown_AddRef(i); return S_OK; } *o = NULL; return E_NOINTERFACE; }
static ULONG WINAPI cb_AddRef(IUnknown *i) { return InterlockedIncrement(&CONTAINING_RECORD(i, struct cb, iface)->ref); }
static ULONG WINAPI cb_Release(IUnknown *i) { return InterlockedDecrement(&CONTAINING_RECORD(i, struct cb, iface)->ref); }
static IUnknownVtbl cb_vtbl = { cb_QI, cb_AddRef, cb_Release };

static void services(INetFwProfile *prof, const char *tag)
{
    INetFwServices *svcs = NULL;
    INetFwService *svc = NULL;
    IUnknown *unk = NULL;
    LONG n = -1;
    NET_FW_SERVICE_TYPE type = 99;
    BSTR name = NULL;
    VARIANT_BOOL en = 5, cust = 5;
    HRESULT hr;
    char buf[256];
    int i;

    hr = INetFwProfile_get_Services(prof, &svcs);
    P("%s_SVCS %08lx\n", tag, hr);
    if (FAILED(hr)) return;
    hr = INetFwServices_get_Count(svcs, &n);
    P("%s_COUNT %08lx %ld\n", tag, hr, n);
    for (i = 0; i < 3; i++)
    {
        hr = INetFwServices_Item(svcs, i, &svc);
        if (FAILED(hr)) { P("%s_ITEM%d %08lx\n", tag, i, hr); continue; }
        INetFwService_get_Name(svc, &name);
        INetFwService_get_Type(svc, &type);
        en = 5; INetFwService_get_Enabled(svc, &en);
        cust = 5; INetFwService_get_Customized(svc, &cust);
        P("%s_ITEM%d %s|%d|%d|%d\n", tag, i, bs(name, buf), (int)type, en != 0, cust != 0);
        SysFreeString(name);
        INetFwService_Release(svc);
    }
    P("%s_ITEM3 %08lx\n", tag, INetFwServices_Item(svcs, NET_FW_SERVICE_NONE, &svc));
    P("%s_ITEMNEG %08lx\n", tag, INetFwServices_Item(svcs, -1, &svc));
    hr = INetFwServices_get__NewEnum(svcs, &unk);
    P("%s_ENUM %08lx %ld\n", tag, hr, enum_count(unk));
    if (unk) IUnknown_Release(unk);
    INetFwServices_Release(svcs);
}

int main(int argc, char **argv)
{
    INetFwMgr *mgr = NULL;
    INetFwPolicy *pol = NULL;
    INetFwProfile *dom = NULL, *std = NULL;
    INetFwServices *svcs = NULL;
    INetFwService *fp = NULL, *rd = NULL, *up = NULL;
    INetFwPolicy2 *p2 = NULL;
    BSTR b = NULL;
    HRESULT hr;
    char buf[256];
    LONG l;
    NET_FW_SCOPE scope;
    NET_FW_IP_VERSION ipv;
    VARIANT_BOOL vb;
    int nonadmin = argc > 1;

    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_Mgr, NULL, CLSCTX_INPROC_SERVER, &IID_Mgr, (void **)&mgr);
    if (FAILED(hr)) { P("NOMGR %08lx\n", hr); return 1; }
    INetFwMgr_get_LocalPolicy(mgr, &pol);
    INetFwPolicy_GetProfileByType(pol, NET_FW_PROFILE_DOMAIN, &dom);
    INetFwPolicy_GetProfileByType(pol, NET_FW_PROFILE_STANDARD, &std);

    if (nonadmin)
    {
        INetFwProfile_get_Services(std, &svcs);
        INetFwServices_Item(svcs, 0, &fp);
        P("NA_ENABLE %08lx\n", INetFwService_put_Enabled(fp, VARIANT_FALSE));
        P("NA_SCOPE %08lx\n", INetFwService_put_Scope(fp, NET_FW_SCOPE_LOCAL_SUBNET));
        INetFwService_get_Enabled(fp, &vb);
        P("NA_STILL %d\n", vb != 0);
        INetFwService_Release(fp);
        INetFwServices_Release(svcs);
        P("DONE\n");
        return 0;
    }

    /* the services as the firewall has them: file-sharing on domain+private, remote-desktop on all, discovery on none */
    services(dom, "D");
    services(std, "S");

    INetFwProfile_get_Services(std, &svcs);
    INetFwServices_Item(svcs, NET_FW_SERVICE_FILE_AND_PRINT, &fp);
    INetFwServices_Item(svcs, NET_FW_SERVICE_UPNP, &up);
    INetFwServices_Item(svcs, NET_FW_SERVICE_REMOTE_DESKTOP, &rd);

    /* scope / addresses / ip version */
    ipv = 99; scope = 99;
    INetFwService_get_IpVersion(fp, &ipv); INetFwService_get_Scope(fp, &scope);
    INetFwService_get_RemoteAddresses(fp, &b);
    P("SC_DEF %d %d %s\n", ipv, scope, bs(b, buf)); SysFreeString(b);
    P("SC_BADSCOPE %08lx\n", INetFwService_put_Scope(fp, 7));
    P("SC_BADIPV %08lx\n", INetFwService_put_IpVersion(fp, 9));
    P("SC_EMPTYADDR %08lx\n", INetFwService_put_RemoteAddresses(fp, SysAllocString(L"")));
    P("SC_SUBNET %08lx\n", INetFwService_put_Scope(fp, NET_FW_SCOPE_LOCAL_SUBNET));
    INetFwService_get_RemoteAddresses(fp, &b); P("SC_SUBNET_ADDR %s\n", bs(b, buf)); SysFreeString(b);
    P("SC_CUSTOM %08lx\n", INetFwService_put_RemoteAddresses(fp, SysAllocString(L"10.0.0.0/8,192.168.1.5")));
    INetFwService_get_Scope(fp, &scope); INetFwService_get_RemoteAddresses(fp, &b);
    P("SC_CUSTOM_GET %d %s\n", scope, bs(b, buf)); SysFreeString(b);
    P("SC_ALL %08lx\n", INetFwService_put_Scope(fp, NET_FW_SCOPE_ALL));
    INetFwService_get_RemoteAddresses(fp, &b); P("SC_ALL_ADDR %s\n", bs(b, buf)); SysFreeString(b);
    P("SC_V4 %08lx\n", INetFwService_put_IpVersion(fp, NET_FW_IP_VERSION_V4));
    INetFwService_get_IpVersion(fp, &ipv); P("SC_V4_GET %d\n", ipv);
    vb = 5; INetFwService_get_Customized(fp, &vb); P("SC_CUSTOMIZED %d\n", vb != 0);
    /* the domain profile's service keeps its own settings */
    {
        INetFwServices *ds; INetFwService *dfp;
        INetFwProfile_get_Services(dom, &ds); INetFwServices_Item(ds, 0, &dfp);
        INetFwService_get_IpVersion(dfp, &ipv); P("SC_DOMAIN_IPV %d\n", ipv);
        INetFwService_Release(dfp); INetFwServices_Release(ds);
    }

    /* enabling and disabling go to the firewall */
    P("EN_STD_OFF %08lx\n", INetFwService_put_Enabled(fp, VARIANT_FALSE));   /* file-sharing: domain,private -> domain */
    P("EN_RD_OFF %08lx\n", INetFwService_put_Enabled(rd, VARIANT_FALSE));    /* remote-desktop: all -> domain,public */
    P("EN_UP_ON %08lx\n", INetFwService_put_Enabled(up, VARIANT_TRUE));      /* discovery: domain,private -> unchanged */

    /* ports */
    {
        INetFwOpenPorts *ports = NULL; INetFwOpenPort *port = NULL; IUnknown *unk = NULL; BSTR nm = NULL;
        NET_FW_IP_PROTOCOL pr; LONG pn = 0;
        hr = INetFwService_get_GloballyOpenPorts(fp, &ports); P("PT_GET %08lx\n", hr);
        INetFwOpenPorts_get_Count(ports, &l); P("PT_COUNT %ld\n", l);
        hr = INetFwOpenPorts_Item(ports, 445, NET_FW_IP_PROTOCOL_TCP, &port);
        INetFwOpenPort_get_Name(port, &nm); INetFwOpenPort_get_Port(port, &pn); INetFwOpenPort_get_Protocol(port, &pr);
        P("PT_445 %08lx %s|%ld|%d\n", hr, bs(nm, buf), pn, pr); SysFreeString(nm); INetFwOpenPort_Release(port);
        P("PT_445UDP %08lx\n", INetFwOpenPorts_Item(ports, 445, NET_FW_IP_PROTOCOL_UDP, &port));
        P("PT_BAD %08lx\n", INetFwOpenPorts_Item(ports, 70000, NET_FW_IP_PROTOCOL_TCP, &port));
        P("PT_ADD %08lx\n", INetFwOpenPorts_Add(ports, NULL));
        INetFwOpenPorts_get__NewEnum(ports, &unk); P("PT_ENUM %ld\n", enum_count(unk)); if (unk) IUnknown_Release(unk);
        INetFwOpenPorts_Release(ports);
    }
    /* IDispatch */
    {
        DISPID id = 0; OLECHAR *n = (OLECHAR *)L"RemoteAddresses"; UINT ti = 9;
        hr = IDispatch_GetIDsOfNames((IDispatch *)fp, &IID_NULL, &n, 1, 0, &id);
        P("DISP_IDS %08lx %ld\n", hr, id);
        IDispatch_GetTypeInfoCount((IDispatch *)fp, &ti); P("DISP_TI %u\n", ti);
        {
            DISPPARAMS dp = {0}; VARIANT res; VariantInit(&res);
            hr = IDispatch_Invoke((IDispatch *)fp, 1, &IID_NULL, 0, DISPATCH_PROPERTYGET, &dp, &res, NULL, NULL);
            P("DISP_NAME %08lx %s\n", hr, V_VT(&res) == VT_BSTR ? bs(V_BSTR(&res), buf) : "?"); VariantClear(&res);
        }
    }
    INetFwService_Release(fp); INetFwService_Release(up); INetFwService_Release(rd); INetFwServices_Release(svcs);

    /* remote administration settings */
    {
        INetFwRemoteAdminSettings *ra = NULL;
        hr = INetFwProfile_get_RemoteAdminSettings(std, &ra); P("RA_GET %08lx\n", hr);
        INetFwRemoteAdminSettings_get_Enabled(ra, &vb); INetFwRemoteAdminSettings_get_Scope(ra, &scope);
        INetFwRemoteAdminSettings_get_IpVersion(ra, &ipv);
        P("RA_DEF %d %d %d\n", vb != 0, scope, ipv);
        P("RA_SET %08lx\n", INetFwRemoteAdminSettings_put_Enabled(ra, VARIANT_TRUE));
        P("RA_ADDR %08lx\n", INetFwRemoteAdminSettings_put_RemoteAddresses(ra, SysAllocString(L"LocalSubnet")));
        P("RA_BADSCOPE %08lx\n", INetFwRemoteAdminSettings_put_Scope(ra, 8));
        INetFwRemoteAdminSettings_Release(ra);
        INetFwProfile_get_RemoteAdminSettings(std, &ra);
        INetFwRemoteAdminSettings_get_Enabled(ra, &vb); INetFwRemoteAdminSettings_get_Scope(ra, &scope);
        INetFwRemoteAdminSettings_get_RemoteAddresses(ra, &b);
        P("RA_BACK %d %d %s\n", vb != 0, scope, bs(b, buf)); SysFreeString(b);
        INetFwRemoteAdminSettings_Release(ra);
        INetFwProfile_get_RemoteAdminSettings(dom, &ra);
        INetFwRemoteAdminSettings_get_Enabled(ra, &vb); P("RA_DOMAIN %d\n", vb != 0);
        INetFwRemoteAdminSettings_Release(ra);
    }
    /* ICMP settings */
    {
        INetFwIcmpSettings *ic = NULL;
        hr = INetFwProfile_get_IcmpSettings(std, &ic); P("IC_GET %08lx\n", hr);
        INetFwIcmpSettings_get_AllowInboundEchoRequest(ic, &vb); P("IC_ECHO_DEF %d\n", vb != 0);
        P("IC_SET %08lx\n", INetFwIcmpSettings_put_AllowInboundEchoRequest(ic, VARIANT_TRUE));
        INetFwIcmpSettings_put_AllowRedirect(ic, VARIANT_TRUE);
        INetFwIcmpSettings_put_AllowOutboundPacketTooBig(ic, VARIANT_TRUE);
        INetFwIcmpSettings_Release(ic);
        INetFwProfile_get_IcmpSettings(std, &ic);
        {
            VARIANT_BOOL e, r, t, d, m;
            INetFwIcmpSettings_get_AllowInboundEchoRequest(ic, &e);
            INetFwIcmpSettings_get_AllowRedirect(ic, &r);
            INetFwIcmpSettings_get_AllowOutboundPacketTooBig(ic, &t);
            INetFwIcmpSettings_get_AllowInboundMaskRequest(ic, &m);
            INetFwIcmpSettings_get_AllowOutboundTimeExceeded(ic, &d);
            P("IC_BACK %d %d %d %d %d\n", e != 0, r != 0, t != 0, m != 0, d != 0);
        }
        INetFwIcmpSettings_Release(ic);
        INetFwProfile_get_IcmpSettings(dom, &ic);
        INetFwIcmpSettings_get_AllowInboundEchoRequest(ic, &vb); P("IC_DOMAIN %d\n", vb != 0);
        INetFwIcmpSettings_Release(ic);
    }

    /* service restriction and rules */
    {
        INetFwServiceRestriction *sr = NULL;
        INetFwRules *r1 = NULL, *r2 = NULL, *r3 = NULL;
        INetFwRule *rule = NULL;
        LONG c1, c2;
        CoCreateInstance(&CLSID_Pol2, NULL, CLSCTX_INPROC_SERVER, &IID_Pol2, (void **)&p2);
        hr = INetFwPolicy2_get_ServiceRestriction(p2, &sr); P("SR_GET %08lx\n", hr);
        INetFwServiceRestriction_ServiceRestricted(sr, SysAllocString(L"Spooler"), SysAllocString(L"C:\\x.exe"), &vb);
        P("SR_DEF %d\n", vb != 0);
        P("SR_ON %08lx\n", INetFwServiceRestriction_RestrictService(sr, SysAllocString(L"Spooler"), SysAllocString(L"C:\\x.exe"), VARIANT_TRUE, VARIANT_FALSE));
        INetFwServiceRestriction_ServiceRestricted(sr, SysAllocString(L"Spooler"), SysAllocString(L"C:\\x.exe"), &vb); P("SR_ON_GET %d\n", vb != 0);
        INetFwServiceRestriction_ServiceRestricted(sr, SysAllocString(L"Spooler"), SysAllocString(L"C:\\y.exe"), &vb); P("SR_OTHER %d\n", vb != 0);
        P("SR_BAD %08lx\n", INetFwServiceRestriction_RestrictService(sr, NULL, NULL, VARIANT_TRUE, VARIANT_FALSE));
        P("SR_OFF %08lx\n", INetFwServiceRestriction_RestrictService(sr, SysAllocString(L"Spooler"), SysAllocString(L"C:\\x.exe"), VARIANT_FALSE, VARIANT_FALSE));
        INetFwServiceRestriction_ServiceRestricted(sr, SysAllocString(L"Spooler"), SysAllocString(L"C:\\x.exe"), &vb); P("SR_OFF_GET %d\n", vb != 0);
        INetFwServiceRestriction_get_Rules(sr, &r1);
        INetFwPolicy2_get_Rules(p2, &r2);
        P("SR_DISTINCT %d\n", r1 != r2);
        CoCreateInstance(&CLSID_Rule, NULL, CLSCTX_INPROC_SERVER, &IID_Rule, (void **)&rule);
        INetFwRule_put_Name(rule, SysAllocString(L"Restricted svc rule"));
        INetFwRule_put_Protocol(rule, 6); INetFwRule_put_LocalPorts(rule, SysAllocString(L"4455"));
        INetFwRule_put_Action(rule, NET_FW_ACTION_BLOCK); INetFwRule_put_Enabled(rule, VARIANT_TRUE);
        P("SR_RULE_ADD %08lx\n", INetFwRules_Add(r1, rule));
        INetFwRules_get_Count(r1, &c1); INetFwRules_get_Count(r2, &c2);
        P("SR_RULE_COUNTS %ld %ld\n", c1, c2);
        INetFwServiceRestriction_get_Rules(sr, &r3); INetFwRules_get_Count(r3, &c1); P("SR_RULE_AGAIN %ld\n", c1);
        INetFwRules_Release(r3);
        INetFwRules_Remove(r1, SysAllocString(L"Restricted svc rule"));
        INetFwRules_get_Count(r1, &c1); P("SR_RULE_REMOVED %ld\n", c1);
        INetFwRule_Release(rule);

        /* rule interfaces */
        CoCreateInstance(&CLSID_Rule, NULL, CLSCTX_INPROC_SERVER, &IID_Rule, (void **)&rule);
        INetFwRule_put_Name(rule, SysAllocString(L"Iface rule"));
        {
            VARIANT v; SAFEARRAY *sa; LONG i; VARIANT e;
            VariantInit(&v);
            INetFwRule_get_Interfaces(rule, &v); P("IF_DEF %d\n", V_VT(&v));
            sa = SafeArrayCreateVector(VT_VARIANT, 0, 2);
            for (i = 0; i < 2; i++) { V_VT(&e) = VT_BSTR; V_BSTR(&e) = SysAllocString(i ? L"Wi-Fi" : L"Ethernet"); SafeArrayPutElement(sa, &i, &e); VariantClear(&e); }
            V_VT(&v) = VT_ARRAY | VT_VARIANT; V_ARRAY(&v) = sa;
            P("IF_PUT %08lx\n", INetFwRule_put_Interfaces(rule, v));
            VariantClear(&v);
            INetFwRule_get_Interfaces(rule, &v);
            {
                LONG lo = 0, hi = -1; VARIANT x; char t1[100] = "", t2[100] = "";
                if (V_VT(&v) == (VT_ARRAY | VT_VARIANT)) { SafeArrayGetLBound(V_ARRAY(&v), 1, &lo); SafeArrayGetUBound(V_ARRAY(&v), 1, &hi);
                    for (i = lo; i <= hi; i++) { VariantInit(&x); SafeArrayGetElement(V_ARRAY(&v), &i, &x); bs(V_BSTR(&x), i == lo ? t1 : t2); VariantClear(&x); } }
                P("IF_GET %d %ld %s %s\n", V_VT(&v), hi - lo + 1, t1, t2);
            }
            VariantClear(&v);
            V_VT(&v) = VT_I4; V_I4(&v) = 5;
            P("IF_BAD %08lx\n", INetFwRule_put_Interfaces(rule, v));
            VariantInit(&v);
            INetFwRule_get_Interfaces(rule, &v); P("IF_KEPT %d\n", V_VT(&v)); VariantClear(&v);
            VariantInit(&v); /* VT_EMPTY clears */
            P("IF_CLEAR %08lx\n", INetFwRule_put_Interfaces(rule, v));
            INetFwRule_get_Interfaces(rule, &v); P("IF_CLEARED %d\n", V_VT(&v));
        }
        INetFwRule_Release(rule);
        INetFwRules_Release(r1); INetFwRules_Release(r2); INetFwServiceRestriction_Release(sr);
    }
    /* NAT event manager */
    {
        IUPnPNAT *nat = NULL; INATEventManager *mgr2 = NULL; struct cb c1 = { { &cb_vtbl }, 1 };
        hr = CoCreateInstance(&CLSID_NAT, NULL, CLSCTX_INPROC_SERVER, &IID_NAT, (void **)&nat);
        P("NAT_CREATE %08lx\n", hr);
        if (SUCCEEDED(hr))
        {
            { void *dyn = (void *)1; hr = nat->v->slot2(nat, &dyn); P("NAT_DYN %08lx %d\n", hr, dyn != NULL); }
            hr = nat->v->slot3(nat, (void **)&mgr2); P("NAT_MGR %08lx\n", hr);
            P("NAT_PUT_IP %08lx\n", mgr2->v->put_ip(mgr2, &c1.iface));
            P("NAT_REF1 %ld\n", c1.ref);
            P("NAT_PUT_N %08lx\n", mgr2->v->put_n(mgr2, &c1.iface));
            P("NAT_REF2 %ld\n", c1.ref);
            mgr2->v->put_ip(mgr2, NULL);
            P("NAT_REF3 %ld\n", c1.ref);
            mgr2->v->release(mgr2);
            P("NAT_REF4 %ld\n", c1.ref);
            nat->v->release(nat);
        }
    }
    P("DONE\n");
    return 0;
}
