/* SAPI voice (patch 2842), run by test/sapi-voice-gate.sh on Xvfb. A native
 * client with its own in-process text-to-speech engine (registered with
 * CoRegisterClassObject and a token in HKCU) so that Speak has something to
 * drive: ISpVoice properties, the event source (queue, interest, the five
 * notification mechanisms), GetStatus, outputs, Skip, SpeakStream and
 * SPF_IS_FILENAME, the engine site, and the ISpeechVoice / ISpeechVoiceStatus
 * automation, late-bound through IDispatch.
 *
 *   sapi-voice-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <initguid.h>
#include <ocidl.h>
#include <olectl.h>
#include <sapi.h>
#include <stdio.h>
#include <wchar.h>

DEFINE_GUID(IID_Bogus, 0x12345678, 0x1234, 0x1234, 0x12, 0x34, 0x12, 0x34, 0x12, 0x34, 0x12, 0x34);
DEFINE_GUID(PR_SPDFID_Text, 0x7ceef9f9, 0x3d13, 0x11d2, 0x9e, 0xe7, 0x00, 0xc0, 0x4f, 0x79, 0x73, 0x96);
DEFINE_GUID(PR_SPDFID_WaveFormatEx, 0xc31adbae, 0x527f, 0x4ff5, 0xa2, 0x30, 0xf6, 0x2b, 0xb6, 0x1f, 0xf7, 0x0c);
DEFINE_GUID(PR_IID_ISpTTSEngine, 0xa74d7c8e, 0x4cc5, 0x4f2f, 0xa6, 0xeb, 0x80, 0x4d, 0xee, 0x18, 0x50, 0x0e);
DEFINE_GUID(PR_IID_ISpTTSEngineSite, 0x9880499b, 0xcce9, 0x11d2, 0xb5, 0x03, 0x00, 0xc0, 0x4f, 0x79, 0x73, 0x96);
DEFINE_GUID(PR_CLSID_Engine, 0xa1b2c3d4, 0x5555, 0x6666, 0x77, 0x77, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88);

#define SPERR_UNINITIALIZED          ((HRESULT)0x80045001)
#define SPERR_ALREADY_INITIALIZED    ((HRESULT)0x80045002)
#define SPERR_NOT_FOUND              ((HRESULT)0x8004503a)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[300]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

/* ---- the engine -------------------------------------------------------- */
typedef struct { void *pNext; } FRAGHEAD;
typedef struct SPVTEXTFRAG_ { struct SPVTEXTFRAG_ *pNext;
    struct { int eAction; WORD LangID, wReserved; LONG EmphAdj, RateAdj; ULONG Volume; struct { LONG a, b; } PitchAdj; ULONG SilenceMSecs;
             void *pPhoneIds; int ePartOfSpeech; struct { const WCHAR *c, *b, *a; } Context; } State;
    const WCHAR *pTextStart; ULONG ulTextLen, ulTextSrcOffset; } FRAG;

typedef struct Site Site;
typedef struct SiteVtbl
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(Site *, REFIID, void **);
    ULONG (STDMETHODCALLTYPE *AddRef)(Site *);
    ULONG (STDMETHODCALLTYPE *Release)(Site *);
    HRESULT (STDMETHODCALLTYPE *AddEvents)(Site *, const SPEVENT *, ULONG);
    HRESULT (STDMETHODCALLTYPE *GetEventInterest)(Site *, ULONGLONG *);
    DWORD (STDMETHODCALLTYPE *GetActions)(Site *);
    HRESULT (STDMETHODCALLTYPE *Write)(Site *, const void *, ULONG, ULONG *);
    HRESULT (STDMETHODCALLTYPE *GetRate)(Site *, LONG *);
    HRESULT (STDMETHODCALLTYPE *GetVolume)(Site *, USHORT *);
    HRESULT (STDMETHODCALLTYPE *GetSkipInfo)(Site *, int *, LONG *);
    HRESULT (STDMETHODCALLTYPE *CompleteSkip)(Site *, LONG);
} SiteVtbl;
struct Site { const SiteVtbl *lpVtbl; };

typedef struct Engine Engine;
typedef struct EngineVtbl
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(Engine *, REFIID, void **);
    ULONG (STDMETHODCALLTYPE *AddRef)(Engine *);
    ULONG (STDMETHODCALLTYPE *Release)(Engine *);
    HRESULT (STDMETHODCALLTYPE *Speak)(Engine *, DWORD, REFGUID, const WAVEFORMATEX *, const FRAG *, Site *);
    HRESULT (STDMETHODCALLTYPE *GetOutputFormat)(Engine *, const GUID *, const WAVEFORMATEX *, GUID *, WAVEFORMATEX **);
} EngineVtbl;
struct Engine { const EngineVtbl *lpVtbl; ISpObjectWithToken tok; LONG ref; };

struct fragrec { int action; WCHAR text[64]; LONG emph, rate, pitch_mid, pitch_range; ULONG vol, silence; WORD lang; WCHAR phones[16], ctx[16]; int part; ULONG off; };
static struct fragrec g_frags[16];
static int g_nfrags;
static WCHAR g_spoken[16][256];
static int g_spoken_n;
static HANDLE g_gate, g_started;
static ULONGLONG g_interest_seen;
static LONG g_rate_seen = -1000;
static USHORT g_volume_seen = 999;
static LONG g_skip_info_idle = -1, g_skip_count_seen = -1;
static int g_skip_type_seen;
static DWORD g_actions_seen;
static HRESULT g_speak_hr = S_OK;
static BOOL g_token_set;

static HRESULT STDMETHODCALLTYPE eng_QI(Engine *This, REFIID iid, void **obj)
{
    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &PR_IID_ISpTTSEngine)) *obj = This;
    else if (IsEqualIID(iid, &IID_ISpObjectWithToken)) *obj = &This->tok;
    else { *obj = NULL; return E_NOINTERFACE; }
    InterlockedIncrement(&This->ref);
    return S_OK;
}
static ULONG STDMETHODCALLTYPE eng_AddRef(Engine *This) { return InterlockedIncrement(&This->ref); }
static ULONG STDMETHODCALLTYPE eng_Release(Engine *This)
{
    ULONG r = InterlockedDecrement(&This->ref);
    if (!r) free(This);
    return r;
}
static HRESULT STDMETHODCALLTYPE eng_GetOutputFormat(Engine *This, const GUID *tf, const WAVEFORMATEX *tw, GUID *out, WAVEFORMATEX **wfx)
{
    WAVEFORMATEX *w = CoTaskMemAlloc(sizeof(*w));
    memset(w, 0, sizeof(*w));
    w->wFormatTag = WAVE_FORMAT_PCM; w->nChannels = 1; w->nSamplesPerSec = 22050; w->nAvgBytesPerSec = 44100;
    w->nBlockAlign = 2; w->wBitsPerSample = 16;
    *out = PR_SPDFID_WaveFormatEx;
    *wfx = w;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE eng_Speak(Engine *This, DWORD flags, REFGUID fmt, const WAVEFORMATEX *wfx, const FRAG *frags, Site *site)
{
    WCHAR text[256] = L"";
    const FRAG *f;
    ULONG i, n;
    SPEVENT ev;
    LONG skip_count = -1;
    int skip_type = 0;
    unsigned char pcm[4410];
    ULONG written;
    DWORD actions;

    g_nfrags = 0;
    for (f = frags; f; f = f->pNext)
    {
        if (f->pTextStart) wcsncat(text, f->pTextStart, min(f->ulTextLen, 200));
        if (g_nfrags < 16)
        {
            struct fragrec *r = &g_frags[g_nfrags++];
            memset(r, 0, sizeof(*r));
            r->action = f->State.eAction;
            if (f->pTextStart) wcsncpy(r->text, f->pTextStart, min(f->ulTextLen, 63));
            r->emph = f->State.EmphAdj; r->rate = f->State.RateAdj; r->vol = f->State.Volume;
            r->pitch_mid = f->State.PitchAdj.a; r->pitch_range = f->State.PitchAdj.b;
            r->silence = f->State.SilenceMSecs; r->lang = f->State.LangID; r->part = f->State.ePartOfSpeech;
            if (f->State.pPhoneIds) wcsncpy(r->phones, f->State.pPhoneIds, 15);
            if (f->State.Context.c) wcsncpy(r->ctx, f->State.Context.c, 15);
            r->off = f->ulTextSrcOffset;
        }
    }
    if (g_spoken_n < 16) wcscpy(g_spoken[g_spoken_n++], text);

    site->lpVtbl->GetEventInterest(site, &g_interest_seen);
    site->lpVtbl->GetRate(site, &g_rate_seen);
    site->lpVtbl->GetVolume(site, &g_volume_seen);
    g_actions_seen = site->lpVtbl->GetActions(site);
    g_skip_info_idle = 0;
    site->lpVtbl->GetSkipInfo(site, &skip_type, &skip_count);
    g_skip_info_idle = skip_count;

    if (g_started) SetEvent(g_started);

    if (!wcscmp(text, L"blockme"))
        WaitForSingleObject(g_gate, 5000);
    if (!wcscmp(text, L"skipme"))
    {
        for (i = 0; i < 500; i++)
        {
            actions = site->lpVtbl->GetActions(site);
            if (actions & 2) break;
            Sleep(10);
        }
        if (actions & 2)
        {
            site->lpVtbl->GetSkipInfo(site, &g_skip_type_seen, &g_skip_count_seen);
            site->lpVtbl->CompleteSkip(site, g_skip_count_seen);
        }
        return S_OK;
    }

    /* word and sentence boundaries, and a bookmark when there is an @ */
    for (i = 0; text[i];)
    {
        while (text[i] == L' ') i++;
        if (!text[i]) break;
        n = 0;
        while (text[i + n] && text[i + n] != L' ') n++;
        memset(&ev, 0, sizeof(ev));
        ev.eEventId = SPEI_WORD_BOUNDARY;
        ev.wParam = n;
        ev.lParam = i;
        site->lpVtbl->AddEvents(site, &ev, 1);
        i += n;
    }
    memset(&ev, 0, sizeof(ev));
    ev.eEventId = SPEI_SENTENCE_BOUNDARY;
    ev.wParam = wcslen(text);
    ev.lParam = 0;
    site->lpVtbl->AddEvents(site, &ev, 1);
    if (wcschr(text, L'@'))
    {
        memset(&ev, 0, sizeof(ev));
        ev.eEventId = SPEI_TTS_BOOKMARK;
        ev.elParamType = SPET_LPARAM_IS_STRING;
        ev.wParam = 7;
        ev.lParam = (LPARAM)L"mark";
        site->lpVtbl->AddEvents(site, &ev, 1);
    }

    for (i = 0; i < sizeof(pcm); i++) pcm[i] = (unsigned char)i;
    site->lpVtbl->Write(site, pcm, sizeof(pcm), &written);
    return g_speak_hr;
}
static const EngineVtbl engine_vtbl = { eng_QI, eng_AddRef, eng_Release, eng_Speak, eng_GetOutputFormat };

static HRESULT STDMETHODCALLTYPE ewt_QI(ISpObjectWithToken *This, REFIID iid, void **obj)
{
    Engine *e = (Engine *)((char *)This - offsetof(Engine, tok));
    return eng_QI(e, iid, obj);
}
static ULONG STDMETHODCALLTYPE ewt_AddRef(ISpObjectWithToken *This) { return eng_AddRef((Engine *)((char *)This - offsetof(Engine, tok))); }
static ULONG STDMETHODCALLTYPE ewt_Release(ISpObjectWithToken *This) { return eng_Release((Engine *)((char *)This - offsetof(Engine, tok))); }
static HRESULT STDMETHODCALLTYPE ewt_Set(ISpObjectWithToken *This, ISpObjectToken *t) { g_token_set = TRUE; return S_OK; }
static HRESULT STDMETHODCALLTYPE ewt_Get(ISpObjectWithToken *This, ISpObjectToken **t) { return E_NOTIMPL; }
static const ISpObjectWithTokenVtbl ewt_vtbl = { ewt_QI, ewt_AddRef, ewt_Release, ewt_Set, ewt_Get };

static HRESULT STDMETHODCALLTYPE cf_QI(IClassFactory *This, REFIID iid, void **obj)
{
    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_IClassFactory)) { *obj = This; return S_OK; }
    *obj = NULL;
    return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE cf_AddRef(IClassFactory *This) { return 2; }
