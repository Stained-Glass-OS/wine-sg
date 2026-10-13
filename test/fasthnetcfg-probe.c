#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <oleauto.h>
#include <netfw.h>
#include <stdio.h>

static const CLSID CLSID_Mgr  = {0x304ce942,0x6e39,0x40d8,{0x94,0x3a,0xb9,0x13,0xc4,0x0c,0x9c,0xd4}};
static const CLSID CLSID_Pol2 = {0xe2b3c97f,0x6ae1,0x41ac,{0x81,0x7a,0xf6,0xf9,0x21,0x66,0xd7,0xdd}};
static const IID IID_Mgr = {0xf7898af5,0xcac4,0x4632,{0xa2,0xec,0xda,0x06,0xe5,0x11,0x1a,0xf2}};
static const IID IID_Pol2 = {0x98325047,0xc671,0x4174,{0x8d,0x81,0xde,0xfc,0xd3,0xf0,0x31,0x86}};
static const CLSID CLSID_Rule = {0x2c5bc43e,0x3369,0x4c33,{0xab,0x0c,0xbe,0x94,0x69,0x67,0x7a,0xf4}};
static const CLSID CLSID_App = {0xec9846b3,0x2762,0x4a6b,{0xa2,0x14,0x6a,0xcb,0x60,0x34,0x62,0xd2}};
static const IID IID_App = {0xb5e64ffa,0xc2c5,0x444e,{0xa3,0x01,0xfb,0x5e,0x00,0x01,0x80,0x50}};
static const CLSID CLSID_Port = {0x0ca545c6,0x37ad,0x4a6c,{0xbf,0x92,0x9f,0x76,0x10,0x06,0x7e,0xf5}};
static const IID IID_Port = {0xe0483ba0,0x47ff,0x4d9c,{0xa6,0xd6,0x77,0x41,0xd0,0xb1,0x95,0xf7}};
static const IID IID_Rule = {0xaf230d27,0xbaba,0x4e42,{0xac,0xed,0xf5,0x24,0xf2,0x2c,0xfc,0xe2}};

#define P(...) printf(__VA_ARGS__)
static int icmp_set(const WCHAR *name, DWORD v)
{
    HKEY k; LONG e = RegCreateKeyExW(HKEY_LOCAL_MACHINE,
        L"System\\CurrentControlSet\\Services\\SharedAccess\\Parameters\\FirewallPolicy\\StandardProfile\\IcmpSettings",
        0,NULL,0,KEY_SET_VALUE,NULL,&k,NULL);
    if (e) return e;
    e = RegSetValueExW(k,name,0,REG_DWORD,(BYTE*)&v,4); RegCloseKey(k); return e;
}
static BOOL pa(INetFwMgr *m, int port, int proto, const WCHAR *app, BOOL *restricted)
{
    VARIANT a, r; BSTR b = app ? SysAllocString(app) : NULL; HRESULT hr;
    VariantInit(&a); VariantInit(&r);
    hr = INetFwMgr_IsPortAllowed(m, b, NET_FW_IP_VERSION_V4, port, NULL, proto, &a, &r);
    SysFreeString(b);
    if (restricted) *restricted = FAILED(hr) ? 0 : V_BOOL(&r) != 0;
    return SUCCEEDED(hr) && V_BOOL(&a) != 0;
}
static int ic(INetFwMgr *m, int type)
{
    VARIANT a, r; VariantInit(&a); VariantInit(&r);
    if (FAILED(INetFwMgr_IsIcmpTypeAllowed(m, NET_FW_IP_VERSION_V4, NULL, type, &a, &r))) return -1;
    return V_BOOL(&a) != 0;
}

