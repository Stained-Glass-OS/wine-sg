/* Probe for patches/sg/5047: D3DKMTOpenAdapterFromHdc opens the adapter of a
 * DC's display: a zeroed description fails STATUS_INVALID_PARAMETER, a DC
 * made for each attached display opens it (same LUID as
 * D3DKMTOpenAdapterFromGdiDisplayName), the desktop DC opens it when there is
 * one display and fails when there are several (gdi32 driver tests, run on
 * Windows). It returned STATUS_NO_MEMORY.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <winternl.h>

/* ddk/d3dkmthk.h (not in mingw's headers) */
typedef UINT D3DKMT_HANDLE;
typedef struct { HDC hDc; D3DKMT_HANDLE hAdapter; LUID AdapterLuid; UINT VidPnSourceId; } D3DKMT_OPENADAPTERFROMHDC;
typedef struct { WCHAR DeviceName[32]; D3DKMT_HANDLE hAdapter; LUID AdapterLuid; UINT VidPnSourceId; } D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME;
typedef struct { D3DKMT_HANDLE hAdapter; } D3DKMT_CLOSEADAPTER;
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#define STATUS_INVALID_PARAMETER_ ((NTSTATUS)0xc000000d)

static NTSTATUS (WINAPI *pOpenFromHdc)(D3DKMT_OPENADAPTERFROMHDC *);
static NTSTATUS (WINAPI *pOpenFromName)(D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME *);
static NTSTATUS (WINAPI *pClose)(const D3DKMT_CLOSEADAPTER *);

int main(void)
{
    HMODULE gdi32 = GetModuleHandleA("gdi32.dll");
    D3DKMT_OPENADAPTERFROMHDC desc;
    DISPLAY_DEVICEW dd = {sizeof(dd)};
    UINT i, count = 0;
    NTSTATUS st;
    HDC hdc;

    pOpenFromHdc = (void *)GetProcAddress(gdi32, "D3DKMTOpenAdapterFromHdc");
    pOpenFromName = (void *)GetProcAddress(gdi32, "D3DKMTOpenAdapterFromGdiDisplayName");
    pClose = (void *)GetProcAddress(gdi32, "D3DKMTCloseAdapter");
    check(pOpenFromHdc && pOpenFromName && pClose, "gdi32 exports");
    if (!pOpenFromHdc) { printf("RESULT: FAIL\n"); return 1; }

    memset(&desc, 0, sizeof(desc));
    st = pOpenFromHdc(&desc);
    check(st == STATUS_INVALID_PARAMETER_, "zeroed description: %#lx", st);

    for (i = 0; EnumDisplayDevicesW(NULL, i, &dd, 0); i++)
    {
        D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME byname;
        D3DKMT_CLOSEADAPTER close_desc;

        if (!(dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)) continue;
        count++;
        hdc = CreateDCW(NULL, dd.DeviceName, NULL, NULL);
        memset(&desc, 0, sizeof(desc));
        desc.hDc = hdc;
        st = pOpenFromHdc(&desc);
        memset(&byname, 0, sizeof(byname));
        lstrcpyW(byname.DeviceName, dd.DeviceName);
        pOpenFromName(&byname);
        check(st == 0 && desc.hAdapter && !memcmp(&desc.AdapterLuid, &byname.AdapterLuid, sizeof(LUID))
              && desc.VidPnSourceId == byname.VidPnSourceId, "%ls: %#lx adapter %#x luid %08lx:%08lx (by name %08lx:%08lx), source %u/%u",
              dd.DeviceName, st, desc.hAdapter, desc.AdapterLuid.HighPart, desc.AdapterLuid.LowPart,
              byname.AdapterLuid.HighPart, byname.AdapterLuid.LowPart, desc.VidPnSourceId, byname.VidPnSourceId);
        if (!st) { close_desc.hAdapter = desc.hAdapter; check(!pClose(&close_desc), "%ls: closes", dd.DeviceName); }
        if (byname.hAdapter) { close_desc.hAdapter = byname.hAdapter; pClose(&close_desc); }
        DeleteDC(hdc);
    }
    check(count >= 1, "%u attached displays", count);

    hdc = GetDC(0);
    memset(&desc, 0, sizeof(desc));
    desc.hDc = hdc;
    st = pOpenFromHdc(&desc);
    ReleaseDC(0, hdc);
    check(st == (count > 1 ? STATUS_INVALID_PARAMETER_ : 0), "the desktop DC over %u displays: %#lx", count, st);
    if (!st) { D3DKMT_CLOSEADAPTER c = {desc.hAdapter}; pClose(&c); }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
