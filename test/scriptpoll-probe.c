/* scriptpoll-probe: the script engines ask their host whether a long-running script may go on, as Windows'
 * do -- IActiveScriptSiteInterruptPoll::QueryContinue on the site, about once a second -- and stop it when
 * the host says S_FALSE (wine-sg 7831; test/scriptpoll-gate.sh). For JScript and VBScript:
 *   stop: a site that answers S_OK twice, then S_FALSE: an endless loop ends (ParseScriptText fails) after
 *         about three seconds, the host asked three times;
 *   go:   a site that always answers S_OK: a loop of about two seconds runs to its end, the host asked;
 *   none: a site without the interface: the same loop runs to its end. */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <activscp.h>
#include <stdio.h>

static const CLSID CLSID_JScript_ = {0xf414c260,0x6ac0,0x11cf,{0xb6,0xd1,0x00,0xaa,0x00,0xbb,0xbb,0x58}};
static const CLSID CLSID_VBScript_ = {0xb54f3741,0x5b07,0x11cf,{0xa4,0xb0,0x00,0xaa,0x00,0x4a,0x55,0xe8}};
static const IID IID_Poll_ = {0x539698a0,0xcdca,0x11cf,{0xa5,0xeb,0x00,0xaa,0x00,0x47,0xa0,0x63}};
#ifdef _WIN64
static const IID IID_Parse_ = {0xc7ef7658,0xe1ee,0x480e,{0x97,0xea,0xd5,0x2c,0xb4,0xd7,0x6d,0x17}};
typedef IActiveScriptParse64 Parse;
#define Parse_InitNew IActiveScriptParse64_InitNew
#define Parse_ParseScriptText IActiveScriptParse64_ParseScriptText
#define Parse_Release IActiveScriptParse64_Release
#else
static const IID IID_Parse_ = {0xbb1a2ae2,0xa4f9,0x11cf,{0x8f,0x20,0x00,0x80,0x5f,0x2c,0xd0,0x64}};
typedef IActiveScriptParse32 Parse;
#define Parse_InitNew IActiveScriptParse32_InitNew
#define Parse_ParseScriptText IActiveScriptParse32_ParseScriptText
#define Parse_Release IActiveScriptParse32_Release
#endif

static int g_mode;      /* 0: no poll interface, 1: always S_OK, 2: S_FALSE on the third question */
static int g_asked;

typedef struct { IActiveScriptSite site; IActiveScriptSiteInterruptPoll poll; } Site;
static Site g_site;

static HRESULT WINAPI S_QI(IActiveScriptSite *i, REFIID r, void **o)
{
    if (IsEqualGUID(r, &IID_IUnknown) || IsEqualGUID(r, &IID_IActiveScriptSite)) { *o = i; return S_OK; }
    if (g_mode && IsEqualGUID(r, &IID_Poll_)) { *o = &g_site.poll; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI S_AR(IActiveScriptSite *i) { return 2; }
static HRESULT WINAPI S_GL(IActiveScriptSite *i, LCID *l) { return E_NOTIMPL; }
static HRESULT WINAPI S_GI(IActiveScriptSite *i, LPCOLESTR n, DWORD m, IUnknown **u, ITypeInfo **t) { return TYPE_E_ELEMENTNOTFOUND; }
static HRESULT WINAPI S_GD(IActiveScriptSite *i, BSTR *b) { return E_NOTIMPL; }
static HRESULT WINAPI S_OST(IActiveScriptSite *i, const VARIANT *v, const EXCEPINFO *e) { return S_OK; }
static HRESULT WINAPI S_SC(IActiveScriptSite *i, SCRIPTSTATE s) { return S_OK; }
static HRESULT WINAPI S_OSE(IActiveScriptSite *i, IActiveScriptError *e) { return S_OK; }
static HRESULT WINAPI S_OE(IActiveScriptSite *i) { return S_OK; }
static IActiveScriptSiteVtbl svt = { S_QI, S_AR, S_AR, S_GL, S_GI, S_GD, S_OST, S_SC, S_OSE, S_OE, S_OE };

static HRESULT WINAPI P_QI(IActiveScriptSiteInterruptPoll *i, REFIID r, void **o) { return S_QI(&g_site.site, r, o); }
static ULONG WINAPI P_AR(IActiveScriptSiteInterruptPoll *i) { return 2; }
static HRESULT WINAPI P_QC(IActiveScriptSiteInterruptPoll *i)
{
    g_asked++;
    return g_mode == 2 && g_asked >= 3 ? S_FALSE : S_OK;
}
static IActiveScriptSiteInterruptPollVtbl pvt = { P_QI, P_AR, P_AR, P_QC };

static void run(const char *engine, const CLSID *clsid, const WCHAR *endless, const WCHAR *twosec, int mode, const char *tag)
{
    IActiveScript *s; Parse *p; EXCEPINFO ei; HRESULT hr; DWORD t0;
    g_mode = mode; g_asked = 0;
    if (FAILED(CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IActiveScript, (void**)&s))) { printf("%s_%s=nocreate\n", engine, tag); return; }
    IActiveScript_QueryInterface(s, &IID_Parse_, (void**)&p);
    Parse_InitNew(p);
    IActiveScript_SetScriptSite(s, &g_site.site);
    IActiveScript_SetScriptState(s, SCRIPTSTATE_STARTED);
    t0 = GetTickCount();
    memset(&ei, 0, sizeof(ei));
    hr = Parse_ParseScriptText(p, mode == 2 ? endless : twosec, NULL, NULL, NULL, 0, 0, 0, NULL, &ei);
    printf("%s_%s=%s asked=%d ms=%lu hr=%08lx\n", engine, tag, SUCCEEDED(hr) ? "done" : "stopped", g_asked, GetTickCount() - t0, hr);
    IActiveScript_Close(s);
    Parse_Release(p);
    IActiveScript_Release(s);
}

int main(void)
{
    g_site.site.lpVtbl = &svt; g_site.poll.lpVtbl = &pvt;
    CoInitialize(NULL);
    setvbuf(stdout, NULL, _IONBF, 0);
    for (int m = 2; m >= 0; m--)
    {
        const char *tag = m == 2 ? "stop" : m == 1 ? "go" : "none";
        run("js", &CLSID_JScript_, L"while(true) {}",
            L"var t = new Date().getTime(); while(new Date().getTime() - t < 2200) {}", m, tag);
        run("vbs", &CLSID_VBScript_, L"Do\nLoop",
            L"t = Timer\nDo While Timer - t < 2.2\nLoop", m, tag);
    }
    printf("DONE\n");
    return 0;
}