static ULONG STDMETHODCALLTYPE cf_Release(IClassFactory *This) { return 1; }
static HRESULT STDMETHODCALLTYPE cf_Create(IClassFactory *This, IUnknown *outer, REFIID iid, void **obj)
{
    Engine *e = calloc(1, sizeof(*e));
    HRESULT hr;
    e->lpVtbl = &engine_vtbl;
    e->tok.lpVtbl = &ewt_vtbl;
    e->ref = 1;
    hr = eng_QI(e, iid, obj);
    eng_Release(e);
    return hr;
}
static HRESULT STDMETHODCALLTYPE cf_Lock(IClassFactory *This, BOOL l) { return S_OK; }
static const IClassFactoryVtbl cf_vtbl = { cf_QI, cf_AddRef, cf_Release, cf_Create, cf_Lock };
static IClassFactory engine_cf = { &cf_vtbl };

/* ---- notification receivers --------------------------------------------- */
static LONG g_cb_count; static WPARAM g_cb_w; static LPARAM g_cb_l;
static void __stdcall callback_fn(WPARAM w, LPARAM l) { g_cb_w = w; g_cb_l = l; InterlockedIncrement(&g_cb_count); }

static LONG g_sink_count;
typedef struct { const ISpNotifySinkVtbl *lpVtbl; } SinkObj;
static HRESULT STDMETHODCALLTYPE sink_QI(ISpNotifySink *This, REFIID iid, void **obj)
{ if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_ISpNotifySink)) { *obj = This; return S_OK; } *obj = NULL; return E_NOINTERFACE; }
static ULONG STDMETHODCALLTYPE sink_AddRef(ISpNotifySink *This) { return 2; }
static ULONG STDMETHODCALLTYPE sink_Release(ISpNotifySink *This) { return 1; }
static HRESULT STDMETHODCALLTYPE sink_Notify(ISpNotifySink *This) { InterlockedIncrement(&g_sink_count); return S_OK; }
static const ISpNotifySinkVtbl sink_vtbl = { sink_QI, sink_AddRef, sink_Release, sink_Notify };
static ISpNotifySink sink_obj = { (ISpNotifySinkVtbl *)&sink_vtbl };

static LONG g_cbi_count; static WPARAM g_cbi_w; static LPARAM g_cbi_l;
struct cbi_vtbl { HRESULT (STDMETHODCALLTYPE *NotifyCallback)(void *, WPARAM, LPARAM); };
static HRESULT STDMETHODCALLTYPE cbi_Notify(void *This, WPARAM w, LPARAM l) { g_cbi_w = w; g_cbi_l = l; InterlockedIncrement(&g_cbi_count); return S_OK; }
static const struct cbi_vtbl cbi_vtbl_obj = { cbi_Notify };
static const struct cbi_vtbl *cbi_obj = &cbi_vtbl_obj;

/* ---- helpers -------------------------------------------------------------- */
#define VCAT L"HKEY_CURRENT_USER\\Software\\SGProbeVoice"
static void reg_set(const WCHAR *sub, const WCHAR *name, const WCHAR *val)
{
    HKEY k;
    RegCreateKeyExW(HKEY_CURRENT_USER, sub, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL);
    RegSetValueExW(k, name, 0, REG_SZ, (const BYTE *)val, (wcslen(val) + 1) * sizeof(WCHAR));
    RegCloseKey(k);
}

static ISpObjectToken *token_for(const WCHAR *name)
{
    ISpObjectToken *t = NULL;
    WCHAR id[256];
    swprintf(id, 256, VCAT L"\\Tokens\\%ls", name);
    CoCreateInstance(&CLSID_SpObjectToken, NULL, CLSCTX_INPROC_SERVER, &IID_ISpObjectToken, (void **)&t);
    if (t && FAILED(ISpObjectToken_SetId(t, NULL, id, FALSE))) { ISpObjectToken_Release(t); t = NULL; }
    return t;
}

static WAVEFORMATEX out_wfx = { WAVE_FORMAT_PCM, 1, 22050, 44100, 2, 16, 0 };

/* A new voice with the probe engine as its voice and a memory stream as its output. */
static ISpVoice *new_voice(IStream **mem_out, ISpStream **sp_out)
{
    ISpVoice *v = NULL;
    ISpObjectToken *t;
    IStream *mem;
    ISpStream *sp;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_SpVoice, NULL, CLSCTX_INPROC_SERVER, &IID_ISpVoice, (void **)&v);
    if (hr != S_OK) return NULL;
    t = token_for(L"Probe");
    hr = ISpVoice_SetVoice(v, t);
    ISpObjectToken_Release(t);
    CreateStreamOnHGlobal(NULL, TRUE, &mem);
    CoCreateInstance(&CLSID_SpStream, NULL, CLSCTX_INPROC_SERVER, &IID_ISpStream, (void **)&sp);
    ISpStream_SetBaseStream(sp, mem, &PR_SPDFID_WaveFormatEx, &out_wfx);
    hr = ISpVoice_SetOutput(v, (IUnknown *)sp, TRUE);
    if (hr != S_OK) printf("note  SetOutput %#lx\n", hr);
    if (mem_out) *mem_out = mem; else IStream_Release(mem);
    if (sp_out) *sp_out = sp; else ISpStream_Release(sp);
    return v;
}

static ULONG mem_size(IStream *mem)
{
    STATSTG st;
    IStream_Stat(mem, &st, STATFLAG_NONAME);
    return st.cbSize.LowPart;
}

/* ---- late binding ------------------------------------------------------ */
static HRESULT dget(IDispatch *d, const WCHAR *name, VARIANT *res, VARIANT *args, int nargs, WORD flags)
{
    DISPID id, named = DISPID_PROPERTYPUT;
    DISPPARAMS dp;
    VARIANT rev[6];
    EXCEPINFO ex;
    UINT argerr = 0;
    HRESULT hr;
    LPOLESTR n = (LPOLESTR)name;
    int i;

    hr = IDispatch_GetIDsOfNames(d, &IID_NULL, &n, 1, LOCALE_USER_DEFAULT, &id);
    if (FAILED(hr)) return hr;
    for (i = 0; i < nargs; i++) rev[i] = args[nargs - 1 - i];
    dp.rgvarg = rev;
    dp.cArgs = nargs;
    dp.rgdispidNamedArgs = (flags & (DISPATCH_PROPERTYPUT | DISPATCH_PROPERTYPUTREF)) ? &named : NULL;
    dp.cNamedArgs = dp.rgdispidNamedArgs ? 1 : 0;
    if (res) VariantInit(res);
    memset(&ex, 0, sizeof(ex));
    hr = IDispatch_Invoke(d, id, &IID_NULL, LOCALE_USER_DEFAULT, flags, &dp, res, &ex, &argerr);
    if (hr == DISP_E_EXCEPTION) hr = ex.scode;
    return hr;
}
static VARIANT vstr(const WCHAR *s) { VARIANT v; V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(s); return v; }
static VARIANT vi4(LONG l) { VARIANT v; V_VT(&v) = VT_I4; V_I4(&v) = l; return v; }
static VARIANT vbool(BOOL b) { VARIANT v; V_VT(&v) = VT_BOOL; V_BOOL(&v) = b ? VARIANT_TRUE : VARIANT_FALSE; return v; }
static VARIANT vdisp(IDispatch *d) { VARIANT v; V_VT(&v) = VT_DISPATCH; V_DISPATCH(&v) = d; return v; }
static HRESULT dcall(IDispatch *d, const WCHAR *name, VARIANT *res, VARIANT *args, int nargs)
{ return dget(d, name, res, args, nargs, DISPATCH_METHOD | DISPATCH_PROPERTYGET); }
static HRESULT dprop(IDispatch *d, const WCHAR *name, VARIANT *res)
{ return dget(d, name, res, NULL, 0, DISPATCH_PROPERTYGET); }
static HRESULT dput(IDispatch *d, const WCHAR *name, VARIANT v)
{ return dget(d, name, NULL, &v, 1, DISPATCH_PROPERTYPUT); }
static HRESULT dputref(IDispatch *d, const WCHAR *name, IDispatch *o)
{ VARIANT v = vdisp(o); return dget(d, name, NULL, &v, 1, DISPATCH_PROPERTYPUTREF); }
static LONG dlong(IDispatch *d, const WCHAR *name, HRESULT *hr)
{
    VARIANT r;
    HRESULT h = dprop(d, name, &r);
    LONG l = -12345;
    if (h == S_OK) { VariantChangeType(&r, &r, 0, VT_I4); l = V_I4(&r); }
    if (hr) *hr = h;
    VariantClear(&r);
    return l;
}

/* ---- ISpVoice properties and events ------------------------------------- */
static void test_properties(void)
{
    ISpVoice *v;
    SPVPRIORITY pri = 99;
    SPEVENTENUM ab = 99;
    ULONG timeout = 0;
    HRESULT hr;
    int i;

    v = new_voice(NULL, NULL);
    CHECKF(v != NULL, "SpVoice with the probe engine");
    if (!v) return;

    hr = ISpVoice_GetPriority(v, &pri);
    CHECKF(hr == S_OK && pri == SPVPRI_NORMAL, "priority starts as normal (%#lx, %d)", hr, pri);
    for (i = 0; i < 3; i++)
    {
        hr = ISpVoice_SetPriority(v, (SPVPRIORITY)i);
        ISpVoice_GetPriority(v, &pri);
        CHECKF(hr == S_OK && pri == i, "SetPriority(%d) is kept (%#lx)", i, hr);
    }
    hr = ISpVoice_SetPriority(v, (SPVPRIORITY)3);
    CHECKF(hr == E_INVALIDARG, "SetPriority(3) is E_INVALIDARG (%#lx)", hr);
    hr = ISpVoice_SetPriority(v, (SPVPRIORITY)-1);
    CHECKF(hr == E_INVALIDARG, "SetPriority(-1) is E_INVALIDARG (%#lx)", hr);
    ISpVoice_GetPriority(v, &pri);
    CHECKF(pri == SPVPRI_OVER, "a rejected priority leaves the old one");
    hr = ISpVoice_GetPriority(v, NULL);
    CHECKF(hr == E_POINTER, "GetPriority(NULL) is E_POINTER (%#lx)", hr);

    hr = ISpVoice_GetAlertBoundary(v, &ab);
    CHECKF(hr == S_OK && ab == SPEI_WORD_BOUNDARY, "alert boundary starts as the word boundary (%#lx, %d)", hr, ab);
    hr = ISpVoice_SetAlertBoundary(v, SPEI_SENTENCE_BOUNDARY);
    ISpVoice_GetAlertBoundary(v, &ab);
    CHECKF(hr == S_OK && ab == SPEI_SENTENCE_BOUNDARY, "SetAlertBoundary(sentence) (%#lx)", hr);
    hr = ISpVoice_SetAlertBoundary(v, SPEI_UNDEFINED);
    CHECKF(hr == E_INVALIDARG, "SetAlertBoundary(0) is E_INVALIDARG (%#lx)", hr);
    hr = ISpVoice_SetAlertBoundary(v, (SPEVENTENUM)16);
    CHECKF(hr == E_INVALIDARG, "SetAlertBoundary(16) is E_INVALIDARG (%#lx)", hr);
    hr = ISpVoice_SetAlertBoundary(v, SPEI_VISEME);
    CHECKF(hr == S_OK, "SetAlertBoundary(viseme) (%#lx)", hr);
    hr = ISpVoice_GetAlertBoundary(v, NULL);
    CHECKF(hr == E_POINTER, "GetAlertBoundary(NULL) is E_POINTER (%#lx)", hr);

    hr = ISpVoice_GetSyncSpeakTimeout(v, &timeout);
    CHECKF(hr == S_OK && timeout == 10000, "sync speak timeout starts at 10000 (%#lx, %lu)", hr, timeout);
    hr = ISpVoice_SetSyncSpeakTimeout(v, 1234);
    ISpVoice_GetSyncSpeakTimeout(v, &timeout);
    CHECKF(hr == S_OK && timeout == 1234, "SetSyncSpeakTimeout is kept");
    hr = ISpVoice_GetSyncSpeakTimeout(v, NULL);
    CHECKF(hr == E_POINTER, "GetSyncSpeakTimeout(NULL) is E_POINTER (%#lx)", hr);

    hr = ISpVoice_Pause(v);
    CHECKF(hr == S_OK, "Pause (%#lx)", hr);
    hr = ISpVoice_Resume(v);
    CHECKF(hr == S_OK, "Resume (%#lx)", hr);
    ISpVoice_Release(v);
}

