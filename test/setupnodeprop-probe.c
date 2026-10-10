/* setupapi/cfgmgr32 batch (patches/sg/2036), run by test/setupnodeprop-gate.sh.
 * CM_Set_DevNode_Registry_Property (A, W, Ex): strings (converted from ANSI),
 * multi-strings, numbers, deleting with no data, and the errors; checked with
 * CM_Get_DevNode_Registry_Property and SetupDiGetDeviceRegistryProperty.
 *
 *   setupnodeprop-probe.exe */
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <stdio.h>
#include <string.h>

static const GUID null_guid;
static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECK(expr) check((expr), #expr)

typedef DWORD (WINAPI *set_a_t)(DWORD, DWORD, void *, ULONG, ULONG);
typedef DWORD (WINAPI *set_ex_t)(DWORD, DWORD, void *, ULONG, ULONG, void *);
typedef DWORD (WINAPI *get_t)(DWORD, DWORD, ULONG *, void *, ULONG *, ULONG);

int main(void)
{
    HMODULE cm = LoadLibraryA("cfgmgr32.dll");
    set_a_t seta = (set_a_t)GetProcAddress(cm, "CM_Set_DevNode_Registry_PropertyA");
    set_a_t setw = (set_a_t)GetProcAddress(cm, "CM_Set_DevNode_Registry_PropertyW");
    set_ex_t setexw = (set_ex_t)GetProcAddress(cm, "CM_Set_DevNode_Registry_Property_ExW");
    get_t geta = (get_t)GetProcAddress(cm, "CM_Get_DevNode_Registry_PropertyA");
    get_t getw = (get_t)GetProcAddress(cm, "CM_Get_DevNode_Registry_PropertyW");
    SP_DEVINFO_DATA d = {sizeof(d)};
    HDEVINFO set;
    char buf[128];
    WCHAR wbuf[128];
    ULONG len, type;
    DWORD value;
    static const char hwids[] = "sgprobe_one\0sgprobe_two\0";

    CHECK(seta && setw && setexw && geta && getw);
    if (!(seta && setw && setexw && geta && getw)) { printf("RESULT: FAIL\n"); return 1; }

    set = SetupDiCreateDeviceInfoList(NULL, NULL);
    CHECK(SetupDiCreateDeviceInfoA(set, "Root\\SGPROBE2\\0000", &null_guid, NULL, NULL, 0, &d));

    /* strings */
    CHECK(seta(d.DevInst, CM_DRP_DEVICEDESC, "Probe Dev", 10, 0) == CR_SUCCESS);
    len = sizeof(wbuf);
    CHECK(getw(d.DevInst, CM_DRP_DEVICEDESC, &type, wbuf, &len, 0) == CR_SUCCESS && type == REG_SZ
            && !lstrcmpW(wbuf, L"Probe Dev") && len == 10 * sizeof(WCHAR));
    len = sizeof(buf);
    CHECK(SetupDiGetDeviceRegistryPropertyA(set, &d, SPDRP_DEVICEDESC, NULL, (BYTE *)buf, sizeof(buf), (DWORD *)&len)
            && !strcmp(buf, "Probe Dev"));
    CHECK(setw(d.DevInst, CM_DRP_DEVICEDESC, (void *)L"Wide Dev", 9 * sizeof(WCHAR), 0) == CR_SUCCESS);
    len = sizeof(buf);
    CHECK(geta(d.DevInst, CM_DRP_DEVICEDESC, &type, buf, &len, 0) == CR_SUCCESS && !strcmp(buf, "Wide Dev"));

    /* multi-string */
    CHECK(seta(d.DevInst, CM_DRP_HARDWAREID, (void *)hwids, sizeof(hwids), 0) == CR_SUCCESS);
    len = sizeof(buf);
    memset(buf, 0, sizeof(buf));
    CHECK(geta(d.DevInst, CM_DRP_HARDWAREID, &type, buf, &len, 0) == CR_SUCCESS && type == REG_MULTI_SZ
            && len == sizeof(hwids) && !memcmp(buf, hwids, sizeof(hwids)));

    /* number */
    value = 0x21;
    CHECK(seta(d.DevInst, CM_DRP_CONFIGFLAGS, &value, sizeof(value), 0) == CR_SUCCESS);
    len = sizeof(value);
    value = 0;
    CHECK(getw(d.DevInst, CM_DRP_CONFIGFLAGS, &type, &value, &len, 0) == CR_SUCCESS && type == REG_DWORD && value == 0x21);
    CHECK(seta(d.DevInst, CM_DRP_CONFIGFLAGS, &value, 3, 0) == CR_INVALID_DATA);

    /* delete */
    CHECK(seta(d.DevInst, CM_DRP_DEVICEDESC, NULL, 0, 0) == CR_SUCCESS);
    len = sizeof(buf);
    CHECK(geta(d.DevInst, CM_DRP_DEVICEDESC, &type, buf, &len, 0) == CR_NO_SUCH_VALUE);
    CHECK(seta(d.DevInst, CM_DRP_DEVICEDESC, NULL, 0, 0) == CR_SUCCESS);

    /* errors */
    CHECK(seta(d.DevInst, 0, "x", 2, 0) == CR_INVALID_PROPERTY);
    CHECK(seta(d.DevInst, CM_DRP_DEVICEDESC, "x", 2, 1) == CR_INVALID_FLAG);
    CHECK(seta(0x7ffffff0, CM_DRP_DEVICEDESC, "x", 2, 0) == CR_NO_SUCH_DEVNODE);
    CHECK(setexw(d.DevInst, CM_DRP_DEVICEDESC, (void *)L"Ex", 3 * sizeof(WCHAR), 0, NULL) == CR_SUCCESS);

    SetupDiDeleteDeviceInfo(set, &d);
    SetupDiDestroyDeviceInfoList(set);
    printf("%d checks failed\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
