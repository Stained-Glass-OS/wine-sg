/* wsfault-probe.c: webservices WsCreateFaultFromError, WsSetFaultErrorDetail and WsSendFaultMessageForError
 * (patches/sg/6077), run by test/wsfault-gate.sh. */
#include <winsock2.h>
#include <windows.h>
#include <webservices.h>
#include <stdio.h>
#include <string.h>

static int bad, total;
#define EXPECT(c) do { total++; if (!(c)) { bad++; printf("MISMATCH line %d: %s\n", __LINE__, #c); } } while (0)

#define PORT 47237
#ifndef WS_E_ENDPOINT_FAULT_RECEIVED
#define WS_E_ENDPOINT_FAULT_RECEIVED ((HRESULT)0x803d0013)
#endif

static HRESULT (WINAPI *pWsCreateFaultFromError)(WS_ERROR *, HRESULT, WS_FAULT_DISCLOSURE, WS_HEAP *, WS_FAULT *);
static HRESULT (WINAPI *pWsSetFaultErrorDetail)(WS_ERROR *, const WS_FAULT_DETAIL_DESCRIPTION *, WS_WRITE_OPTION,
                                                const void *, ULONG);
static HRESULT (WINAPI *pWsSendFaultMessageForError)(WS_CHANNEL *, WS_MESSAGE *, WS_ERROR *, HRESULT,
                                                     WS_FAULT_DISCLOSURE, WS_MESSAGE *, WS_ERROR *);

static WS_XML_STRING s_req = {3, (BYTE *)"req"}, s_val = {3, (BYTE *)"val"}, s_ns = {6, (BYTE *)"urn:sg"},
                     s_act = {7, (BYTE *)"act_req"}, s_info = {4, (BYTE *)"Info"},
                     s_fault_act = {14, (BYTE *)"urn:sg:failure"};
struct in { INT32 val; };
static WS_FIELD_DESCRIPTION f_in;
static WS_FIELD_DESCRIPTION *fl_in[] = {&f_in};
static WS_STRUCT_DESCRIPTION st_in = {sizeof(struct in), 4, fl_in, 1, &s_req, &s_ns};
static WS_ELEMENT_DESCRIPTION e_in = {&s_req, &s_ns, WS_STRUCT_TYPE, &st_in};
static WS_MESSAGE_DESCRIPTION m_in = {&s_act, &e_in};
static WS_ELEMENT_DESCRIPTION e_info = {&s_info, &s_ns, WS_INT32_TYPE, NULL};
static WS_FAULT_DETAIL_DESCRIPTION d_info = {&s_fault_act, &e_info};

static BOOL has(const WS_STRING *s, const WCHAR *what)
{
    WCHAR buf[512];
    int n = min(s->length, ARRAYSIZE(buf) - 1);
    memcpy(buf, s->chars, n * sizeof(WCHAR));
    buf[n] = 0;
    return wcsstr(buf, what) != NULL;
}

static HRESULT server_hr = E_FAIL;
static HANDLE server_ready;
static WS_ENVELOPE_VERSION server_env;