static void test_events(void)
{
    ISpVoice *v;
    IStream *mem;
    SPEVENT ev[16];
    SPEVENTSOURCEINFO info;
    ULONG n, stream, i;
    HANDLE h;
    HRESULT hr;
    static const struct { int id; } want[] = { { SPEI_START_INPUT_STREAM }, { SPEI_WORD_BOUNDARY }, { SPEI_WORD_BOUNDARY },
        { SPEI_SENTENCE_BOUNDARY }, { SPEI_TTS_BOOKMARK }, { SPEI_END_INPUT_STREAM } };

    v = new_voice(&mem, NULL);
    hr = ISpVoice_GetInfo(v, &info);
    CHECKF(hr == S_OK && info.ullEventInterest == SPFEI_ALL_TTS_EVENTS && info.ullQueuedInterest == SPFEI_ALL_TTS_EVENTS && info.ulCount == 0,
           "GetInfo: all TTS events, empty queue (%#lx, %s)", hr, "x");
    hr = ISpVoice_GetInfo(v, NULL);
    CHECKF(hr == E_POINTER, "GetInfo(NULL) is E_POINTER (%#lx)", hr);

    hr = ISpVoice_SetInterest(v, 0, 0);
    CHECKF(hr == E_INVALIDARG, "SetInterest without the flag bits is E_INVALIDARG (%#lx)", hr);
    hr = ISpVoice_SetInterest(v, SPFEI(SPEI_WORD_BOUNDARY), SPFEI(SPEI_END_INPUT_STREAM));
    CHECKF(hr == E_INVALIDARG, "SetInterest with queued not within interest is E_INVALIDARG (%#lx)", hr);
    hr = ISpVoice_GetEvents(v, 0, ev, &n);
    CHECKF(hr == E_INVALIDARG, "GetEvents(0) is E_INVALIDARG (%#lx)", hr);
    hr = ISpVoice_GetEvents(v, 4, NULL, &n);
    CHECKF(hr == E_POINTER, "GetEvents without an array is E_POINTER (%#lx)", hr);
    n = 77;
    hr = ISpVoice_GetEvents(v, 4, ev, &n);
    CHECKF(hr == S_FALSE && n == 0, "GetEvents of an empty queue is S_FALSE with 0 (%#lx, %lu)", hr, n);

    /* Win32 event notification */
    h = ISpVoice_GetNotifyEventHandle(v);
    CHECKF(h == NULL, "GetNotifyEventHandle before a notification is chosen is NULL");
    hr = ISpVoice_WaitForNotifyEvent(v, 0);
    CHECKF(hr == SPERR_UNINITIALIZED, "WaitForNotifyEvent before a notification is chosen (%#lx)", hr);
    hr = ISpVoice_SetNotifyWin32Event(v);
    CHECKF(hr == S_OK, "SetNotifyWin32Event (%#lx)", hr);
    hr = ISpVoice_SetNotifyWin32Event(v);
    CHECKF(hr == SPERR_ALREADY_INITIALIZED, "SetNotifyWin32Event twice (%#lx)", hr);
    hr = ISpVoice_SetNotifySink(v, &sink_obj);
    CHECKF(hr == SPERR_ALREADY_INITIALIZED, "another notification kind is refused (%#lx)", hr);
    h = ISpVoice_GetNotifyEventHandle(v);
    CHECKF(h != NULL, "GetNotifyEventHandle gives the event");
    hr = ISpVoice_WaitForNotifyEvent(v, 0);
    CHECKF(hr == S_FALSE, "WaitForNotifyEvent without events times out with S_FALSE (%#lx)", hr);

    g_spoken_n = 0;
    stream = 0;
    hr = ISpVoice_Speak(v, L"ab cd @", 0, &stream);
    CHECKF(hr == S_OK && stream != 0, "Speak (%#lx, stream %lu)", hr, stream);
    CHECKF(g_spoken_n == 1 && !wcscmp(g_spoken[0], L"ab cd @"), "the engine got the text");
    CHECKF(g_interest_seen == SPFEI_ALL_TTS_EVENTS, "engine site GetEventInterest reports the voice's interest");
    CHECKF(mem_size(mem) == 4410, "the engine's audio reached the output (%lu bytes)", mem_size(mem));
    hr = ISpVoice_WaitForNotifyEvent(v, 0);
    CHECKF(hr == S_OK, "WaitForNotifyEvent after events (%#lx)", hr);
    CHECKF(WaitForSingleObject(h, 0) == WAIT_OBJECT_0, "the notify event is signaled while events are queued");
    ISpVoice_GetInfo(v, &info);
    CHECKF(info.ulCount == 7, "GetInfo counts the queued events (%lu)", info.ulCount);
    n = 0;
    memset(ev, 0, sizeof(ev));
    hr = ISpVoice_GetEvents(v, 2, ev, &n);
    CHECKF(hr == S_OK && n == 2, "GetEvents(2) takes two (%#lx, %lu)", hr, n);
    CHECKF(ev[0].eEventId == SPEI_START_INPUT_STREAM && ev[0].ulStreamNum == stream, "first event: start of the input stream, stream number %lu", stream);
    CHECKF(ev[1].eEventId == SPEI_WORD_BOUNDARY && ev[1].wParam == 2 && ev[1].lParam == 0 && ev[1].ulStreamNum == stream,
           "second event: word boundary, length 2 at 0 (%lu %ld)", (ULONG)ev[1].wParam, (long)ev[1].lParam);
    memset(ev, 0, sizeof(ev));
    hr = ISpVoice_GetEvents(v, 16, ev, &n);
    CHECKF(hr == S_FALSE && n == 5, "GetEvents(16) returns the other five and S_FALSE (%#lx, %lu)", hr, n);
    CHECKF(ev[0].eEventId == SPEI_WORD_BOUNDARY && ev[0].wParam == 2 && ev[0].lParam == 3, "third event: word boundary, length 2 at 3");
    CHECKF(ev[1].eEventId == SPEI_WORD_BOUNDARY && ev[1].wParam == 1 && ev[1].lParam == 6, "fourth event: word boundary, length 1 at 6");
    CHECKF(ev[2].eEventId == SPEI_SENTENCE_BOUNDARY && ev[2].wParam == 7, "fifth event: sentence boundary, length 7");
    CHECKF(ev[3].eEventId == SPEI_TTS_BOOKMARK && ev[3].elParamType == SPET_LPARAM_IS_STRING && ev[3].wParam == 7 &&
           ev[3].lParam && !wcscmp((WCHAR *)ev[3].lParam, L"mark"), "bookmark event carries id 7 and the string mark");
    CHECKF(ev[3].lParam != (LPARAM)L"mark", "the bookmark string was copied for the queue");
    CHECKF(ev[4].eEventId == SPEI_END_INPUT_STREAM && ev[4].ulStreamNum == stream, "last event: end of the input stream");
    CoTaskMemFree((void *)ev[3].lParam);
    (void)want; (void)i;
    CHECKF(WaitForSingleObject(h, 0) == WAIT_TIMEOUT, "the notify event is reset when the queue is empty");
    hr = ISpVoice_WaitForNotifyEvent(v, 0);
    CHECKF(hr == S_FALSE, "WaitForNotifyEvent after draining times out");

    /* Interest: only the end of the stream is queued; words do not notify. */
    hr = ISpVoice_SetInterest(v, SPFEI(SPEI_END_INPUT_STREAM), SPFEI(SPEI_END_INPUT_STREAM));
    CHECKF(hr == S_OK, "SetInterest(end of stream) (%#lx)", hr);
    ISpVoice_GetInfo(v, &info);
    CHECKF(info.ullEventInterest == SPFEI(SPEI_END_INPUT_STREAM) && info.ullQueuedInterest == SPFEI(SPEI_END_INPUT_STREAM),
           "GetInfo reads the interest back");
    ISpVoice_Speak(v, L"x y", 0, &stream);
    CHECKF(g_interest_seen == SPFEI(SPEI_END_INPUT_STREAM), "the engine site follows the interest");
    memset(ev, 0, sizeof(ev));
    hr = ISpVoice_GetEvents(v, 16, ev, &n);
    CHECKF(n == 1 && ev[0].eEventId == SPEI_END_INPUT_STREAM && ev[0].ulStreamNum == stream, "only the end event was queued (%lu events)", n);
    ISpVoice_Release(v);
    IStream_Release(mem);
}

static LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM w, LPARAM l) { return DefWindowProcW(h, m, w, l); }

static void test_notify_kinds(void)
{
    ISpVoice *v;
    IStream *mem;
    HRESULT hr;
    WNDCLASSW wc = { 0 };
    HWND hwnd;
    MSG msg;
    int got;

    /* window message */
    v = new_voice(&mem, NULL);
    wc.lpfnWndProc = wndproc; wc.hInstance = GetModuleHandleW(NULL); wc.lpszClassName = L"SGProbeSapiWnd";
    RegisterClassW(&wc);
    hwnd = CreateWindowW(L"SGProbeSapiWnd", L"probe", 0, 0, 0, 10, 10, NULL, NULL, wc.hInstance, NULL);
    hr = ISpVoice_SetNotifyWindowMessage(v, NULL, WM_USER + 5, 1, 2);
    CHECKF(hr == E_INVALIDARG, "SetNotifyWindowMessage(NULL window) is E_INVALIDARG (%#lx)", hr);
    hr = ISpVoice_SetNotifyWindowMessage(v, hwnd, WM_USER + 5, 11, 22);
    CHECKF(hr == S_OK, "SetNotifyWindowMessage (%#lx)", hr);
    hr = ISpVoice_SetNotifyWindowMessage(v, hwnd, WM_USER + 5, 11, 22);
    CHECKF(hr == SPERR_ALREADY_INITIALIZED, "SetNotifyWindowMessage twice (%#lx)", hr);
    ISpVoice_Speak(v, L"w", 0, NULL);
    got = 0;
    while (PeekMessageW(&msg, hwnd, WM_USER + 5, WM_USER + 5, PM_REMOVE))
        if (msg.wParam == 11 && msg.lParam == 22) got++;
    CHECKF(got >= 1, "the window got its message with wparam and lparam (%d)", got);
    DestroyWindow(hwnd);
    ISpVoice_Release(v);
    IStream_Release(mem);

    /* callback function */
    v = new_voice(&mem, NULL);
    hr = ISpVoice_SetNotifyCallbackFunction(v, NULL, 1, 2);
    CHECKF(hr == E_INVALIDARG, "SetNotifyCallbackFunction(NULL) is E_INVALIDARG (%#lx)", hr);
    hr = ISpVoice_SetNotifyCallbackFunction(v, callback_fn, 33, 44);
    CHECKF(hr == S_OK, "SetNotifyCallbackFunction (%#lx)", hr);
    g_cb_count = 0;
    ISpVoice_Speak(v, L"c", 0, NULL);
    CHECKF(g_cb_count >= 1 && g_cb_w == 33 && g_cb_l == 44, "the callback ran with its arguments (%ld)", g_cb_count);
    ISpVoice_Release(v);
    IStream_Release(mem);

    /* sink */
    v = new_voice(&mem, NULL);
    hr = ISpVoice_SetNotifySink(v, NULL);
    CHECKF(hr == E_INVALIDARG, "SetNotifySink(NULL) is E_INVALIDARG (%#lx)", hr);
    hr = ISpVoice_SetNotifySink(v, &sink_obj);
    CHECKF(hr == S_OK, "SetNotifySink (%#lx)", hr);
    g_sink_count = 0;
    ISpVoice_Speak(v, L"s", 0, NULL);
    CHECKF(g_sink_count >= 1, "the sink was notified (%ld)", g_sink_count);
    ISpVoice_Release(v);
    IStream_Release(mem);

    /* callback interface */
    v = new_voice(&mem, NULL);
    hr = ISpVoice_SetNotifyCallbackInterface(v, NULL, 1, 2);
    CHECKF(hr == E_INVALIDARG, "SetNotifyCallbackInterface(NULL) is E_INVALIDARG (%#lx)", hr);
    hr = ISpVoice_SetNotifyCallbackInterface(v, (ISpNotifyCallback *)&cbi_obj, 55, 66);
    CHECKF(hr == S_OK, "SetNotifyCallbackInterface (%#lx)", hr);
    g_cbi_count = 0;
    ISpVoice_Speak(v, L"i", 0, NULL);
    CHECKF(g_cbi_count >= 1 && g_cbi_w == 55 && g_cbi_l == 66, "the callback interface was called with its arguments (%ld)", g_cbi_count);
    ISpVoice_Release(v);
    IStream_Release(mem);
}

