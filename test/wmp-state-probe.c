/* wmp's control state: the player's playState/openState/Status, enabled,
 * fullScreen, uiMode and friends, the closed caption and error objects, the
 * settings (mute, playCount, rate, balance, modes, baseURL, defaultFrame),
 * the network object (statistics, bufferingTime, per-protocol proxy
 * settings) and the controls (isAvailable, pause, fastForward, positions),
 * run by test/wmp-state-gate.sh (patches/sg/2851).
 *
 *   wmp-state-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <wininet.h>
#include <wmp.h>
#include <stdio.h>
#include <string.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define NS_S_COMMAND_NOT_AVAILABLE ((HRESULT)0x000d1105)

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[320]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

static BOOL bstr_is(BSTR b, const WCHAR *s) { return b ? !wcscmp(b, s) : !*s; }

static void pump(DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while (GetTickCount() < end)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(10);
    }
}

struct env
{
    IWMPPlayer4 *player;
    IWMPSettings *settings;
    IWMPControls *controls;
    IWMPNetwork *network;
};

static BOOL setup(struct env *e)
{
    HRESULT hr = CoCreateInstance(&CLSID_WindowsMediaPlayer, NULL, CLSCTX_INPROC_SERVER, &IID_IWMPPlayer4, (void **)&e->player);
    if (FAILED(hr)) { CHECKF(0, "CoCreateInstance(WindowsMediaPlayer) = %08lx", hr); return FALSE; }
    IWMPPlayer4_get_settings(e->player, &e->settings);
    IWMPPlayer4_get_controls(e->player, &e->controls);
    IWMPPlayer4_get_network(e->player, &e->network);
    return TRUE;
}

static void teardown(struct env *e)
{
    IWMPNetwork_Release(e->network);
    IWMPControls_Release(e->controls);
    IWMPSettings_Release(e->settings);
    IWMPPlayer4_Release(e->player);
}

/* a boolean property of the player */
#define BOOLPROP(obj, getter, setter, name, deflt) do { \
    VARIANT_BOOL _v = 5; HRESULT _hr = getter(obj, &_v); \
    CHECKF(_hr == S_OK && _v == (deflt), "%s defaults to %d (hr %08lx, %d)", name, (deflt), _hr, _v); \
    _hr = setter(obj, (deflt) ? VARIANT_FALSE : VARIANT_TRUE); \
    _v = 5; getter(obj, &_v); \
    CHECKF(_hr == S_OK && _v == ((deflt) ? VARIANT_FALSE : VARIANT_TRUE), "%s flips (hr %08lx, %d)", name, _hr, _v); \
    setter(obj, (deflt) ? VARIANT_TRUE : VARIANT_FALSE); \
    _v = 5; getter(obj, &_v); \
    CHECKF(_v == (deflt), "%s flips back", name); \
    _hr = getter(obj, NULL); CHECKF(_hr == E_POINTER, "%s(NULL) = E_POINTER (%08lx)", name, _hr); \
} while (0)