int main(int argc, char **argv)
{
    INetFwMgr *mgr; INetFwPolicy2 *p2; INetFwPolicy *pol; INetFwProfile *dom, *std, *cur;
    VARIANT_BOOL b, b2; NET_FW_ACTION act; NET_FW_PROFILE_TYPE t; HRESULT hr; VARIANT v; BOOL restr;
    INetFwRules *rules; INetFwRule *rule; LONG count;
    int nonadmin = argc > 1;

    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_Mgr,NULL,CLSCTX_INPROC_SERVER,&IID_Mgr,(void**)&mgr);
    if (FAILED(hr)) { P("NOMGR %08lx\n", hr); return 1; }
    hr = CoCreateInstance(&CLSID_Pol2,NULL,CLSCTX_INPROC_SERVER,&IID_Pol2,(void**)&p2);
    if (FAILED(hr)) { P("NOPOL2 %08lx\n", hr); return 1; }

    if (nonadmin)
    {
        hr = INetFwPolicy2_RestoreLocalFirewallDefaults(p2); P("RESTORE_NA %08lx\n", hr);
        hr = INetFwMgr_get_LocalPolicy(mgr,&pol);
        INetFwPolicy_GetProfileByType(pol,NET_FW_PROFILE_STANDARD,&std);
        hr = INetFwProfile_put_FirewallEnabled(std, VARIANT_TRUE); P("ENABLE_NA %08lx\n", hr);
        P("DONE\n"); return 0;
    }

    /* defaults */
    INetFwPolicy2_get_BlockAllInboundTraffic(p2,NET_FW_PROFILE2_PRIVATE,&b); P("D_BLOCKALL %d\n", b!=0);
    INetFwPolicy2_get_NotificationsDisabled(p2,NET_FW_PROFILE2_PRIVATE,&b); P("D_NOTIF %d\n", b!=0);
    INetFwPolicy2_get_UnicastResponsesToMulticastBroadcastDisabled(p2,NET_FW_PROFILE2_PUBLIC,&b); P("D_UNI %d\n", b!=0);
    INetFwPolicy2_get_DefaultInboundAction(p2,NET_FW_PROFILE2_PUBLIC,&act); P("D_IN %d\n", act);
    INetFwPolicy2_get_DefaultOutboundAction(p2,NET_FW_PROFILE2_PUBLIC,&act); P("D_OUT %d\n", act);
    hr = INetFwPolicy2_get_BlockAllInboundTraffic(p2,NET_FW_PROFILE2_ALL,&b); P("D_ALLGET %08lx\n", hr);
    P("D_ICMP8 %d ICMP3 %d\n", ic(mgr,8), ic(mgr,3));

    /* set and read back, by profile */
    hr = INetFwPolicy2_put_BlockAllInboundTraffic(p2,NET_FW_PROFILE2_PUBLIC,VARIANT_TRUE); P("S_BLOCK %08lx\n", hr);
    INetFwPolicy2_get_BlockAllInboundTraffic(p2,NET_FW_PROFILE2_PUBLIC,&b); P("S_BLOCK_PUB %d\n", b!=0);
    INetFwPolicy2_get_BlockAllInboundTraffic(p2,NET_FW_PROFILE2_PRIVATE,&b); P("S_BLOCK_PRIV %d\n", b!=0);
    hr = INetFwPolicy2_put_DefaultOutboundAction(p2,NET_FW_PROFILE2_ALL,NET_FW_ACTION_BLOCK); P("S_OUT %08lx\n", hr);
    INetFwPolicy2_get_DefaultOutboundAction(p2,NET_FW_PROFILE2_DOMAIN,&act); P("S_OUT_DOM %d\n", act);
    INetFwPolicy2_get_DefaultOutboundAction(p2,NET_FW_PROFILE2_PRIVATE,&act); P("S_OUT_PRIV %d\n", act);
    hr = INetFwPolicy2_put_DefaultInboundAction(p2,NET_FW_PROFILE2_PRIVATE,7); P("S_BADACT %08lx\n", hr);
    hr = INetFwPolicy2_put_NotificationsDisabled(p2,NET_FW_PROFILE2_PRIVATE,VARIANT_TRUE);
    INetFwPolicy2_get_NotificationsDisabled(p2,NET_FW_PROFILE2_PRIVATE,&b); P("S_NOTIF %d\n", b!=0);

    /* excluded interfaces */
    {
        SAFEARRAY *sa = SafeArrayCreateVector(VT_VARIANT,0,2); VARIANT *e; VARIANT in; LONG n;
        SafeArrayAccessData(sa,(void**)&e);
        V_VT(&e[0])=VT_BSTR; V_BSTR(&e[0])=SysAllocString(L"Ethernet");
        V_VT(&e[1])=VT_BSTR; V_BSTR(&e[1])=SysAllocString(L"Wi-Fi");
        SafeArrayUnaccessData(sa);
        VariantInit(&in); V_VT(&in)=VT_ARRAY|VT_VARIANT; V_ARRAY(&in)=sa;
        hr = INetFwPolicy2_put_ExcludedInterfaces(p2,NET_FW_PROFILE2_PRIVATE,in); P("X_PUT %08lx\n", hr);
        VariantClear(&in);
        VariantInit(&v);
        hr = INetFwPolicy2_get_ExcludedInterfaces(p2,NET_FW_PROFILE2_PRIVATE,&v);
        n = 0; if (SUCCEEDED(hr) && V_VT(&v)==(VT_ARRAY|VT_VARIANT)) { LONG lo,hi; SafeArrayGetLBound(V_ARRAY(&v),1,&lo); SafeArrayGetUBound(V_ARRAY(&v),1,&hi); n = hi-lo+1;
            { VARIANT *el; SafeArrayAccessData(V_ARRAY(&v),(void**)&el); P("X_ITEMS %d %ls %ls\n", (int)n, V_BSTR(&el[0]), V_BSTR(&el[1])); SafeArrayUnaccessData(V_ARRAY(&v)); } }
        else P("X_ITEMS 0\n");
        VariantClear(&v);
        VariantInit(&v);
        hr = INetFwPolicy2_get_ExcludedInterfaces(p2,NET_FW_PROFILE2_PUBLIC,&v); P("X_PUB %08lx %d\n", hr, V_VT(&v));
        VariantClear(&v);
    }

    /* the older interfaces */
    INetFwMgr_get_CurrentProfileType(mgr,&t); P("L_CURTYPE %d\n", t);
    INetFwMgr_get_LocalPolicy(mgr,&pol);
    hr = INetFwPolicy_GetProfileByType(pol,NET_FW_PROFILE_DOMAIN,&dom); P("L_GETDOM %08lx\n", hr);
    hr = INetFwPolicy_GetProfileByType(pol,NET_FW_PROFILE_STANDARD,&std);
    hr = INetFwPolicy_GetProfileByType(pol,(NET_FW_PROFILE_TYPE)9,&cur); P("L_BADTYPE %08lx\n", hr);
    INetFwPolicy_get_CurrentProfile(pol,&cur);
    INetFwProfile_get_Type(dom,&t); P("L_DOMTYPE %d\n", t);
    INetFwProfile_get_Type(std,&t); P("L_STDTYPE %d\n", t);
    INetFwProfile_get_Type(cur,&t); P("L_CURPTYPE %d\n", t);
    hr = INetFwProfile_put_ExceptionsNotAllowed(dom,VARIANT_TRUE); P("L_PUTEXC %08lx\n", hr);
    INetFwProfile_get_ExceptionsNotAllowed(dom,&b); P("L_EXC_DOM %d\n", b!=0);
    INetFwProfile_get_ExceptionsNotAllowed(std,&b); P("L_EXC_STD %d\n", b!=0);
    INetFwPolicy2_get_BlockAllInboundTraffic(p2,NET_FW_PROFILE2_DOMAIN,&b); P("L_EXC_P2DOM %d\n", b!=0);
    INetFwProfile_put_NotificationsDisabled(std,VARIANT_TRUE);
    INetFwPolicy2_get_NotificationsDisabled(p2,NET_FW_PROFILE2_PUBLIC,&b); P("L_NOTIF_PUB %d\n", b!=0);
    INetFwProfile_get_FirewallEnabled(std,&b); P("L_FWON_STD %d\n", b!=0);
    INetFwProfile_get_FirewallEnabled(dom,&b); P("L_FWON_DOM %d\n", b!=0);
    hr = INetFwProfile_put_FirewallEnabled(std,VARIANT_FALSE); P("L_PUTFW %08lx\n", hr);

    /* is a port allowed */
    P("PA_NONE %d\n", pa(mgr,3400,NET_FW_IP_PROTOCOL_TCP,NULL,NULL));
    INetFwPolicy2_put_BlockAllInboundTraffic(p2,NET_FW_PROFILE2_ALL,VARIANT_FALSE);
    INetFwProfile_put_ExceptionsNotAllowed(dom,VARIANT_FALSE);
    INetFwPolicy2_get_Rules(p2,&rules);
    CoCreateInstance(&CLSID_Rule,NULL,CLSCTX_INPROC_SERVER,&IID_Rule,(void**)&rule);
    { BSTR n = SysAllocString(L"Gate port"); INetFwRule_put_Name(rule,n); SysFreeString(n);
      n = SysAllocString(L"3400,4000-4010"); INetFwRule_put_LocalPorts(rule,n); SysFreeString(n); }
    INetFwRule_put_Protocol(rule,NET_FW_IP_PROTOCOL_TCP); INetFwRule_put_Direction(rule,NET_FW_RULE_DIR_IN);
    INetFwRule_put_Action(rule,NET_FW_ACTION_ALLOW); INetFwRule_put_Profiles(rule,NET_FW_PROFILE2_ALL); INetFwRule_put_Enabled(rule,VARIANT_TRUE);
    hr = INetFwRules_Add(rules,rule); P("RULE_ADD %08lx\n", hr);
    /* the status file says private is the current network, on */
    P("PA_3400 %d PA_4005 %d PA_3401 %d PA_UDP %d\n", pa(mgr,3400,NET_FW_IP_PROTOCOL_TCP,NULL,&restr), pa(mgr,4005,NET_FW_IP_PROTOCOL_TCP,NULL,NULL),
      pa(mgr,3401,NET_FW_IP_PROTOCOL_TCP,NULL,NULL), pa(mgr,3400,NET_FW_IP_PROTOCOL_UDP,NULL,NULL));
    P("PA_RESTR %d\n", restr);
    INetFwPolicy2_put_BlockAllInboundTraffic(p2,NET_FW_PROFILE2_PRIVATE,VARIANT_TRUE);
    P("PA_BLOCKALL %d\n", pa(mgr,3400,NET_FW_IP_PROTOCOL_TCP,NULL,NULL));
    INetFwPolicy2_put_BlockAllInboundTraffic(p2,NET_FW_PROFILE2_PRIVATE,VARIANT_FALSE);
    { BSTR n = SysAllocString(L"10.0.0.0/8"); INetFwRule_put_RemoteAddresses(rule,n); SysFreeString(n); }
    pa(mgr,3400,NET_FW_IP_PROTOCOL_TCP,NULL,&restr); P("PA_SCOPED %d\n", restr);
    { BSTR n = SysAllocString(L"C:\\Gate\\app.exe"); INetFwRule_put_ApplicationName(rule,n); SysFreeString(n); }
    P("PA_APP %d PA_OTHERAPP %d\n", pa(mgr,3400,NET_FW_IP_PROTOCOL_TCP,L"C:\\gate\\APP.EXE",NULL), pa(mgr,3400,NET_FW_IP_PROTOCOL_TCP,L"C:\\other.exe",NULL));
    hr = INetFwMgr_IsPortAllowed(mgr,NULL,NET_FW_IP_VERSION_V4,70000,NULL,NET_FW_IP_PROTOCOL_TCP,&v,&v); P("PA_BADPORT %08lx\n", hr);

    /* icmp */
    icmp_set(L"AllowInboundEchoRequest",1);
    P("ICMP8 %d ICMP13 %d ICMP99 %d\n", ic(mgr,8), ic(mgr,13), ic(mgr,99));

    /* restore */
    INetFwRules_get_Count(rules,&count); P("PRE_COUNT %ld\n", count);
    hr = INetFwPolicy2_RestoreLocalFirewallDefaults(p2); P("RESTORE %08lx\n", hr);
    INetFwRules_get_Count(rules,&count); P("POST_COUNT %ld\n", count);
    INetFwPolicy2_get_BlockAllInboundTraffic(p2,NET_FW_PROFILE2_PUBLIC,&b); P("POST_BLOCK %d\n", b!=0);
    INetFwPolicy2_get_DefaultOutboundAction(p2,NET_FW_PROFILE2_PRIVATE,&act); P("POST_OUT %d\n", act);
    VariantInit(&v); INetFwPolicy2_get_ExcludedInterfaces(p2,NET_FW_PROFILE2_PRIVATE,&v); P("POST_X %d\n", V_VT(&v));
    hr = INetFwMgr_RestoreDefaults(mgr); P("MGR_RESTORE %08lx\n", hr);

    /* the older collections: authorized applications and open ports */
    {
        INetFwAuthorizedApplications *apps; INetFwOpenPorts *ports; INetFwAuthorizedApplication *app, *app2;
        INetFwOpenPort *port, *port2; BSTR b; NET_FW_SCOPE sc; IUnknown *en; IEnumVARIANT *ev; VARIANT it; ULONG got; LONG cnt;
        INetFwProfile_get_AuthorizedApplications(cur,&apps);
        INetFwProfile_get_GloballyOpenPorts(cur,&ports);
        INetFwAuthorizedApplications_get_Count(apps,&cnt); P("LA_COUNT0 %ld\n", cnt);
        CoCreateInstance(&CLSID_App,NULL,CLSCTX_INPROC_SERVER,&IID_App,(void**)&app);
        b = SysAllocString(L"C:\\Gate\\legacy.exe"); INetFwAuthorizedApplication_put_ProcessImageFileName(app,b); SysFreeString(b);
        b = SysAllocString(L"Legacy app"); INetFwAuthorizedApplication_put_Name(app,b); SysFreeString(b);
        INetFwAuthorizedApplication_get_Scope(app,&sc); P("LA_SCOPE0 %d\n", sc);
        INetFwAuthorizedApplication_put_Scope(app,NET_FW_SCOPE_LOCAL_SUBNET);
        INetFwAuthorizedApplication_get_RemoteAddresses(app,&b); P("LA_REMOTE %ls\n", b); SysFreeString(b);
        hr = INetFwAuthorizedApplications_Add(apps,app); P("LA_ADD %08lx\n", hr);
        INetFwAuthorizedApplications_get_Count(apps,&cnt); P("LA_COUNT1 %ld\n", cnt);
        b = SysAllocString(L"c:\\gate\\LEGACY.EXE");
        hr = INetFwAuthorizedApplications_Item(apps,b,&app2); P("LA_ITEM %08lx\n", hr);
        if (SUCCEEDED(hr)) {
            BSTR n, r; INetFwAuthorizedApplication_get_Name(app2,&n); INetFwAuthorizedApplication_get_RemoteAddresses(app2,&r);
            INetFwAuthorizedApplication_get_Scope(app2,&sc); P("LA_ITEMDATA %ls|%ls|%d\n", n, r, sc); SysFreeString(n); SysFreeString(r);
            INetFwAuthorizedApplication_Release(app2);
        }
        SysFreeString(b);
        hr = INetFwAuthorizedApplications_get__NewEnum(apps,&en);
        if (SUCCEEDED(hr)) { IUnknown_QueryInterface(en,&IID_IEnumVARIANT,(void**)&ev); VariantInit(&it);
            hr = IEnumVARIANT_Next(ev,1,&it,&got); P("LA_ENUM %08lx %lu %d\n", hr, got, V_VT(&it)); VariantClear(&it);
            hr = IEnumVARIANT_Next(ev,1,&it,&got); P("LA_ENUM2 %08lx %lu\n", hr, got); IEnumVARIANT_Release(ev); IUnknown_Release(en); }
        b = SysAllocString(L"C:\\no\\such.exe"); hr = INetFwAuthorizedApplications_Item(apps,b,&app2); P("LA_NOITEM %08lx\n", hr); SysFreeString(b);
        b = SysAllocString(L"C:\\GATE\\legacy.exe"); hr = INetFwAuthorizedApplications_Remove(apps,b); P("LA_REMOVE %08lx\n", hr); SysFreeString(b);
        INetFwAuthorizedApplications_get_Count(apps,&cnt); P("LA_COUNT2 %ld\n", cnt);

        INetFwOpenPorts_get_Count(ports,&cnt); P("LP_COUNT0 %ld\n", cnt);
        CoCreateInstance(&CLSID_Port,NULL,CLSCTX_INPROC_SERVER,&IID_Port,(void**)&port);
        INetFwOpenPort_put_Port(port,5060); INetFwOpenPort_put_Protocol(port,NET_FW_IP_PROTOCOL_UDP);
        b = SysAllocString(L"Legacy port"); INetFwOpenPort_put_Name(port,b); SysFreeString(b);
        b = SysAllocString(L"192.168.1.0/24"); INetFwOpenPort_put_RemoteAddresses(port,b); SysFreeString(b);
        INetFwOpenPort_get_Scope(port,&sc); P("LP_SCOPE %d\n", sc);
        INetFwOpenPort_get_BuiltIn(port,&b2); P("LP_BUILTIN %d\n", b2!=0);
        hr = INetFwOpenPorts_Add(ports,port); P("LP_ADD %08lx\n", hr);
        INetFwOpenPorts_get_Count(ports,&cnt); P("LP_COUNT1 %ld\n", cnt);
        hr = INetFwOpenPorts_Item(ports,5060,NET_FW_IP_PROTOCOL_UDP,&port2); P("LP_ITEM %08lx\n", hr);
        if (SUCCEEDED(hr)) { BSTR n, r; INetFwOpenPort_get_Name(port2,&n); INetFwOpenPort_get_RemoteAddresses(port2,&r); P("LP_ITEMDATA %ls|%ls\n", n, r); SysFreeString(n); SysFreeString(r); INetFwOpenPort_Release(port2); }
        hr = INetFwOpenPorts_Item(ports,5060,NET_FW_IP_PROTOCOL_TCP,&port2); P("LP_NOITEM %08lx\n", hr);
        hr = INetFwOpenPorts_get__NewEnum(ports,&en);
        if (SUCCEEDED(hr)) { IUnknown_QueryInterface(en,&IID_IEnumVARIANT,(void**)&ev); VariantInit(&it);
            hr = IEnumVARIANT_Next(ev,1,&it,&got); P("LP_ENUM %08lx %lu %d\n", hr, got, V_VT(&it)); VariantClear(&it); IEnumVARIANT_Release(ev); IUnknown_Release(en); }
        hr = INetFwOpenPorts_Remove(ports,5060,NET_FW_IP_PROTOCOL_UDP); P("LP_REMOVE %08lx\n", hr);
        INetFwOpenPorts_get_Count(ports,&cnt); P("LP_COUNT2 %ld\n", cnt);
    }
    P("DONE\n");
    return 0;
}
