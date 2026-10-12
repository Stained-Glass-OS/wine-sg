/* IMFPluginControl (patches/sg/2967), run by test/mfplat-plugin-gate.sh. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
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

static const GUID clsid_a = {0x11111111, 0x1111, 0x1111, {1,1,1,1,1,1,1,1}};
static const GUID clsid_b = {0x22222222, 0x2222, 0x2222, {2,2,2,2,2,2,2,2}};
static const GUID clsid_c = {0x33333333, 0x3333, 0x3333, {3,3,3,3,3,3,3,3}};

static void test_preferred(IMFPluginControl *pc)
{
    static const struct { DWORD type; const WCHAR *sel; const GUID *clsid; } set[] = {
        { MF_Plugin_Type_MFT, L"video/x-test", &clsid_a },
        { MF_Plugin_Type_MFT, L"audio/x-test", &clsid_b },
        { MF_Plugin_Type_MediaSource, L"video/x-test", &clsid_c },
    };
    unsigned int i;
    WCHAR *sel;
    GUID guid;
    HRESULT hr;

    hr = IMFPluginControl_GetPreferredClsid(pc, MF_Plugin_Type_MFT, L"video/x-test", &guid);
    CHECKF(hr == MF_E_NOT_FOUND, "get unset hr %#lx", hr);
    hr = IMFPluginControl_GetPreferredClsidByIndex(pc, MF_Plugin_Type_MFT, 0, &sel, &guid);
    CHECKF(hr == MF_E_NO_MORE_TYPES, "by index empty hr %#lx", hr);

    for (i = 0; i < ARRAYSIZE(set); i++)
    {
        hr = IMFPluginControl_SetPreferredClsid(pc, set[i].type, set[i].sel, set[i].clsid);
        CHECKF(hr == S_OK, "set %u hr %#lx", i, hr);
    }
    for (i = 0; i < ARRAYSIZE(set); i++)
    {
        memset(&guid, 0, sizeof(guid));
        hr = IMFPluginControl_GetPreferredClsid(pc, set[i].type, set[i].sel, &guid);
        CHECKF(hr == S_OK && IsEqualGUID(&guid, set[i].clsid), "get %u hr %#lx", i, hr);
    }
    hr = IMFPluginControl_GetPreferredClsid(pc, MF_Plugin_Type_MFT, L"other", &guid);
    CHECKF(hr == MF_E_NOT_FOUND, "get other selector hr %#lx", hr);
    hr = IMFPluginControl_GetPreferredClsid(pc, MF_Plugin_Type_MFT, NULL, &guid);
    CHECKF(hr == E_POINTER, "get NULL selector hr %#lx", hr);
    hr = IMFPluginControl_GetPreferredClsid(pc, MF_Plugin_Type_MFT, L"x", NULL);
    CHECKF(hr == E_POINTER, "get NULL clsid hr %#lx", hr);
    hr = IMFPluginControl_GetPreferredClsid(pc, 7, L"video/x-test", &guid);
    CHECKF(hr == E_INVALIDARG, "get bad type hr %#lx", hr);

    /* enumeration of the MFT preferences: both entries, any order */
    {
        int seen_a = 0, seen_b = 0, n = 0;
        for (i = 0; i < 4; i++)
        {
            sel = NULL;
            hr = IMFPluginControl_GetPreferredClsidByIndex(pc, MF_Plugin_Type_MFT, i, &sel, &guid);
            if (hr != S_OK) break;
            n++;
            if (IsEqualGUID(&guid, &clsid_a) && !wcscmp(sel, L"video/x-test")) seen_a = 1;
            if (IsEqualGUID(&guid, &clsid_b) && !wcscmp(sel, L"audio/x-test")) seen_b = 1;
            CoTaskMemFree(sel);
        }
        CHECKF(n == 2 && seen_a && seen_b && hr == MF_E_NO_MORE_TYPES, "by index: %d entries a %d b %d hr %#lx", n, seen_a, seen_b, hr);
    }

    hr = IMFPluginControl_SetPreferredClsid(pc, MF_Plugin_Type_MFT, L"video/x-test", &clsid_c);
    memset(&guid, 0, sizeof(guid));
    IMFPluginControl_GetPreferredClsid(pc, MF_Plugin_Type_MFT, L"video/x-test", &guid);
    CHECKF(hr == S_OK && IsEqualGUID(&guid, &clsid_c), "overwrite hr %#lx", hr);

    for (i = 0; i < ARRAYSIZE(set); i++)
    {
        hr = IMFPluginControl_SetPreferredClsid(pc, set[i].type, set[i].sel, NULL);
        CHECKF(hr == S_OK, "clear %u hr %#lx", i, hr);
        hr = IMFPluginControl_GetPreferredClsid(pc, set[i].type, set[i].sel, &guid);
        CHECKF(hr == MF_E_NOT_FOUND, "get cleared %u hr %#lx", i, hr);
    }
}

