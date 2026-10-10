/* qdvd's DVD navigator and DVD graph builder without a disc (patch 2900), run
 * by test/qdvd-nav-gate.sh. Table-driven: every IDvdControl2 and IDvdInfo2
 * method is called in the navigator's only state (stop domain, no disc) and its
 * HRESULT compared with the documented one; the state that can be set while
 * stopped is read back; the IDvdGraphBuilder methods are checked in each of the
 * builder's states.
 *
 *   qdvd-nav-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <dshow.h>
#include <stdio.h>
#include <string.h>


/* The mingw headers carry an older IDvdControl2/IDvdInfo2 (strmif.h): the
 * current vtables are declared by hand, with pointer arguments as void *. */
typedef struct XCtl XCtl;
typedef struct XInfo XInfo;
#define STD STDMETHODCALLTYPE
typedef struct { HRESULT (STD *QueryInterface)(XCtl *, REFIID, void **); ULONG (STD *AddRef)(XCtl *); ULONG (STD *Release)(XCtl *);
    HRESULT (STD *PlayTitle)(XCtl *, ULONG, DWORD, void **);
    HRESULT (STD *PlayChapterInTitle)(XCtl *, ULONG, ULONG, DWORD, void **);
    HRESULT (STD *PlayTimeInTitle)(XCtl *, ULONG, DVD_HMSF_TIMECODE *, DWORD, void **);
    HRESULT (STD *Stop)(XCtl *);
    HRESULT (STD *ReturnFromSubmenu)(XCtl *, DWORD, void **);
    HRESULT (STD *PlayAtTime)(XCtl *, DVD_HMSF_TIMECODE *, DWORD, void **);
    HRESULT (STD *PlayChapter)(XCtl *, ULONG, DWORD, void **);
    HRESULT (STD *PlayPrevChapter)(XCtl *, DWORD, void **);
    HRESULT (STD *ReplayChapter)(XCtl *, DWORD, void **);
    HRESULT (STD *PlayNextChapter)(XCtl *, DWORD, void **);
    HRESULT (STD *PlayForwards)(XCtl *, double, DWORD, void **);
    HRESULT (STD *PlayBackwards)(XCtl *, double, DWORD, void **);
    HRESULT (STD *ShowMenu)(XCtl *, int, DWORD, void **);
    HRESULT (STD *Resume)(XCtl *, DWORD, void **);
    HRESULT (STD *SelectRelativeButton)(XCtl *, int);
    HRESULT (STD *ActivateButton)(XCtl *);
    HRESULT (STD *SelectButton)(XCtl *, ULONG);
    HRESULT (STD *SelectAndActivateButton)(XCtl *, ULONG);
    HRESULT (STD *StillOff)(XCtl *);
    HRESULT (STD *Pause)(XCtl *, BOOL);
    HRESULT (STD *SelectAudioStream)(XCtl *, ULONG, DWORD, void **);
    HRESULT (STD *SelectSubpictureStream)(XCtl *, ULONG, DWORD, void **);
    HRESULT (STD *SetSubpictureState)(XCtl *, BOOL, DWORD, void **);
    HRESULT (STD *SelectAngle)(XCtl *, ULONG, DWORD, void **);
    HRESULT (STD *SelectParentalLevel)(XCtl *, ULONG);
    HRESULT (STD *SelectParentalCountry)(XCtl *, BYTE *);
    HRESULT (STD *SelectKaraokeAudioPresentationMode)(XCtl *, ULONG);
    HRESULT (STD *SelectVideoModePreference)(XCtl *, ULONG);
    HRESULT (STD *SetDVDDirectory)(XCtl *, const WCHAR *);
    HRESULT (STD *ActivateAtPosition)(XCtl *, POINT);
    HRESULT (STD *SelectAtPosition)(XCtl *, POINT);
    HRESULT (STD *PlayChaptersAutoStop)(XCtl *, ULONG, ULONG, ULONG, DWORD, void **);
    HRESULT (STD *AcceptParentalLevelChange)(XCtl *, BOOL);
    HRESULT (STD *SetOption)(XCtl *, int, BOOL);
    HRESULT (STD *SetState)(XCtl *, void *, DWORD, void **);
    HRESULT (STD *PlayPeriodInTitleAutoStop)(XCtl *, ULONG, DVD_HMSF_TIMECODE *, DVD_HMSF_TIMECODE *, DWORD, void **);
    HRESULT (STD *SetGPRM)(XCtl *, ULONG, WORD, DWORD, void **);
    HRESULT (STD *SelectDefaultMenuLanguage)(XCtl *, LCID);
    HRESULT (STD *SelectDefaultAudioLanguage)(XCtl *, LCID, int);
    HRESULT (STD *SelectDefaultSubpictureLanguage)(XCtl *, LCID, int);
} XCtlVtbl;
struct XCtl { const XCtlVtbl *lpVtbl; };
typedef struct { HRESULT (STD *QueryInterface)(XInfo *, REFIID, void **); ULONG (STD *AddRef)(XInfo *); ULONG (STD *Release)(XInfo *);
    HRESULT (STD *GetCurrentDomain)(XInfo *, int *);
    HRESULT (STD *GetCurrentLocation)(XInfo *, void *);
    HRESULT (STD *GetTotalTitleTime)(XInfo *, void *, ULONG *);
    HRESULT (STD *GetCurrentButton)(XInfo *, ULONG *, ULONG *);
    HRESULT (STD *GetCurrentAngle)(XInfo *, ULONG *, ULONG *);
    HRESULT (STD *GetCurrentAudio)(XInfo *, ULONG *, ULONG *);
    HRESULT (STD *GetCurrentSubpicture)(XInfo *, ULONG *, ULONG *, BOOL *);
    HRESULT (STD *GetCurrentUOPS)(XInfo *, ULONG *);
    HRESULT (STD *GetAllSPRMs)(XInfo *, void *);
    HRESULT (STD *GetAllGPRMs)(XInfo *, void *);
    HRESULT (STD *GetAudioLanguage)(XInfo *, ULONG, LCID *);
    HRESULT (STD *GetSubpictureLanguage)(XInfo *, ULONG, LCID *);
    HRESULT (STD *GetTitleAttributes)(XInfo *, ULONG, void *, void *);
    HRESULT (STD *GetVMGAttributes)(XInfo *, void *);
    HRESULT (STD *GetVideoAttributes)(XInfo *, void *);
    HRESULT (STD *GetAudioAttributes)(XInfo *, ULONG, void *);
    HRESULT (STD *GetKaraokeAttributes)(XInfo *, ULONG, void *);
    HRESULT (STD *GetSubpictureAttributes)(XInfo *, ULONG, void *);
    HRESULT (STD *GetCurrentVolumeInfo)(XInfo *, ULONG *, ULONG *, int *, ULONG *);
    HRESULT (STD *GetDVDTextNumberOfLanguages)(XInfo *, ULONG *);
    HRESULT (STD *GetDVDTextLanguageInfo)(XInfo *, ULONG, ULONG *, LCID *, int *);
    HRESULT (STD *GetDVDTextStringAsNative)(XInfo *, ULONG, ULONG, BYTE *, ULONG, ULONG *, int *);
    HRESULT (STD *GetDVDTextStringAsUnicode)(XInfo *, ULONG, ULONG, WCHAR *, ULONG, ULONG *, int *);
    HRESULT (STD *GetPlayerParentalLevel)(XInfo *, ULONG *, BYTE *);
    HRESULT (STD *GetNumberOfChapters)(XInfo *, ULONG, ULONG *);
    HRESULT (STD *GetTitleParentalLevels)(XInfo *, ULONG, ULONG *);
    HRESULT (STD *GetDVDDirectory)(XInfo *, WCHAR *, ULONG, ULONG *);
    HRESULT (STD *IsAudioStreamEnabled)(XInfo *, ULONG, BOOL *);
    HRESULT (STD *GetDiscID)(XInfo *, const WCHAR *, ULONGLONG *);
    HRESULT (STD *GetState)(XInfo *, void **);
    HRESULT (STD *GetMenuLanguages)(XInfo *, LCID *, ULONG, ULONG *);
    HRESULT (STD *GetButtonAtPosition)(XInfo *, POINT, ULONG *);
    HRESULT (STD *GetCmdFromEvent)(XInfo *, LONG_PTR, void **);
    HRESULT (STD *GetDefaultMenuLanguage)(XInfo *, LCID *);
    HRESULT (STD *GetDefaultAudioLanguage)(XInfo *, LCID *, int *);
    HRESULT (STD *GetDefaultSubpictureLanguage)(XInfo *, LCID *, int *);
    HRESULT (STD *GetDecoderCaps)(XInfo *, void *);
    HRESULT (STD *GetButtonRect)(XInfo *, ULONG, RECT *);
    HRESULT (STD *IsSubpictureStreamEnabled)(XInfo *, ULONG, BOOL *);
} XInfoVtbl;
struct XInfo { const XInfoVtbl *lpVtbl; };
typedef struct { DWORD dwSize, dwAudioCaps; double fwdv, fwda, fwds, bwdv, bwda, bwds; DWORD r1, r2, r3, r4; } XCaps;
typedef struct { HRESULT hrVPEStatus; BOOL bDvdVolInvalid, bDvdVolUnknown, bNoLine21In, bNoLine21Out; int iNumStreams, iNumStreamsFailed; DWORD dwFailedStreamsFlag; } XRender;
typedef struct XBuilder XBuilder;
typedef struct { HRESULT (STD *QueryInterface)(XBuilder *, REFIID, void **); ULONG (STD *AddRef)(XBuilder *); ULONG (STD *Release)(XBuilder *);
    HRESULT (STD *GetFiltergraph)(XBuilder *, IGraphBuilder **);
    HRESULT (STD *GetDvdInterface)(XBuilder *, const GUID *, void **);
    HRESULT (STD *RenderDvdVideoVolume)(XBuilder *, const WCHAR *, DWORD, XRender *);
} XBuilderVtbl;
struct XBuilder { const XBuilderVtbl *lpVtbl; };
DEFINE_GUID(XIID_IDvdControl2, 0x33bc7430, 0xeec0, 0x11d2, 0x82, 0x01, 0x00, 0xa0, 0xc9, 0xd7, 0x48, 0x42);
DEFINE_GUID(XIID_IDvdInfo2, 0x34151510, 0xeec0, 0x11d2, 0x82, 0x01, 0x00, 0xa0, 0xc9, 0xd7, 0x48, 0x42);
DEFINE_GUID(XIID_IDvdGraphBuilder, 0xfcc152b6, 0xf372, 0x11d0, 0x8e, 0x00, 0x00, 0xc0, 0x4f, 0xd7, 0xc0, 0x8b);
#define XC(o, m, ...) ((o)->lpVtbl->m((o), ##__VA_ARGS__))
static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[256]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

