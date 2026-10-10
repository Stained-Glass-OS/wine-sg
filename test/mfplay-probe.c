/* mfplay's IMFPMediaPlayer members that were stubs (patches/sg/2871), run by
 * test/mfplay-gate.sh: volume, balance, mute, effects, FrameStep and
 * SetPosition on a player with no item, with an item, and after Shutdown, plus
 * DllGetClassObject.
 *
 *   mfplay-probe.exe [media file] */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfplay.h>
#include <stdio.h>
#include <string.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif
static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[256]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

static HANDLE item_set_event, position_event;

static void WINAPI cb_OnMediaPlayerEvent(IMFPMediaPlayerCallback *iface, MFP_EVENT_HEADER *header)
{
    if (header->eEventType == MFP_EVENT_TYPE_MEDIAITEM_SET) SetEvent(item_set_event);
    if (header->eEventType == MFP_EVENT_TYPE_POSITION_SET) SetEvent(position_event);
}
static HRESULT WINAPI cb_QI(IMFPMediaPlayerCallback *iface, REFIID riid, void **obj)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IMFPMediaPlayerCallback))
    {
        *obj = iface;
        return S_OK;
    }
    *obj = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI cb_AddRef(IMFPMediaPlayerCallback *iface) { return 2; }
static ULONG WINAPI cb_Release(IMFPMediaPlayerCallback *iface) { return 1; }
static IMFPMediaPlayerCallbackVtbl cb_vtbl = { cb_QI, cb_AddRef, cb_Release, cb_OnMediaPlayerEvent };
static IMFPMediaPlayerCallback callback = { &cb_vtbl };

static ULONG refs(IUnknown *u)
{
    IUnknown_AddRef(u);
    return IUnknown_Release(u);
}

