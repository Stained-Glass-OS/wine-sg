/* dinput's IDirectInputJoyConfig8 methods and force feedback effect files (patches
 * 2876..2879), run by test/dinputleft-gate.sh.  Only what the interface documentation
 * fixes is checked: the cooperative level / acquire state machine, argument validation,
 * and that what is written (types, configurations, user values, effects) reads back.
 *
 *   dinputleft-probe.exe */
#define COBJMACROS
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <initguid.h>
#include <dinput.h>
#include <dinputd.h>
#include <stdio.h>
#include <string.h>

DEFINE_GUID(IID_IDirectInputJoyConfig8_sg, 0xeb0d7dfa, 0x1990, 0x4f27, 0xb4, 0xd6, 0xed, 0xf2, 0xee, 0xc4, 0xa4, 0x4c);


static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[256]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)
#define HR(call, expected) do { HRESULT _hr = (call); CHECKF(_hr == (expected), "%s = %#lx (expected %#lx)", #call, _hr, (HRESULT)(expected)); } while (0)

struct enum_ctx { int found, stop_after, calls; };
static BOOL CALLBACK enum_types(LPCWSTR name, void *ctx)
{
    struct enum_ctx *c = ctx;
    c->calls++;
    if (!wcscmp(name, L"SG_TEST_TYPE")) c->found++;
    return c->stop_after && c->calls >= c->stop_after ? FALSE : TRUE;
}