static const GUID test_iid = {0x33333333};
#define INV VFW_E_DVD_INVALIDDOMAIN

enum ctl_id {
    C_PlayTitle, C_PlayChapterInTitle, C_PlayTimeInTitle, C_Stop, C_ReturnFromSubmenu, C_PlayAtTime,
    C_PlayChapter, C_PlayPrevChapter, C_ReplayChapter, C_PlayNextChapter, C_PlayForwards, C_PlayBackwards,
    C_ShowMenu, C_Resume, C_SelectRelativeButton, C_ActivateButton, C_SelectButton, C_SelectAndActivateButton,
    C_StillOff, C_Pause, C_SelectAudioStream, C_SelectSubpictureStream, C_SetSubpictureState, C_SelectAngle,
    C_ActivateAtPosition, C_SelectAtPosition, C_PlayChaptersAutoStop, C_AcceptParentalLevelChange,
    C_SetState, C_PlayPeriodInTitleAutoStop, C_SetGPRM,
};

static DVD_HMSF_TIMECODE tc = {0, 1, 2, 3};
static DVD_HMSF_TIMECODE bad_tc = {0, 61, 2, 3};

/* a = the variant: 0 = valid arguments, 1 = bad flags, 2 = bad argument */
static HRESULT call_ctl(XCtl *c, int id, int variant, void **cmd)
{
    DWORD fl = variant == 1 ? 0x100 : 0;
    POINT pt = {5, 5};
    switch (id)
    {
    case C_PlayTitle: return XC(c, PlayTitle, variant == 2 ? 0 : 1, fl, cmd);
    case C_PlayChapterInTitle: return XC(c, PlayChapterInTitle, 1, variant == 2 ? 0 : 1, fl, cmd);
    case C_PlayTimeInTitle: return XC(c, PlayTimeInTitle, 1, variant == 2 ? &bad_tc : &tc, fl, cmd);
    case C_Stop: return XC(c, Stop);
    case C_ReturnFromSubmenu: return XC(c, ReturnFromSubmenu, fl, cmd);
    case C_PlayAtTime: return XC(c, PlayAtTime, variant == 2 ? &bad_tc : &tc, fl, cmd);
    case C_PlayChapter: return XC(c, PlayChapter, variant == 2 ? 0 : 1, fl, cmd);
    case C_PlayPrevChapter: return XC(c, PlayPrevChapter, fl, cmd);
    case C_ReplayChapter: return XC(c, ReplayChapter, fl, cmd);
    case C_PlayNextChapter: return XC(c, PlayNextChapter, fl, cmd);
    case C_PlayForwards: return XC(c, PlayForwards, 2.0, fl, cmd);
    case C_PlayBackwards: return XC(c, PlayBackwards, 2.0, fl, cmd);
    case C_ShowMenu: return XC(c, ShowMenu, variant == 2 ? 99 : DVD_MENU_Root, fl, cmd);
    case C_Resume: return XC(c, Resume, fl, cmd);
    case C_SelectRelativeButton: return XC(c, SelectRelativeButton, variant == 2 ? 9 : DVD_Relative_Left);
    case C_ActivateButton: return XC(c, ActivateButton);
    case C_SelectButton: return XC(c, SelectButton, variant == 2 ? 0 : 1);
    case C_SelectAndActivateButton: return XC(c, SelectAndActivateButton, variant == 2 ? 37 : 1);
    case C_StillOff: return XC(c, StillOff);
    case C_Pause: return XC(c, Pause, TRUE);
    case C_SelectAudioStream: return XC(c, SelectAudioStream, variant == 2 ? 8 : 1, fl, cmd);
    case C_SelectSubpictureStream: return XC(c, SelectSubpictureStream, variant == 2 ? 32 : 1, fl, cmd);
    case C_SetSubpictureState: return XC(c, SetSubpictureState, TRUE, fl, cmd);
    case C_SelectAngle: return XC(c, SelectAngle, variant == 2 ? 10 : 1, fl, cmd);
    case C_ActivateAtPosition: return XC(c, ActivateAtPosition, pt);
    case C_SelectAtPosition: return XC(c, SelectAtPosition, pt);
    case C_PlayChaptersAutoStop: return XC(c, PlayChaptersAutoStop, 1, 1, variant == 2 ? 0 : 2, fl, cmd);
    case C_AcceptParentalLevelChange: return XC(c, AcceptParentalLevelChange, TRUE);
    case C_SetState: return XC(c, SetState, variant == 2 ? NULL : (void *)(DWORD_PTR)0x1000, fl, cmd);
    case C_PlayPeriodInTitleAutoStop: return XC(c, PlayPeriodInTitleAutoStop, 1, &tc, variant == 2 ? &bad_tc : &tc, fl, cmd);
    case C_SetGPRM: return XC(c, SetGPRM, variant == 2 ? 16 : 3, 7, fl, cmd);
    }
    return E_UNEXPECTED;
}