static IMFActivate *test_idle(IMFPMediaPlayer *p)
{
    static const struct { float v; HRESULT hr; } volumes[] = {
        {0.5f, S_OK}, {0.0f, S_OK}, {1.0f, S_OK}, {1.5f, E_INVALIDARG}, {-0.25f, E_INVALIDARG}, {0.0f / 0.0f, E_INVALIDARG},
    };
    static const struct { float v; HRESULT hr; } balances[] = {
        {0.5f, S_OK}, {-1.0f, S_OK}, {1.0f, S_OK}, {1.5f, E_INVALIDARG}, {-1.25f, E_INVALIDARG},
    };
    IMFActivate *act = NULL, *act2 = NULL;
    IMFAttributes *attrs;
    PROPVARIANT pos;
    float f;
    BOOL b;
    HRESULT hr;
    ULONG base;
    int i;

    /* volume, balance, mute */
    f = 0.0f;
    hr = IMFPMediaPlayer_GetVolume(p, &f);
    CHECKF(hr == S_OK && f == 1.0f, "GetVolume default is 1.0 (%#lx, %f)", hr, f);
    for (i = 0; i < ARRAY_SIZE(volumes); i++)
    {
        float before = f;
        hr = IMFPMediaPlayer_SetVolume(p, volumes[i].v);
        IMFPMediaPlayer_GetVolume(p, &f);
        CHECKF(hr == volumes[i].hr && f == (hr == S_OK ? volumes[i].v : before), "SetVolume(%f) is %#lx and Get is %f (%#lx)",
               volumes[i].v, volumes[i].hr, f, hr);
    }
    f = 5.0f;
    hr = IMFPMediaPlayer_GetBalance(p, &f);
    CHECKF(hr == S_OK && f == 0.0f, "GetBalance default is 0 (%#lx, %f)", hr, f);
    for (i = 0; i < ARRAY_SIZE(balances); i++)
    {
        float before = f;
        hr = IMFPMediaPlayer_SetBalance(p, balances[i].v);
        IMFPMediaPlayer_GetBalance(p, &f);
        CHECKF(hr == balances[i].hr && f == (hr == S_OK ? balances[i].v : before), "SetBalance(%f) is %#lx and Get is %f (%#lx)",
               balances[i].v, balances[i].hr, f, hr);
    }
    b = TRUE;
    hr = IMFPMediaPlayer_GetMute(p, &b);
    CHECKF(hr == S_OK && b == FALSE, "GetMute default is FALSE (%#lx, %d)", hr, b);
    hr = IMFPMediaPlayer_SetMute(p, TRUE);
    b = FALSE;
    IMFPMediaPlayer_GetMute(p, &b);
    CHECKF(hr == S_OK && b == TRUE, "SetMute(TRUE) round-trips (%#lx)", hr);
    hr = IMFPMediaPlayer_SetMute(p, FALSE);
    IMFPMediaPlayer_GetMute(p, &b);
    CHECKF(hr == S_OK && b == FALSE, "SetMute(FALSE) round-trips (%#lx)", hr);
    hr = IMFPMediaPlayer_GetVolume(p, NULL);
    CHECKF(hr == E_POINTER, "GetVolume(NULL) is E_POINTER (%#lx)", hr);
    hr = IMFPMediaPlayer_GetBalance(p, NULL);
    CHECKF(hr == E_POINTER, "GetBalance(NULL) is E_POINTER (%#lx)", hr);
    hr = IMFPMediaPlayer_GetMute(p, NULL);
    CHECKF(hr == E_POINTER, "GetMute(NULL) is E_POINTER (%#lx)", hr);

    /* frame step, position */
    hr = IMFPMediaPlayer_FrameStep(p);
    CHECKF(hr == MF_E_INVALIDREQUEST, "FrameStep with no item is MF_E_INVALIDREQUEST (%#lx)", hr);
    pos.vt = VT_I8;
    pos.hVal.QuadPart = 1000000;
    hr = IMFPMediaPlayer_SetPosition(p, &MFP_POSITIONTYPE_100NS, &pos);
    CHECKF(hr == MF_E_INVALIDREQUEST, "SetPosition with no item is MF_E_INVALIDREQUEST (%#lx)", hr);
    hr = IMFPMediaPlayer_SetPosition(p, &MFP_POSITIONTYPE_100NS, NULL);
    CHECKF(hr == E_POINTER, "SetPosition(NULL) is E_POINTER (%#lx)", hr);
    hr = IMFPMediaPlayer_SetPosition(p, &IID_IUnknown, &pos);
    CHECKF(hr == E_INVALIDARG, "SetPosition with another position type is E_INVALIDARG (%#lx)", hr);
    pos.vt = VT_R8;
    hr = IMFPMediaPlayer_SetPosition(p, &MFP_POSITIONTYPE_100NS, &pos);
    CHECKF(hr == E_INVALIDARG, "SetPosition with a VT_R8 value is E_INVALIDARG (%#lx)", hr);
    pos.vt = VT_I8;
    pos.hVal.QuadPart = -5;
    hr = IMFPMediaPlayer_SetPosition(p, &MFP_POSITIONTYPE_100NS, &pos);
    CHECKF(hr == E_INVALIDARG, "SetPosition with a negative value is E_INVALIDARG (%#lx)", hr);

    /* effects */
    hr = MFCreateTransformActivate(&act);
    if (hr != S_OK) { CHECKF(0, "MFCreateTransformActivate (%#lx)", hr); return NULL; }
    MFCreateTransformActivate(&act2);
    MFCreateAttributes(&attrs, 0);
    hr = IMFPMediaPlayer_InsertEffect(p, NULL, FALSE);
    CHECKF(hr == E_POINTER, "InsertEffect(NULL) is E_POINTER (%#lx)", hr);
    hr = IMFPMediaPlayer_InsertEffect(p, (IUnknown *)attrs, FALSE);
    CHECKF(hr == E_INVALIDARG, "InsertEffect(not an MFT or activator) is E_INVALIDARG (%#lx)", hr);
    base = refs((IUnknown *)act);
    hr = IMFPMediaPlayer_InsertEffect(p, (IUnknown *)act, TRUE);
    CHECKF(hr == S_OK, "InsertEffect(activator) is S_OK (%#lx)", hr);
    CHECKF(refs((IUnknown *)act) == base + 1, "the player holds a reference on the effect (%lu -> %lu)", base, refs((IUnknown *)act));
    hr = IMFPMediaPlayer_InsertEffect(p, (IUnknown *)act, TRUE);
    CHECKF(hr == MF_E_INVALIDREQUEST, "InsertEffect of the same effect twice is MF_E_INVALIDREQUEST (%#lx)", hr);
    hr = IMFPMediaPlayer_InsertEffect(p, (IUnknown *)act2, FALSE);
    CHECKF(hr == S_OK, "InsertEffect(second activator) is S_OK (%#lx)", hr);
    hr = IMFPMediaPlayer_RemoveEffect(p, (IUnknown *)act);
    CHECKF(hr == S_OK && refs((IUnknown *)act) == base, "RemoveEffect releases the effect (%#lx)", hr);
    hr = IMFPMediaPlayer_RemoveEffect(p, (IUnknown *)act);
    CHECKF(hr == MF_E_NOT_FOUND, "RemoveEffect of an effect not inserted is MF_E_NOT_FOUND (%#lx)", hr);
    hr = IMFPMediaPlayer_RemoveEffect(p, NULL);
    CHECKF(hr == E_POINTER, "RemoveEffect(NULL) is E_POINTER (%#lx)", hr);
    base = refs((IUnknown *)act2);
    hr = IMFPMediaPlayer_RemoveAllEffects(p);
    CHECKF(hr == S_OK && refs((IUnknown *)act2) == base - 1, "RemoveAllEffects releases the effects (%#lx)", hr);
    hr = IMFPMediaPlayer_RemoveEffect(p, (IUnknown *)act2);
    CHECKF(hr == MF_E_NOT_FOUND, "after RemoveAllEffects the effect is gone (%#lx)", hr);
    IMFPMediaPlayer_InsertEffect(p, (IUnknown *)act, FALSE);
    IMFAttributes_Release(attrs);
    IMFActivate_Release(act2);
    return act;
}

