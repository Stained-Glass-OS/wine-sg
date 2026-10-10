/* windows.web batch (patches/sg/2037), run by test/webverbs-gate.sh. Families:
 * HttpClient.GetAsync / GetAsync with an option / DeleteAsync / PostAsync /
 * PutAsync against a local server (argument: its port): the verb on the
 * wire, the body sent, the status and the body of the response. The WinRT
 * interfaces are declared here by hand.
 *
 *   webverbs-probe.exe PORT */
#define COBJMACROS
#include <windows.h>
#include <roapi.h>
#include <winstring.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

typedef struct { HRESULT (WINAPI *QueryInterface)(void *, REFIID, void **); ULONG (WINAPI *AddRef)(void *); ULONG (WINAPI *Release)(void *);
                 HRESULT (WINAPI *GetIids)(void *, ULONG *, IID **); HRESULT (WINAPI *GetRuntimeClassName)(void *, HSTRING *); HRESULT (WINAPI *GetTrustLevel)(void *, int *); } Base;
typedef struct { const void *vtbl; } Obj;
typedef struct { Base b; HRESULT (WINAPI *CreateUri)(void *, HSTRING, void **); } UriFactoryVtbl;
typedef struct { Base b; HRESULT (WINAPI *CreateFromString)(void *, HSTRING, void **); } StringContentFactoryVtbl;
typedef struct { Base b; HRESULT (WINAPI *DeleteAsync)(void *, void *, void **); HRESULT (WINAPI *GetAsync)(void *, void *, void **);
                 HRESULT (WINAPI *GetWithOptionAsync)(void *, void *, int, void **); HRESULT (WINAPI *GetBufferAsync)(void *, void *, void **);
                 HRESULT (WINAPI *GetInputStreamAsync)(void *, void *, void **); HRESULT (WINAPI *GetStringAsync)(void *, void *, void **);
                 HRESULT (WINAPI *PostAsync)(void *, void *, void *, void **); HRESULT (WINAPI *PutAsync)(void *, void *, void *, void **); } ClientVtbl;
typedef struct { Base b; HRESULT (WINAPI *put_Progress)(void *, void *); HRESULT (WINAPI *get_Progress)(void *, void **);
                 HRESULT (WINAPI *put_Completed)(void *, void *); HRESULT (WINAPI *get_Completed)(void *, void **); HRESULT (WINAPI *GetResults)(void *, void *); } AsyncVtbl;
typedef struct { Base b; HRESULT (WINAPI *get_Content)(void *, void **); HRESULT (WINAPI *put_Content)(void *, void *); HRESULT (WINAPI *get_Headers)(void *, void **);
                 HRESULT (WINAPI *get_IsSuccessStatusCode)(void *, unsigned char *); HRESULT (WINAPI *get_ReasonPhrase)(void *, HSTRING *);
                 HRESULT (WINAPI *put_ReasonPhrase)(void *, HSTRING); HRESULT (WINAPI *get_RequestMessage)(void *, void **); HRESULT (WINAPI *put_RequestMessage)(void *, void *);
                 HRESULT (WINAPI *get_Source)(void *, int *); HRESULT (WINAPI *put_Source)(void *, int); HRESULT (WINAPI *get_StatusCode)(void *, int *); } ResponseVtbl;
typedef struct { Base b; HRESULT (WINAPI *get_Headers)(void *, void **); HRESULT (WINAPI *BufferAllAsync)(void *, void **); HRESULT (WINAPI *ReadAsBufferAsync)(void *, void **);
                 HRESULT (WINAPI *ReadAsInputStreamAsync)(void *, void **); HRESULT (WINAPI *ReadAsStringAsync)(void *, void **); } ContentVtbl;

#define VT(type, o) ((const type *)((Obj *)(o))->vtbl)
#define BASE(o) VT(Base, o)