/* ---- status, outputs, skip, streams ------------------------------------ */
static void test_status_and_output(void)
{
    ISpVoice *v;
    IStream *mem;
    ISpStream *sp;
    ISpObjectToken *tok, *tok2, *out_tok;
    ISpStreamFormat *fmt;
    SPVOICESTATUS st;
    WCHAR *bm;
    ULONG stream, n;
    HANDLE done_ev;
    HRESULT hr;
    IUnknown *u1, *u2;

    v = new_voice(&mem, &sp);
    memset(&st, 0xcc, sizeof(st));
    bm = NULL;
    hr = ISpVoice_GetStatus(v, &st, &bm);
    CHECKF(hr == S_OK && st.ulCurrentStream == 0 && st.ulLastStreamQueued == 0 && st.hrLastResult == S_OK && st.dwRunningState == SPRS_DONE &&
           st.ulInputWordPos == 0 && st.ulInputWordLen == 0 && st.lBookmarkId == 0, "GetStatus of an idle new voice (%#lx, state %lu)", hr, st.dwRunningState);
    CHECKF(bm && !*bm, "GetStatus: no bookmark gives an empty string");
    CoTaskMemFree(bm);
    hr = ISpVoice_GetStatus(v, NULL, NULL);
    CHECKF(hr == E_POINTER, "GetStatus(NULL) is E_POINTER (%#lx)", hr);

    done_ev = ISpVoice_SpeakCompleteEvent(v);
    CHECKF(done_ev != NULL && WaitForSingleObject(done_ev, 0) == WAIT_OBJECT_0, "SpeakCompleteEvent is signaled while idle");

    g_gate = CreateEventW(NULL, TRUE, FALSE, NULL);
    g_started = CreateEventW(NULL, FALSE, FALSE, NULL);
    stream = 0;
    hr = ISpVoice_Speak(v, L"blockme", SPF_ASYNC, &stream);
    CHECKF(hr == S_OK && stream == 1, "asynchronous Speak returns stream 1 (%#lx, %lu)", hr, stream);
    WaitForSingleObject(g_started, 5000);
    hr = ISpVoice_GetStatus(v, &st, NULL);
    CHECKF(hr == S_OK && st.dwRunningState == SPRS_IS_SPEAKING && st.ulCurrentStream == 1 && st.ulLastStreamQueued == 1,
           "while speaking: running, stream 1 (%#lx, state %lu)", hr, st.dwRunningState);
    CHECKF(WaitForSingleObject(done_ev, 0) == WAIT_TIMEOUT, "SpeakCompleteEvent is not signaled while speaking");
    hr = ISpVoice_WaitUntilDone(v, 0);
    CHECKF(hr == S_FALSE, "WaitUntilDone(0) while speaking is S_FALSE (%#lx)", hr);
    SetEvent(g_gate);
    hr = ISpVoice_WaitUntilDone(v, 5000);
    CHECKF(hr == S_OK, "WaitUntilDone after the gate opens (%#lx)", hr);
    CHECKF(WaitForSingleObject(done_ev, 1000) == WAIT_OBJECT_0, "SpeakCompleteEvent is signaled when done");
    ISpVoice_Speak(v, L"one two @", 0, &stream);
    bm = NULL;
    hr = ISpVoice_GetStatus(v, &st, &bm);
    CHECKF(hr == S_OK && st.ulLastStreamQueued == 2 && st.ulCurrentStream == 2 && st.dwRunningState == SPRS_DONE && st.hrLastResult == S_OK,
           "after the second Speak: stream 2, done, S_OK (%lu %lu)", st.ulLastStreamQueued, st.ulCurrentStream);
    CHECKF(st.ulInputWordPos == 8 && st.ulInputWordLen == 1, "the last word was at 8, length 1 (%lu, %lu)", st.ulInputWordPos, st.ulInputWordLen);
    CHECKF(st.ulInputSentPos == 0 && st.ulInputSentLen == 9, "the sentence was length 9 at 0 (%lu, %lu)", st.ulInputSentPos, st.ulInputSentLen);
    CHECKF(st.lBookmarkId == 7 && bm && !wcscmp(bm, L"mark"), "the last bookmark was 7, mark");
    CoTaskMemFree(bm);
    g_speak_hr = E_FAIL;
    hr = ISpVoice_Speak(v, L"fail", 0, &stream);
    CHECKF(hr == E_FAIL, "Speak returns the engine's failure (%#lx)", hr);
    ISpVoice_GetStatus(v, &st, NULL);
    CHECKF(st.hrLastResult == E_FAIL, "GetStatus keeps the last result (%#lx)", st.hrLastResult);
    g_speak_hr = S_OK;

    /* Rate and volume reach the engine */
    ISpVoice_SetRate(v, 3);
    ISpVoice_SetVolume(v, 55);
    ISpVoice_Speak(v, L"rv", 0, NULL);
    CHECKF(g_rate_seen == 3 && g_volume_seen == 55, "the engine site reports rate 3 and volume 55 (%ld, %u)", g_rate_seen, g_volume_seen);

    /* Output as a stream */
    out_tok = (ISpObjectToken *)0x55;
    hr = ISpVoice_GetOutputObjectToken(v, &out_tok);
    CHECKF(hr == S_FALSE && out_tok == NULL, "GetOutputObjectToken is S_FALSE for a stream output (%#lx)", hr);
    hr = ISpVoice_GetOutputObjectToken(v, NULL);
    CHECKF(hr == E_POINTER, "GetOutputObjectToken(NULL) is E_POINTER (%#lx)", hr);
    fmt = NULL;
    hr = ISpVoice_GetOutputStream(v, &fmt);
    CHECKF(hr == S_OK && fmt, "GetOutputStream (%#lx)", hr);
    if (fmt)
    {
        ISpStreamFormat_QueryInterface(fmt, &IID_IUnknown, (void **)&u1);
        ISpStream_QueryInterface(sp, &IID_IUnknown, (void **)&u2);
        CHECKF(u1 == u2, "GetOutputStream returns the stream that was given");
        IUnknown_Release(u1); IUnknown_Release(u2);
        ISpStreamFormat_Release(fmt);
    }
    hr = ISpVoice_GetOutputStream(v, NULL);
    CHECKF(hr == E_POINTER, "GetOutputStream(NULL) is E_POINTER (%#lx)", hr);

    /* Output as a token */
    reg_set(L"Software\\SGProbeVoice\\Tokens\\AudioTok", L"CLSID", L"{715d9c59-4442-11d2-9605-00c04f8ee628}");
    tok = token_for(L"AudioTok");
    hr = ISpVoice_SetOutput(v, (IUnknown *)tok, TRUE);
    CHECKF(hr == S_OK, "SetOutput(token) (%#lx)", hr);
    out_tok = NULL;
    hr = ISpVoice_GetOutputObjectToken(v, &out_tok);
    CHECKF(hr == S_OK && out_tok, "GetOutputObjectToken after SetOutput(token) (%#lx)", hr);
    if (out_tok)
    {
        WCHAR *id1 = NULL, *id2 = NULL;
        ISpObjectToken_GetId(out_tok, &id1);
        ISpObjectToken_GetId(tok, &id2);
        CHECKF(id1 && id2 && !wcscmp(id1, id2), "the output token is the token given");
        CoTaskMemFree(id1); CoTaskMemFree(id2);
        ISpObjectToken_Release(out_tok);
    }
    tok2 = NULL;
    hr = ISpVoice_GetOutputStream(v, &fmt);
    CHECKF(hr == S_OK && fmt, "GetOutputStream after SetOutput(token): the object the token made (%#lx)", hr);
    if (fmt) ISpStreamFormat_Release(fmt);
    ISpObjectToken_Release(tok);
    (void)tok2; (void)n;

    CloseHandle(g_gate); CloseHandle(g_started);
    g_gate = g_started = NULL;
    ISpStream_Release(sp);
    IStream_Release(mem);
    ISpVoice_Release(v);
}

static void test_skip(void)
{
    ISpVoice *v;
    IStream *mem;
    ULONG skipped = 99, stream;
    HRESULT hr;

    v = new_voice(&mem, NULL);
    hr = ISpVoice_Skip(v, NULL, 1, &skipped);
    CHECKF(hr == E_POINTER, "Skip(NULL type) is E_POINTER (%#lx)", hr);
    hr = ISpVoice_Skip(v, L"Sentence", 1, NULL);
    CHECKF(hr == E_POINTER, "Skip(NULL count) is E_POINTER (%#lx)", hr);
    hr = ISpVoice_Skip(v, L"Word", 1, &skipped);
    CHECKF(hr == E_INVALIDARG, "Skip of an unknown type is E_INVALIDARG (%#lx)", hr);
    hr = ISpVoice_Skip(v, L"sentence", 2, &skipped);
    CHECKF(hr == S_OK && skipped == 0, "Skip with nothing speaking skips nothing (%#lx, %lu)", hr, skipped);

    g_started = CreateEventW(NULL, FALSE, FALSE, NULL);
    g_skip_count_seen = -1;
    hr = ISpVoice_Speak(v, L"skipme", SPF_ASYNC, &stream);
    CHECKF(hr == S_OK, "Speak(skipme) async (%#lx)", hr);
    WaitForSingleObject(g_started, 5000);
    CHECKF(g_skip_info_idle == 0, "GetSkipInfo with no request reports 0 items (%ld)", g_skip_info_idle);
    skipped = 99;
    hr = ISpVoice_Skip(v, L"Sentence", 3, &skipped);
    CHECKF(hr == S_OK && skipped == 3, "Skip(3 sentences) returns what the engine completed (%#lx, %lu)", hr, skipped);
    CHECKF(g_skip_count_seen == 3 && g_skip_type_seen == 1, "the engine saw a request for 3 sentences (%ld, %d)", g_skip_count_seen, g_skip_type_seen);
    ISpVoice_WaitUntilDone(v, 5000);
    CloseHandle(g_started); g_started = NULL;
    ISpVoice_Release(v);
    IStream_Release(mem);
}