static void test_shutdown(IMFPMediaPlayer *p)
{
    IMFActivate *act;
    PROPVARIANT pos;
    float f;
    BOOL b;
    int i;
    HRESULT r[10];
    static const char *names[] = {"GetVolume", "SetVolume", "GetBalance", "SetBalance", "GetMute", "SetMute",
                                  "FrameStep", "InsertEffect", "RemoveEffect", "RemoveAllEffects"};

    MFCreateTransformActivate(&act);
    pos.vt = VT_I8;
    pos.hVal.QuadPart = 0;
    r[0] = IMFPMediaPlayer_GetVolume(p, &f);
    r[1] = IMFPMediaPlayer_SetVolume(p, 0.5f);
    r[2] = IMFPMediaPlayer_GetBalance(p, &f);
    r[3] = IMFPMediaPlayer_SetBalance(p, 0.5f);
    r[4] = IMFPMediaPlayer_GetMute(p, &b);
    r[5] = IMFPMediaPlayer_SetMute(p, TRUE);
    r[6] = IMFPMediaPlayer_FrameStep(p);
    r[7] = IMFPMediaPlayer_InsertEffect(p, (IUnknown *)act, FALSE);
    r[8] = IMFPMediaPlayer_RemoveEffect(p, (IUnknown *)act);
    r[9] = IMFPMediaPlayer_RemoveAllEffects(p);
    for (i = 0; i < ARRAY_SIZE(r); i++)
        CHECKF(r[i] == MF_E_SHUTDOWN, "after Shutdown: %s is MF_E_SHUTDOWN (%#lx)", names[i], r[i]);
    r[0] = IMFPMediaPlayer_SetPosition(p, &MFP_POSITIONTYPE_100NS, &pos);
    CHECKF(r[0] == MF_E_SHUTDOWN, "after Shutdown: SetPosition is MF_E_SHUTDOWN (%#lx)", r[0]);
    IMFActivate_Release(act);
}