static DWORD WINAPI server_proc(void *arg)
{
    WS_CHANNEL_PROPERTY prop = {WS_CHANNEL_PROPERTY_ENVELOPE_VERSION, &server_env, sizeof(server_env)};
    WS_LISTENER_PROPERTY lprop = {WS_LISTENER_PROPERTY_CHANNEL_TYPE, NULL, 0};
    WS_LISTENER *listener;
    WS_CHANNEL *channel;
    WS_MESSAGE *req, *rsp;
    WS_ERROR *err;
    WS_HEAP *heap;
    WS_STRING url;
    WS_STRING str = {12, (WCHAR *)L"disk is full"};
    WCHAR buf[64];
    struct in in = {0};
    INT32 info = 77;
    HRESULT hr;

    (void)lprop;
    url.length = wsprintfW(buf, L"http://127.0.0.1:%u/svc", PORT);
    url.chars = buf;
    if ((hr = WsCreateListener(WS_CHANNEL_TYPE_REPLY, WS_HTTP_CHANNEL_BINDING, NULL, 0, NULL, &listener, NULL)) ||
        (hr = WsOpenListener(listener, &url, NULL, NULL)) ||
        (hr = WsCreateChannelForListener(listener, &prop, 1, &channel, NULL)))
    {
        server_hr = hr;
        SetEvent(server_ready);
        return 1;
    }
    SetEvent(server_ready);
    WsCreateHeap(1 << 16, 0, NULL, 0, &heap, NULL);
    hr = WsAcceptChannel(listener, channel, NULL, NULL);
    if (!hr) hr = WsCreateMessageForChannel(channel, NULL, 0, &req, NULL);
    if (!hr) hr = WsCreateMessageForChannel(channel, NULL, 0, &rsp, NULL);
    if (!hr) hr = WsReadMessageStart(channel, req, NULL, NULL);
    if (!hr) hr = WsReadBody(req, &e_in, WS_READ_REQUIRED_VALUE, heap, &in, sizeof(in), NULL);
    if (!hr) hr = WsReadMessageEnd(channel, req, NULL, NULL);
    if (!hr)
    {
        /* the operation failed: its error goes back as a fault, with a detail */
        WsCreateError(NULL, 0, &err);
        WsAddErrorString(err, &str);
        pWsSetFaultErrorDetail(err, &d_info, WS_WRITE_REQUIRED_VALUE, &info, sizeof(info));
        hr = pWsSendFaultMessageForError(channel, rsp, err, E_FAIL, WS_FULL_FAULT_DISCLOSURE, req, NULL);
        WsFreeError(err);
    }
    server_hr = hr;
    WsFreeHeap(heap);
    WsCloseChannel(channel, NULL, NULL);
    WsFreeChannel(channel);
    WsCloseListener(listener, NULL, NULL);
    WsFreeListener(listener);
    return 0;
}

static void test_fault_reply(WS_ENVELOPE_VERSION env)
{
    WS_CHANNEL_PROPERTY prop = {WS_CHANNEL_PROPERTY_ENVELOPE_VERSION, &env, sizeof(env)};
    WS_ENDPOINT_ADDRESS ep;
    WS_CHANNEL *channel;
    WS_MESSAGE *req, *rsp;
    WS_ERROR *err;
    WS_HEAP *heap;
    WS_FAULT *fault = NULL;
    WS_XML_STRING action = {0};
    struct in in = {5};
    WCHAR url[64];
    HANDLE thread;
    INT32 info = 0;
    HRESULT hr;

    server_env = env;
    server_hr = E_FAIL;
    ResetEvent(server_ready);
    thread = CreateThread(NULL, 0, server_proc, NULL, 0, NULL);
    WaitForSingleObject(server_ready, 5000);

    WsCreateHeap(1 << 16, 0, NULL, 0, &heap, NULL);
    WsCreateError(NULL, 0, &err);
    hr = WsCreateChannel(WS_CHANNEL_TYPE_REQUEST, WS_HTTP_CHANNEL_BINDING, &prop, 1, NULL, &channel, NULL);
    EXPECT(hr == S_OK);
    memset(&ep, 0, sizeof(ep));
    ep.url.length = wsprintfW(url, L"http://127.0.0.1:%u/svc", PORT);
    ep.url.chars = url;
    hr = WsOpenChannel(channel, &ep, NULL, NULL);
    EXPECT(hr == S_OK);
    WsCreateMessageForChannel(channel, NULL, 0, &req, NULL);
    WsCreateMessageForChannel(channel, NULL, 0, &rsp, NULL);
    hr = WsRequestReply(channel, req, &m_in, WS_WRITE_REQUIRED_VALUE, &in, sizeof(in), rsp, &m_in,
                        WS_READ_REQUIRED_VALUE, heap, &in, sizeof(in), NULL, err);
    printf("env %d: request %#lx\n", env, hr);
    EXPECT(hr == WS_E_ENDPOINT_FAULT_RECEIVED);
    hr = WsGetFaultErrorProperty(err, WS_FAULT_ERROR_PROPERTY_FAULT, &fault, sizeof(fault));
    EXPECT(hr == S_OK && fault);
    if (fault)
    {
        EXPECT(fault->reasonCount >= 1 && has(&fault->reasons[0].text, L"disk is full"));
        EXPECT(fault->code && fault->code->value.localName.length);
    }
    WsGetFaultErrorProperty(err, WS_FAULT_ERROR_PROPERTY_ACTION, &action, sizeof(action));
    EXPECT(action.length == s_fault_act.length && !memcmp(action.bytes, s_fault_act.bytes, action.length));
    hr = WsGetFaultErrorDetail(err, &d_info, WS_READ_REQUIRED_VALUE, heap, &info, sizeof(info));
    EXPECT(hr == S_OK && info == 77);

    WaitForSingleObject(thread, 10000);
    CloseHandle(thread);
    printf("server %#lx\n", server_hr);
    EXPECT(server_hr == S_OK);
    WsFreeMessage(req);
    WsFreeMessage(rsp);
    WsCloseChannel(channel, NULL, NULL);
    WsFreeChannel(channel);
    WsFreeError(err);
    WsFreeHeap(heap);
}