static void write_file(const WCHAR *name, const void *data, DWORD size)
{
    HANDLE h = CreateFileW(name, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD w;
    WriteFile(h, data, size, &w, NULL);
    CloseHandle(h);
}

static void test_speak_stream(void)
{
    ISpVoice *v;
    IStream *mem, *src, *dest;
    ISpStream *wavsp;
    LARGE_INTEGER zero = { 0 };
    ULARGE_INTEGER pos;
    ULONG n, stream = 0;
    HRESULT hr;
    WCHAR path[MAX_PATH];
    unsigned char data[300];
    unsigned char *out;
    HGLOBAL hg;
    int i;
    static const WCHAR utf16[] = { 0xfeff, 'u', 'n', 'i', 0 };

    v = new_voice(&mem, NULL);

    /* ANSI text */
    CreateStreamOnHGlobal(NULL, TRUE, &src);
    IStream_Write(src, "hello ansi", 10, &n);
    IStream_Seek(src, zero, STREAM_SEEK_SET, &pos);
    g_spoken_n = 0;
    hr = ISpVoice_SpeakStream(v, src, 0, &stream);
    CHECKF(hr == S_OK && g_spoken_n == 1 && !wcscmp(g_spoken[0], L"hello ansi"), "SpeakStream of an ANSI text stream (%#lx)", hr);
    IStream_Release(src);

    /* UTF-16 with a byte order mark */
    CreateStreamOnHGlobal(NULL, TRUE, &src);
    IStream_Write(src, utf16, sizeof(utf16) - sizeof(WCHAR), &n);
    IStream_Seek(src, zero, STREAM_SEEK_SET, &pos);
    g_spoken_n = 0;
    hr = ISpVoice_SpeakStream(v, src, 0, &stream);
    CHECKF(hr == S_OK && g_spoken_n == 1 && !wcscmp(g_spoken[0], L"uni"), "SpeakStream of a UTF-16 stream with a BOM (%#lx, %ls)", hr, g_spoken[0]);
    IStream_Release(src);

    /* UTF-8 with a byte order mark */
    CreateStreamOnHGlobal(NULL, TRUE, &src);
    IStream_Write(src, "\xef\xbb\xbf" "caf" "\xc3\xa9", 8, &n);
    IStream_Seek(src, zero, STREAM_SEEK_SET, &pos);
    g_spoken_n = 0;
    hr = ISpVoice_SpeakStream(v, src, 0, &stream);
    CHECKF(hr == S_OK && g_spoken_n == 1 && !wcscmp(g_spoken[0], L"caf\u00e9"), "SpeakStream of a UTF-8 stream with a BOM (%#lx)", hr);
    IStream_Release(src);

    hr = ISpVoice_SpeakStream(v, NULL, 0, &stream);
    CHECKF(hr == E_POINTER, "SpeakStream(NULL) is E_POINTER (%#lx)", hr);

    /* A wave stream goes to the output as audio, without the engine */
    for (i = 0; i < (int)sizeof(data); i++) data[i] = (unsigned char)(255 - i);
    CreateStreamOnHGlobal(NULL, TRUE, &src);
    IStream_Write(src, data, sizeof(data), &n);
    IStream_Seek(src, zero, STREAM_SEEK_SET, &pos);
    CoCreateInstance(&CLSID_SpStream, NULL, CLSCTX_INPROC_SERVER, &IID_ISpStream, (void **)&wavsp);
    ISpStream_SetBaseStream(wavsp, src, &PR_SPDFID_WaveFormatEx, &out_wfx);
    g_spoken_n = 0;
    {
        LARGE_INTEGER back = { 0 };
        STATSTG st;
        IStream_Seek(mem, back, STREAM_SEEK_SET, NULL);
        IStream_Stat(mem, &st, STATFLAG_NONAME);
        (void)st;
    }
    {
        ULARGE_INTEGER sz = { 0 };
        IStream_SetSize(mem, sz);
        IStream_Seek(mem, zero, STREAM_SEEK_SET, NULL);
    }
    hr = ISpVoice_SpeakStream(v, (IStream *)wavsp, 0, &stream);
    CHECKF(hr == S_OK && stream != 0 && g_spoken_n == 0, "SpeakStream of a wave stream (%#lx, engine calls %d)", hr, g_spoken_n);
    CHECKF(mem_size(mem) == sizeof(data), "the output holds exactly the audio (%lu bytes)", mem_size(mem));
    GetHGlobalFromStream(mem, &hg);
    out = GlobalLock(hg);
    CHECKF(out && !memcmp(out, data, sizeof(data)), "the audio bytes are the ones in the stream");
    GlobalUnlock(hg);
    ISpStream_Release(wavsp);
    IStream_Release(src);

    /* Speak with SPF_IS_FILENAME */
    GetTempPathW(MAX_PATH, path);
    wcscat(path, L"sgvoicetext.txt");
    write_file(path, "from file", 9);
    g_spoken_n = 0;
    hr = ISpVoice_Speak(v, path, SPF_IS_FILENAME, &stream);
    CHECKF(hr == S_OK && g_spoken_n == 1 && !wcscmp(g_spoken[0], L"from file"), "Speak(SPF_IS_FILENAME) of a text file (%#lx)", hr);
    DeleteFileW(path);
    hr = ISpVoice_Speak(v, path, SPF_IS_FILENAME, &stream);
    CHECKF(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) || hr == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND), "Speak(SPF_IS_FILENAME) of a missing file (%#lx)", hr);

    /* a wave file: header, then the data */
    {
        ISpStream *file;
        ULONG w;
        ULARGE_INTEGER sz = { 0 };
        IStream_SetSize(mem, sz);
        IStream_Seek(mem, zero, STREAM_SEEK_SET, NULL);
        wcscpy(path + wcslen(path) - 3, L"wav");
        CoCreateInstance(&CLSID_SpStream, NULL, CLSCTX_INPROC_SERVER, &IID_ISpStream, (void **)&file);
        ISpStream_BindToFile(file, path, SPFM_CREATE_ALWAYS, &PR_SPDFID_WaveFormatEx, &out_wfx, 0);
        ISpStream_Write(file, data, 100, &w);
        ISpStream_Close(file);
        ISpStream_Release(file);
        hr = ISpVoice_Speak(v, path, SPF_IS_FILENAME, &stream);
        CHECKF(hr == S_OK && mem_size(mem) == 100, "Speak(SPF_IS_FILENAME) of a wave file plays its 100 data bytes (%#lx, %lu)", hr, mem_size(mem));
        DeleteFileW(path);
    }
    (void)dest;
    ISpVoice_Release(v);
    IStream_Release(mem);
}