static void test_player(void)
{
    struct env e;
    WMPOpenState os = 99;
    WMPPlayState ps = 99;
    VARIANT_BOOL vb;
    BSTR b, url;
    IWMPClosedCaption *cc;
    IWMPError *err;
    IWMPErrorItem *item;
    IWMPMedia *media;
    IDispatch *d;
    LONG l;
    DWORD flags;
    HRESULT hr;
    unsigned int i;

    if (!setup(&e)) return;

    hr = IWMPPlayer4_get_openState(e.player, &os);
    CHECKF(hr == S_OK && os == wmposUndefined, "fresh openState = Undefined (%08lx %d)", hr, os);
    hr = IWMPPlayer4_get_playState(e.player, &ps);
    CHECKF(hr == S_OK && ps == wmppsUndefined, "fresh playState = Undefined (%08lx %d)", hr, ps);
    CHECKF(IWMPPlayer4_get_openState(e.player, NULL) == E_POINTER, "openState(NULL) = E_POINTER");
    CHECKF(IWMPPlayer4_get_playState(e.player, NULL) == E_POINTER, "playState(NULL) = E_POINTER");
    b = NULL; hr = IWMPPlayer4_get_status(e.player, &b);
    CHECKF(hr == S_OK && bstr_is(b, L""), "fresh Status is empty (%08lx)", hr);
    SysFreeString(b);
    CHECKF(IWMPPlayer4_get_status(e.player, NULL) == E_POINTER, "Status(NULL) = E_POINTER");

    BOOLPROP(e.player, IWMPPlayer4_get_enabled, IWMPPlayer4_put_enabled, "enabled", VARIANT_TRUE);
    BOOLPROP(e.player, IWMPPlayer4_get_fullScreen, IWMPPlayer4_put_fullScreen, "fullScreen", VARIANT_FALSE);
    BOOLPROP(e.player, IWMPPlayer4_get_enableContextMenu, IWMPPlayer4_put_enableContextMenu, "enableContextMenu", VARIANT_TRUE);
    BOOLPROP(e.player, IWMPPlayer4_get_stretchToFit, IWMPPlayer4_put_stretchToFit, "stretchToFit", VARIANT_FALSE);
    BOOLPROP(e.player, IWMPPlayer4_get_windowlessVideo, IWMPPlayer4_put_windowlessVideo, "windowlessVideo", VARIANT_FALSE);

    b = NULL; hr = IWMPPlayer4_get_uiMode(e.player, &b);
    CHECKF(hr == S_OK && bstr_is(b, L"full"), "uiMode defaults to full (%08lx)", hr);
    SysFreeString(b);
    {
        static const struct { const WCHAR *set; const WCHAR *get; } modes[] = {
            {L"invisible", L"invisible"}, {L"NONE", L"none"}, {L"Mini", L"mini"}, {L"custom", L"custom"}, {L"FULL", L"full"}};
        for (i = 0; i < ARRAY_SIZE(modes); i++)
        {
            BSTR set = SysAllocString(modes[i].set);
            hr = IWMPPlayer4_put_uiMode(e.player, set);
            SysFreeString(set);
            b = NULL; IWMPPlayer4_get_uiMode(e.player, &b);
            CHECKF(hr == S_OK && bstr_is(b, modes[i].get), "uiMode %ls reads back as %ls (%08lx)", modes[i].set, modes[i].get, hr);
            SysFreeString(b);
        }
        url = SysAllocString(L"sideways");
        hr = IWMPPlayer4_put_uiMode(e.player, url);
        CHECKF(hr == E_INVALIDARG, "uiMode(sideways) = E_INVALIDARG (%08lx)", hr);
        SysFreeString(url);
        hr = IWMPPlayer4_put_uiMode(e.player, NULL);
        CHECKF(hr == E_INVALIDARG, "uiMode(NULL) = E_INVALIDARG (%08lx)", hr);
        b = NULL; IWMPPlayer4_get_uiMode(e.player, &b);
        CHECKF(bstr_is(b, L"full"), "a refused uiMode leaves the old one");
        SysFreeString(b);
    }

    vb = 5; hr = IWMPPlayer4_get_isRemote(e.player, &vb);
    CHECKF(hr == S_OK && vb == (GetSystemMetrics(SM_REMOTESESSION) ? VARIANT_TRUE : VARIANT_FALSE), "isRemote follows the session (%08lx %d)", hr, vb);
    CHECKF(IWMPPlayer4_get_isRemote(e.player, NULL) == E_POINTER, "isRemote(NULL) = E_POINTER");
    vb = 5; hr = IWMPPlayer4_get_isOnline(e.player, &vb);
    CHECKF(hr == S_OK && vb == (InternetGetConnectedState(&flags, 0) ? VARIANT_TRUE : VARIANT_FALSE), "isOnline follows the connection (%08lx %d)", hr, vb);
    CHECKF(IWMPPlayer4_get_isOnline(e.player, NULL) == E_POINTER, "isOnline(NULL) = E_POINTER");

    hr = IWMPPlayer4_launchURL(e.player, NULL);
    CHECKF(hr == E_INVALIDARG, "launchURL(NULL) = E_INVALIDARG (%08lx)", hr);
    b = SysAllocString(L"");
    hr = IWMPPlayer4_launchURL(e.player, b);
    CHECKF(hr == E_INVALIDARG, "launchURL(\"\") = E_INVALIDARG (%08lx)", hr);
    SysFreeString(b);

    /* the error object: always an empty queue */
    err = NULL; hr = IWMPPlayer4_get_Error(e.player, &err);
    CHECKF(hr == S_OK && err, "get_Error (%08lx)", hr);
    CHECKF(IWMPPlayer4_get_Error(e.player, NULL) == E_POINTER, "get_Error(NULL) = E_POINTER");
    if (err)
    {
        l = 5; hr = IWMPError_get_errorCount(err, &l);
        CHECKF(hr == S_OK && l == 0, "errorCount = 0 (%08lx %ld)", hr, l);
        item = (IWMPErrorItem *)1; hr = IWMPError_get_Item(err, 0, &item);
        CHECKF(hr == E_INVALIDARG && !item, "Item(0) of an empty queue = E_INVALIDARG, NULL (%08lx)", hr);
        hr = IWMPError_clearErrorQueue(err);
        CHECKF(hr == S_OK, "clearErrorQueue (%08lx)", hr);
        hr = IWMPError_get_errorCount(err, NULL);
        CHECKF(hr == E_POINTER, "errorCount(NULL) = E_POINTER");
        hr = IWMPError_QueryInterface(err, &IID_IDispatch, (void **)&d);
        CHECKF(hr == S_OK && d == (IDispatch *)err, "error object QueryInterface(IDispatch)");
        if (hr == S_OK)
        {
            DISPID id; LPOLESTR n = (LPOLESTR)L"errorCount"; VARIANT v; DISPPARAMS dp = {0};
            hr = IDispatch_GetIDsOfNames(d, &IID_NULL, &n, 1, 0, &id);
            CHECKF(hr == S_OK && id == 0x354, "error object GetIDsOfNames(errorCount) = %lx (%08lx)", id, hr);
            VariantInit(&v);
            hr = IDispatch_Invoke(d, id, &IID_NULL, 0, DISPATCH_PROPERTYGET, &dp, &v, NULL, NULL);
            CHECKF(hr == S_OK && V_VT(&v) == VT_I4 && V_I4(&v) == 0, "error object Invoke(errorCount)");
            IDispatch_Release(d);
        }
        IWMPError_Release(err);
    }

    /* the closed caption object round-trips its four strings */
    cc = NULL; hr = IWMPPlayer4_get_closedCaption(e.player, &cc);
    CHECKF(hr == S_OK && cc, "get_closedCaption (%08lx)", hr);
    CHECKF(IWMPPlayer4_get_closedCaption(e.player, NULL) == E_POINTER, "get_closedCaption(NULL) = E_POINTER");
    if (cc)
    {
        struct { HRESULT (WINAPI *get)(IWMPClosedCaption *, BSTR *); HRESULT (WINAPI *put)(IWMPClosedCaption *, BSTR); const char *n; } props[] = {
            {cc->lpVtbl->get_SAMIStyle, cc->lpVtbl->put_SAMIStyle, "SAMIStyle"},
            {cc->lpVtbl->get_SAMILang, cc->lpVtbl->put_SAMILang, "SAMILang"},
            {cc->lpVtbl->get_SAMIFileName, cc->lpVtbl->put_SAMIFileName, "SAMIFileName"},
            {cc->lpVtbl->get_captioningId, cc->lpVtbl->put_captioningId, "captioningId"},
        };
        for (i = 0; i < 4; i++)
        {
            b = NULL; hr = props[i].get(cc, &b);
            CHECKF(hr == S_OK && bstr_is(b, L""), "%s defaults to empty (%08lx)", props[i].n, hr);
            SysFreeString(b);
            url = SysAllocString(i == 1 ? L"English" : L"value");
            hr = props[i].put(cc, url);
            SysFreeString(url);
            b = NULL; props[i].get(cc, &b);
            CHECKF(hr == S_OK && bstr_is(b, i == 1 ? L"English" : L"value"), "%s round trip (%08lx)", props[i].n, hr);
            SysFreeString(b);
            hr = props[i].get(cc, NULL);
            CHECKF(hr == E_POINTER, "%s(NULL) = E_POINTER", props[i].n);
        }
        /* distinct storage: the first still holds its own value after the second changed */
        hr = cc->lpVtbl->get_SAMIStyle(cc, &b);
        CHECKF(hr == S_OK && bstr_is(b, L"value"), "SAMIStyle is not overwritten by the other properties");
        SysFreeString(b);
        hr = cc->lpVtbl->put_SAMIStyle(cc, NULL);
        b = (BSTR)1; cc->lpVtbl->get_SAMIStyle(cc, &b);
        CHECKF(hr == S_OK && bstr_is(b, L""), "a NULL string clears the property");
        SysFreeString(b);
        IWMPClosedCaption_Release(cc);
    }

    /* the state follows the events of a URL that is opened without playing */
    IWMPSettings_put_autoStart(e.settings, VARIANT_FALSE);
    url = SysAllocString(L"C:\\nonexistent\\song.mp3");
    hr = IWMPPlayer4_put_URL(e.player, url);
    CHECKF(hr == S_OK, "put_URL (%08lx)", hr);
    SysFreeString(url);
    IWMPPlayer4_get_playState(e.player, &ps);
    IWMPPlayer4_get_openState(e.player, &os);
    CHECKF(ps == wmppsReady, "playState after put_URL = Ready (%d)", ps);
    CHECKF(os == wmposPlaylistOpenNoMedia, "openState after put_URL = PlaylistOpenNoMedia (%d)", os);
    b = NULL; IWMPPlayer4_get_status(e.player, &b);
    CHECKF(bstr_is(b, L"Ready"), "Status after put_URL = Ready");
    SysFreeString(b);

    /* close() drops the media and returns to the undefined state */
    hr = IWMPPlayer4_close(e.player);
    CHECKF(hr == S_OK, "close (%08lx)", hr);
    IWMPPlayer4_get_playState(e.player, &ps);
    IWMPPlayer4_get_openState(e.player, &os);
    CHECKF(ps == wmppsUndefined && os == wmposUndefined, "state after close = Undefined (%d %d)", ps, os);
    IWMPPlayer4_get_currentMedia(e.player, &media);
    CHECKF(media == NULL, "no current media after close");
    b = NULL; IWMPPlayer4_get_URL(e.player, &b);
    CHECKF(bstr_is(b, L""), "URL is empty after close");
    SysFreeString(b);
    hr = IWMPPlayer4_close(e.player);
    CHECKF(hr == S_OK, "close with nothing open (%08lx)", hr);

    teardown(&e);
}