static const GUID IID_Client = { 0x7fda1151, 0x3574, 0x4880, { 0xa8, 0xba, 0xe6, 0xb1, 0xe0, 0x06, 0x1f, 0x3d } };
static const GUID IID_UriFactory = { 0x44a9796f, 0x723e, 0x4fdf, { 0xa2, 0x18, 0x03, 0x3e, 0x75, 0xb0, 0xc0, 0x84 } };
static const GUID IID_StringContentFactory = { 0x46649d5b, 0x2e93, 0x48eb, { 0x8e, 0x61, 0x19, 0x67, 0x78, 0x78, 0xe5, 0x7f } };
static const GUID IID_Content = { 0x6b14a441, 0xfba7, 0x4bd2, { 0xaf, 0x0a, 0x83, 0x9d, 0xe7, 0xc2, 0x95, 0xda } };
static const GUID IID_Response = { 0xfee200fb, 0x8664, 0x44e0, { 0x95, 0xd9, 0x42, 0x69, 0x61, 0x99, 0xbf, 0xfc } };
static const GUID IID_OpResponse = { 0xf5e2fc07, 0x1d68, 0x5d16, { 0x82, 0xc6, 0x5e, 0xd2, 0x0e, 0xd6, 0xb4, 0x10 } };

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static void *activation_factory(const WCHAR *name, const GUID *iid)
{
    HSTRING h;
    void *f = NULL;
    WindowsCreateString(name, wcslen(name), &h);
    RoGetActivationFactory(h, iid, &f);
    WindowsDeleteString(h);
    return f;
}

static void *make_uri(int port, const char *path)
{
    void *factory = activation_factory(L"Windows.Foundation.Uri", &IID_UriFactory), *uri = NULL;
    WCHAR url[200];
    HSTRING h;
    swprintf(url, 200, L"http://127.0.0.1:%d%hs", port, path);
    WindowsCreateString(url, wcslen(url), &h);
    VT(UriFactoryVtbl, factory)->CreateUri(factory, h, &uri);
    WindowsDeleteString(h);
    BASE(factory)->Release(factory);
    return uri;
}

static void *make_content(const WCHAR *text)
{
    void *factory = activation_factory(L"Windows.Web.Http.HttpStringContent", &IID_StringContentFactory), *content = NULL;
    HSTRING h;
    WindowsCreateString(text, wcslen(text), &h);
    VT(StringContentFactoryVtbl, factory)->CreateFromString(factory, h, &content);
    WindowsDeleteString(h);
    BASE(factory)->Release(factory);
    return content;
}

/* the status and the body of the response an operation completes with */
static HRESULT read_response(void *op, int *status, char *body, size_t size)
{
    void *response = NULL, *content = NULL, *str_op = NULL;
    HRESULT hr;
    HSTRING str = NULL;
    void *resp_iface = NULL;

    *status = 0;
    body[0] = 0;
    if (!op) return E_POINTER;
    hr = VT(AsyncVtbl, op)->GetResults(op, &response);
    if (FAILED(hr) || !response) return FAILED(hr) ? hr : E_POINTER;
    BASE(response)->QueryInterface(response, &IID_Response, &resp_iface);
    if (resp_iface)
    {
        VT(ResponseVtbl, resp_iface)->get_StatusCode(resp_iface, status);
        VT(ResponseVtbl, resp_iface)->get_Content(resp_iface, &content);
        if (content)
        {
            if (SUCCEEDED(VT(ContentVtbl, content)->ReadAsStringAsync(content, &str_op)) && str_op)
            {
                if (SUCCEEDED(VT(AsyncVtbl, str_op)->GetResults(str_op, &str)) && str)
                {
                    UINT32 len;
                    const WCHAR *w = WindowsGetStringRawBuffer(str, &len);
                    WideCharToMultiByte(CP_UTF8, 0, w, len, body, size - 1, NULL, NULL);
                    body[WideCharToMultiByte(CP_UTF8, 0, w, len, NULL, 0, NULL, NULL) < (int)size - 1 ?
                            WideCharToMultiByte(CP_UTF8, 0, w, len, NULL, 0, NULL, NULL) : size - 1] = 0;
                    WindowsDeleteString(str);
                }
                BASE(str_op)->Release(str_op);
            }
            BASE(content)->Release(content);
        }
        BASE(resp_iface)->Release(resp_iface);
    }
    BASE(response)->Release(response);
    return S_OK;
}

