/* wsauth-probe.c: HTTP header authentication on a webservices HTTP channel (wine-sg 7811), for
 * test/wsauth-gate.sh, against wsauth-server.py on 127.0.0.1:PORT */
#include <windows.h>
#include <webservices.h>
#include <stdio.h>

#define PORT 18391

/* the security binding structures, as the documentation describes them (named apart, in case the
 * header in use has them) */
typedef struct { WS_SECURITY_BINDING binding; void *clientCredential; } SG_HTTP_HEADER_AUTH_BINDING;
typedef struct { ULONG credentialType; } SG_CRED;
typedef struct { SG_CRED credential; WS_STRING username, password, domain; } SG_STRING_CRED;
#define SG_STRING_CRED_TYPE   1
#define SG_DEFAULT_CRED_TYPE  2
#define SG_HTTP_HEADER_AUTH_BINDING_TYPE  2   /* WS_HTTP_HEADER_AUTH_SECURITY_BINDING_TYPE */
#define SG_SCHEME_BASIC  0x2
#define SG_SCHEME_NTLM   0x8
#define SG_PROP_SCHEME   6                    /* WS_SECURITY_BINDING_PROPERTY_HTTP_HEADER_AUTH_SCHEME */

static WS_XML_STRING s_req = {3, (BYTE *)"req"}, s_val = {3, (BYTE *)"val"}, s_ns = {6, (BYTE *)"urn:sg"},
                     s_act = {7, (BYTE *)"act_req"};
struct in { INT32 val; };
static WS_FIELD_DESCRIPTION f_in;
static WS_FIELD_DESCRIPTION *fl_in[] = {&f_in};
static WS_STRUCT_DESCRIPTION st_in = {sizeof(struct in), 4, fl_in, 1, &s_req, &s_ns};
static WS_ELEMENT_DESCRIPTION e_in = {&s_req, &s_ns, WS_STRUCT_TYPE, &st_in};
static WS_MESSAGE_DESCRIPTION m_in = {&s_act, &e_in};

static WS_STRING wstr(const WCHAR *s) { WS_STRING r = { lstrlenW(s), (WCHAR *)s }; return r; }

/* one request to PATH with the given credential (NULL: no security description); prints TAG=hr val */
static void call(const char *tag, const WCHAR *path, SG_CRED *cred, ULONG schemes)
{
    WS_ENVELOPE_VERSION env = WS_ENVELOPE_VERSION_SOAP_1_2;
    WS_CHANNEL_PROPERTY prop = { WS_CHANNEL_PROPERTY_ENVELOPE_VERSION, &env, sizeof(env) };
    WS_SECURITY_BINDING_PROPERTY bprop = { SG_PROP_SCHEME, &schemes, sizeof(schemes) };
    SG_HTTP_HEADER_AUTH_BINDING binding;
    WS_SECURITY_BINDING *bindings[1] = { (WS_SECURITY_BINDING *)&binding };
    WS_SECURITY_DESCRIPTION desc = { bindings, 1, NULL, 0 };
    WS_CHANNEL *channel = NULL;
    WS_MESSAGE *req = NULL, *rsp = NULL;
    WS_ENDPOINT_ADDRESS ep;
    WS_HEAP *heap;
    WCHAR url[100];
    struct in in = { 1234 };
    HRESULT hr;

    memset(&binding, 0, sizeof(binding));
    binding.binding.bindingType = SG_HTTP_HEADER_AUTH_BINDING_TYPE;
    binding.binding.properties = schemes ? &bprop : NULL;
    binding.binding.propertyCount = schemes ? 1 : 0;
    binding.clientCredential = cred;
    WsCreateHeap(1 << 16, 0, NULL, 0, &heap, NULL);
    hr = WsCreateChannel(WS_CHANNEL_TYPE_REQUEST, WS_HTTP_CHANNEL_BINDING, &prop, 1, cred ? &desc : NULL,
                         &channel, NULL);
    if (hr) { printf("%s=create %#lx\n", tag, hr); WsFreeHeap(heap); return; }
    memset(&ep, 0, sizeof(ep));
    ep.url.length = wsprintfW(url, L"http://127.0.0.1:%u%s", PORT, path);
    ep.url.chars = url;
    hr = WsOpenChannel(channel, &ep, NULL, NULL);
    if (!hr) hr = WsCreateMessageForChannel(channel, NULL, 0, &req, NULL);
    if (!hr) hr = WsCreateMessageForChannel(channel, NULL, 0, &rsp, NULL);
    if (!hr) hr = WsRequestReply(channel, req, &m_in, WS_WRITE_REQUIRED_VALUE, &in, sizeof(in), rsp, &m_in,
                                 WS_READ_REQUIRED_VALUE, heap, &in, sizeof(in), NULL, NULL);
    printf("%s=%#lx %d\n", tag, hr, hr ? 0 : in.val);
    if (req) WsFreeMessage(req);
    if (rsp) WsFreeMessage(rsp);
    WsCloseChannel(channel, NULL, NULL);
    WsFreeChannel(channel);
    WsFreeHeap(heap);
}

/* the same through a service proxy: WsCreateServiceProxy with the security description (how 0), or
 * WsCreateServiceProxyFromTemplate with the header auth template, its credential (1) or none (2) */
typedef struct { WS_SECURITY_BINDING_PROPERTIES props; void *clientCredential; } SG_AUTH_TMPL;
typedef struct { WS_CHANNEL_PROPERTIES channel; WS_SECURITY_PROPERTIES security; SG_AUTH_TMPL auth; } SG_HEADER_AUTH_TMPL;
#define SG_HTTP_HEADER_AUTH_BINDING_TEMPLATE_TYPE 2   /* after HTTP and HTTP_SSL */