static void test_settings(void)
{
    struct env e;
    IWMPSettings *s;
    VARIANT_BOOL vb;
    double r;
    LONG l;
    BSTR b, n;
    HRESULT hr;
    unsigned int i;

    if (!setup(&e)) return;
    s = e.settings;

    BOOLPROP(s, IWMPSettings_get_mute, IWMPSettings_put_mute, "mute", VARIANT_FALSE);
    BOOLPROP(s, IWMPSettings_get_invokeURLs, IWMPSettings_put_invokeURLs, "invokeURLs", VARIANT_TRUE);

    l = 0; hr = IWMPSettings_get_playCount(s, &l);
    CHECKF(hr == S_OK && l == 1, "playCount defaults to 1 (%08lx %ld)", hr, l);
    hr = IWMPSettings_put_playCount(s, 5);
    IWMPSettings_get_playCount(s, &l);
    CHECKF(hr == S_OK && l == 5, "playCount round trip (%08lx %ld)", hr, l);
    hr = IWMPSettings_put_playCount(s, 0);
    CHECKF(hr == E_INVALIDARG, "playCount(0) = E_INVALIDARG (%08lx)", hr);
    hr = IWMPSettings_put_playCount(s, -3);
    CHECKF(hr == E_INVALIDARG, "playCount(-3) = E_INVALIDARG (%08lx)", hr);
    IWMPSettings_get_playCount(s, &l);
    CHECKF(l == 5, "a refused playCount leaves the old one (%ld)", l);
    CHECKF(IWMPSettings_get_playCount(s, NULL) == E_POINTER, "playCount(NULL) = E_POINTER");

    r = 0; hr = IWMPSettings_get_rate(s, &r);
    CHECKF(hr == S_OK && r == 1.0, "rate defaults to 1 (%08lx %f)", hr, r);
    hr = IWMPSettings_put_rate(s, 2.5);
    IWMPSettings_get_rate(s, &r);
    CHECKF(hr == S_OK && r == 2.5, "rate round trip (%08lx %f)", hr, r);
    hr = IWMPSettings_put_rate(s, 0.0);
    CHECKF(hr == E_INVALIDARG, "rate(0) = E_INVALIDARG (%08lx)", hr);
    hr = IWMPSettings_put_rate(s, 10.5);
    CHECKF(hr == E_INVALIDARG, "rate(10.5) = E_INVALIDARG (%08lx)", hr);
    hr = IWMPSettings_put_rate(s, -11.0);
    CHECKF(hr == E_INVALIDARG, "rate(-11) = E_INVALIDARG (%08lx)", hr);
    IWMPSettings_get_rate(s, &r);
    CHECKF(r == 2.5, "a refused rate leaves the old one (%f)", r);
    CHECKF(IWMPSettings_get_rate(s, NULL) == E_POINTER, "rate(NULL) = E_POINTER");

    l = 99; hr = IWMPSettings_get_balance(s, &l);
    CHECKF(hr == S_OK && l == 0, "balance defaults to 0 (%08lx %ld)", hr, l);
    hr = IWMPSettings_put_balance(s, -100);
    IWMPSettings_get_balance(s, &l);
    CHECKF(hr == S_OK && l == -100, "balance -100 round trip (%08lx %ld)", hr, l);
    hr = IWMPSettings_put_balance(s, 100);
    IWMPSettings_get_balance(s, &l);
    CHECKF(hr == S_OK && l == 100, "balance 100 round trip (%08lx %ld)", hr, l);
    hr = IWMPSettings_put_balance(s, 101);
    CHECKF(hr == E_INVALIDARG, "balance(101) = E_INVALIDARG (%08lx)", hr);
    hr = IWMPSettings_put_balance(s, -101);
    CHECKF(hr == E_INVALIDARG, "balance(-101) = E_INVALIDARG (%08lx)", hr);
    IWMPSettings_get_balance(s, &l);
    CHECKF(l == 100, "a refused balance leaves the old one (%ld)", l);
    CHECKF(IWMPSettings_get_balance(s, NULL) == E_POINTER, "balance(NULL) = E_POINTER");

    /* enableErrorDialogs */
    vb = 5; hr = IWMPSettings_put_enableErrorDialogs(s, VARIANT_TRUE);
    IWMPSettings_get_enableErrorDialogs(s, &vb);
    CHECKF(hr == S_OK && vb == VARIANT_TRUE, "enableErrorDialogs round trip");

    /* the strings */
    b = (BSTR)1; hr = IWMPSettings_get_baseURL(s, &b);
    CHECKF(hr == S_OK && bstr_is(b, L""), "baseURL defaults to empty (%08lx)", hr);
    SysFreeString(b);
    n = SysAllocString(L"http://example.com/media/");
    hr = IWMPSettings_put_baseURL(s, n);
    SysFreeString(n);
    IWMPSettings_get_baseURL(s, &b);
    CHECKF(hr == S_OK && bstr_is(b, L"http://example.com/media/"), "baseURL round trip (%08lx)", hr);
    SysFreeString(b);
    b = (BSTR)1; hr = IWMPSettings_get_defaultFrame(s, &b);
    CHECKF(hr == S_OK && bstr_is(b, L""), "defaultFrame defaults to empty (%08lx)", hr);
    SysFreeString(b);
    n = SysAllocString(L"_top");
    hr = IWMPSettings_put_defaultFrame(s, n);
    SysFreeString(n);
    IWMPSettings_get_defaultFrame(s, &b);
    CHECKF(hr == S_OK && bstr_is(b, L"_top"), "defaultFrame round trip (%08lx)", hr);
    SysFreeString(b);
    IWMPSettings_get_baseURL(s, &b);
    CHECKF(bstr_is(b, L"http://example.com/media/"), "baseURL and defaultFrame are separate");
    SysFreeString(b);
    CHECKF(IWMPSettings_get_baseURL(s, NULL) == E_POINTER, "baseURL(NULL) = E_POINTER");
    CHECKF(IWMPSettings_get_defaultFrame(s, NULL) == E_POINTER, "defaultFrame(NULL) = E_POINTER");

    /* modes: autoRewind is on, the others off, each is its own flag */
    {
        static const WCHAR *names[] = {L"autoRewind", L"loop", L"showFrame", L"shuffle"};
        for (i = 0; i < 4; i++)
        {
            n = SysAllocString(names[i]);
            vb = 5; hr = IWMPSettings_getMode(s, n, &vb);
            CHECKF(hr == S_OK && vb == (i == 0 ? VARIANT_TRUE : VARIANT_FALSE), "mode %ls default (%08lx %d)", names[i], hr, vb);
            SysFreeString(n);
        }
        n = SysAllocString(L"LOOP");
        hr = IWMPSettings_setMode(s, n, VARIANT_TRUE);
        CHECKF(hr == S_OK, "setMode(LOOP) (%08lx)", hr);
        SysFreeString(n);
        for (i = 0; i < 4; i++)
        {
            n = SysAllocString(names[i]);
            IWMPSettings_getMode(s, n, &vb);
            CHECKF(vb == ((i == 0 || i == 1) ? VARIANT_TRUE : VARIANT_FALSE), "after setMode(loop): mode %ls = %d", names[i], vb);
            SysFreeString(n);
        }
        n = SysAllocString(L"shuffle");
        IWMPSettings_setMode(s, n, VARIANT_TRUE);
        IWMPSettings_getMode(s, n, &vb);
        CHECKF(vb == VARIANT_TRUE, "shuffle on");
        SysFreeString(n);
        n = SysAllocString(L"autoRewind");
        IWMPSettings_setMode(s, n, VARIANT_FALSE);
        IWMPSettings_getMode(s, n, &vb);
        CHECKF(vb == VARIANT_FALSE, "autoRewind off");
        SysFreeString(n);
        n = SysAllocString(L"bogus");
        hr = IWMPSettings_getMode(s, n, &vb);
        CHECKF(hr == E_INVALIDARG, "getMode(bogus) = E_INVALIDARG (%08lx)", hr);
        hr = IWMPSettings_setMode(s, n, VARIANT_TRUE);
        CHECKF(hr == E_INVALIDARG, "setMode(bogus) = E_INVALIDARG (%08lx)", hr);
        SysFreeString(n);
        hr = IWMPSettings_getMode(s, NULL, &vb);
        CHECKF(hr == E_INVALIDARG, "getMode(NULL) = E_INVALIDARG (%08lx)", hr);
        n = SysAllocString(L"loop");
        hr = IWMPSettings_getMode(s, n, NULL);
        CHECKF(hr == E_POINTER, "getMode(loop, NULL) = E_POINTER (%08lx)", hr);
        SysFreeString(n);
    }

    /* isAvailable */
    {
        static const struct { const WCHAR *item; VARIANT_BOOL avail; } items[] = {
            {L"AutoStart", VARIANT_TRUE}, {L"volume", VARIANT_TRUE}, {L"MUTE", VARIANT_TRUE}, {L"balance", VARIANT_TRUE},
            {L"PlayCount", VARIANT_TRUE}, {L"Mode", VARIANT_TRUE}, {L"rate", VARIANT_FALSE}, {L"nothing", VARIANT_FALSE}};
        for (i = 0; i < ARRAY_SIZE(items); i++)
        {
            n = SysAllocString(items[i].item);
            vb = 5; hr = IWMPSettings_get_isAvailable(s, n, &vb);
            CHECKF(hr == S_OK && vb == items[i].avail, "isAvailable(%ls) = %d (%08lx %d)", items[i].item, items[i].avail, hr, vb);
            SysFreeString(n);
        }
        hr = IWMPSettings_get_isAvailable(s, NULL, &vb);
        CHECKF(hr == E_INVALIDARG && vb == VARIANT_FALSE, "isAvailable(NULL) = E_INVALIDARG (%08lx)", hr);
        n = SysAllocString(L"volume");
        hr = IWMPSettings_get_isAvailable(s, n, NULL);
        CHECKF(hr == E_POINTER, "isAvailable(volume, NULL) = E_POINTER");
        SysFreeString(n);
    }

    teardown(&e);
}