int main(int argc, char **argv)
{
    int port = argc > 1 ? atoi(argv[1]) : 0, status;
    void *client = NULL, *client_if = NULL, *op = NULL, *uri, *content;
    HSTRING cls;
    char body[256];
    HRESULT hr;

    RoInitialize(RO_INIT_MULTITHREADED);
    WindowsCreateString(L"Windows.Web.Http.HttpClient", 27, &cls);
    RoActivateInstance(cls, (IInspectable **)&client);
    WindowsDeleteString(cls);
    if (client) BASE(client)->QueryInterface(client, &IID_Client, &client_if);
    check(client_if != NULL && port != 0, "an HttpClient and a port");
    if (!client_if) { printf("RESULT: FAIL\n"); return 1; }

    uri = make_uri(port, "/thing");
    check(uri != NULL, "a Uri");

    hr = VT(ClientVtbl, client_if)->GetAsync(client_if, uri, &op);
    check(hr == S_OK && op != NULL, "GetAsync starts");
    hr = read_response(op, &status, body, sizeof(body));
    check(hr == S_OK && status == 200 && !strcmp(body, "GET /thing"), "GetAsync: the server saw GET and answered 200");
    if (strcmp(body, "GET /thing")) printf("   status %d body [%s]\n", status, body);
    if (op) BASE(op)->Release(op);

    op = NULL;
    hr = VT(ClientVtbl, client_if)->GetWithOptionAsync(client_if, uri, 1, &op);
    hr = read_response(op, &status, body, sizeof(body));
    check(status == 200 && !strcmp(body, "GET /thing"), "GetWithOptionAsync is a GET too");
    if (op) BASE(op)->Release(op);

    op = NULL;
    hr = VT(ClientVtbl, client_if)->DeleteAsync(client_if, uri, &op);
    check(hr == S_OK, "DeleteAsync starts");
    hr = read_response(op, &status, body, sizeof(body));
    check(status == 200 && !strcmp(body, "DELETE /thing"), "DeleteAsync: the server saw DELETE");
    if (op) BASE(op)->Release(op);

    content = make_content(L"payload \xe9");
    op = NULL;
    hr = VT(ClientVtbl, client_if)->PostAsync(client_if, uri, content, &op);
    check(hr == S_OK, "PostAsync starts");
    hr = read_response(op, &status, body, sizeof(body));
    check(status == 200 && !strcmp(body, "POST /thing payload \xc3\xa9"), "PostAsync: the server saw POST and the body");
    if (strncmp(body, "POST", 4)) printf("   status %d body [%s]\n", status, body);
    if (op) BASE(op)->Release(op);

    op = NULL;
    hr = VT(ClientVtbl, client_if)->PutAsync(client_if, uri, content, &op);
    check(hr == S_OK, "PutAsync starts");
    hr = read_response(op, &status, body, sizeof(body));
    check(status == 200 && !strcmp(body, "PUT /thing payload \xc3\xa9"), "PutAsync: the server saw PUT and the body");
    if (op) BASE(op)->Release(op);
    BASE(content)->Release(content);

    BASE(uri)->Release(uri);
    uri = make_uri(port, "/missing");
    op = NULL;
    hr = VT(ClientVtbl, client_if)->GetAsync(client_if, uri, &op);
    read_response(op, &status, body, sizeof(body));
    check(status == 404, "a missing page is 404");
    if (op) BASE(op)->Release(op);
    BASE(uri)->Release(uri);

    op = NULL;
    hr = VT(ClientVtbl, client_if)->GetAsync(client_if, NULL, &op);
    check(FAILED(hr) && op == NULL, "GetAsync without a Uri fails");

    BASE(client_if)->Release(client_if);
    BASE(client)->Release(client);
    printf("%d checks failed\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