/* ---- automation ------------------------------------------------------------ */
static void test_automation(void)
{
    IDispatch *voice, *status, *fs, *obj;
    VARIANT r, a[4];
    HRESULT hr;
    LONG l;
    ITypeInfo *ti;
    UINT cnt;
    WCHAR path[MAX_PATH];
    ISpVoice *spv;
    ISpObjectToken *t;
    IStream *mem;
    ISpStream *sp;

    hr = CoCreateInstance(&CLSID_SpVoice, NULL, CLSCTX_INPROC_SERVER, &IID_IDispatch, (void **)&voice);
    CHECKF(hr == S_OK, "SpVoice as IDispatch");
    if (hr != S_OK) return;
    IDispatch_QueryInterface(voice, &IID_ISpVoice, (void **)&spv);
    t = token_for(L"Probe");
    ISpVoice_SetVoice(spv, t);
    ISpObjectToken_Release(t);
    CreateStreamOnHGlobal(NULL, TRUE, &mem);
    CoCreateInstance(&CLSID_SpStream, NULL, CLSCTX_INPROC_SERVER, &IID_ISpStream, (void **)&sp);
    ISpStream_SetBaseStream(sp, mem, &PR_SPDFID_WaveFormatEx, &out_wfx);
    ISpVoice_SetOutput(spv, (IUnknown *)sp, TRUE);

    cnt = 0;
    hr = IDispatch_GetTypeInfoCount(voice, &cnt);
    CHECKF(hr == S_OK && cnt == 1, "voice GetTypeInfoCount");
    hr = IDispatch_GetTypeInfo(voice, 0, LOCALE_USER_DEFAULT, &ti);
    if (hr == S_OK) ITypeInfo_Release(ti);

    /* properties */
    CHECKF(dlong(voice, L"Priority", &hr) == 0 && hr == S_OK, "Voice.Priority starts as Normal");
    hr = dput(voice, L"Priority", vi4(2));
    CHECKF(hr == S_OK && dlong(voice, L"Priority", NULL) == 2, "Voice.Priority = SVPOver (%#lx)", hr);
    hr = dput(voice, L"Priority", vi4(7));
    CHECKF(hr == E_INVALIDARG, "Voice.Priority = 7 is E_INVALIDARG (%#lx)", hr);
    CHECKF(dlong(voice, L"AlertBoundary", &hr) == 32 && hr == S_OK, "Voice.AlertBoundary starts as SVEWordBoundary (32)");
    hr = dput(voice, L"AlertBoundary", vi4(128));
    CHECKF(hr == S_OK && dlong(voice, L"AlertBoundary", NULL) == 128, "Voice.AlertBoundary = SVESentenceBoundary (%#lx)", hr);
    hr = dput(voice, L"AlertBoundary", vi4(0));
    CHECKF(hr == E_INVALIDARG, "Voice.AlertBoundary = 0 is E_INVALIDARG (%#lx)", hr);
    hr = dput(voice, L"AlertBoundary", vi4(3));
    CHECKF(hr == E_INVALIDARG, "Voice.AlertBoundary = 3 (two events) is E_INVALIDARG (%#lx)", hr);
    CHECKF(dlong(voice, L"EventInterests", &hr) == 0x83fe && hr == S_OK, "Voice.EventInterests starts as SVEAllEvents (%#lx)", (unsigned long)dlong(voice, L"EventInterests", NULL));
    hr = dput(voice, L"EventInterests", vi4(0x24));
    CHECKF(hr == S_OK && dlong(voice, L"EventInterests", NULL) == 0x24, "Voice.EventInterests = 0x24 reads back (%#lx)", hr);
    {
        SPEVENTSOURCEINFO info;
        ISpVoice_GetInfo(spv, &info);
        CHECKF(info.ullEventInterest == (0x24 | SPFEI_FLAGCHECK) && info.ullQueuedInterest == info.ullEventInterest, "ISpVoice sees the interest set through the property");
    }
    hr = dput(voice, L"EventInterests", vi4(0x83fe));
    hr = dput(voice, L"SynchronousSpeakTimeout", vi4(4321));
    CHECKF(hr == S_OK && dlong(voice, L"SynchronousSpeakTimeout", NULL) == 4321, "Voice.SynchronousSpeakTimeout (%#lx)", hr);
    hr = dprop(voice, L"AllowAudioOutputFormatChangesOnNextSet", &r);
    CHECKF(hr == S_OK && V_VT(&r) == VT_BOOL && V_BOOL(&r) == VARIANT_TRUE, "Voice.AllowAudioOutputFormatChangesOnNextSet starts True (%#lx)", hr);
    hr = dput(voice, L"AllowAudioOutputFormatChangesOnNextSet", vbool(FALSE));
    hr = dprop(voice, L"AllowAudioOutputFormatChangesOnNextSet", &r);
    CHECKF(hr == S_OK && V_BOOL(&r) == VARIANT_FALSE, "Voice.AllowAudioOutputFormatChangesOnNextSet = False");
    dput(voice, L"AllowAudioOutputFormatChangesOnNextSet", vbool(TRUE));
    hr = dput(voice, L"Rate", vi4(-4));
    CHECKF(hr == S_OK && dlong(voice, L"Rate", NULL) == -4, "Voice.Rate (%#lx)", hr);
    dput(voice, L"Rate", vi4(0));

    /* AudioOutput: Nothing for a stream; AudioOutputStream wraps the stream */
    hr = dprop(voice, L"AudioOutput", &r);
    CHECKF(hr == S_OK && V_VT(&r) == VT_DISPATCH && !V_DISPATCH(&r), "Voice.AudioOutput is Nothing for a stream output (%#lx)", hr);
    hr = dprop(voice, L"AudioOutputStream", &r);
    CHECKF(hr == S_OK && V_VT(&r) == VT_DISPATCH && V_DISPATCH(&r), "Voice.AudioOutputStream (%#lx)", hr);
    if (hr == S_OK && V_VT(&r) == VT_DISPATCH && V_DISPATCH(&r))
    {
        obj = V_DISPATCH(&r);
        hr = dprop(obj, L"Format", &r);
        CHECKF(hr == S_OK && V_VT(&r) == VT_DISPATCH && V_DISPATCH(&r), "AudioOutputStream.Format (%#lx)", hr);
        if (hr == S_OK && V_DISPATCH(&r))
        {
            CHECKF(dlong(V_DISPATCH(&r), L"Type", NULL) == 22, "AudioOutputStream.Format.Type is 22 kHz 16 bit mono");
            IDispatch_Release(V_DISPATCH(&r));
        }
        IDispatch_Release(obj);
    }

    /* Status */
    hr = dprop(voice, L"Status", &r);
    status = (hr == S_OK && V_VT(&r) == VT_DISPATCH) ? V_DISPATCH(&r) : NULL;
    CHECKF(status != NULL, "Voice.Status (%#lx)", hr);
    if (status)
    {
        CHECKF(dlong(status, L"CurrentStreamNumber", NULL) == 0 && dlong(status, L"LastStreamNumberQueued", NULL) == 0 &&
               dlong(status, L"RunningState", NULL) == 1 && dlong(status, L"LastHResult", NULL) == 0, "idle status: stream 0, done, S_OK");
        hr = dprop(status, L"LastBookmark", &r);
        CHECKF(hr == S_OK && V_VT(&r) == VT_BSTR && !*V_BSTR(&r), "idle status: LastBookmark is empty");
        VariantClear(&r);
        IDispatch_Release(status);
    }

    /* Speak, then Status is a snapshot of the result */
    g_spoken_n = 0;
    a[0] = vstr(L"auto @"); a[1] = vi4(0);
    hr = dcall(voice, L"Speak", &r, a, 2);
    CHECKF(hr == S_OK && V_VT(&r) == VT_I4 && V_I4(&r) == 1, "Voice.Speak returns stream number 1 (%#lx)", hr);
    SysFreeString(V_BSTR(&a[0]));
    CHECKF(g_spoken_n == 1 && !wcscmp(g_spoken[0], L"auto @"), "the engine got the text from automation");
    hr = dprop(voice, L"Status", &r);
    status = (hr == S_OK && V_VT(&r) == VT_DISPATCH) ? V_DISPATCH(&r) : NULL;
    if (status)
    {
        CHECKF(dlong(status, L"CurrentStreamNumber", NULL) == 1 && dlong(status, L"LastStreamNumberQueued", NULL) == 1,
               "status after Speak: stream 1");
        CHECKF(dlong(status, L"InputWordPosition", NULL) == 5 && dlong(status, L"InputWordLength", NULL) == 1, "status: last word at 5, length 1");
        CHECKF(dlong(status, L"InputSentencePosition", NULL) == 0 && dlong(status, L"InputSentenceLength", NULL) == 6, "status: sentence 0, length 6");
        CHECKF(dlong(status, L"LastBookmarkId", NULL) == 7, "status: LastBookmarkId 7");
        hr = dprop(status, L"LastBookmark", &r);
        CHECKF(hr == S_OK && V_VT(&r) == VT_BSTR && !wcscmp(V_BSTR(&r), L"mark"), "status: LastBookmark is mark");
        VariantClear(&r);
        CHECKF(dlong(status, L"PhonemeId", NULL) == 0 && dlong(status, L"VisemeId", NULL) == 0, "status: no phoneme or viseme");
        IDispatch_Release(status);
    }

    /* WaitUntilDone, SpeakCompleteEvent, Pause, Resume, Skip, UI */
    hr = dcall(voice, L"WaitUntilDone", &r, (a[0] = vi4(1000), a), 1);
    CHECKF(hr == S_OK && V_VT(&r) == VT_BOOL && V_BOOL(&r) == VARIANT_TRUE, "Voice.WaitUntilDone is True when idle (%#lx)", hr);
    hr = dcall(voice, L"WaitUntilDone", &r, (a[0] = vi4(-1), a), 1);
    CHECKF(hr == S_OK && V_BOOL(&r) == VARIANT_TRUE, "Voice.WaitUntilDone(-1) is True when idle (%#lx)", hr);
    hr = dprop(voice, L"SpeakCompleteEvent", &r);
    hr = dcall(voice, L"SpeakCompleteEvent", &r, NULL, 0);
    CHECKF(hr == S_OK && V_I4(&r) != 0, "Voice.SpeakCompleteEvent returns a handle (%#lx)", hr);
    hr = dcall(voice, L"Pause", &r, NULL, 0);
    CHECKF(hr == S_OK, "Voice.Pause (%#lx)", hr);
    hr = dcall(voice, L"Resume", &r, NULL, 0);
    CHECKF(hr == S_OK, "Voice.Resume (%#lx)", hr);
    a[0] = vstr(L"Sentence"); a[1] = vi4(1);
    hr = dcall(voice, L"Skip", &r, a, 2);
    CHECKF(hr == S_OK && V_VT(&r) == VT_I4 && V_I4(&r) == 0, "Voice.Skip when idle skips 0 (%#lx)", hr);
    SysFreeString(V_BSTR(&a[0]));
    a[0] = vstr(L"Word"); a[1] = vi4(1);
    hr = dcall(voice, L"Skip", &r, a, 2);
    CHECKF(hr == E_INVALIDARG, "Voice.Skip of an unknown type is E_INVALIDARG (%#lx)", hr);
    SysFreeString(V_BSTR(&a[0]));
    hr = dcall(voice, L"IsUISupported", &r, (a[0] = vstr(L"AddRemoveWord"), a), 1);
    CHECKF(hr == S_OK && V_VT(&r) == VT_BOOL && V_BOOL(&r) == VARIANT_FALSE, "Voice.IsUISupported is False (%#lx)", hr);
    SysFreeString(V_BSTR(&a[0]));
    a[0] = vi4(0); a[1] = vstr(L"t"); a[2] = vstr(L"AddRemoveWord");
    hr = dcall(voice, L"DisplayUI", &r, a, 3);
    CHECKF(hr == SPERR_NOT_FOUND, "Voice.DisplayUI with no UI is SPERR_NOT_FOUND (%#lx)", hr);
    SysFreeString(V_BSTR(&a[1])); SysFreeString(V_BSTR(&a[2]));
    a[0] = vstr(L""); a[1] = vstr(L"");
    hr = dcall(voice, L"GetAudioOutputs", &r, a, 2);
    CHECKF(hr == S_OK && V_VT(&r) == VT_DISPATCH && V_DISPATCH(&r), "Voice.GetAudioOutputs (%#lx)", hr);
    if (hr == S_OK && V_DISPATCH(&r)) IDispatch_Release(V_DISPATCH(&r));
    SysFreeString(V_BSTR(&a[0])); SysFreeString(V_BSTR(&a[1]));

    /* AudioOutputStream = a file stream: the voice writes a wave file */
    GetTempPathW(MAX_PATH, path);
    wcscat(path, L"sgvoiceauto.wav");
    DeleteFileW(path);
    hr = CoCreateInstance(&CLSID_SpFileStream, NULL, CLSCTX_INPROC_SERVER, &IID_IDispatch, (void **)&fs);
    a[0] = vstr(path); a[1] = vi4(3); a[2] = vbool(FALSE);
    hr = dcall(fs, L"Open", &r, a, 3);
    CHECKF(hr == S_OK, "FileStream.Open for writing (%#lx)", hr);
    SysFreeString(V_BSTR(&a[0]));
    hr = dputref(voice, L"AudioOutputStream", fs);
    CHECKF(hr == S_OK, "Voice.AudioOutputStream = file stream (%#lx)", hr);
    a[0] = vstr(L"to file"); a[1] = vi4(0);
    hr = dcall(voice, L"Speak", &r, a, 2);
    CHECKF(hr == S_OK, "Voice.Speak into the file stream (%#lx)", hr);
    SysFreeString(V_BSTR(&a[0]));
    hr = dcall(fs, L"Close", &r, NULL, 0);
    CHECKF(hr == S_OK, "FileStream.Close (%#lx)", hr);
    {
        HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        DWORD size = h == INVALID_HANDLE_VALUE ? 0 : GetFileSize(h, NULL), rd;
        unsigned char buf[64] = { 0 };
        if (h != INVALID_HANDLE_VALUE) { ReadFile(h, buf, sizeof(buf), &rd, NULL); CloseHandle(h); }
        CHECKF(size == 44 + 4410 && !memcmp(buf, "RIFF", 4) && *(DWORD *)(buf + 40) == 4410 && *(DWORD *)(buf + 4) == 36 + 4410,
               "the voice wrote a wave file: 44 byte header and 4410 data bytes, sizes patched (%lu bytes)", size);
    }
    DeleteFileW(path);

    /* SpeakStream with the file stream's reading side */
    hr = dputref(voice, L"AudioOutputStream", NULL);
    CHECKF(hr == S_OK, "Voice.AudioOutputStream = Nothing goes back to the default output (%#lx)", hr);
    IDispatch_Release(fs);

    ISpStream_Release(sp);
    IStream_Release(mem);
    ISpVoice_Release(spv);
    IDispatch_Release(voice);
    (void)l;
}



