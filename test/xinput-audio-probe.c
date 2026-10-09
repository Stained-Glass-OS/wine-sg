/* xinput's audio device functions (patches/sg/2835), run by
 * test/xinput-audio-gate.sh with a virtual gamepad (uinput) connected as user 0:
 * XInputGetDSoundAudioDeviceGuids (xinput1_3, ERROR_NOT_SUPPORTED and a FIXME
 * before) and XInputGetAudioDeviceIds (xinput1_4, an "@ stub" export). A
 * controller without a headset has no sound device: GUID_NULL and empty ids.
 *
 *   xinput-audio-probe.exe */
#include <windows.h>
#include <xinput.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[256]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

typedef DWORD (WINAPI *guids_fn)(DWORD, GUID *, GUID *);
typedef DWORD (WINAPI *ids_fn)(DWORD, WCHAR *, UINT *, WCHAR *, UINT *);

int main(void)
{
    HMODULE x13 = LoadLibraryA("xinput1_3.dll"), x14 = LoadLibraryA("xinput1_4.dll");
    guids_fn get_guids = x13 ? (guids_fn)GetProcAddress(x13, "XInputGetDSoundAudioDeviceGuids") : NULL;
    ids_fn get_ids = x14 ? (ids_fn)GetProcAddress(x14, "XInputGetAudioDeviceIds") : NULL;
    DWORD (WINAPI *get_state)(DWORD, XINPUT_STATE *) = x14 ? (void *)GetProcAddress(x14, "XInputGetState") : NULL;
    DWORD (WINAPI *get_state13)(DWORD, XINPUT_STATE *) = x13 ? (void *)GetProcAddress(x13, "XInputGetState") : NULL;
    XINPUT_STATE state;
    GUID render, capture;
    WCHAR rid[8] = L"xxxxxxx", cid[8] = L"yyyyyyy";
    UINT rcount, ccount;
    DWORD ret;
    int i;

    check(get_guids != NULL, "xinput1_3 exports XInputGetDSoundAudioDeviceGuids");
    check(get_ids != NULL, "xinput1_4 exports XInputGetAudioDeviceIds");
    if (!get_guids || !get_ids || !get_state) return 1;

    for (i = 0; i < 300; i++)
    {
        memset(&state, 0, sizeof(state));
        if (get_state(0, &state) == ERROR_SUCCESS) break;
        Sleep(100);
    }
    if (i == 300)
    {
        printf("note  no XInput controller appeared as user 0: only the error paths are checked\n");
    }
    else check(1, "an XInput controller is connected as user 0");
    /* each xinput DLL tracks the controllers itself: xinput1_3 must see it too */
    for (i = 0; get_state13 && i < 300 && get_state13(0, &state) != ERROR_SUCCESS; i++) Sleep(100);

    ret = get_guids(XUSER_MAX_COUNT, &render, &capture);
    CHECKF(ret == ERROR_BAD_ARGUMENTS, "guids: user %u is ERROR_BAD_ARGUMENTS (%lu)", XUSER_MAX_COUNT, ret);
    ret = get_guids(3, &render, &capture);
    CHECKF(ret == ERROR_DEVICE_NOT_CONNECTED, "guids: user 3 is ERROR_DEVICE_NOT_CONNECTED (%lu)", ret);
    ret = get_ids(XUSER_MAX_COUNT, rid, &rcount, cid, &ccount);
    CHECKF(ret == ERROR_BAD_ARGUMENTS, "ids: user %u is ERROR_BAD_ARGUMENTS (%lu)", XUSER_MAX_COUNT, ret);
    rcount = ccount = 8;
    ret = get_ids(3, rid, &rcount, cid, &ccount);
    CHECKF(ret == ERROR_DEVICE_NOT_CONNECTED, "ids: user 3 is ERROR_DEVICE_NOT_CONNECTED (%lu)", ret);

    if (i < 300 && get_state13)
    {
        memset(&render, 0xaa, sizeof(render));
        memset(&capture, 0xaa, sizeof(capture));
        ret = get_guids(0, &render, &capture);
        CHECKF(ret == ERROR_SUCCESS && IsEqualGUID(&render, &GUID_NULL) && IsEqualGUID(&capture, &GUID_NULL),
               "guids: a controller without a headset has GUID_NULL devices (%lu)", ret);
        ret = get_guids(0, NULL, &capture);
        CHECKF(ret == ERROR_BAD_ARGUMENTS, "guids: a NULL render guid is ERROR_BAD_ARGUMENTS (%lu)", ret);

        rcount = ccount = 8;
        ret = get_ids(0, rid, &rcount, cid, &ccount);
        CHECKF(ret == ERROR_SUCCESS && rcount == 0 && ccount == 0 && !rid[0] && !cid[0],
               "ids: no headset gives empty ids of length 0 (%lu, %u, %u)", ret, rcount, ccount);
        ret = get_ids(0, NULL, NULL, NULL, NULL);
        CHECKF(ret == ERROR_SUCCESS, "ids: counts may be left out (%lu)", ret);
    }
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