static void test_network(void)
{
    struct env e;
    IWMPNetwork *n;
    static const WCHAR *protos[] = {L"http", L"MMS", L"rtsp"};
    LONG l, port;
    VARIANT_BOOL vb;
    BSTR b, s, p;
    HRESULT hr;
    unsigned int i;

    if (!setup(&e)) return;
    n = e.network;

    {
        struct { const char *name; HRESULT (WINAPI *get)(IWMPNetwork *, LONG *); } stats[] = {
            {"bandWidth", n->lpVtbl->get_bandWidth}, {"recoveredPackets", n->lpVtbl->get_recoveredPackets},
            {"receivedPackets", n->lpVtbl->get_receivedPackets}, {"lostPackets", n->lpVtbl->get_lostPackets},
            {"receptionQuality", n->lpVtbl->get_receptionQuality}, {"bufferingCount", n->lpVtbl->get_bufferingCount},
            {"frameRate", n->lpVtbl->get_frameRate}, {"maxBitRate", n->lpVtbl->get_maxBitRate},
            {"bitRate", n->lpVtbl->get_bitRate}, {"encodedFrameRate", n->lpVtbl->get_encodedFrameRate},
            {"framesSkipped", n->lpVtbl->get_framesSkipped}};
        for (i = 0; i < ARRAY_SIZE(stats); i++)
        {
            l = 77; hr = stats[i].get(n, &l);
            CHECKF(hr == S_OK && l == 0, "idle %s = 0 (%08lx %ld)", stats[i].name, hr, l);
            hr = stats[i].get(n, NULL);
            CHECKF(hr == E_POINTER, "%s(NULL) = E_POINTER", stats[i].name);
        }
    }
    l = 77; hr = IWMPNetwork_get_bufferingProgress(n, &l);
    CHECKF(hr == S_FALSE && l == 0, "idle bufferingProgress = 0, S_FALSE (%08lx %ld)", hr, l);
    l = 77; hr = IWMPNetwork_get_downloadProgress(n, &l);
    CHECKF(hr == S_FALSE && l == 0, "idle downloadProgress = 0, S_FALSE (%08lx %ld)", hr, l);
    CHECKF(IWMPNetwork_get_bufferingProgress(n, NULL) == E_POINTER, "bufferingProgress(NULL) = E_POINTER");
    CHECKF(IWMPNetwork_get_downloadProgress(n, NULL) == E_POINTER, "downloadProgress(NULL) = E_POINTER");

    l = 0; hr = IWMPNetwork_get_bufferingTime(n, &l);
    CHECKF(hr == S_OK && l == 5000, "bufferingTime defaults to 5000 ms (%08lx %ld)", hr, l);
    hr = IWMPNetwork_put_bufferingTime(n, 1234);
    IWMPNetwork_get_bufferingTime(n, &l);
    CHECKF(hr == S_OK && l == 1234, "bufferingTime round trip (%08lx %ld)", hr, l);
    hr = IWMPNetwork_put_bufferingTime(n, 60000);
    CHECKF(hr == S_OK, "bufferingTime(60000) ok (%08lx)", hr);
    hr = IWMPNetwork_put_bufferingTime(n, 60001);
    CHECKF(hr == E_INVALIDARG, "bufferingTime(60001) = E_INVALIDARG (%08lx)", hr);
    hr = IWMPNetwork_put_bufferingTime(n, -1);
    CHECKF(hr == E_INVALIDARG, "bufferingTime(-1) = E_INVALIDARG (%08lx)", hr);
    IWMPNetwork_get_bufferingTime(n, &l);
    CHECKF(l == 60000, "a refused bufferingTime leaves the old one (%ld)", l);
    CHECKF(IWMPNetwork_get_bufferingTime(n, NULL) == E_POINTER, "bufferingTime(NULL) = E_POINTER");

    l = 5; hr = IWMPNetwork_get_maxBandwidth(n, &l);
    CHECKF(hr == S_OK && l == 0, "maxBandwidth defaults to 0 (%08lx %ld)", hr, l);
    hr = IWMPNetwork_put_maxBandwidth(n, 56000);
    IWMPNetwork_get_maxBandwidth(n, &l);
    CHECKF(hr == S_OK && l == 56000, "maxBandwidth round trip (%08lx %ld)", hr, l);
    CHECKF(IWMPNetwork_get_maxBandwidth(n, NULL) == E_POINTER, "maxBandwidth(NULL) = E_POINTER");

    /* source protocol follows the opened address */
    b = (BSTR)1; hr = IWMPNetwork_get_sourceProtocol(n, &b);
    CHECKF(hr == S_OK && bstr_is(b, L""), "idle sourceProtocol is empty (%08lx)", hr);
    SysFreeString(b);
    IWMPSettings_put_autoStart(e.settings, VARIANT_FALSE);
    {
        static const struct { const WCHAR *url; const WCHAR *proto; } urls[] = {
            {L"C:\\music\\a.mp3", L"File"}, {L"file:///C:/music/a.mp3", L"File"}, {L"http://example.com/a.mp3", L"HTTP"},
            {L"https://example.com/a.mp3", L"HTTP"}, {L"mms://example.com/live", L"MMS"}, {L"rtsp://example.com/live", L"RTSP"}};
        for (i = 0; i < ARRAY_SIZE(urls); i++)
        {
            s = SysAllocString(urls[i].url);
            IWMPPlayer4_put_URL(e.player, s);
            SysFreeString(s);
            b = NULL; hr = IWMPNetwork_get_sourceProtocol(n, &b);
            CHECKF(hr == S_OK && bstr_is(b, urls[i].proto), "sourceProtocol of %ls = %ls (%08lx %ls)", urls[i].url, urls[i].proto, hr, b ? b : L"(null)");
            SysFreeString(b);
        }
        hr = IWMPNetwork_get_sourceProtocol(n, NULL);
        CHECKF(hr == E_POINTER, "sourceProtocol(NULL) = E_POINTER");
    }

    /* proxies, per protocol */
    for (i = 0; i < 3; i++)
    {
        p = SysAllocString(protos[i]);
        l = 99; hr = IWMPNetwork_getProxySettings(n, p, &l);
        CHECKF(hr == S_OK && l == 3, "%ls proxy setting defaults to 3 (%08lx %ld)", protos[i], hr, l);
        b = (BSTR)1; hr = IWMPNetwork_getProxyName(n, p, &b);
        CHECKF(hr == S_OK && bstr_is(b, L""), "%ls proxy name defaults to empty", protos[i]);
        SysFreeString(b);
        port = 99; hr = IWMPNetwork_getProxyPort(n, p, &port);
        CHECKF(hr == S_OK && port == 0, "%ls proxy port defaults to 0 (%08lx %ld)", protos[i], hr, port);
        b = (BSTR)1; hr = IWMPNetwork_getProxyExceptionList(n, p, &b);
        CHECKF(hr == S_OK && bstr_is(b, L""), "%ls exception list defaults to empty", protos[i]);
        SysFreeString(b);
        vb = 5; hr = IWMPNetwork_getProxyBypassForLocal(n, p, &vb);
        CHECKF(hr == S_OK && vb == VARIANT_FALSE, "%ls bypass for local defaults to false (%08lx %d)", protos[i], hr, vb);
        SysFreeString(p);
    }
    p = SysAllocString(L"HTTP");
    hr = IWMPNetwork_setProxySettings(n, p, 2);
    CHECKF(hr == S_OK, "setProxySettings(HTTP, 2) (%08lx)", hr);
    s = SysAllocString(L"proxy.example.com");
    hr = IWMPNetwork_setProxyName(n, p, s);
    SysFreeString(s);
    CHECKF(hr == S_OK, "setProxyName (%08lx)", hr);
    hr = IWMPNetwork_setProxyPort(n, p, 8080);
    CHECKF(hr == S_OK, "setProxyPort (%08lx)", hr);
    s = SysAllocString(L"*.example.com;localhost");
    hr = IWMPNetwork_setProxyExceptionList(n, p, s);
    SysFreeString(s);
    CHECKF(hr == S_OK, "setProxyExceptionList (%08lx)", hr);
    hr = IWMPNetwork_setProxyBypassForLocal(n, p, VARIANT_TRUE);
    CHECKF(hr == S_OK, "setProxyBypassForLocal (%08lx)", hr);
    SysFreeString(p);
    /* read back through the lower-case protocol name: names are case-insensitive */
    p = SysAllocString(L"http");
    l = 0; IWMPNetwork_getProxySettings(n, p, &l);
    CHECKF(l == 2, "http proxy setting reads back 2 (%ld)", l);
    b = NULL; IWMPNetwork_getProxyName(n, p, &b);
    CHECKF(bstr_is(b, L"proxy.example.com"), "http proxy name reads back");
    SysFreeString(b);
    port = 0; IWMPNetwork_getProxyPort(n, p, &port);
    CHECKF(port == 8080, "http proxy port reads back (%ld)", port);
    b = NULL; IWMPNetwork_getProxyExceptionList(n, p, &b);
    CHECKF(bstr_is(b, L"*.example.com;localhost"), "http exception list reads back");
    SysFreeString(b);
    vb = 0; IWMPNetwork_getProxyBypassForLocal(n, p, &vb);
    CHECKF(vb == VARIANT_TRUE, "http bypass for local reads back");
    SysFreeString(p);
    /* and the other protocols are untouched */
    for (i = 1; i < 3; i++)
    {
        p = SysAllocString(protos[i]);
        l = 0; IWMPNetwork_getProxySettings(n, p, &l);
        b = NULL; IWMPNetwork_getProxyName(n, p, &b);
        port = 9; IWMPNetwork_getProxyPort(n, p, &port);
        vb = 5; IWMPNetwork_getProxyBypassForLocal(n, p, &vb);
        CHECKF(l == 3 && bstr_is(b, L"") && port == 0 && vb == VARIANT_FALSE, "%ls proxy is independent of http (%ld %ld %d)", protos[i], l, port, vb);
        SysFreeString(b);
        SysFreeString(p);
    }
    p = SysAllocString(L"rtsp");
    s = SysAllocString(L"rtsp-proxy");
    IWMPNetwork_setProxyName(n, p, s);
    SysFreeString(s);
    b = NULL; SysFreeString(p); p = SysAllocString(L"mms"); IWMPNetwork_getProxyName(n, p, &b);
    CHECKF(bstr_is(b, L""), "rtsp proxy name does not show in mms");
    SysFreeString(b);
    hr = IWMPNetwork_setProxySettings(n, p, 4);
    CHECKF(hr == E_INVALIDARG, "setProxySettings(4) = E_INVALIDARG (%08lx)", hr);
    hr = IWMPNetwork_setProxySettings(n, p, -1);
    CHECKF(hr == E_INVALIDARG, "setProxySettings(-1) = E_INVALIDARG (%08lx)", hr);
    hr = IWMPNetwork_setProxyPort(n, p, 70000);
    CHECKF(hr == E_INVALIDARG, "setProxyPort(70000) = E_INVALIDARG (%08lx)", hr);
    hr = IWMPNetwork_getProxySettings(n, p, NULL);
    CHECKF(hr == E_POINTER, "getProxySettings(NULL) = E_POINTER (%08lx)", hr);
    SysFreeString(p);
    p = SysAllocString(L"ftp");
    hr = IWMPNetwork_getProxySettings(n, p, &l);
    CHECKF(hr == E_INVALIDARG, "getProxySettings(ftp) = E_INVALIDARG (%08lx)", hr);
    hr = IWMPNetwork_setProxySettings(n, p, 1);
    CHECKF(hr == E_INVALIDARG, "setProxySettings(ftp) = E_INVALIDARG (%08lx)", hr);
    hr = IWMPNetwork_getProxyName(n, p, &b);
    CHECKF(hr == E_INVALIDARG, "getProxyName(ftp) = E_INVALIDARG (%08lx)", hr);
    hr = IWMPNetwork_setProxyBypassForLocal(n, p, VARIANT_TRUE);
    CHECKF(hr == E_INVALIDARG, "setProxyBypassForLocal(ftp) = E_INVALIDARG (%08lx)", hr);
    SysFreeString(p);
    hr = IWMPNetwork_getProxySettings(n, NULL, &l);
    CHECKF(hr == E_INVALIDARG, "getProxySettings(NULL protocol) = E_INVALIDARG (%08lx)", hr);

    teardown(&e);
}