static const struct { const char *name; int id; int has_cmd; int has_bad_arg; HRESULT bad_hr; } ctl_table[] = {
    {"PlayTitle", C_PlayTitle, 1, 1, E_INVALIDARG},
    {"PlayChapterInTitle", C_PlayChapterInTitle, 1, 1, E_INVALIDARG},
    {"PlayTimeInTitle", C_PlayTimeInTitle, 1, 1, E_INVALIDARG},
    {"ReturnFromSubmenu", C_ReturnFromSubmenu, 1, 0, 0},
    {"PlayAtTime", C_PlayAtTime, 1, 1, E_INVALIDARG},
    {"PlayChapter", C_PlayChapter, 1, 1, E_INVALIDARG},
    {"PlayPrevChapter", C_PlayPrevChapter, 1, 0, 0},
    {"ReplayChapter", C_ReplayChapter, 1, 0, 0},
    {"PlayNextChapter", C_PlayNextChapter, 1, 0, 0},
    {"PlayForwards", C_PlayForwards, 1, 0, 0},
    {"PlayBackwards", C_PlayBackwards, 1, 0, 0},
    {"ShowMenu", C_ShowMenu, 1, 1, E_INVALIDARG},
    {"Resume", C_Resume, 1, 0, 0},
    {"SelectAudioStream", C_SelectAudioStream, 1, 1, E_INVALIDARG},
    {"SelectSubpictureStream", C_SelectSubpictureStream, 1, 1, E_INVALIDARG},
    {"SetSubpictureState", C_SetSubpictureState, 1, 0, 0},
    {"SelectAngle", C_SelectAngle, 1, 1, E_INVALIDARG},
    {"PlayChaptersAutoStop", C_PlayChaptersAutoStop, 1, 1, E_INVALIDARG},
    {"SetState", C_SetState, 1, 1, E_POINTER},
    {"PlayPeriodInTitleAutoStop", C_PlayPeriodInTitleAutoStop, 1, 1, E_INVALIDARG},
    {"SetGPRM", C_SetGPRM, 1, 1, E_INVALIDARG},
    {"SelectRelativeButton", C_SelectRelativeButton, 0, 1, E_INVALIDARG},
    {"ActivateButton", C_ActivateButton, 0, 0, 0},
    {"SelectButton", C_SelectButton, 0, 1, E_INVALIDARG},
    {"SelectAndActivateButton", C_SelectAndActivateButton, 0, 1, E_INVALIDARG},
    {"StillOff", C_StillOff, 0, 0, 0},
    {"Pause", C_Pause, 0, 0, 0},
    {"ActivateAtPosition", C_ActivateAtPosition, 0, 0, 0},
    {"SelectAtPosition", C_SelectAtPosition, 0, 0, 0},
    {"AcceptParentalLevelChange", C_AcceptParentalLevelChange, 0, 0, 0},
};