int main(void)
{
    HMODULE ws = LoadLibraryA("webservices.dll");
    WS_STRING s1 = {13, (WCHAR *)L"first problem"}, s2 = {14, (WCHAR *)L"second problem"};
    WS_ERROR *err;
    WS_HEAP *heap;
    WS_FAULT fault, given;
    WS_FAULT_CODE code;
    WS_FAULT_REASON reason;
    WS_XML_STRING act = {0}, cname = {6, (BYTE *)"Sender"}, cns = {39, (BYTE *)"http://www.w3.org/2003/05/soap-envelope"};
    WSADATA wsa;
    INT32 info = 0, value = 42;
    HRESULT hr;

    pWsCreateFaultFromError = (void *)GetProcAddress(ws, "WsCreateFaultFromError");
    pWsSetFaultErrorDetail = (void *)GetProcAddress(ws, "WsSetFaultErrorDetail");
    pWsSendFaultMessageForError = (void *)GetProcAddress(ws, "WsSendFaultMessageForError");
    EXPECT(pWsCreateFaultFromError && pWsSetFaultErrorDetail && pWsSendFaultMessageForError);
    if (!pWsCreateFaultFromError || !pWsSetFaultErrorDetail || !pWsSendFaultMessageForError)
    {
        printf("RESULT: FAIL\n");
        return 1;
    }
    f_in.mapping = WS_ELEMENT_FIELD_MAPPING; f_in.localName = &s_val; f_in.ns = &s_ns; f_in.type = WS_INT32_TYPE;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    WsCreateHeap(1 << 16, 0, NULL, 0, &heap, NULL);

    /* arguments */
    WsCreateError(NULL, 0, &err);
    EXPECT(pWsCreateFaultFromError(err, S_OK, WS_FULL_FAULT_DISCLOSURE, heap, &fault) == E_INVALIDARG);
    EXPECT(pWsCreateFaultFromError(NULL, E_FAIL, WS_FULL_FAULT_DISCLOSURE, heap, &fault) == E_INVALIDARG);
    EXPECT(pWsCreateFaultFromError(err, E_FAIL, WS_FULL_FAULT_DISCLOSURE, NULL, &fault) == E_INVALIDARG);
    EXPECT(pWsCreateFaultFromError(err, E_FAIL, 7, heap, &fault) == E_INVALIDARG);

    /* no strings: the text of the failure code; a generic fault code */
    memset(&fault, 0, sizeof(fault));
    hr = pWsCreateFaultFromError(err, E_OUTOFMEMORY, WS_FULL_FAULT_DISCLOSURE, heap, &fault);
    EXPECT(hr == S_OK);
    EXPECT(fault.reasonCount == 1 && has(&fault.reasons[0].text, L"memory"));
    EXPECT(fault.reasonCount == 1 && fault.reasons[0].lang.length > 0);
    EXPECT(fault.code && fault.code->value.localName.length && !fault.detail);

    /* the error's strings, oldest first; minimal disclosure hides them */
    WsAddErrorString(err, &s1);
    WsAddErrorString(err, &s2);
    memset(&fault, 0, sizeof(fault));
    hr = pWsCreateFaultFromError(err, E_FAIL, WS_FULL_FAULT_DISCLOSURE, heap, &fault);
    EXPECT(hr == S_OK && fault.reasonCount == 1);
    EXPECT(has(&fault.reasons[0].text, L"first problem second problem"));
    memset(&fault, 0, sizeof(fault));
    hr = pWsCreateFaultFromError(err, E_FAIL, WS_MINIMAL_FAULT_DISCLOSURE, heap, &fault);
    EXPECT(hr == S_OK && fault.reasonCount == 1);
    EXPECT(!has(&fault.reasons[0].text, L"problem") && fault.reasons[0].text.length > 0);
    WsFreeError(err);

    /* a fault the error carries is used as it is */
    WsCreateError(NULL, 0, &err);
    memset(&given, 0, sizeof(given));
    memset(&code, 0, sizeof(code));
    code.value.localName = cname;
    code.value.ns = cns;
    reason.text.chars = (WCHAR *)L"custom reason";
    reason.text.length = 13;
    reason.lang.chars = (WCHAR *)L"en-US";
    reason.lang.length = 5;
    given.code = &code;
    given.reasons = &reason;
    given.reasonCount = 1;
    WsSetFaultErrorProperty(err, WS_FAULT_ERROR_PROPERTY_FAULT, &given, sizeof(given));
    memset(&fault, 0, sizeof(fault));
    hr = pWsCreateFaultFromError(err, E_FAIL, WS_MINIMAL_FAULT_DISCLOSURE, heap, &fault);
    EXPECT(hr == S_OK && fault.reasonCount == 1 && has(&fault.reasons[0].text, L"custom reason"));
    EXPECT(fault.code && fault.code->value.localName.length == 6 && !memcmp(fault.code->value.localName.bytes, "Sender", 6));
    WsFreeError(err);

    /* the detail: written into the error's fault, read back, with its action */
    WsCreateError(NULL, 0, &err);
    hr = pWsSetFaultErrorDetail(err, &d_info, WS_WRITE_REQUIRED_VALUE, &value, sizeof(value));
    EXPECT(hr == S_OK);
    hr = WsGetFaultErrorDetail(err, &d_info, WS_READ_REQUIRED_VALUE, heap, &info, sizeof(info));
    EXPECT(hr == S_OK && info == 42);
    WsGetFaultErrorProperty(err, WS_FAULT_ERROR_PROPERTY_ACTION, &act, sizeof(act));
    EXPECT(act.length == s_fault_act.length && !memcmp(act.bytes, s_fault_act.bytes, act.length));
    memset(&fault, 0, sizeof(fault));
    hr = pWsCreateFaultFromError(err, E_FAIL, WS_FULL_FAULT_DISCLOSURE, heap, &fault);
    EXPECT(hr == S_OK && fault.detail != NULL);
    EXPECT(pWsSetFaultErrorDetail(err, NULL, WS_WRITE_REQUIRED_VALUE, &value, sizeof(value)) == E_INVALIDARG);
    WsFreeError(err);

    /* a fault reply over HTTP, SOAP 1.2 and 1.1 */
    server_ready = CreateEventA(NULL, TRUE, FALSE, NULL);
    test_fault_reply(WS_ENVELOPE_VERSION_SOAP_1_2);
    test_fault_reply(WS_ENVELOPE_VERSION_SOAP_1_1);
    EXPECT(pWsSendFaultMessageForError(NULL, NULL, NULL, E_FAIL, WS_FULL_FAULT_DISCLOSURE, NULL, NULL) == E_INVALIDARG);

    WsFreeHeap(heap);
    printf("checks %d, mismatches %d\n", total, bad);
    printf("RESULT: %s\n", bad ? "FAIL" : "PASS");
    return bad != 0;
}