/* ---- SAPI XML ---------------------------------------------------------------- */
static void test_xml(void)
{
    ISpVoice *v;
    IStream *mem;
    HRESULT hr;
    ULONG stream;
    int calls;

    v = new_voice(&mem, NULL);

    /* nesting, relative and absolute values, entity references */
    g_spoken_n = 0;
    hr = ISpVoice_Speak(v, L"<volume level=\"50\">loud <rate speed=\"3\">fast</rate> <emph>E</emph></volume> tail", SPF_IS_XML, &stream);
    CHECKF(hr == S_OK && g_spoken_n == 1, "Speak with SPF_IS_XML (%#lx)", hr);
    CHECKF(g_nfrags == 5, "five fragments (%d)", g_nfrags);
    if (g_nfrags == 5)
    {
        CHECKF(!wcscmp(g_frags[0].text, L"loud ") && g_frags[0].vol == 50 && g_frags[0].rate == 0 && g_frags[0].action == 0 && g_frags[0].off == 19,
               "fragment 0: \"loud \" at volume 50, spoken, source offset 19 (%ls, %lu, %lu)", g_frags[0].text, g_frags[0].vol, g_frags[0].off);
        CHECKF(!wcscmp(g_frags[1].text, L"fast") && g_frags[1].vol == 50 && g_frags[1].rate == 3, "fragment 1: \"fast\" at rate +3, still volume 50");
        CHECKF(!wcscmp(g_frags[2].text, L" ") && g_frags[2].rate == 0 && g_frags[2].vol == 50, "fragment 2: the space after the rate element is back to rate 0");
        CHECKF(!wcscmp(g_frags[3].text, L"E") && g_frags[3].emph == 1 && g_frags[3].vol == 50, "fragment 3: emphasised E");
        CHECKF(!wcscmp(g_frags[4].text, L" tail") && g_frags[4].vol == 100 && g_frags[4].emph == 0, "fragment 4: after the volume element: volume 100 again, no emphasis");
    }

    g_spoken_n = 0;
    ISpVoice_Speak(v, L"a &amp; b &lt;c&gt; &#65;&#x42; &quot;q&apos;", SPF_IS_XML, &stream);
    CHECKF(g_spoken_n == 1 && !wcscmp(g_spoken[0], L"a & b <c> AB \"q'"), "character references are decoded (%ls)", g_spoken[0]);

    ISpVoice_Speak(v, L"x<silence msec=\"250\"/>y", SPF_IS_XML, &stream);
    CHECKF(g_nfrags == 3 && g_frags[1].action == SPVA_Silence && g_frags[1].silence == 250 && !wcscmp(g_frags[0].text, L"x") && !wcscmp(g_frags[2].text, L"y"),
           "silence: a fragment of 250 ms between x and y (%d fragments)", g_nfrags);
    ISpVoice_Speak(v, L"<bookmark mark=\"m1\"/>after", SPF_IS_XML, &stream);
    CHECKF(g_nfrags == 2 && g_frags[0].action == SPVA_Bookmark && !wcscmp(g_frags[0].text, L"m1") && !wcscmp(g_frags[1].text, L"after"),
           "bookmark: a fragment whose text is the mark");
    ISpVoice_Speak(v, L"<spell>abc</spell>d", SPF_IS_XML, &stream);
    CHECKF(g_nfrags == 2 && g_frags[0].action == SPVA_SpellOut && g_frags[1].action == SPVA_Speak, "spell: spell out for the content only");
    ISpVoice_Speak(v, L"<pron sym=\"h eh l ow\">hello</pron>", SPF_IS_XML, &stream);
    CHECKF(g_nfrags == 1 && g_frags[0].action == SPVA_Pronounce && !wcscmp(g_frags[0].phones, L"h eh l ow") && !wcscmp(g_frags[0].text, L"hello"),
           "pron: the phones go with the text");
    ISpVoice_Speak(v, L"<pitch middle=\"2\"><pitch middle=\"3\" range=\"-1\">p</pitch>q</pitch><pitch absmiddle=\"-4\">r</pitch>", SPF_IS_XML, &stream);
    CHECKF(g_nfrags == 3 && g_frags[0].pitch_mid == 5 && g_frags[0].pitch_range == -1 && g_frags[1].pitch_mid == 2 && g_frags[1].pitch_range == 0 && g_frags[2].pitch_mid == -4,
           "pitch: relative values add up, absolute ones replace (%ld %ld %ld)", g_frags[0].pitch_mid, g_frags[1].pitch_mid, g_frags[2].pitch_mid);
    ISpVoice_Speak(v, L"<lang langid=\"409\"><context id=\"date_mdy\"><partofsp part=\"verb\">go</partofsp></context></lang>", SPF_IS_XML, &stream);
    CHECKF(g_nfrags == 1 && g_frags[0].lang == 0x409 && !wcscmp(g_frags[0].ctx, L"date_mdy") && g_frags[0].part == 0x2000, "lang, context and part of speech");
    ISpVoice_Speak(v, L"<volume level=\"20\"/>quiet <rate absspeed=\"-2\"/>slow", SPF_IS_XML, &stream);
    CHECKF(g_nfrags == 2 && g_frags[0].vol == 20 && g_frags[1].vol == 20 && g_frags[1].rate == -2 && g_frags[0].rate == 0,
           "self closing tags change the state for what follows");
    ISpVoice_Speak(v, L"<VOLUME LEVEL='30'>CaSe</VOLUME>", SPF_IS_XML, &stream);
    CHECKF(g_nfrags == 1 && g_frags[0].vol == 30, "tag and attribute names are case insensitive, single quotes work");
    ISpVoice_Speak(v, L"<volume level=\"500\">big</volume><volume level=\"-5\">neg</volume>", SPF_IS_XML, &stream);
    CHECKF(g_nfrags == 2 && g_frags[0].vol == 100 && g_frags[1].vol == 0, "the volume is kept within 0 to 100");

    /* tags that are not SAPI's go to the engine as they are */
    ISpVoice_Speak(v, L"<foo a=\"1\">z</foo>", SPF_IS_XML, &stream);
    CHECKF(g_nfrags == 2 && g_frags[0].action == SPVA_ParseUnknownTag && !wcscmp(g_frags[0].text, L"<foo a=\"1\">") && !wcscmp(g_frags[1].text, L"z"),
           "an unknown tag is a fragment for the engine");

    /* markup with nothing to say still reaches the engine */
    g_spoken_n = 0;
    hr = ISpVoice_Speak(v, L"<volume level=\"5\"/>", SPF_IS_XML, &stream);
    CHECKF(hr == S_OK && g_spoken_n == 1 && g_nfrags == 1 && !g_frags[0].text[0], "markup without text gives one empty fragment (%#lx)", hr);

    /* malformed markup is refused before the engine is called */
    {
        static const WCHAR *bad[] = { L"<volume level=\"5\">x", L"x</volume>", L"<a b=1>x</a>", L"<volume", L"<volume level=\"5>x</volume>",
                                      L"<emph>x</spell>", L"< >", L"<volume level>x</volume>" };
        int i;
        for (i = 0; i < (int)ARRAY_SIZE(bad); i++)
        {
            g_spoken_n = 0;
            hr = ISpVoice_Speak(v, bad[i], SPF_IS_XML, &stream);
            CHECKF(hr == (HRESULT)0x80045042 && g_spoken_n == 0, "malformed markup \"%ls\" is SPERR_XML_BAD_SYNTAX (%#lx)", bad[i], hr);
        }
    }

    /* flags */
    g_spoken_n = 0;
    hr = ISpVoice_Speak(v, L"<volume level=\"7\">f</volume>", SPF_IS_XML | SPF_PERSIST_XML, &stream);
    CHECKF(hr == S_OK && g_nfrags == 1 && g_frags[0].vol == 7, "SPF_PERSIST_XML is accepted (%#lx)", hr);
    hr = ISpVoice_Speak(v, L"<volume level=\"8\">f</volume>", SPF_PARSE_SAPI, &stream);
    CHECKF(hr == S_OK && g_nfrags == 1 && g_frags[0].vol == 8, "SPF_PARSE_SAPI parses the markup (%#lx)", hr);
    hr = ISpVoice_Speak(v, L"<speak>x</speak>", SPF_IS_XML | SPF_PARSE_SSML, &stream);
    CHECKF(hr == E_NOTIMPL, "SSML is not parsed (%#lx)", hr);
    g_spoken_n = 0;
    hr = ISpVoice_Speak(v, L"<volume level=\"9\">plain</volume>", 0, &stream);
    calls = g_spoken_n;
    CHECKF(hr == S_OK && calls == 1 && g_nfrags == 1 && !wcscmp(g_frags[0].text, L"<volume level=\"9\">plain</volume>") && g_frags[0].vol == 100,
           "without the XML flags the tags are spoken as text (%#lx)", hr);

    /* through a stream */
    {
        IStream *src;
        LARGE_INTEGER zero = { 0 };
        ULONG n;
        CreateStreamOnHGlobal(NULL, TRUE, &src);
        IStream_Write(src, "<volume level=\"15\">s</volume>", 29, &n);
        IStream_Seek(src, zero, STREAM_SEEK_SET, NULL);
        hr = ISpVoice_SpeakStream(v, src, SPF_IS_XML, &stream);
        CHECKF(hr == S_OK && g_nfrags == 1 && g_frags[0].vol == 15, "SpeakStream passes the markup flags on (%#lx)", hr);
        IStream_Release(src);
    }
    ISpVoice_Release(v);
    IStream_Release(mem);
}

/* ---- connection points: _ISpeechVoiceEvents ------------------------------ */
DEFINE_GUID(PR_DIID_ISpeechVoiceEvents, 0xa372acd1, 0x3bef, 0x4bbd, 0x8f, 0xfb, 0xcb, 0x3e, 0x2b, 0x41, 0x6a, 0xf8);

struct call { DISPID id; int argc; LONG num[6]; WCHAR str[16]; DWORD tid; };
static struct call g_calls[64];
static int g_ncalls;

typedef struct { const IDispatchVtbl *lpVtbl; LONG ref; BOOL dispatch_only; } EvSink;
static HRESULT STDMETHODCALLTYPE evs_QI(IDispatch *This, REFIID iid, void **obj)
{
    EvSink *s = (EvSink *)This;
    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_IDispatch) || (!s->dispatch_only && IsEqualIID(iid, &PR_DIID_ISpeechVoiceEvents)))
    { *obj = This; InterlockedIncrement(&s->ref); return S_OK; }
    *obj = NULL;
    return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE evs_AddRef(IDispatch *This) { return InterlockedIncrement(&((EvSink *)This)->ref); }
static ULONG STDMETHODCALLTYPE evs_Release(IDispatch *This) { return InterlockedDecrement(&((EvSink *)This)->ref); }
static HRESULT STDMETHODCALLTYPE evs_Count(IDispatch *This, UINT *c) { *c = 0; return S_OK; }
static HRESULT STDMETHODCALLTYPE evs_Info(IDispatch *This, UINT i, LCID l, ITypeInfo **t) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE evs_Names(IDispatch *This, REFIID r, LPOLESTR *n, UINT c, LCID l, DISPID *d) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE evs_Invoke(IDispatch *This, DISPID id, REFIID riid, LCID lcid, WORD flags, DISPPARAMS *dp, VARIANT *res, EXCEPINFO *ex, UINT *err)
{
    struct call *c;
    UINT i;
    if (g_ncalls >= 64) return S_OK;
    c = &g_calls[g_ncalls++];
    memset(c, 0, sizeof(*c));
    c->id = id;
    c->argc = dp->cArgs;
    c->tid = GetCurrentThreadId();
    /* rgvarg has the last argument first; store them in the order of the method */
    for (i = 0; i < dp->cArgs && i < 6; i++)
    {
        VARIANT *v = &dp->rgvarg[dp->cArgs - 1 - i];
        VARIANT t;
        VariantInit(&t);
        if (V_VT(v) == VT_BSTR) { wcsncpy(c->str, V_BSTR(v), 15); c->num[i] = -777; }
        else if (SUCCEEDED(VariantChangeType(&t, v, 0, VT_I4))) c->num[i] = V_I4(&t);
    }
    return S_OK;
}
static const IDispatchVtbl evs_vtbl = { evs_QI, evs_AddRef, evs_Release, evs_Count, evs_Info, evs_Names, evs_Invoke };

typedef struct { const IUnknownVtbl *lpVtbl; } PlainUnk;
static HRESULT STDMETHODCALLTYPE pu_QI(IUnknown *This, REFIID iid, void **obj)
{ if (IsEqualIID(iid, &IID_IUnknown)) { *obj = This; return S_OK; } *obj = NULL; return E_NOINTERFACE; }
static ULONG STDMETHODCALLTYPE pu_AddRef(IUnknown *This) { return 2; }
static ULONG STDMETHODCALLTYPE pu_Release(IUnknown *This) { return 1; }
static const IUnknownVtbl pu_vtbl = { pu_QI, pu_AddRef, pu_Release };

static void pump(void)
{
    MSG msg;
    int i;
    for (i = 0; i < 20; i++)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(5);
    }
}