static void test_joyconfig(HINSTANCE inst, HWND hwnd)
{
    IDirectInput8W *di8;
    IDirectInputJoyConfig8 *cfg;
    DIJOYTYPEINFO ti, out;
    DIJOYCONFIG jc, jo;
    DIJOYUSERVALUES uv, uo;
    struct enum_ctx ctx = {0};
    WCHAR new_name[MAX_JOYSTRING];
    HKEY key;
    HRESULT hr;
    LONG ret;

    hr = DirectInput8Create(inst, DIRECTINPUT_VERSION, &IID_IDirectInput8W, (void **)&di8, NULL);
    CHECKF(hr == S_OK, "DirectInput8Create (%#lx)", hr);
    if (hr != S_OK) return;
    hr = IDirectInput8_QueryInterface(di8, &IID_IDirectInputJoyConfig8_sg, (void **)&cfg);
    CHECKF(hr == S_OK, "QueryInterface JoyConfig8 (%#lx)", hr);
    if (hr != S_OK) { IDirectInput8_Release(di8); return; }

    /* state machine */
    HR(IDirectInputJoyConfig8_Acquire(cfg), DIERR_INVALIDPARAM);
    HR(IDirectInputJoyConfig8_Unacquire(cfg), DIERR_NOTACQUIRED);
    HR(IDirectInputJoyConfig8_SetCooperativeLevel(cfg, hwnd, 0), DIERR_INVALIDPARAM);
    HR(IDirectInputJoyConfig8_SetCooperativeLevel(cfg, hwnd, DISCL_EXCLUSIVE), DIERR_INVALIDPARAM);
    HR(IDirectInputJoyConfig8_SetCooperativeLevel(cfg, hwnd, DISCL_BACKGROUND), DIERR_INVALIDPARAM);
    HR(IDirectInputJoyConfig8_SetCooperativeLevel(cfg, hwnd, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND), DIERR_INVALIDPARAM);
    HR(IDirectInputJoyConfig8_Acquire(cfg), DIERR_INVALIDPARAM);

    memset(&ti, 0, sizeof(ti));
    ti.dwSize = sizeof(ti);
    wcscpy(ti.wszDisplayName, L"Sg Test Stick");
    wcscpy(ti.wszCallout, L"sgcallout.dll");
    wcscpy(ti.wszHardwareId, L"HID\\VID_1234&PID_5678");
    wcscpy(ti.wszMapFile, L"sgmap.ini");
    ti.hws.dwFlags = 3;
    ti.hws.dwNumButtons = 12;
    ti.dwFlags1 = 0x11223344;
    ti.dwFlags2 = 0x55667788;
    ti.clsidConfig = IID_IDirectInputJoyConfig8_sg;
    HR(IDirectInputJoyConfig8_SetTypeInfo(cfg, L"SG_TEST_TYPE", &ti, DITC_DISPLAYNAME, new_name), DIERR_NOTACQUIRED);
    HR(IDirectInputJoyConfig8_SetConfig(cfg, 7, NULL, 0), DIERR_INVALIDPARAM);
    HR(IDirectInputJoyConfig8_DeleteType(cfg, L"SG_TEST_TYPE"), DIERR_NOTACQUIRED);
    HR(IDirectInputJoyConfig8_SendNotify(cfg), DIERR_NOTACQUIRED);

    HR(IDirectInputJoyConfig8_SetCooperativeLevel(cfg, hwnd, DISCL_EXCLUSIVE | DISCL_BACKGROUND), DI_OK);
    HR(IDirectInputJoyConfig8_Acquire(cfg), DI_OK);
    HR(IDirectInputJoyConfig8_SetCooperativeLevel(cfg, hwnd, DISCL_EXCLUSIVE | DISCL_BACKGROUND), DIERR_ACQUIRED);
    HR(IDirectInputJoyConfig8_SendNotify(cfg), DI_OK);

    /* OEM types */
    memset(&out, 0, sizeof(out));
    HR(IDirectInputJoyConfig8_GetTypeInfo(cfg, L"SG_TEST_TYPE", &out, DITC_DISPLAYNAME), DIERR_INVALIDPARAM); /* dwSize is 0 */
    out.dwSize = sizeof(out);
    HR(IDirectInputJoyConfig8_GetTypeInfo(cfg, L"SG_TEST_TYPE", &out, DITC_DISPLAYNAME), DIERR_NOTFOUND);
    HR(IDirectInputJoyConfig8_GetTypeInfo(cfg, NULL, &out, DITC_DISPLAYNAME), DIERR_INVALIDPARAM);
    HR(IDirectInputJoyConfig8_GetTypeInfo(cfg, L"SG_TEST_TYPE", NULL, DITC_DISPLAYNAME), DIERR_INVALIDPARAM);
    HR(IDirectInputJoyConfig8_SetTypeInfo(cfg, L"SG_TEST_TYPE", &ti, 0x100, NULL), DIERR_INVALIDPARAM);
    HR(IDirectInputJoyConfig8_EnumTypes(cfg, NULL, NULL), DIERR_INVALIDPARAM);
    HR(IDirectInputJoyConfig8_EnumTypes(cfg, enum_types, &ctx), DI_OK);
    CHECKF(ctx.found == 0, "type not listed before it exists (%d)", ctx.found);

    new_name[0] = 0;
    HR(IDirectInputJoyConfig8_SetTypeInfo(cfg, L"SG_TEST_TYPE", &ti, DITC_REGHWSETTINGS | DITC_CLSIDCONFIG | DITC_DISPLAYNAME |
                                          DITC_CALLOUT | DITC_HARDWAREID | DITC_FLAGS1 | DITC_FLAGS2 | DITC_MAPFILE, new_name), DI_OK);
    memset(&out, 0xcc, sizeof(out));
    out.dwSize = sizeof(out);
    HR(IDirectInputJoyConfig8_GetTypeInfo(cfg, L"SG_TEST_TYPE", &out, DITC_REGHWSETTINGS | DITC_CLSIDCONFIG | DITC_DISPLAYNAME |
                                          DITC_CALLOUT | DITC_HARDWAREID | DITC_FLAGS1 | DITC_FLAGS2 | DITC_MAPFILE), DI_OK);
    CHECKF(!wcscmp(out.wszDisplayName, ti.wszDisplayName), "display name read back");
    CHECKF(!wcscmp(out.wszCallout, ti.wszCallout), "callout read back");
    CHECKF(!wcscmp(out.wszHardwareId, ti.wszHardwareId), "hardware id read back");
    CHECKF(!wcscmp(out.wszMapFile, ti.wszMapFile), "map file read back");
    CHECKF(out.hws.dwFlags == 3 && out.hws.dwNumButtons == 12, "hws read back (%lu %lu)", out.hws.dwFlags, out.hws.dwNumButtons);
    CHECKF(out.dwFlags1 == 0x11223344, "flags1 read back (%#lx)", out.dwFlags1);
    CHECKF(out.dwFlags2 == 0x55667788, "flags2 read back (%#lx)", out.dwFlags2);
    CHECKF(IsEqualGUID(&out.clsidConfig, &ti.clsidConfig), "clsid read back");

    /* only the flagged members are written / read */
    memset(&jc, 0, sizeof(jc));
    wcscpy(ti.wszDisplayName, L"Renamed");
    HR(IDirectInputJoyConfig8_SetTypeInfo(cfg, L"SG_TEST_TYPE", &ti, DITC_DISPLAYNAME, NULL), DI_OK);
    memset(&out, 0xcc, sizeof(out));
    out.dwSize = sizeof(out);
    HR(IDirectInputJoyConfig8_GetTypeInfo(cfg, L"SG_TEST_TYPE", &out, DITC_DISPLAYNAME | DITC_CALLOUT), DI_OK);
    CHECKF(!wcscmp(out.wszDisplayName, L"Renamed") && !wcscmp(out.wszCallout, L"sgcallout.dll"), "partial update keeps other members");
    CHECKF(out.dwFlags1 == 0xcccccccc, "unrequested members are not written (%#lx)", out.dwFlags1);

    ctx.calls = ctx.found = 0;
    HR(IDirectInputJoyConfig8_EnumTypes(cfg, enum_types, &ctx), DI_OK);
    CHECKF(ctx.found == 1, "type listed once (%d)", ctx.found);
    HR(IDirectInputJoyConfig8_SetTypeInfo(cfg, L"SG_TEST_TYPE_B", &ti, DITC_DISPLAYNAME, NULL), DI_OK);
    ctx.calls = ctx.found = 0;
    HR(IDirectInputJoyConfig8_EnumTypes(cfg, enum_types, &ctx), DI_OK);
    CHECKF(ctx.calls >= 2, "both types listed (%d)", ctx.calls);
    ctx.calls = ctx.found = 0; ctx.stop_after = 1;
    HR(IDirectInputJoyConfig8_EnumTypes(cfg, enum_types, &ctx), DI_OK);
    CHECKF(ctx.calls == 1, "EnumTypes stops when the callback returns FALSE (%d)", ctx.calls);
    HR(IDirectInputJoyConfig8_DeleteType(cfg, L"SG_TEST_TYPE_B"), DI_OK);

    ret = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Control\\MediaProperties\\PrivateProperties\\Joystick\\OEM\\SG_TEST_TYPE",
                        0, KEY_READ, &key);
    CHECKF(ret == ERROR_SUCCESS, "documented OEM registry key exists (%ld)", ret);
    if (!ret) RegCloseKey(key);
    HR(IDirectInputJoyConfig8_OpenTypeKey(cfg, L"SG_TEST_TYPE", KEY_READ, &key), DI_OK);
    if (hr == DI_OK) RegCloseKey(key);
    HR(IDirectInputJoyConfig8_OpenTypeKey(cfg, L"SG_NO_SUCH_TYPE", KEY_READ, &key), DIERR_NOTFOUND);
    HR(IDirectInputJoyConfig8_OpenTypeKey(cfg, NULL, KEY_READ, &key), DIERR_INVALIDPARAM);
    HR(IDirectInputJoyConfig8_OpenTypeKey(cfg, L"SG_TEST_TYPE", KEY_READ, NULL), DIERR_INVALIDPARAM);

    HR(IDirectInputJoyConfig8_DeleteType(cfg, L"SG_TEST_TYPE"), DI_OK);
    HR(IDirectInputJoyConfig8_GetTypeInfo(cfg, L"SG_TEST_TYPE", &out, DITC_DISPLAYNAME), DIERR_NOTFOUND);
    HR(IDirectInputJoyConfig8_DeleteType(cfg, L"SG_TEST_TYPE"), DIERR_NOTFOUND);
    ctx.calls = ctx.found = ctx.stop_after = 0;
    HR(IDirectInputJoyConfig8_EnumTypes(cfg, enum_types, &ctx), DI_OK);
    CHECKF(ctx.found == 0, "deleted type not listed (%d)", ctx.found);

    /* per id configuration (ids past the attached devices only exist once stored) */
    memset(&jc, 0, sizeof(jc));
    jc.dwSize = sizeof(jc);
    jc.guidInstance = IID_IDirectInputJoyConfig8_sg;
    jc.guidGameport = GUID_SysMouse;
    jc.dwGain = 4321;
    jc.hwc.dwType = 5;
    jc.hwc.hws.dwNumButtons = 9;
    wcscpy(jc.wszType, L"SG_TEST_TYPE");
    wcscpy(jc.wszCallout, L"sgcallout2.dll");
    jo.dwSize = sizeof(jo);
    HR(IDirectInputJoyConfig8_GetConfig(cfg, 7, &jo, DIJC_GAIN), DIERR_NOMOREITEMS);
    jo.dwSize = 12;
    HR(IDirectInputJoyConfig8_GetConfig(cfg, 7, &jo, DIJC_GAIN), DIERR_INVALIDPARAM);
    jo.dwSize = sizeof(jo);
    HR(IDirectInputJoyConfig8_GetConfig(cfg, 7, &jo, 0x100), DIERR_INVALIDPARAM);
    HR(IDirectInputJoyConfig8_SetConfig(cfg, 7, &jc, 0x100), DIERR_INVALIDPARAM);
    HR(IDirectInputJoyConfig8_SetConfig(cfg, 7, &jc, DIJC_GUIDINSTANCE | DIJC_REGHWCONFIGTYPE | DIJC_GAIN | DIJC_CALLOUT | DIJC_WDMGAMEPORT), DI_OK);
    memset(&jo, 0xcc, sizeof(jo));
    jo.dwSize = sizeof(jo);
    HR(IDirectInputJoyConfig8_GetConfig(cfg, 7, &jo, DIJC_GAIN | DIJC_CALLOUT), DI_OK);
    CHECKF(jo.dwGain == 4321, "gain read back (%lu)", jo.dwGain);
    CHECKF(!wcscmp(jo.wszCallout, L"sgcallout2.dll"), "callout read back");
    CHECKF(jo.hwc.dwType == 0xcccccccc, "unrequested hwc untouched");
    HR(IDirectInputJoyConfig8_GetConfig(cfg, 7, &jo, DIJC_GUIDINSTANCE | DIJC_REGHWCONFIGTYPE | DIJC_WDMGAMEPORT), DI_OK);
    CHECKF(IsEqualGUID(&jo.guidInstance, &jc.guidInstance), "guidInstance read back");
    CHECKF(IsEqualGUID(&jo.guidGameport, &jc.guidGameport), "guidGameport read back");
    CHECKF(jo.hwc.dwType == 5 && jo.hwc.hws.dwNumButtons == 9 && !wcscmp(jo.wszType, L"SG_TEST_TYPE"), "hw config and type read back");
    HR(IDirectInputJoyConfig8_DeleteConfig(cfg, 7), DI_OK);
    HR(IDirectInputJoyConfig8_GetConfig(cfg, 7, &jo, DIJC_GAIN), DIERR_NOMOREITEMS);

    /* user values */
    memset(&uv, 0, sizeof(uv));
    uv.dwSize = sizeof(uv);
    uv.ruv.dwTimeOut = 777;
    uv.ruv.jrvRanges.jpMax.dwX = 1000;
    wcscpy(uv.wszGlobalDriver, L"sgdriver.dll");
    wcscpy(uv.wszGameportEmulator, L"sgemu.dll");
    HR(IDirectInputJoyConfig8_SetUserValues(cfg, &uv, 0x100), DIERR_INVALIDPARAM);
    HR(IDirectInputJoyConfig8_SetUserValues(cfg, &uv, DIJU_USERVALUES | DIJU_GLOBALDRIVER | DIJU_GAMEPORTEMULATOR), DI_OK);
    memset(&uo, 0xcc, sizeof(uo));
    uo.dwSize = sizeof(uo);
    HR(IDirectInputJoyConfig8_GetUserValues(cfg, &uo, DIJU_USERVALUES | DIJU_GLOBALDRIVER | DIJU_GAMEPORTEMULATOR), DI_OK);
    CHECKF(uo.ruv.dwTimeOut == 777 && uo.ruv.jrvRanges.jpMax.dwX == 1000, "user values read back");
    CHECKF(!wcscmp(uo.wszGlobalDriver, L"sgdriver.dll") && !wcscmp(uo.wszGameportEmulator, L"sgemu.dll"), "driver strings read back");
    uo.dwSize = 8;
    HR(IDirectInputJoyConfig8_GetUserValues(cfg, &uo, DIJU_USERVALUES), DIERR_INVALIDPARAM);

    HR(IDirectInputJoyConfig8_AddNewHardware(cfg, hwnd, NULL), DIERR_INVALIDPARAM);
    HR(IDirectInputJoyConfig8_OpenAppStatusKey(cfg, NULL), DIERR_INVALIDPARAM);
    HR(IDirectInputJoyConfig8_OpenAppStatusKey(cfg, &key), DI_OK);
    if (hr == DI_OK)
    {
        DWORD v = 1;
        ret = RegSetValueExW(key, L"SgTest", 0, REG_DWORD, (BYTE *)&v, sizeof(v));
        CHECKF(ret == ERROR_SUCCESS, "app status key is writable (%ld)", ret);
        RegDeleteValueW(key, L"SgTest");
        RegCloseKey(key);
    }

    HR(IDirectInputJoyConfig8_Unacquire(cfg), DI_OK);
    HR(IDirectInputJoyConfig8_Unacquire(cfg), DIERR_NOTACQUIRED);
    HR(IDirectInputJoyConfig8_SetConfig(cfg, 7, &jc, DIJC_GAIN), DIERR_NOTACQUIRED);
    HR(IDirectInputJoyConfig8_DeleteConfig(cfg, 7), DIERR_NOTACQUIRED);
    HR(IDirectInputJoyConfig8_SetUserValues(cfg, &uv, DIJU_USERVALUES), DIERR_NOTACQUIRED);
    HR(IDirectInputJoyConfig8_OpenTypeKey(cfg, L"SG_TEST_TYPE", KEY_ALL_ACCESS, &key), DIERR_NOTACQUIRED);

    IDirectInputJoyConfig8_Release(cfg);
    IDirectInput8_Release(di8);
}