static void test_control(XCtl *ctl)
{
    unsigned i;
    HRESULT hr;
    void *cmd;

    hr = call_ctl(ctl, C_Stop, 0, NULL);
    CHECKF(hr == S_OK, "Stop on a stopped navigator is S_OK (%#lx)", hr);

    for (i = 0; i < sizeof(ctl_table) / sizeof(ctl_table[0]); i++)
    {
        cmd = (void *)(DWORD_PTR)0x1234;
        hr = call_ctl(ctl, ctl_table[i].id, 0, ctl_table[i].has_cmd ? &cmd : NULL);
        CHECKF(hr == INV, "%s in the stop domain is VFW_E_DVD_INVALIDDOMAIN (%#lx)", ctl_table[i].name, hr);
        if (ctl_table[i].has_cmd)
            CHECKF(cmd == NULL, "%s clears the command pointer", ctl_table[i].name);
        if (ctl_table[i].has_cmd)
        {
            hr = call_ctl(ctl, ctl_table[i].id, 0, NULL);
            CHECKF(hr == INV, "%s with a NULL command pointer is still VFW_E_DVD_INVALIDDOMAIN (%#lx)", ctl_table[i].name, hr);
            hr = call_ctl(ctl, ctl_table[i].id, 1, &cmd);
            CHECKF(hr == E_INVALIDARG, "%s with an unknown command flag is E_INVALIDARG (%#lx)", ctl_table[i].name, hr);
        }
        if (ctl_table[i].has_bad_arg)
        {
            hr = call_ctl(ctl, ctl_table[i].id, 2, &cmd);
            CHECKF(hr == ctl_table[i].bad_hr, "%s with an invalid argument is %#lx (%#lx)", ctl_table[i].name, ctl_table[i].bad_hr, hr);
        }
    }
    hr = XC(ctl, PlayTimeInTitle, 1, NULL, 0, &cmd);
    CHECKF(hr == E_POINTER, "PlayTimeInTitle(NULL time) is E_POINTER (%#lx)", hr);
    hr = XC(ctl, PlayAtTime, NULL, 0, &cmd);
    CHECKF(hr == E_POINTER, "PlayAtTime(NULL time) is E_POINTER (%#lx)", hr);
    hr = XC(ctl, PlayPeriodInTitleAutoStop, 1, NULL, &tc, 0, &cmd);
    CHECKF(hr == E_POINTER, "PlayPeriodInTitleAutoStop(NULL start) is E_POINTER (%#lx)", hr);
}

