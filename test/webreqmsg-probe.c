/* webreqmsg-probe PORT: an HttpResponseMessage that HttpClient.SendRequestAsync (and GetAsync) hands back
 * names the request it answers (RequestMessage), as on Windows. React Native for Windows (Office's
 * react-native-win32.dll, the Copilot button in Excel, PowerPoint, Outlook) reads
 * response.RequestMessage().RequestUri() and crashed on the NULL (wine-sg 5929).
 * The WinRT interfaces are called through raw vtable slots. */
#include <windows.h>
#include <roapi.h>
#include <winstring.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

typedef HRESULT (WINAPI *F1)(void *, void *);
typedef HRESULT (WINAPI *F2)(void *, void *, void *);
typedef HRESULT (WINAPI *F3)(void *, void *, void *, void *);
#define SLOT(o, n) ((*(void ***)(o))[n])
#define C1(o, n, a) ((F1)SLOT(o, n))(o, (void *)(a))
#define C2(o, n, a, b) ((F2)SLOT(o, n))(o, (void *)(a), (void *)(b))
#define C3(o, n, a, b, c) ((F3)SLOT(o, n))(o, (void *)(a), (void *)(b), (void *)(c))
#define REL(o) do { if (o) { ((ULONG (WINAPI *)(void *))SLOT(o, 2))(o); (o) = NULL; } } while (0)
#define GUIDDEF(name, a, b, c, d0, d1, d2, d3, d4, d5, d6, d7) \
    static const GUID name = { a, b, c, { d0, d1, d2, d3, d4, d5, d6, d7 } }
GUIDDEF(IID_UriFactory, 0x44a9796f, 0x723e, 0x4fdf, 0xa2, 0x18, 0x03, 0x3e, 0x75, 0xb0, 0xc0, 0x84);
GUIDDEF(IID_UriClass, 0x9e365e57, 0x48b2, 0x4160, 0x95, 0x6f, 0xc7, 0x38, 0x51, 0x20, 0xbb, 0xfc);
GUIDDEF(IID_Client, 0x7fda1151, 0x3574, 0x4880, 0xa8, 0xba, 0xe6, 0xb1, 0xe0, 0x06, 0x1f, 0x3d);
GUIDDEF(IID_RequestFactory, 0x5bac994e, 0x3886, 0x412e, 0xae, 0xc3, 0x52, 0xec, 0x7f, 0x25, 0x61, 0x6f);
GUIDDEF(IID_MethodFactory, 0x3c51d10d, 0x36d7, 0x40f8, 0xa8, 0x6d, 0xe7, 0x59, 0xca, 0xf2, 0xf8, 0x3f);
GUIDDEF(IID_Response, 0xfee200fb, 0x8664, 0x44e0, 0x95, 0xd9, 0x42, 0x69, 0x61, 0x99, 0xbf, 0xfc);
GUIDDEF(IID_Request, 0xf5762b3c, 0x74d4, 0x4811, 0xb5, 0xdc, 0x9f, 0x8b, 0x4e, 0x2f, 0x9a, 0xbf);
GUIDDEF(IID_Unknown_, 0x00000000, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);

/* IHttpResponseMessage: 12 get_RequestMessage, 13 put_RequestMessage; IHttpRequestMessage: 12 get_RequestUri;
 * IUriRuntimeClass: 6 get_AbsoluteUri; IHttpClient: 7 GetAsync(uri), 14 SendRequestAsync(request);
 * IAsyncOperationWithProgress: 10 GetResults */