struct file_ctx { int calls, stop_after; DIFILEEFFECT seen[4]; DIEFFECT eff[4]; DIENVELOPE env[4]; LONG dirs[4][2]; DWORD axes[4][2]; BYTE params[4][16]; };
static BOOL CALLBACK enum_file(const DIFILEEFFECT *fe, void *ref)
{
    struct file_ctx *c = ref;
    int i = c->calls++;
    if (i < 4)
    {
        const DIEFFECT *e = fe->lpDiEffect;
        c->seen[i] = *fe;
        c->eff[i] = *e;
        if (e->lpEnvelope) c->env[i] = *e->lpEnvelope;
        if (e->rglDirection) memcpy(c->dirs[i], e->rglDirection, e->cAxes * sizeof(LONG));
        if (e->rgdwAxes) memcpy(c->axes[i], e->rgdwAxes, e->cAxes * sizeof(DWORD));
        if (e->cbTypeSpecificParams) memcpy(c->params[i], e->lpvTypeSpecificParams, e->cbTypeSpecificParams);
    }
    return c->stop_after && c->calls >= c->stop_after ? FALSE : TRUE;
}

static void test_effect_files(HINSTANCE inst)
{
    IDirectInput8W *di8;
    IDirectInputDevice8W *dev;
    DIFILEEFFECT fe[2];
    DIEFFECT e1, e2;
    DIENVELOPE env = {sizeof(DIENVELOPE), 100, 200, 300, 400};
    DICONSTANTFORCE cf = {-1234};
    DWORD axes[2] = {0x00000100, 0x00000200};
    LONG dirs[2] = {4500, 9000};
    WCHAR path[MAX_PATH], bad[MAX_PATH];
    struct file_ctx c;
    HANDLE file;
    DWORD written;
    HRESULT hr;

    hr = DirectInput8Create(inst, DIRECTINPUT_VERSION, &IID_IDirectInput8W, (void **)&di8, NULL);
    CHECKF(hr == S_OK, "DirectInput8Create (%#lx)", hr);
    if (hr != S_OK) return;
    hr = IDirectInput8_CreateDevice(di8, &GUID_SysKeyboard, &dev, NULL);
    CHECKF(hr == S_OK, "CreateDevice (%#lx)", hr);
    if (hr != S_OK) { IDirectInput8_Release(di8); return; }

    GetTempPathW(MAX_PATH, path);
    wcscat(path, L"sgtest.ffe");
    wcscpy(bad, path); wcscat(bad, L".bad");

    memset(&e1, 0, sizeof(e1));
    e1.dwSize = sizeof(e1);
    e1.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
    e1.dwDuration = 5000000;
    e1.dwSamplePeriod = 11;
    e1.dwGain = 7777;
    e1.dwTriggerButton = 3;
    e1.dwTriggerRepeatInterval = 99;
    e1.cAxes = 2;
    e1.rgdwAxes = axes;
    e1.rglDirection = dirs;
    e1.lpEnvelope = &env;
    e1.cbTypeSpecificParams = sizeof(cf);
    e1.lpvTypeSpecificParams = &cf;
    e1.dwStartDelay = 42;
    memset(&e2, 0, sizeof(e2));
    e2.dwSize = sizeof(DIEFFECT_DX5);
    e2.dwDuration = INFINITE;
    e2.dwGain = 1;

    memset(fe, 0, sizeof(fe));
    fe[0].dwSize = sizeof(DIFILEEFFECT);
    fe[0].GuidEffect = GUID_ConstantForce;
    fe[0].lpDiEffect = &e1;
    strcpy(fe[0].szFriendlyName, "First effect");
    fe[1].dwSize = sizeof(DIFILEEFFECT);
    fe[1].GuidEffect = GUID_Sine;
    fe[1].lpDiEffect = &e2;
    strcpy(fe[1].szFriendlyName, "Second");

    HR(IDirectInputDevice8_WriteEffectToFile(dev, NULL, 2, fe, 0), DIERR_INVALIDPARAM);
    HR(IDirectInputDevice8_WriteEffectToFile(dev, path, 0, fe, 0), DIERR_INVALIDPARAM);
    HR(IDirectInputDevice8_WriteEffectToFile(dev, path, 2, NULL, 0), DIERR_INVALIDPARAM);
    HR(IDirectInputDevice8_WriteEffectToFile(dev, path, 2, fe, 0x1000), DIERR_INVALIDPARAM);
    fe[1].dwSize = 4;
    HR(IDirectInputDevice8_WriteEffectToFile(dev, path, 2, fe, 0), DIERR_INVALIDPARAM);
    fe[1].dwSize = sizeof(DIFILEEFFECT);
    HR(IDirectInputDevice8_WriteEffectToFile(dev, path, 2, fe, 0), DI_OK);

    memset(&c, 0, sizeof(c));
    HR(IDirectInputDevice8_EnumEffectsInFile(dev, path, enum_file, &c, 0), DI_OK);
    CHECKF(c.calls == 2, "two effects enumerated (%d)", c.calls);
    if (c.calls == 2)
    {
        CHECKF(IsEqualGUID(&c.seen[0].GuidEffect, &GUID_ConstantForce) && IsEqualGUID(&c.seen[1].GuidEffect, &GUID_Sine), "effect guids");
        CHECKF(!strcmp(c.seen[0].szFriendlyName, "First effect") && !strcmp(c.seen[1].szFriendlyName, "Second"), "friendly names");
        CHECKF(c.seen[0].dwSize == sizeof(DIFILEEFFECT), "DIFILEEFFECT size (%lu)", c.seen[0].dwSize);
        CHECKF(c.eff[0].dwSize == sizeof(DIEFFECT) && c.eff[1].dwSize == sizeof(DIEFFECT_DX5), "effect sizes (%lu, %lu)", c.eff[0].dwSize, c.eff[1].dwSize);
        CHECKF(c.eff[0].dwFlags == e1.dwFlags && c.eff[0].dwDuration == 5000000 && c.eff[0].dwSamplePeriod == 11, "flags, duration, period");
        CHECKF(c.eff[0].dwGain == 7777 && c.eff[0].dwTriggerButton == 3 && c.eff[0].dwTriggerRepeatInterval == 99, "gain, trigger");
        CHECKF(c.eff[0].dwStartDelay == 42, "start delay (%lu)", c.eff[0].dwStartDelay);
        CHECKF(c.eff[0].cAxes == 2 && c.axes[0][0] == axes[0] && c.axes[0][1] == axes[1], "axes");
        CHECKF(c.dirs[0][0] == 4500 && c.dirs[0][1] == 9000, "directions");
        CHECKF(c.eff[0].lpEnvelope && c.env[0].dwAttackLevel == 100 && c.env[0].dwAttackTime == 200 &&
               c.env[0].dwFadeLevel == 300 && c.env[0].dwFadeTime == 400, "envelope");
        CHECKF(c.eff[0].cbTypeSpecificParams == sizeof(cf) && ((DICONSTANTFORCE *)c.params[0])->lMagnitude == -1234, "type specific params");
        CHECKF(c.eff[1].dwDuration == INFINITE && c.eff[1].dwGain == 1, "second effect values");
        CHECKF(c.eff[1].lpEnvelope == NULL && c.eff[1].cAxes == 0 && c.eff[1].cbTypeSpecificParams == 0, "second effect has no optional parts");
    }

    memset(&c, 0, sizeof(c));
    c.stop_after = 1;
    HR(IDirectInputDevice8_EnumEffectsInFile(dev, path, enum_file, &c, 0), DI_OK);
    CHECKF(c.calls == 1, "enumeration stops when the callback returns FALSE (%d)", c.calls);
    HR(IDirectInputDevice8_EnumEffectsInFile(dev, NULL, enum_file, &c, 0), DIERR_INVALIDPARAM);
    HR(IDirectInputDevice8_EnumEffectsInFile(dev, path, NULL, &c, 0), DIERR_INVALIDPARAM);
    HR(IDirectInputDevice8_EnumEffectsInFile(dev, path, enum_file, &c, 0x1000), DIERR_INVALIDPARAM);

    hr = IDirectInputDevice8_EnumEffectsInFile(dev, L"C:\\no\\such\\dir\\x.ffe", enum_file, &c, 0);
    CHECKF(FAILED(hr), "missing file fails (%#lx)", hr);
    file = CreateFileW(bad, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(file, "this is not a RIFF file at all", 30, &written, NULL);
    CloseHandle(file);
    HR(IDirectInputDevice8_EnumEffectsInFile(dev, bad, enum_file, &c, 0), DIERR_INVALIDPARAM);

    DeleteFileW(path);
    DeleteFileW(bad);
    IDirectInputDevice8_Release(dev);
    IDirectInput8_Release(di8);
}

int main(void)
{
    HINSTANCE inst = GetModuleHandleW(NULL);
    HWND hwnd = CreateWindowW(L"static", L"sg", WS_POPUP, 0, 0, 10, 10, NULL, NULL, inst, NULL);

    CoInitialize(NULL);
    if (!hwnd) hwnd = GetDesktopWindow();
    test_joyconfig(inst, hwnd);
    test_effect_files(inst);
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