static void test_disabled(IMFPluginControl *pc)
{
    GUID guid;
    HRESULT hr;
    int i;

    hr = IMFPluginControl_IsDisabled(pc, MF_Plugin_Type_MFT, &clsid_a);
    CHECKF(hr == S_FALSE, "not disabled hr %#lx", hr);
    hr = IMFPluginControl_GetDisabledByIndex(pc, MF_Plugin_Type_MFT, 0, &guid);
    CHECKF(hr == MF_E_NO_MORE_TYPES, "disabled by index empty hr %#lx", hr);

    hr = IMFPluginControl_SetDisabled(pc, MF_Plugin_Type_MFT, &clsid_a, TRUE);
    CHECKF(hr == S_OK, "disable a hr %#lx", hr);
    hr = IMFPluginControl_SetDisabled(pc, MF_Plugin_Type_MFT, &clsid_b, TRUE);
    CHECKF(hr == S_OK, "disable b hr %#lx", hr);
    hr = IMFPluginControl_IsDisabled(pc, MF_Plugin_Type_MFT, &clsid_a);
    CHECKF(hr == S_OK, "a disabled hr %#lx", hr);
    hr = IMFPluginControl_IsDisabled(pc, MF_Plugin_Type_MFT, &clsid_c);
    CHECKF(hr == S_FALSE, "c not disabled hr %#lx", hr);
    hr = IMFPluginControl_IsDisabled(pc, MF_Plugin_Type_MediaSource, &clsid_a);
    CHECKF(hr == S_FALSE, "a not disabled as media source hr %#lx", hr);
    hr = IMFPluginControl_IsDisabled(pc, 5, &clsid_a);
    CHECKF(hr == E_INVALIDARG, "bad type hr %#lx", hr);

    {
        int seen_a = 0, seen_b = 0, n = 0;
        for (i = 0; i < 4; i++)
        {
            hr = IMFPluginControl_GetDisabledByIndex(pc, MF_Plugin_Type_MFT, i, &guid);
            if (hr != S_OK) break;
            n++;
            seen_a |= IsEqualGUID(&guid, &clsid_a);
            seen_b |= IsEqualGUID(&guid, &clsid_b);
        }
        CHECKF(n == 2 && seen_a && seen_b && hr == MF_E_NO_MORE_TYPES, "disabled by index: %d entries hr %#lx", n, hr);
    }

    hr = IMFPluginControl_SetDisabled(pc, MF_Plugin_Type_MFT, &clsid_a, FALSE);
    CHECKF(hr == S_OK, "enable a hr %#lx", hr);
    hr = IMFPluginControl_IsDisabled(pc, MF_Plugin_Type_MFT, &clsid_a);
    CHECKF(hr == S_FALSE, "a enabled again hr %#lx", hr);
    IMFPluginControl_SetDisabled(pc, MF_Plugin_Type_MFT, &clsid_b, FALSE);
    hr = IMFPluginControl_GetDisabledByIndex(pc, MF_Plugin_Type_MFT, 0, &guid);
    CHECKF(hr == MF_E_NO_MORE_TYPES, "all enabled again hr %#lx", hr);
}

int main(void)
{
    IMFPluginControl *pc, *pc2;
    IUnknown *unk;
    HRESULT hr;

    CoInitialize(NULL);
    hr = MFGetPluginControl(&pc);
    CHECKF(hr == S_OK && pc, "MFGetPluginControl hr %#lx", hr);
    if (hr != S_OK) return 1;
    hr = IMFPluginControl_QueryInterface(pc, &IID_IMFPluginControl, (void **)&pc2);
    CHECKF(hr == S_OK, "QI IMFPluginControl hr %#lx", hr);
    if (hr == S_OK) IMFPluginControl_Release(pc2);
    hr = IMFPluginControl_QueryInterface(pc, &IID_IMFAttributes, (void **)&unk);
    CHECKF(hr == E_NOINTERFACE && !unk, "QI IMFAttributes hr %#lx", hr);

    test_preferred(pc);
    test_disabled(pc);

    IMFPluginControl_Release(pc);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