static void test_settings(XCtl *ctl, XInfo *info)
{
    LCID lang = 0xdead;
    int aext = 99;
    int sext = 99;
    ULONG level = 0;
    BYTE country[2] = {0, 0};
    int domain = 0;
    HRESULT hr;
    int i;

    hr = XC(info, GetCurrentDomain, &domain);
    CHECKF(hr == S_OK && domain == DVD_DOMAIN_Stop, "GetCurrentDomain is the stop domain (%#lx, %d)", hr, domain);
    hr = XC(info, GetCurrentDomain, NULL);
    CHECKF(hr == E_POINTER, "GetCurrentDomain(NULL) is E_POINTER (%#lx)", hr);

    hr = XC(info, GetPlayerParentalLevel, &level, country);
    CHECKF(hr == S_OK && level == 0xffffffff && country[0] == 'U' && country[1] == 'S',
            "default parental level is unset (%#lx, %#lx, %c%c)", hr, level, country[0], country[1]);
    for (i = 1; i <= 8; i += 7)
    {
        hr = XC(ctl, SelectParentalLevel, i);
        CHECKF(hr == S_OK, "SelectParentalLevel(%d) (%#lx)", i, hr);
        level = 0;
        XC(info, GetPlayerParentalLevel, &level, country);
        CHECKF(level == (ULONG)i, "parental level %d reads back as %lu", i, level);
    }
    hr = XC(ctl, SelectParentalLevel, 9);
    CHECKF(hr == E_INVALIDARG, "SelectParentalLevel(9) is E_INVALIDARG (%#lx)", hr);
    hr = XC(ctl, SelectParentalLevel, 0);
    CHECKF(hr == E_INVALIDARG, "SelectParentalLevel(0) is E_INVALIDARG (%#lx)", hr);
    level = 0;
    XC(info, GetPlayerParentalLevel, &level, country);
    CHECKF(level == 8, "a rejected parental level leaves the old one (%lu)", level);
    hr = XC(ctl, SelectParentalLevel, 0xffffffff);
    CHECKF(hr == S_OK, "SelectParentalLevel(-1) is S_OK (%#lx)", hr);
    {
        BYTE de[2] = {'D', 'E'};
        hr = XC(ctl, SelectParentalCountry, de);
        CHECKF(hr == S_OK, "SelectParentalCountry (%#lx)", hr);
        XC(info, GetPlayerParentalLevel, &level, country);
        CHECKF(country[0] == 'D' && country[1] == 'E' && level == 0xffffffff, "country reads back as DE (%c%c)", country[0], country[1]);
        hr = XC(ctl, SelectParentalCountry, NULL);
        CHECKF(hr == E_POINTER, "SelectParentalCountry(NULL) is E_POINTER (%#lx)", hr);
    }
    hr = XC(info, GetPlayerParentalLevel, NULL, country);
    CHECKF(hr == E_POINTER, "GetPlayerParentalLevel(NULL level) is E_POINTER (%#lx)", hr);

    hr = XC(ctl, SelectDefaultMenuLanguage, 0x407);
    CHECKF(hr == S_OK, "SelectDefaultMenuLanguage (%#lx)", hr);
    hr = XC(info, GetDefaultMenuLanguage, &lang);
    CHECKF(hr == S_OK && lang == 0x407, "default menu language reads back (%#lx, %#lx)", hr, lang);
    hr = XC(ctl, SelectDefaultMenuLanguage, 0x7fff7fff);
    CHECKF(hr == E_INVALIDARG, "SelectDefaultMenuLanguage(bad LCID) is E_INVALIDARG (%#lx)", hr);
    XC(info, GetDefaultMenuLanguage, &lang);
    CHECKF(lang == 0x407, "a rejected language leaves the old one (%#lx)", lang);
    hr = XC(info, GetDefaultMenuLanguage, NULL);
    CHECKF(hr == E_POINTER, "GetDefaultMenuLanguage(NULL) is E_POINTER (%#lx)", hr);

    hr = XC(ctl, SelectDefaultAudioLanguage, 0x40c, DVD_AUD_EXT_VisuallyImpaired);
    CHECKF(hr == S_OK, "SelectDefaultAudioLanguage (%#lx)", hr);
    hr = XC(info, GetDefaultAudioLanguage, &lang, &aext);
    CHECKF(hr == S_OK && lang == 0x40c && aext == DVD_AUD_EXT_VisuallyImpaired, "audio default reads back (%#lx, %#lx, %d)", hr, lang, aext);
    hr = XC(ctl, SelectDefaultAudioLanguage, 0x409, 17);
    CHECKF(hr == E_INVALIDARG, "SelectDefaultAudioLanguage(bad extension) is E_INVALIDARG (%#lx)", hr);
    XC(info, GetDefaultAudioLanguage, &lang, &aext);
    CHECKF(lang == 0x40c, "a rejected audio default leaves the old one (%#lx)", lang);
    hr = XC(info, GetDefaultAudioLanguage, &lang, NULL);
    CHECKF(hr == E_POINTER, "GetDefaultAudioLanguage(NULL extension) is E_POINTER (%#lx)", hr);

    hr = XC(ctl, SelectDefaultSubpictureLanguage, 0x410, DVD_SP_EXT_Forced);
    CHECKF(hr == S_OK, "SelectDefaultSubpictureLanguage (%#lx)", hr);
    hr = XC(info, GetDefaultSubpictureLanguage, &lang, &sext);
    CHECKF(hr == S_OK && lang == 0x410 && sext == DVD_SP_EXT_Forced, "subpicture default reads back (%#lx, %#lx, %d)", hr, lang, sext);
    hr = XC(ctl, SelectDefaultSubpictureLanguage, 0x410, 4);
    CHECKF(hr == E_INVALIDARG, "SelectDefaultSubpictureLanguage(extension 4) is E_INVALIDARG (%#lx)", hr);

    hr = XC(ctl, SetOption, 1, TRUE);
    CHECKF(hr == S_OK, "SetOption(ResetOnStop) (%#lx)", hr);
    hr = XC(ctl, SetOption, 19, FALSE);
    CHECKF(hr == S_OK, "SetOption(EnableCC) (%#lx)", hr);
    hr = XC(ctl, SetOption, 0, TRUE);
    CHECKF(hr == E_INVALIDARG, "SetOption(0) is E_INVALIDARG (%#lx)", hr);
    hr = XC(ctl, SetOption, 20, TRUE);
    CHECKF(hr == E_INVALIDARG, "SetOption(20) is E_INVALIDARG (%#lx)", hr);
    hr = XC(ctl, SelectKaraokeAudioPresentationMode, 3);
    CHECKF(hr == S_OK, "SelectKaraokeAudioPresentationMode (%#lx)", hr);
    hr = XC(ctl, SelectVideoModePreference, DISPLAY_16x9);
    CHECKF(hr == S_OK, "SelectVideoModePreference(16x9) (%#lx)", hr);
    hr = XC(ctl, SelectVideoModePreference, 4);
    CHECKF(hr == E_INVALIDARG, "SelectVideoModePreference(4) is E_INVALIDARG (%#lx)", hr);
}