static void test_controls(void)
{
    struct env e;
    IWMPControls *c;
    static const WCHAR *all[] = {L"play", L"pause", L"stop", L"fastForward", L"fastReverse", L"next", L"previous", L"step",
            L"currentItem", L"currentMarker", L"currentPosition", L"currentPositionTimeCode"};
    VARIANT_BOOL vb;
    DOUBLE pos;
    BSTR b, s;
    HRESULT hr;
    LONG l;
    unsigned int i;

    if (!setup(&e)) return;
    c = e.controls;
    IWMPSettings_put_autoStart(e.settings, VARIANT_FALSE);

    for (i = 0; i < ARRAY_SIZE(all); i++)
    {
        s = SysAllocString(all[i]);
        vb = 5; hr = IWMPControls_get_isAvailable(c, s, &vb);
        CHECKF(hr == S_OK && vb == VARIANT_FALSE, "nothing open: isAvailable(%ls) = FALSE (%08lx %d)", all[i], hr, vb);
        SysFreeString(s);
    }
    s = SysAllocString(L"PLAY");
    hr = IWMPControls_get_isAvailable(c, s, NULL);
    CHECKF(hr == E_POINTER, "isAvailable(NULL out) = E_POINTER (%08lx)", hr);
    SysFreeString(s);
    vb = 5; hr = IWMPControls_get_isAvailable(c, NULL, &vb);
    CHECKF(hr == E_INVALIDARG && vb == VARIANT_FALSE, "isAvailable(NULL item) = E_INVALIDARG (%08lx)", hr);

    hr = IWMPControls_pause(c);
    CHECKF(hr == NS_S_COMMAND_NOT_AVAILABLE, "idle pause = COMMAND_NOT_AVAILABLE (%08lx)", hr);
    hr = IWMPControls_fastForward(c);
    CHECKF(hr == NS_S_COMMAND_NOT_AVAILABLE, "idle fastForward = COMMAND_NOT_AVAILABLE (%08lx)", hr);
    hr = IWMPControls_fastReverse(c);
    CHECKF(hr == NS_S_COMMAND_NOT_AVAILABLE, "idle fastReverse = COMMAND_NOT_AVAILABLE (%08lx)", hr);
    hr = IWMPControls_stop(c);
    CHECKF(hr == NS_S_COMMAND_NOT_AVAILABLE, "idle stop = COMMAND_NOT_AVAILABLE (%08lx)", hr);

    pos = 7.0; hr = IWMPControls_get_currentPosition(c, &pos);
    CHECKF(hr == S_FALSE && pos == 0.0, "idle currentPosition = 0, S_FALSE (%08lx %f)", hr, pos);
    CHECKF(IWMPControls_get_currentPosition(c, NULL) == E_POINTER, "currentPosition(NULL) = E_POINTER");
    b = (BSTR)1; hr = IWMPControls_get_currentPositionString(c, &b);
    CHECKF(hr == S_FALSE && bstr_is(b, L""), "idle currentPositionString = empty, S_FALSE (%08lx)", hr);
    SysFreeString(b);
    CHECKF(IWMPControls_get_currentPositionString(c, NULL) == E_POINTER, "currentPositionString(NULL) = E_POINTER");

    l = 5; hr = IWMPControls_get_currentMarker(c, &l);
    CHECKF(hr == S_OK && l == 0, "currentMarker = 0 (%08lx %ld)", hr, l);
    CHECKF(IWMPControls_get_currentMarker(c, NULL) == E_POINTER, "currentMarker(NULL) = E_POINTER");
    hr = IWMPControls_put_currentMarker(c, 1);
    CHECKF(hr == E_INVALIDARG, "currentMarker(1) with no markers = E_INVALIDARG (%08lx)", hr);

    /* media opened without playing: it can be played, nothing else */
    s = SysAllocString(L"C:\\nonexistent\\song.mp3");
    IWMPPlayer4_put_URL(e.player, s);
    SysFreeString(s);
    for (i = 0; i < ARRAY_SIZE(all); i++)
    {
        BOOL expect = !wcscmp(all[i], L"play") || !wcscmp(all[i], L"currentItem");
        s = SysAllocString(all[i]);
        vb = 5; hr = IWMPControls_get_isAvailable(c, s, &vb);
        CHECKF(hr == S_OK && vb == (expect ? VARIANT_TRUE : VARIANT_FALSE), "media open: isAvailable(%ls) = %d (%08lx %d)", all[i], expect, hr, vb);
        SysFreeString(s);
    }
    IWMPPlayer4_close(e.player);
    s = SysAllocString(L"play");
    vb = 5; IWMPControls_get_isAvailable(c, s, &vb);
    CHECKF(vb == VARIANT_FALSE, "after close: isAvailable(play) = FALSE");
    SysFreeString(s);

    teardown(&e);
}

