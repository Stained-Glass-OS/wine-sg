/* A fake graphics driver tablet interface for the wintab32 gate (patch 3652).  The probe loads it as
   the display driver of wintab32 only (the registry value is restored right after). */
#include <windows.h>

typedef struct { UINT pkContext, pkStatus; LONG pkTime; DWORD pkChanged; UINT pkSerialNumber, pkCursor;
    DWORD pkButtons, pkX, pkY, pkZ; UINT pkNormalPressure, pkTangentPressure; int ori[3]; int rot[3]; } PKT;
typedef struct { LONG axMin, axMax; UINT axUnits; LONG axRes; } AX;

static PKT current;
static HWND def_hwnd;
static volatile LONG hook1_count, hook2_count;
static volatile UINT hook1_last_buttons;
static LRESULT (WINAPI *hook_next)(HANDLE, int, WPARAM, LPARAM);
static HANDLE hook2_handle;

__declspec(dllexport) int CDECL LoadTabletInfo(HWND h) { def_hwnd = h; return 1; }
__declspec(dllexport) int CDECL AttachEventQueueToTablet(HWND h) { return 1; }
__declspec(dllexport) int CDECL GetCurrentPacket(PKT *p) { *p = current; return 1; }

__declspec(dllexport) void CDECL FakeSetPacket(UINT cursor, DWORD buttons, UINT np, UINT tp)
{
    static UINT serial;
    memset(&current, 0, sizeof(current));
    current.pkCursor = cursor; current.pkButtons = buttons; current.pkNormalPressure = np;
    current.pkTangentPressure = tp; current.pkSerialNumber = ++serial;
    current.pkChanged = 0xffffffff;
}
__declspec(dllexport) HWND CDECL FakeDefaultWindow(void) { return def_hwnd; }

__declspec(dllexport) UINT CDECL WTInfoW(UINT cat, UINT idx, void *out)
{
    if (cat == 1 && idx == 5) { if (out) *(UINT *)out = 2; return sizeof(UINT); }       /* IFC_NCURSORS */
    if (cat == 100 && (idx == 15 || idx == 16))                                          /* DVC_NPRESSURE / TPRESSURE */
    {
        AX a = { 0, 1023, 0, 0 };
        if (out) *(AX *)out = a;
        return sizeof(a);
    }
    if (cat >= 200 && cat < 202)
    {
        if (idx == 7)   /* CSR_BUTTONMAP: identity */
        {
            if (out) { int i; for (i = 0; i < 32; i++) ((BYTE *)out)[i] = i; }
            return 32;
        }
        if (idx == 9)  { if (out) *(BYTE *)out = 1; return 1; }   /* CSR_NPBUTTON */
        if (idx == 12) { if (out) *(BYTE *)out = 2; return 1; }   /* CSR_TPBUTTON */
    }
    return 0;
}

/* record hooks */
__declspec(dllexport) LRESULT WINAPI RecHook(int code, WPARAM wp, LPARAM lp)
{
    InterlockedIncrement(&hook1_count);
    hook1_last_buttons = ((PKT *)lp)->pkButtons;
    return 1;
}
__declspec(dllexport) LRESULT WINAPI RecHook2(int code, WPARAM wp, LPARAM lp)
{
    LRESULT r;
    InterlockedIncrement(&hook2_count);
    r = hook_next ? hook_next(hook2_handle, code, wp, lp) : 0;
    return r + 10;
}
__declspec(dllexport) void CDECL FakeSetNext(void *fn, HANDLE h) { hook_next = fn; hook2_handle = h; }
__declspec(dllexport) LONG CDECL FakeHook1Count(void) { return hook1_count; }
__declspec(dllexport) LONG CDECL FakeHook2Count(void) { return hook2_count; }
__declspec(dllexport) UINT CDECL FakeHook1Buttons(void) { return hook1_last_buttons; }