static void test_directory(XCtl *ctl, XInfo *info)
{
    WCHAR buf[MAX_PATH], small[4];
    ULONG size = 0;
    ULONGLONG id = 5;
    HRESULT hr;
    HANDLE h;

    hr = XC(info, GetDVDDirectory, buf, MAX_PATH, &size);
    CHECKF(hr == VFW_E_DVD_INVALID_DISC, "GetDVDDirectory without a volume is VFW_E_DVD_INVALID_DISC (%#lx)", hr);
    hr = XC(info, GetDVDDirectory, buf, MAX_PATH, NULL);
    CHECKF(hr == E_POINTER, "GetDVDDirectory(NULL size) is E_POINTER (%#lx)", hr);
    hr = XC(ctl, SetDVDDirectory, NULL);
    CHECKF(hr == E_POINTER, "SetDVDDirectory(NULL) is E_POINTER (%#lx)", hr);
    hr = XC(ctl, SetDVDDirectory, L"C:\\no-such-dvd");
    CHECKF(hr == VFW_E_DVD_INVALID_DISC, "SetDVDDirectory(no volume) is VFW_E_DVD_INVALID_DISC (%#lx)", hr);
    hr = XC(info, GetDVDDirectory, buf, MAX_PATH, &size);
    CHECKF(hr == VFW_E_DVD_INVALID_DISC, "a failed SetDVDDirectory stores nothing (%#lx)", hr);
    hr = XC(info, GetDiscID, NULL, &id);
    CHECKF(hr == VFW_E_DVD_INVALID_DISC && id == 0, "GetDiscID without a volume (%#lx)", hr);
    hr = XC(info, GetDiscID, L"C:\\no-such-dvd", NULL);
    CHECKF(hr == E_POINTER, "GetDiscID(NULL id) is E_POINTER (%#lx)", hr);

    CreateDirectoryW(L"C:\\fakedvd", NULL);
    CreateDirectoryW(L"C:\\fakedvd\\VIDEO_TS", NULL);
    h = CreateFileW(L"C:\\fakedvd\\VIDEO_TS\\VIDEO_TS.IFO", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    hr = XC(ctl, SetDVDDirectory, L"C:\\fakedvd");
    CHECKF(hr == S_OK, "SetDVDDirectory(volume) (%#lx)", hr);
    size = 0; buf[0] = 0;
    hr = XC(info, GetDVDDirectory, buf, MAX_PATH, &size);
    CHECKF(hr == S_OK && size == 11 && !wcscmp(buf, L"C:\\fakedvd"), "GetDVDDirectory reads the path back (%#lx, %lu, %ls)", hr, size, buf);
    size = 0;
    hr = XC(info, GetDVDDirectory, small, 4, &size);
    CHECKF(hr == E_INVALIDARG && size == 11, "GetDVDDirectory with a small buffer is E_INVALIDARG and reports the size (%#lx, %lu)", hr, size);
    hr = XC(info, GetDVDDirectory, NULL, 0, &size);
    CHECKF(hr == E_POINTER, "GetDVDDirectory(NULL buffer) is E_POINTER (%#lx)", hr);
}

enum info_id {
    I_GetCurrentLocation, I_GetTotalTitleTime, I_GetCurrentButton, I_GetCurrentAngle, I_GetCurrentAudio,
    I_GetCurrentSubpicture, I_GetCurrentUOPS, I_GetAllSPRMs, I_GetAllGPRMs, I_GetAudioLanguage,
    I_GetSubpictureLanguage, I_GetTitleAttributes, I_GetVMGAttributes, I_GetVideoAttributes,
    I_GetAudioAttributes, I_GetKaraokeAttributes, I_GetSubpictureAttributes, I_GetCurrentVolumeInfo,
    I_GetDVDTextNumberOfLanguages, I_GetDVDTextLanguageInfo, I_GetDVDTextStringAsNative,
    I_GetDVDTextStringAsUnicode, I_GetNumberOfChapters, I_GetTitleParentalLevels, I_IsAudioStreamEnabled,
    I_IsSubpictureStreamEnabled, I_GetState, I_GetMenuLanguages, I_GetButtonAtPosition, I_GetButtonRect,
};

/* variant 0 = valid, 1 = NULL out pointer, 2 = bad index */
static HRESULT call_info(XInfo *i, int id, int v)
{
    union { DVD_PLAYBACK_LOCATION2 loc; DVD_HMSF_TIMECODE t; SPRMARRAY sp; GPRMARRAY gp; DVD_MenuAttributes ma;
        DVD_TitleAttributes ta; DVD_VideoAttributes va; DVD_AudioAttributes aa; DVD_KaraokeAttributes ka;
        DVD_SubpictureAttributes sa; BYTE raw[512]; } u;
    ULONG a = 0, b = 0, c = 0;
    BOOL e = 0;
    LCID l;
    POINT pt = {1, 1};
    RECT rc;
    void *st;
    int side;
    int cs;
    int ty;
    BYTE nb[8];
    WCHAR wb[8];
    LCID langs[2];
    memset(&u, 0, sizeof(u));
#define P(x) (v == 1 ? NULL : (x))
    switch (id)
    {
    case I_GetCurrentLocation: return XC(i, GetCurrentLocation, P(&u.loc));
    case I_GetTotalTitleTime: return XC(i, GetTotalTitleTime, P(&u.t), &a);
    case I_GetCurrentButton: return XC(i, GetCurrentButton, &a, P(&b));
    case I_GetCurrentAngle: return XC(i, GetCurrentAngle, P(&a), &b);
    case I_GetCurrentAudio: return XC(i, GetCurrentAudio, &a, P(&b));
    case I_GetCurrentSubpicture: return XC(i, GetCurrentSubpicture, &a, &b, P(&e));
    case I_GetCurrentUOPS: return XC(i, GetCurrentUOPS, P(&a));
    case I_GetAllSPRMs: return XC(i, GetAllSPRMs, P(&u.sp));
    case I_GetAllGPRMs: return XC(i, GetAllGPRMs, P(&u.gp));
    case I_GetAudioLanguage: return XC(i, GetAudioLanguage, v == 2 ? 8 : 0, P(&l));
    case I_GetSubpictureLanguage: return XC(i, GetSubpictureLanguage, v == 2 ? 32 : 0, P(&l));
    case I_GetTitleAttributes: return XC(i, GetTitleAttributes, v == 2 ? 100 : 1, &u.ma, P(&u.ta));
    case I_GetVMGAttributes: return XC(i, GetVMGAttributes, P(&u.ma));
    case I_GetVideoAttributes: return XC(i, GetVideoAttributes, P(&u.va));
    case I_GetAudioAttributes: return XC(i, GetAudioAttributes, v == 2 ? 8 : 0, P(&u.aa));
    case I_GetKaraokeAttributes: return XC(i, GetKaraokeAttributes, v == 2 ? 8 : 0, P(&u.ka));
    case I_GetSubpictureAttributes: return XC(i, GetSubpictureAttributes, v == 2 ? 32 : 0, P(&u.sa));
    case I_GetCurrentVolumeInfo: return XC(i, GetCurrentVolumeInfo, &a, &b, P(&side), &c);
    case I_GetDVDTextNumberOfLanguages: return XC(i, GetDVDTextNumberOfLanguages, P(&a));
    case I_GetDVDTextLanguageInfo: return XC(i, GetDVDTextLanguageInfo, 0, &a, P(&l), &cs);
    case I_GetDVDTextStringAsNative: return XC(i, GetDVDTextStringAsNative, 0, 0, nb, 8, P(&a), &ty);
    case I_GetDVDTextStringAsUnicode: return XC(i, GetDVDTextStringAsUnicode, 0, 0, wb, 8, P(&a), &ty);
    case I_GetNumberOfChapters: return XC(i, GetNumberOfChapters, v == 2 ? 0 : 1, P(&a));
    case I_GetTitleParentalLevels: return XC(i, GetTitleParentalLevels, v == 2 ? 100 : 1, P(&a));
    case I_IsAudioStreamEnabled: return XC(i, IsAudioStreamEnabled, v == 2 ? 8 : 0, P(&e));
    case I_IsSubpictureStreamEnabled: return XC(i, IsSubpictureStreamEnabled, v == 2 ? 32 : 0, P(&e));
    case I_GetState: return XC(i, GetState, P(&st));
    case I_GetMenuLanguages: return XC(i, GetMenuLanguages, langs, 2, P(&a));
    case I_GetButtonAtPosition: return XC(i, GetButtonAtPosition, pt, P(&a));
    case I_GetButtonRect: return XC(i, GetButtonRect, v == 2 ? 0 : 1, P(&rc));
    }
    return E_UNEXPECTED;
}

static const struct { const char *name; int id; int has_bad_index; } info_table[] = {
    {"GetCurrentLocation", I_GetCurrentLocation, 0}, {"GetTotalTitleTime", I_GetTotalTitleTime, 0},
    {"GetCurrentButton", I_GetCurrentButton, 0}, {"GetCurrentAngle", I_GetCurrentAngle, 0},
    {"GetCurrentAudio", I_GetCurrentAudio, 0}, {"GetCurrentSubpicture", I_GetCurrentSubpicture, 0},
    {"GetCurrentUOPS", I_GetCurrentUOPS, 0}, {"GetAllSPRMs", I_GetAllSPRMs, 0}, {"GetAllGPRMs", I_GetAllGPRMs, 0},
    {"GetAudioLanguage", I_GetAudioLanguage, 1}, {"GetSubpictureLanguage", I_GetSubpictureLanguage, 1},
    {"GetTitleAttributes", I_GetTitleAttributes, 1}, {"GetVMGAttributes", I_GetVMGAttributes, 0},
    {"GetVideoAttributes", I_GetVideoAttributes, 0}, {"GetAudioAttributes", I_GetAudioAttributes, 1},
    {"GetKaraokeAttributes", I_GetKaraokeAttributes, 1}, {"GetSubpictureAttributes", I_GetSubpictureAttributes, 1},
    {"GetCurrentVolumeInfo", I_GetCurrentVolumeInfo, 0}, {"GetDVDTextNumberOfLanguages", I_GetDVDTextNumberOfLanguages, 0},
    {"GetDVDTextLanguageInfo", I_GetDVDTextLanguageInfo, 0}, {"GetDVDTextStringAsNative", I_GetDVDTextStringAsNative, 0},
    {"GetDVDTextStringAsUnicode", I_GetDVDTextStringAsUnicode, 0}, {"GetNumberOfChapters", I_GetNumberOfChapters, 1},
    {"GetTitleParentalLevels", I_GetTitleParentalLevels, 1}, {"IsAudioStreamEnabled", I_IsAudioStreamEnabled, 1},
    {"IsSubpictureStreamEnabled", I_IsSubpictureStreamEnabled, 1}, {"GetState", I_GetState, 0},
    {"GetMenuLanguages", I_GetMenuLanguages, 0}, {"GetButtonAtPosition", I_GetButtonAtPosition, 0},
    {"GetButtonRect", I_GetButtonRect, 1},
};

static void test_info(XInfo *info)
{
    XCaps caps;
    void *cmd = (void *)(DWORD_PTR)1;
    unsigned i;
    HRESULT hr;

    for (i = 0; i < sizeof(info_table) / sizeof(info_table[0]); i++)
    {
        hr = call_info(info, info_table[i].id, 0);
        CHECKF(hr == INV, "%s in the stop domain is VFW_E_DVD_INVALIDDOMAIN (%#lx)", info_table[i].name, hr);
        hr = call_info(info, info_table[i].id, 1);
        CHECKF(hr == E_POINTER, "%s with a NULL out pointer is E_POINTER (%#lx)", info_table[i].name, hr);
        if (info_table[i].has_bad_index)
        {
            hr = call_info(info, info_table[i].id, 2);
            CHECKF(hr == E_INVALIDARG, "%s with an out-of-range index is E_INVALIDARG (%#lx)", info_table[i].name, hr);
        }
    }
    hr = XC(info, GetCmdFromEvent, 1, &cmd);
    CHECKF(hr == E_INVALIDARG && !cmd, "GetCmdFromEvent for an unknown event is E_INVALIDARG (%#lx)", hr);
    hr = XC(info, GetCmdFromEvent, 1, NULL);
    CHECKF(hr == E_POINTER, "GetCmdFromEvent(NULL) is E_POINTER (%#lx)", hr);

    memset(&caps, 0xcc, sizeof(caps));
    caps.dwSize = sizeof(caps);
    hr = XC(info, GetDecoderCaps, &caps);
    CHECKF(hr == S_OK && caps.dwSize == sizeof(caps) && caps.dwAudioCaps == 0 && caps.fwdv == 0.0 && caps.r4 == 0,
            "GetDecoderCaps fills a zero capability set (%#lx)", hr);
    caps.dwSize = 4;
    hr = XC(info, GetDecoderCaps, &caps);
    CHECKF(hr == E_INVALIDARG, "GetDecoderCaps with a wrong size is E_INVALIDARG (%#lx)", hr);
    hr = XC(info, GetDecoderCaps, NULL);
    CHECKF(hr == E_POINTER, "GetDecoderCaps(NULL) is E_POINTER (%#lx)", hr);
}

static void test_builder(void)
{
    XBuilder *b;
    IGraphBuilder *g, *g2;
    XRender st;
    XCtl *ctl;
    XInfo *info;
    IMediaControl *mc;
    IUnknown *unk;
    ULONG size = 0;
    WCHAR buf[MAX_PATH];
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_DvdGraphBuilder, NULL, CLSCTX_INPROC_SERVER, &XIID_IDvdGraphBuilder, (void **)&b);
    if (hr != S_OK) { check(0, "create the DVD graph builder"); return; }

    hr = XC(b, GetFiltergraph, NULL);
    CHECKF(hr == E_POINTER, "GetFiltergraph(NULL) is E_POINTER (%#lx)", hr);
    hr = XC(b, GetFiltergraph, &g);
    CHECKF(hr == S_OK && g, "GetFiltergraph returns a graph (%#lx)", hr);
    hr = XC(b, GetFiltergraph, &g2);
    CHECKF(hr == S_OK && g2 == g, "GetFiltergraph returns the same graph again (%p, %p)", g, g2);
    if (g2) IGraphBuilder_Release(g2);
    if (g) IGraphBuilder_Release(g);

    hr = XC(b, GetDvdInterface, &XIID_IDvdControl2, NULL);
    CHECKF(hr == E_POINTER, "GetDvdInterface(NULL out) is E_POINTER (%#lx)", hr);
    unk = (IUnknown *)(DWORD_PTR)1;
    hr = XC(b, GetDvdInterface, &XIID_IDvdControl2, (void **)&unk);
    CHECKF(hr == VFW_E_DVD_GRAPHNOTREADY && !unk, "GetDvdInterface before rendering is VFW_E_DVD_GRAPHNOTREADY (%#lx)", hr);

    hr = XC(b, RenderDvdVideoVolume, L"C:\\fakedvd", 0, NULL);
    CHECKF(hr == E_POINTER, "RenderDvdVideoVolume(NULL status) is E_POINTER (%#lx)", hr);
    memset(&st, 0xcc, sizeof(st));
    hr = XC(b, RenderDvdVideoVolume, L"C:\\fakedvd", 0x10000, &st);
    CHECKF(hr == E_INVALIDARG, "RenderDvdVideoVolume(unknown flag) is E_INVALIDARG (%#lx)", hr);
    hr = XC(b, RenderDvdVideoVolume, L"C:\\fakedvd", AM_DVD_HWDEC_ONLY | AM_DVD_SWDEC_ONLY, &st);
    CHECKF(hr == E_INVALIDARG, "RenderDvdVideoVolume(hardware and software only) is E_INVALIDARG (%#lx)", hr);

    memset(&st, 0xcc, sizeof(st));
    hr = XC(b, RenderDvdVideoVolume, L"C:\\no-such-dvd", 0, &st);
    CHECKF(hr == VFW_E_DVD_RENDERFAIL && st.bDvdVolInvalid && !st.bDvdVolUnknown && st.iNumStreams == 0,
            "RenderDvdVideoVolume(no volume) is VFW_E_DVD_RENDERFAIL with bDvdVolInvalid (%#lx, %d)", hr, st.bDvdVolInvalid);
    hr = XC(b, GetDvdInterface, &XIID_IDvdControl2, (void **)&unk);
    CHECKF(hr == VFW_E_DVD_GRAPHNOTREADY, "a failed render builds no navigator (%#lx)", hr);
    memset(&st, 0xcc, sizeof(st));
    hr = XC(b, RenderDvdVideoVolume, NULL, 0, &st);
    CHECKF(hr == VFW_E_DVD_RENDERFAIL && st.bDvdVolUnknown && !st.bDvdVolInvalid,
            "RenderDvdVideoVolume(NULL path, no drive) is VFW_E_DVD_RENDERFAIL with bDvdVolUnknown (%#lx)", hr);

    memset(&st, 0xcc, sizeof(st));
    hr = XC(b, RenderDvdVideoVolume, L"C:\\fakedvd", 0, &st);
    CHECKF(hr == VFW_E_DVD_RENDERFAIL && !st.bDvdVolInvalid && !st.bDvdVolUnknown && st.iNumStreams == 3
            && st.iNumStreamsFailed == 3 && st.dwFailedStreamsFlag == 7 && st.hrVPEStatus == 0,
            "RenderDvdVideoVolume(volume, no decoders) fails all three streams (%#lx, %d/%d, %#lx)", hr,
            st.iNumStreamsFailed, st.iNumStreams, st.dwFailedStreamsFlag);

    hr = XC(b, GetDvdInterface, &XIID_IDvdControl2, (void **)&ctl);
    CHECKF(hr == S_OK && ctl, "GetDvdInterface(IDvdControl2) after rendering (%#lx)", hr);
    hr = XC(b, GetDvdInterface, &XIID_IDvdInfo2, (void **)&info);
    CHECKF(hr == S_OK && info, "GetDvdInterface(IDvdInfo2) after rendering (%#lx)", hr);
    if (ctl && info)
    {
        size = 0;
        hr = XC(info, GetDVDDirectory, buf, MAX_PATH, &size);
        CHECKF(hr == S_OK && !wcscmp(buf, L"C:\\fakedvd"), "the navigator in the graph holds the rendered volume (%#lx, %ls)", hr, buf);
    }
    if (ctl) XC(ctl, Release);
    if (info) XC(info, Release);
    hr = XC(b, GetDvdInterface, &IID_IMediaControl, (void **)&mc);
    CHECKF(hr == S_OK && mc, "GetDvdInterface(IMediaControl) comes from the graph (%#lx)", hr);
    if (mc) IMediaControl_Release(mc);
    unk = (IUnknown *)(DWORD_PTR)1;
    hr = XC(b, GetDvdInterface, &test_iid, (void **)&unk);
    CHECKF(hr == E_NOINTERFACE && !unk, "GetDvdInterface(unknown interface) is E_NOINTERFACE (%#lx)", hr);
    hr = XC(b, GetDvdInterface, NULL, (void **)&unk);
    CHECKF(hr == E_POINTER, "GetDvdInterface(NULL iid) is E_POINTER (%#lx)", hr);
    XC(b, Release);
}

int main(void)
{
    IBaseFilter *filter;
    XCtl *ctl;
    XInfo *info;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = CoCreateInstance(&CLSID_DVDNavigator, NULL, CLSCTX_INPROC_SERVER, &IID_IBaseFilter, (void **)&filter);
    if (hr != S_OK) { check(0, "create the navigator"); return 1; }
    hr = IBaseFilter_QueryInterface(filter, &XIID_IDvdControl2, (void **)&ctl);
    if (hr != S_OK) { check(0, "QI IDvdControl2"); return 1; }
    hr = IBaseFilter_QueryInterface(filter, &XIID_IDvdInfo2, (void **)&info);
    if (hr != S_OK) { check(0, "QI IDvdInfo2"); return 1; }
    test_control(ctl);
    test_info(info);
    test_settings(ctl, info);
    test_directory(ctl, info);
    XC(ctl, Release);
    XC(info, Release);
    IBaseFilter_Release(filter);
    test_builder();
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