static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
static HSTRING hs(const WCHAR *s) { HSTRING h = NULL; WindowsCreateString(s, wcslen(s), &h); return h; }
static void *factory(const WCHAR *name, const GUID *iid)
{
    HSTRING h = hs(name); void *f = NULL;
    RoGetActivationFactory(h, iid, &f);
    WindowsDeleteString(h);
    return f;
}
static void *activate(const WCHAR *name)
{
    HSTRING h = hs(name); void *obj = NULL;
    RoActivateInstance(h, (IInspectable **)&obj);
    WindowsDeleteString(h);
    return obj;
}
static void *qi(void *o, const GUID *iid) { void *r = NULL; if (o) C2(o, 0, iid, &r); return r; }
static BOOL same(void *a, void *b)
{
    void *ua = qi(a, &IID_Unknown_), *ub = qi(b, &IID_Unknown_);
    BOOL r = ua && ua == ub;
    REL(ua); REL(ub);
    return r;
}
static BOOL uri_is(void *request, const WCHAR *want)
{
    void *uri = NULL, *cls;
    HSTRING h = NULL;
    BOOL r = FALSE;
    if (!request || FAILED(C1(request, 12, &uri)) || !uri) return FALSE;
    cls = qi(uri, &IID_UriClass);
    if (cls && SUCCEEDED(C1(cls, 6, &h))) r = !wcscmp(WindowsGetStringRawBuffer(h, NULL), want);
    WindowsDeleteString(h);
    REL(cls); REL(uri);
    return r;
}
static void *make_uri(const WCHAR *url)
{
    void *f = factory(L"Windows.Foundation.Uri", &IID_UriFactory), *uri = NULL;
    HSTRING h = hs(url);
    if (f) C2(f, 6, h, &uri);
    WindowsDeleteString(h);
    REL(f);
    return uri;
}

int main(int argc, char **argv)
{
    int port = argc > 1 ? atoi(argv[1]) : 0;
    void *client, *clientI, *mf, *rf, *method = NULL, *uri, *request = NULL, *op = NULL, *response = NULL, *resp, *got = NULL;
    WCHAR url[200];
    HSTRING h;
    HRESULT hr;

    RoInitialize(RO_INIT_MULTITHREADED);
    swprintf(url, 200, L"http://127.0.0.1:%d/hello", port);
    client = activate(L"Windows.Web.Http.HttpClient");
    clientI = qi(client, &IID_Client);
    mf = factory(L"Windows.Web.Http.HttpMethod", &IID_MethodFactory);
    rf = factory(L"Windows.Web.Http.HttpRequestMessage", &IID_RequestFactory);
    check(clientI && mf && rf, "HttpClient, HttpMethod and HttpRequestMessage are there");
    if (!clientI || !mf || !rf) goto out;

    /* SendRequestAsync: the response names the very request sent */
    h = hs(L"GET");
    C2(mf, 6, h, &method);
    WindowsDeleteString(h);
    uri = make_uri(url);
    hr = C3(rf, 6, method, uri, &request);
    check(hr == S_OK && request, "GET request message");
    REL(uri);
    hr = C2(clientI, 14, request, &op);
    check(hr == S_OK && op, "SendRequestAsync");
    hr = op ? C1(op, 10, &response) : E_FAIL;
    check(hr == S_OK && response, "response");
    resp = qi(response, &IID_Response);
    hr = resp ? C1(resp, 12, &got) : E_FAIL;
    check(hr == S_OK && got, "response.RequestMessage is set");
    check(got && same(got, request), "response.RequestMessage is the request sent");
    check(uri_is(got, url), "response.RequestMessage.RequestUri is the URL asked for");
    REL(got);
    /* put_RequestMessage replaces it, NULL clears it */
    hr = resp ? C1(resp, 13, NULL) : E_FAIL;
    got = (void *)1;
    if (resp) C1(resp, 12, &got);
    check(hr == S_OK && !got, "RequestMessage = NULL clears it");
    hr = resp ? C1(resp, 13, request) : E_FAIL;
    got = NULL;
    if (resp) C1(resp, 12, &got);
    check(hr == S_OK && got && same(got, request), "RequestMessage can be set");
    REL(got); REL(resp); REL(response); REL(op);

    /* GetAsync builds its own request: the response still names one, with the URL */
    uri = make_uri(url);
    hr = C2(clientI, 7, uri, &op);
    check(hr == S_OK && op, "GetAsync");
    hr = op ? C1(op, 10, &response) : E_FAIL;
    resp = qi(response, &IID_Response);
    got = NULL;
    if (resp) C1(resp, 12, &got);
    check(got != NULL, "GetAsync: response.RequestMessage is set");
    check(uri_is(got, url), "GetAsync: its RequestUri is the URL asked for");
    REL(got); REL(resp); REL(response); REL(op); REL(uri);

out:
    REL(request); REL(method); REL(rf); REL(mf); REL(clientI); REL(client);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