static void proxy_call(const char *tag, const WCHAR *path, SG_CRED *cred, ULONG schemes, int how)
{
    WS_ENVELOPE_VERSION env = WS_ENVELOPE_VERSION_SOAP_1_2;
    WS_CHANNEL_PROPERTY prop = { WS_CHANNEL_PROPERTY_ENVELOPE_VERSION, &env, sizeof(env) };
    WS_SECURITY_BINDING_PROPERTY bprop = { SG_PROP_SCHEME, &schemes, sizeof(schemes) };
    SG_HTTP_HEADER_AUTH_BINDING binding;
    WS_SECURITY_BINDING *bindings[1] = { (WS_SECURITY_BINDING *)&binding };
    WS_SECURITY_DESCRIPTION desc = { bindings, 1, NULL, 0 };
    WS_PARAMETER_DESCRIPTION param[2] = {{WS_PARAMETER_TYPE_NORMAL, 0, 0xffff}, {WS_PARAMETER_TYPE_NORMAL, 0xffff, 0}};
    WS_OPERATION_DESCRIPTION op = {1, &m_in, &m_in, 0, 0, 2, param, NULL, WS_NON_RPC_LITERAL_OPERATION};
    SG_HEADER_AUTH_TMPL tmpl;
    BYTE policy[512];
    WS_SERVICE_PROXY *proxy = NULL;
    WS_ENDPOINT_ADDRESS ep;
    WS_HEAP *heap;
    WCHAR url[100];
    INT32 val = 1234, outval = 0, *pout = &outval;
    const void *args[2] = { &val, &pout };   /* an output parameter's argument points at its pointer */
    HRESULT hr;

    memset(&binding, 0, sizeof(binding));
    binding.binding.bindingType = SG_HTTP_HEADER_AUTH_BINDING_TYPE;
    binding.binding.properties = &bprop;
    binding.binding.propertyCount = 1;
    binding.clientCredential = cred;
    memset(&tmpl, 0, sizeof(tmpl));
    tmpl.channel.properties = &prop;
    tmpl.channel.propertyCount = 1;
    tmpl.auth.props.properties = &bprop;
    tmpl.auth.props.propertyCount = 1;
    tmpl.auth.clientCredential = how == 1 ? cred : NULL;
    memset(policy, 0, sizeof(policy));
    WsCreateHeap(1 << 16, 0, NULL, 0, &heap, NULL);
    if (!how)
        hr = WsCreateServiceProxy(WS_CHANNEL_TYPE_REQUEST, WS_HTTP_CHANNEL_BINDING, &desc, NULL, 0, &prop, 1,
                                  &proxy, NULL);
    else
        hr = WsCreateServiceProxyFromTemplate(WS_CHANNEL_TYPE_REQUEST, NULL, 0,
                                              SG_HTTP_HEADER_AUTH_BINDING_TEMPLATE_TYPE, &tmpl, sizeof(tmpl),
                                              policy, sizeof(policy), &proxy, NULL);
    if (hr) { printf("%s=create %#lx\n", tag, hr); WsFreeHeap(heap); return; }
    memset(&ep, 0, sizeof(ep));
    ep.url.length = wsprintfW(url, L"http://127.0.0.1:%u%s", PORT, path);
    ep.url.chars = url;
    hr = WsOpenServiceProxy(proxy, &ep, NULL, NULL);
    if (!hr)
    {
        hr = WsCall(proxy, &op, args, heap, NULL, 0, NULL, NULL);
        WsCloseServiceProxy(proxy, NULL, NULL);
    }
    printf("%s=%#lx %d\n", tag, hr, hr ? 0 : outval);
    WsFreeServiceProxy(proxy);
    WsFreeHeap(heap);
}

int main(void)
{
    SG_STRING_CRED good = { { SG_STRING_CRED_TYPE }, {0}, {0}, {0} }, bad = good;
    SG_CRED defaults = { SG_DEFAULT_CRED_TYPE }, unknown = { 77 };

    setvbuf(stdout, NULL, _IONBF, 0);
    f_in.mapping = WS_ELEMENT_FIELD_MAPPING;
    f_in.localName = &s_val;
    f_in.ns = &s_ns;
    f_in.type = WS_INT32_TYPE;
    good.username = wstr(L"sgtest");
    good.password = wstr(L"sgpass");
    bad.username = wstr(L"sgtest");
    bad.password = wstr(L"wrong");

    call("NoCred", L"/svc", NULL, 0);
    call("Basic", L"/svc", &good.credential, SG_SCHEME_BASIC);
    call("WrongPass", L"/svc", &bad.credential, SG_SCHEME_BASIC);
    call("NtlmOnly", L"/svc", &good.credential, SG_SCHEME_NTLM);
    call("DefaultsBasic", L"/svc", &defaults, SG_SCHEME_BASIC | SG_SCHEME_NTLM);
    call("Deny", L"/deny", &good.credential, SG_SCHEME_BASIC);
    call("Missing", L"/nothing", NULL, 0);
    call("BadCred", L"/svc", &unknown, SG_SCHEME_BASIC);
    call("BadScheme", L"/svc", &good.credential, 0x100);
    proxy_call("Proxy", L"/svc", &good.credential, SG_SCHEME_BASIC, 0);
    proxy_call("Template", L"/svc", &good.credential, SG_SCHEME_BASIC, 1);
    proxy_call("TemplateDefaults", L"/svc", NULL, SG_SCHEME_BASIC, 2);
    printf("Done=1\n");
    return 0;
}