/* the playback state of a real graph: a short silent wave file */
static void write_wave(const WCHAR *path, unsigned int seconds)
{
    static const BYTE hdr[] = {'R','I','F','F',0,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,1,0,1,0,0x40,0x1f,0,0,0x40,0x1f,0,0,1,0,8,0,'d','a','t','a',0,0,0,0};
    BYTE h[sizeof(hdr)];
    DWORD size = 8000 * seconds, w, n;
    BYTE *data = malloc(size);
    HANDLE f;

    memcpy(h, hdr, sizeof(h));
    n = size + 36; memcpy(h + 4, &n, 4);
    memcpy(h + 40, &size, 4);
    memset(data, 0x80, size);
    f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(f, h, sizeof(h), &w, NULL);
    WriteFile(f, data, size, &w, NULL);
    CloseHandle(f);
    free(data);
}

static void test_playback(void)
{
    struct env e;
    WCHAR path[MAX_PATH];
    BSTR url, b;
    WMPPlayState ps;
    VARIANT_BOOL vb;
    DOUBLE pos;
    HRESULT hr;
    unsigned int i;
    static const WCHAR *names[] = {L"play", L"pause", L"stop"};

    if (!setup(&e)) return;
    GetTempPathW(MAX_PATH, path);
    lstrcatW(path, L"wmp-state-probe.wav");
    write_wave(path, 6);
    url = SysAllocString(path);

    IWMPSettings_put_autoStart(e.settings, VARIANT_FALSE);
    hr = IWMPPlayer4_put_URL(e.player, url);
    SysFreeString(url);
    hr = IWMPControls_play(e.controls);
    /* the first graph in a fresh prefix can fail while the media framework starts: try again */
    for (i = 0; FAILED(hr) && i < 3; i++)
    {
        pump(1500);
        hr = IWMPControls_play(e.controls);
    }
    pump(300);
    if (FAILED(hr))
    {
        printf("SKIP  no playback possible here (play = %08lx)\n", hr);
        goto done;
    }
    IWMPPlayer4_get_playState(e.player, &ps);
    CHECKF(ps == wmppsPlaying, "playing: playState = Playing (%d)", ps);
    b = NULL; IWMPPlayer4_get_status(e.player, &b);
    CHECKF(bstr_is(b, L"Playing"), "playing: Status = Playing");
    SysFreeString(b);
    {
        static const struct { const WCHAR *item; VARIANT_BOOL v; } playing[] = {{L"play", VARIANT_FALSE}, {L"pause", VARIANT_TRUE}, {L"stop", VARIANT_TRUE}, {L"currentPosition", VARIANT_TRUE}};
        for (i = 0; i < ARRAY_SIZE(playing); i++)
        {
            b = SysAllocString(playing[i].item);
            vb = 5; hr = IWMPControls_get_isAvailable(e.controls, b, &vb);
            CHECKF(hr == S_OK && vb == playing[i].v, "playing: isAvailable(%ls) = %d (%08lx %d)", playing[i].item, playing[i].v, hr, vb);
            SysFreeString(b);
        }
    }
    hr = IWMPControls_put_currentPosition(e.controls, 2.5);
    CHECKF(hr == S_OK, "put_currentPosition(2.5) (%08lx)", hr);
    hr = IWMPControls_pause(e.controls);
    CHECKF(hr == S_OK, "pause (%08lx)", hr);
    IWMPPlayer4_get_playState(e.player, &ps);
    CHECKF(ps == wmppsPaused, "paused: playState = Paused (%d)", ps);
    b = NULL; IWMPPlayer4_get_status(e.player, &b);
    CHECKF(bstr_is(b, L"Paused"), "paused: Status = Paused");
    SysFreeString(b);
    b = SysAllocString(L"play");
    vb = 5; IWMPControls_get_isAvailable(e.controls, b, &vb);
    CHECKF(vb == VARIANT_TRUE, "paused: isAvailable(play) = TRUE");
    SysFreeString(b);
    b = SysAllocString(L"pause");
    vb = 5; IWMPControls_get_isAvailable(e.controls, b, &vb);
    CHECKF(vb == VARIANT_FALSE, "paused: isAvailable(pause) = FALSE");
    SysFreeString(b);
    b = NULL; hr = IWMPControls_get_currentPositionString(e.controls, &b);
    pos = 0; IWMPControls_get_currentPosition(e.controls, &pos);
    CHECKF(hr == S_OK && pos >= 2.4 && pos < 3.0 && bstr_is(b, L"00:02"), "paused at %.2f s: currentPositionString = 00:02 (%08lx %ls)", pos, hr, b ? b : L"(null)");
    SysFreeString(b);
    IWMPControls_stop(e.controls);
    IWMPPlayer4_get_playState(e.player, &ps);
    CHECKF(ps == wmppsStopped, "stopped: playState = Stopped (%d)", ps);
    for (i = 0; i < 3; i++)
    {
        b = SysAllocString(names[i]);
        vb = 5; IWMPControls_get_isAvailable(e.controls, b, &vb);
        CHECKF(vb == (i == 0 ? VARIANT_TRUE : VARIANT_FALSE), "stopped: isAvailable(%ls) = %d", names[i], i == 0);
        SysFreeString(b);
    }
done:
    DeleteFileW(path);
    teardown(&e);
}

int main(void)
{
    CoInitialize(NULL);
    test_player();
    test_settings();
    test_network();
    test_controls();
    test_playback();
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