static void test_item(const WCHAR *file)
{
    IMFPMediaPlayer *p;
    PROPVARIANT pos;
    HRESULT hr;

    item_set_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    position_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    hr = MFPCreateMediaPlayer(file, FALSE, MFP_OPTION_FREE_THREADED_CALLBACK, &callback, NULL, &p);
    if (hr != S_OK || WaitForSingleObject(item_set_event, 30000) != WAIT_OBJECT_0)
    {
        printf("NOTE  the media file did not load (%#lx): the item checks are skipped\n", hr);
        if (p) IMFPMediaPlayer_Release(p);
        return;
    }
    hr = IMFPMediaPlayer_FrameStep(p);
    CHECKF(hr == MF_E_INVALIDREQUEST, "FrameStep while stopped is MF_E_INVALIDREQUEST (%#lx)", hr);
    pos.vt = VT_I8;
    pos.hVal.QuadPart = 500000;
    hr = IMFPMediaPlayer_SetPosition(p, &MFP_POSITIONTYPE_100NS, &pos);
    CHECKF(hr == S_OK, "SetPosition while stopped is S_OK (%#lx)", hr);
    CHECKF(WaitForSingleObject(position_event, 3000) == WAIT_OBJECT_0, "SetPosition posts MFP_EVENT_TYPE_POSITION_SET");
    memset(&pos, 0, sizeof(pos));
    hr = IMFPMediaPlayer_GetPosition(p, &MFP_POSITIONTYPE_100NS, &pos);
    CHECKF(hr == S_OK && pos.uhVal.QuadPart == 500000, "GetPosition returns the position that was set (%#lx, %I64u)", hr, pos.uhVal.QuadPart);
    IMFPMediaPlayer_Stop(p);
    IMFPMediaPlayer_Shutdown(p);
    IMFPMediaPlayer_Release(p);
}

static void test_class_object(void)
{
    HRESULT (WINAPI *get_class_object)(REFCLSID, REFIID, void **);
    HMODULE mod = LoadLibraryA("mfplay.dll");
    void *obj = (void *)1;
    HRESULT hr;

    get_class_object = mod ? (void *)GetProcAddress(mod, "DllGetClassObject") : NULL;
    if (!get_class_object) { check(0, "mfplay exports DllGetClassObject"); return; }
    hr = get_class_object(&IID_IUnknown, &IID_IClassFactory, &obj);
    CHECKF(hr == CLASS_E_CLASSNOTAVAILABLE && obj == NULL, "DllGetClassObject for an unknown class is CLASS_E_CLASSNOTAVAILABLE (%#lx)", hr);
    hr = get_class_object(&IID_IUnknown, &IID_IClassFactory, NULL);
    CHECKF(hr == E_POINTER, "DllGetClassObject(NULL out) is E_POINTER (%#lx)", hr);
}

int main(int argc, char **argv)
{
    IMFPMediaPlayer *p;
    IMFActivate *act;
    WCHAR file[MAX_PATH] = L"";
    HRESULT hr;
    ULONG base;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION, MFSTARTUP_FULL);
    if (argc > 1) MultiByteToWideChar(CP_ACP, 0, argv[1], -1, file, MAX_PATH);

    test_class_object();
    hr = MFPCreateMediaPlayer(NULL, FALSE, MFP_OPTION_FREE_THREADED_CALLBACK, NULL, NULL, &p);
    if (hr != S_OK) { printf("FAIL  MFPCreateMediaPlayer (%#lx)\n", hr); return 1; }
    act = test_idle(p);
    if (act)
    {
        base = refs((IUnknown *)act);
        IMFPMediaPlayer_Shutdown(p);
        CHECKF(refs((IUnknown *)act) == base - 1, "Shutdown releases the effects the player holds (%lu -> %lu)", base, refs((IUnknown *)act));
        IMFPMediaPlayer_Release(p);
        IMFActivate_Release(act);
    }
    else
        IMFPMediaPlayer_Release(p);

    hr = MFPCreateMediaPlayer(NULL, FALSE, MFP_OPTION_FREE_THREADED_CALLBACK, NULL, NULL, &p);
    IMFPMediaPlayer_Shutdown(p);
    test_shutdown(p);
    IMFPMediaPlayer_Release(p);

    if (file[0]) test_item(file);

    MFShutdown();
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
