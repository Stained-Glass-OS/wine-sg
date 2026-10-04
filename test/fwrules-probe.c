/* fwrules-gate.sh's probe: a program adding a firewall rule as installers do
 * (INetFwPolicy2::Rules), finding it, listing it, changing it, removing it */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <netfw.h>
#include <stdio.h>

static const CLSID CLSID_NetFwPolicy2_ = {0xe2b3c97f,0x6ae1,0x41ac,{0x81,0x7a,0xf6,0xf9,0x21,0x66,0xd7,0xdd}};
static const CLSID CLSID_NetFwRule_ = {0x2c5bc43e,0x3369,0x4c33,{0xab,0x0c,0xbe,0x94,0x69,0x67,0x7a,0xf4}};

static int listed(INetFwRules *rules, const WCHAR *name)
{
    IUnknown *unk = NULL; IEnumVARIANT *en = NULL; VARIANT v; ULONG got; int found = 0;
    if (FAILED(INetFwRules_get__NewEnum(rules, &unk)) || !unk) return -1;
    if (FAILED(IUnknown_QueryInterface(unk, &IID_IEnumVARIANT, (void **)&en))) { IUnknown_Release(unk); return -1; }
    while (IEnumVARIANT_Next(en, 1, &v, &got) == S_OK && got) {
        INetFwRule *r; BSTR n = NULL;
        if (V_VT(&v) == VT_DISPATCH && SUCCEEDED(IDispatch_QueryInterface(V_DISPATCH(&v), &IID_INetFwRule, (void **)&r))) {
            if (SUCCEEDED(INetFwRule_get_Name(r, &n)) && n && !lstrcmpW(n, name)) found++;
            SysFreeString(n); INetFwRule_Release(r);
        }
        VariantClear(&v);
    }
    IEnumVARIANT_Release(en); IUnknown_Release(unk);
    return found;
}

int main(void)
{
    INetFwPolicy2 *pol; INetFwRules *rules; INetFwRule *rule, *got; LONG n0 = -1, n1 = -1, n2 = -1, l; BSTR s;
    VARIANT_BOOL en; HRESULT hr;
    BSTR name = SysAllocString(L"SG Gate Media Server");
    CoInitialize(NULL);
    if (FAILED(hr = CoCreateInstance(&CLSID_NetFwPolicy2_, NULL, CLSCTX_INPROC_SERVER, &IID_INetFwPolicy2, (void **)&pol))) { printf("NOPOLICY %08lx\n", hr); return 1; }
    if (FAILED(hr = INetFwPolicy2_get_Rules(pol, &rules))) { printf("NORULES %08lx\n", hr); return 1; }
    INetFwRules_get_Count(rules, &n0);
    printf("COUNT0 %ld\n", n0);
    printf("LISTED0 %d\n", listed(rules, name));
    if (FAILED(hr = CoCreateInstance(&CLSID_NetFwRule_, NULL, CLSCTX_INPROC_SERVER, &IID_INetFwRule, (void **)&rule))) { printf("NORULE %08lx\n", hr); return 1; }
    INetFwRule_put_Name(rule, name);
    INetFwRule_put_Description(rule, s = SysAllocString(L"Lets players find it")); SysFreeString(s);
    INetFwRule_put_ApplicationName(rule, s = SysAllocString(L"C:\\Program Files\\Gate\\server.exe")); SysFreeString(s);
    INetFwRule_put_Protocol(rule, 6);
    INetFwRule_put_LocalPorts(rule, s = SysAllocString(L"1400,3400")); SysFreeString(s);
    INetFwRule_put_Direction(rule, NET_FW_RULE_DIR_IN);
    INetFwRule_put_Action(rule, NET_FW_ACTION_ALLOW);
    INetFwRule_put_Profiles(rule, NET_FW_PROFILE2_PRIVATE | NET_FW_PROFILE2_DOMAIN);
    INetFwRule_put_Enabled(rule, VARIANT_TRUE);
    INetFwRule_put_Grouping(rule, s = SysAllocString(L"SG Gate Group")); SysFreeString(s);
    hr = INetFwRules_Add(rules, rule);
    printf("ADD %08lx\n", hr);
    INetFwRules_get_Count(rules, &n1);
    printf("COUNT1 %ld\n", n1);
    printf("LISTED1 %d\n", listed(rules, name));
    if (SUCCEEDED(hr = INetFwRules_Item(rules, name, &got))) {
        INetFwRule_get_ApplicationName(got, &s); printf("APP %ls\n", s ? s : L"(null)"); SysFreeString(s);
        INetFwRule_get_LocalPorts(got, &s); printf("PORTS %ls\n", s ? s : L"(null)"); SysFreeString(s);
        INetFwRule_get_Protocol(got, &l); printf("PROTO %ld\n", l);
        INetFwRule_get_Profiles(got, &l); printf("PROFILES %ld\n", l);
        INetFwRule_get_Enabled(got, &en); printf("ENABLED %d\n", en == VARIANT_TRUE);
        /* live: a change to the rule got is stored */
        INetFwRule_put_Enabled(got, VARIANT_FALSE);
        INetFwRule_Release(got);
        if (SUCCEEDED(INetFwRules_Item(rules, name, &got))) { INetFwRule_get_Enabled(got, &en); printf("ENABLED2 %d\n", en == VARIANT_TRUE); INetFwRule_Release(got); }
    } else printf("ITEM %08lx\n", hr);
    {   /* what installers ask the policy, and rule groups */
        VARIANT_BOOL on = 0; LONG prof = 0; BSTR g = SysAllocString(L"SG Gate Group");
        hr = INetFwPolicy2_get_FirewallEnabled(pol, NET_FW_PROFILE2_PRIVATE, &on); printf("FWON %08lx %d\n", hr, on == VARIANT_TRUE);
        hr = INetFwPolicy2_get_CurrentProfileTypes(pol, &prof); printf("PROFILE %08lx %ld\n", hr, prof);
        hr = INetFwPolicy2_EnableRuleGroup(pol, NET_FW_PROFILE2_ALL, g, VARIANT_TRUE); printf("GROUPON %08lx\n", hr);
        hr = INetFwPolicy2_IsRuleGroupEnabled(pol, NET_FW_PROFILE2_PRIVATE, g, &on); printf("GROUPENABLED %08lx %d\n", hr, on == VARIANT_TRUE);
        SysFreeString(g);
    }
    hr = INetFwRules_Remove(rules, name);
    INetFwRules_get_Count(rules, &n2);
    printf("REMOVE %08lx COUNT2 %ld\n", hr, n2);
    hr = INetFwRules_Item(rules, name, &got);
    printf("ITEMGONE %08lx\n", hr);
    printf("DONE\n");
    return 0;
}