static void test_connection_points(void)
{
    ISpVoice *v;
    IStream *mem;
    IConnectionPointContainer *cpc, *cpc2;
    IEnumConnectionPoints *ecp, *ecp2;
    IEnumConnections *ec, *ec2;
    IConnectionPoint *cp, *cp2;
    EvSink sink = { &evs_vtbl, 1, FALSE }, sink2 = { &evs_vtbl, 1, TRUE };
    PlainUnk plain = { &pu_vtbl };
    CONNECTDATA cd[3];
    DWORD cookie1 = 0, cookie2 = 0;
    ULONG n, stream = 0;
    IID iid;
    IUnknown *u1, *u2;
    HRESULT hr;
    int i;

    v = new_voice(&mem, NULL);
    hr = ISpVoice_QueryInterface(v, &IID_IConnectionPointContainer, (void **)&cpc);
    CHECKF(hr == S_OK, "the voice offers IConnectionPointContainer (%#lx)", hr);
    if (hr != S_OK) return;

    cp = (IConnectionPoint *)1;
    hr = IConnectionPointContainer_FindConnectionPoint(cpc, &IID_Bogus, &cp);
    CHECKF(hr == CONNECT_E_NOCONNECTION && cp == NULL, "FindConnectionPoint of an unknown IID (%#lx)", hr);
    hr = IConnectionPointContainer_FindConnectionPoint(cpc, &PR_DIID_ISpeechVoiceEvents, NULL);
    CHECKF(hr == E_POINTER, "FindConnectionPoint(NULL) is E_POINTER (%#lx)", hr);
    hr = IConnectionPointContainer_FindConnectionPoint(cpc, &PR_DIID_ISpeechVoiceEvents, &cp);
    CHECKF(hr == S_OK && cp, "FindConnectionPoint(_ISpeechVoiceEvents) (%#lx)", hr);
    hr = IConnectionPoint_GetConnectionInterface(cp, &iid);
    CHECKF(hr == S_OK && IsEqualIID(&iid, &PR_DIID_ISpeechVoiceEvents), "GetConnectionInterface is _ISpeechVoiceEvents");
    hr = IConnectionPoint_GetConnectionPointContainer(cp, &cpc2);
    CHECKF(hr == S_OK, "GetConnectionPointContainer (%#lx)", hr);
    if (hr == S_OK)
    {
        IConnectionPointContainer_QueryInterface(cpc, &IID_IUnknown, (void **)&u1);
        IConnectionPointContainer_QueryInterface(cpc2, &IID_IUnknown, (void **)&u2);
        CHECKF(u1 == u2, "the container of the connection point is the voice");
        IUnknown_Release(u1); IUnknown_Release(u2); IConnectionPointContainer_Release(cpc2);
    }

    /* EnumConnectionPoints */
    hr = IConnectionPointContainer_EnumConnectionPoints(cpc, &ecp);
    CHECKF(hr == S_OK && ecp, "EnumConnectionPoints (%#lx)", hr);
    if (hr == S_OK)
    {
        cp2 = NULL; n = 0;
        hr = IEnumConnectionPoints_Next(ecp, 1, &cp2, &n);
        CHECKF(hr == S_OK && n == 1 && cp2, "EnumConnectionPoints: the first Next gives the connection point");
        if (cp2)
        {
            hr = IConnectionPoint_GetConnectionInterface(cp2, &iid);
            CHECKF(IsEqualIID(&iid, &PR_DIID_ISpeechVoiceEvents), "the enumerated connection point is _ISpeechVoiceEvents");
            IConnectionPoint_Release(cp2);
        }
        hr = IEnumConnectionPoints_Next(ecp, 1, &cp2, &n);
        CHECKF(hr == S_FALSE && n == 0, "EnumConnectionPoints: the second Next is S_FALSE");
        hr = IEnumConnectionPoints_Reset(ecp);
        hr = IEnumConnectionPoints_Skip(ecp, 1);
        CHECKF(hr == S_OK, "EnumConnectionPoints: Skip(1) is S_OK (%#lx)", hr);
        hr = IEnumConnectionPoints_Skip(ecp, 1);
        CHECKF(hr == S_FALSE, "EnumConnectionPoints: Skip past the end is S_FALSE (%#lx)", hr);
        IEnumConnectionPoints_Reset(ecp);
        hr = IEnumConnectionPoints_Clone(ecp, &ecp2);
        CHECKF(hr == S_OK && ecp2, "EnumConnectionPoints: Clone");
        if (ecp2) { cp2 = NULL; IEnumConnectionPoints_Next(ecp2, 1, &cp2, &n); CHECKF(n == 1, "the clone enumerates the connection point"); if (cp2) IConnectionPoint_Release(cp2); IEnumConnectionPoints_Release(ecp2); }
        IEnumConnectionPoints_Release(ecp);
    }

    /* Advise */
    hr = IConnectionPoint_Advise(cp, NULL, &cookie1);
    CHECKF(hr == E_POINTER, "Advise(NULL) is E_POINTER (%#lx)", hr);
    hr = IConnectionPoint_Advise(cp, (IUnknown *)&sink, NULL);
    CHECKF(hr == E_POINTER, "Advise without a cookie is E_POINTER (%#lx)", hr);
    hr = IConnectionPoint_Advise(cp, (IUnknown *)&plain, &cookie1);
    CHECKF(hr == CONNECT_E_CANNOTCONNECT, "Advise of an object without IDispatch is CONNECT_E_CANNOTCONNECT (%#lx)", hr);
    hr = IConnectionPoint_Advise(cp, (IUnknown *)&sink, &cookie1);
    CHECKF(hr == S_OK && cookie1 != 0, "Advise (%#lx, cookie %lu)", hr, cookie1);
    hr = IConnectionPoint_Advise(cp, (IUnknown *)&sink2, &cookie2);
    CHECKF(hr == S_OK && cookie2 != 0 && cookie2 != cookie1, "Advise of an IDispatch-only sink gets another cookie (%#lx)", hr);
    CHECKF(sink.ref == 2 && sink2.ref == 2, "the connection holds a reference on each sink (%ld, %ld)", sink.ref, sink2.ref);

    hr = IConnectionPoint_EnumConnections(cp, &ec);
    CHECKF(hr == S_OK && ec, "EnumConnections (%#lx)", hr);
    if (hr == S_OK)
    {
        memset(cd, 0, sizeof(cd));
        hr = IEnumConnections_Next(ec, 3, cd, &n);
        CHECKF(hr == S_FALSE && n == 2 && cd[0].dwCookie == cookie1 && cd[1].dwCookie == cookie2 && cd[0].pUnk == (IUnknown *)&sink,
               "EnumConnections lists both sinks with their cookies (%#lx, %lu)", hr, n);
        for (i = 0; i < (int)n; i++) IUnknown_Release(cd[i].pUnk);
        IEnumConnections_Reset(ec);
        hr = IEnumConnections_Skip(ec, 1);
        CHECKF(hr == S_OK, "EnumConnections: Skip(1) (%#lx)", hr);
        hr = IEnumConnections_Clone(ec, &ec2);
        CHECKF(hr == S_OK, "EnumConnections: Clone");
        if (hr == S_OK)
        {
            hr = IEnumConnections_Next(ec2, 1, cd, &n);
            CHECKF(hr == S_OK && n == 1 && cd[0].dwCookie == cookie2, "the clone continues at the second connection");
            if (n) IUnknown_Release(cd[0].pUnk);
            IEnumConnections_Release(ec2);
        }
        hr = IEnumConnections_Skip(ec, 5);
        CHECKF(hr == S_FALSE, "EnumConnections: Skip past the end is S_FALSE");
        IEnumConnections_Release(ec);
    }

    hr = IConnectionPoint_Unadvise(cp, cookie2 + 100);
    CHECKF(hr == CONNECT_E_NOCONNECTION, "Unadvise of an unknown cookie is CONNECT_E_NOCONNECTION (%#lx)", hr);
    hr = IConnectionPoint_Unadvise(cp, cookie2);
    CHECKF(hr == S_OK && sink2.ref == 1, "Unadvise (%#lx)", hr);
    hr = IConnectionPoint_Unadvise(cp, cookie2);
    CHECKF(hr == CONNECT_E_NOCONNECTION, "Unadvise twice (%#lx)", hr);

    /* events arrive on this thread, through its message queue */
    g_ncalls = 0;
    ISpVoice_Speak(v, L"ab cd @", 0, &stream);
    CHECKF(g_ncalls == 0, "nothing is delivered before the thread runs its message loop (%d calls)", g_ncalls);
    pump();
    CHECKF(g_ncalls == 7, "after the message loop: seven events delivered (%d)", g_ncalls);
    if (g_ncalls == 7)
    {
        CHECKF(g_calls[0].id == 1 && g_calls[0].argc == 2 && g_calls[0].num[0] == (LONG)stream, "StreamStart(stream %lu, position)", stream);
        CHECKF(g_calls[1].id == 5 && g_calls[1].argc == 4 && g_calls[1].num[2] == 0 && g_calls[1].num[3] == 2, "Word(stream, position, 0, 2)");
        CHECKF(g_calls[2].id == 5 && g_calls[2].num[2] == 3 && g_calls[2].num[3] == 2, "Word(stream, position, 3, 2)");
        CHECKF(g_calls[3].id == 5 && g_calls[3].num[2] == 6 && g_calls[3].num[3] == 1, "Word(stream, position, 6, 1)");
        CHECKF(g_calls[4].id == 7 && g_calls[4].argc == 4 && g_calls[4].num[2] == 0 && g_calls[4].num[3] == 7, "SentenceBoundary(stream, position, 0, 7)");
        CHECKF(g_calls[5].id == 4 && g_calls[5].argc == 4 && !wcscmp(g_calls[5].str, L"mark") && g_calls[5].num[3] == 7, "Bookmark(stream, position, \"mark\", 7)");
        CHECKF(g_calls[6].id == 2 && g_calls[6].argc == 2 && g_calls[6].num[0] == (LONG)stream, "StreamEnd(stream, position)");
        for (i = 0; i < 7; i++)
            if (g_calls[i].tid != GetCurrentThreadId()) break;
        CHECKF(i == 7, "every event was delivered on the thread that created the voice");
    }

    /* the interest limits what is delivered */
    hr = ISpVoice_SetInterest(v, SPFEI(SPEI_END_INPUT_STREAM), SPFEI(SPEI_END_INPUT_STREAM));
    g_ncalls = 0;
    ISpVoice_Speak(v, L"x", 0, &stream);
    pump();
    CHECKF(g_ncalls == 1 && g_calls[0].id == 2, "with only the end of the stream of interest, only StreamEnd is delivered (%d)", g_ncalls);

    hr = IConnectionPoint_Unadvise(cp, cookie1);
    CHECKF(hr == S_OK && sink.ref == 1, "Unadvise of the last sink (%#lx)", hr);
    g_ncalls = 0;
    ISpVoice_Speak(v, L"y", 0, &stream);
    pump();
    CHECKF(g_ncalls == 0, "after Unadvise nothing is delivered (%d)", g_ncalls);

    /* a voice that goes away with a sink connected and events pending */
    ISpVoice_SetInterest(v, SPFEI_ALL_TTS_EVENTS, SPFEI_ALL_TTS_EVENTS);
    hr = IConnectionPoint_Advise(cp, (IUnknown *)&sink, &cookie1);
    ISpVoice_Speak(v, L"z", 0, &stream);
    IConnectionPoint_Release(cp);
    IConnectionPointContainer_Release(cpc);
    ISpVoice_Release(v);
    IStream_Release(mem);
    pump();
    CHECKF(sink.ref == 1, "the sinks are released with the voice (%ld)", sink.ref);
}

int main(void)
{
    DWORD reg;
    HRESULT hr;

    CoInitialize(NULL);
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\SGProbeVoice");
    reg_set(L"Software\\SGProbeVoice\\Tokens\\Probe", L"CLSID", L"{a1b2c3d4-5555-6666-7777-888888888888}");
    reg_set(L"Software\\SGProbeVoice\\Tokens\\Probe\\Attributes", L"Name", L"Probe");
    hr = CoRegisterClassObject(&PR_CLSID_Engine, (IUnknown *)&engine_cf, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &reg);
    check(hr == S_OK, "register the probe engine");

    test_properties();
    test_events();
    test_notify_kinds();
    test_status_and_output();
    test_skip();
    test_speak_stream();
    test_automation();
    test_xml();
    test_connection_points();

    CoRevokeClassObject(reg);
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\SGProbeVoice");
    CoUninitialize();
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures ? 1 : 0;
}
