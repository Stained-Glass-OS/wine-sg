/* Power schemes and the platform role (patches/sg/1699), run by
 * test/powerschemes-gate.sh: PowerGetActiveScheme, PowerReadDCValue,
 * PowerEnumerate and PowerReadFriendlyName were FIXMEs returning
 * ERROR_CALL_NOT_IMPLEMENTED, PowerSetActiveScheme and
 * PowerWriteACValueIndex kept nothing, the value index, duplicate, delete,
 * range and description calls were missing, PowerDeterminePlatformRole(Ex)
 * always said desktop, and the notification registrations gave 0xdeadbeef
 * and never called back. */
#include <windows.h>
#include <powrprof.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static const GUID balanced = {0x381b4222,0xf694,0x41f0,{0x96,0x85,0xff,0x5b,0xb2,0x60,0xdf,0x2e}};
static const GUID high = {0x8c5e7fda,0xe8bf,0x4a96,{0x9a,0x85,0xa6,0xe2,0x3a,0x8c,0x63,0x5c}};
static const GUID video = {0x7516b95f,0xf776,0x4464,{0x8c,0x53,0x06,0x16,0x7f,0x40,0xcc,0x99}};
static const GUID video_off = {0x3c0bc021,0xc8a8,0x4e07,{0xa9,0x73,0x6b,0x14,0xcb,0xcb,0x2b,0x7e}};
static const GUID processor = {0x54533251,0x82be,0x4824,{0x96,0xc1,0x47,0xb6,0x0b,0x74,0x0d,0x00}};
static const GUID cpu_max = {0xbc5038f7,0x23e0,0x4960,{0x96,0xda,0x33,0xab,0xaf,0x59,0x35,0xec}};
static const GUID buttons = {0x4f971e89,0xeebd,0x4455,{0xa8,0xde,0x9e,0x59,0x04,0x0e,0x73,0x47}};
static const GUID lid = {0x5ca83367,0x6e45,0x459f,{0xa2,0x7b,0x47,0x6b,0x1d,0x01,0xc9,0x36}};
static const GUID active_note = {0x31f9f286,0x5084,0x42fe,{0xb7,0x20,0x2b,0x02,0x64,0x99,0x37,0x63}};
static const GUID personality = {0x245d8541,0x3943,0x4422,{0xb0,0x25,0x13,0xa7,0x84,0xf6,0x79,0xb7}};

typedef DWORD (WINAPI *index_fn)(HKEY, const GUID *, const GUID *, const GUID *, DWORD *);
typedef DWORD (WINAPI *write_fn)(HKEY, const GUID *, const GUID *, const GUID *, DWORD);
typedef DWORD (WINAPI *dup_fn)(HKEY, const GUID *, GUID **);
typedef DWORD (WINAPI *del_fn)(HKEY, const GUID *);
typedef DWORD (WINAPI *range_fn)(HKEY, const GUID *, const GUID *, DWORD *);
typedef DWORD (WINAPI *text_fn)(HKEY, const GUID *, const GUID *, const GUID *, UCHAR *, DWORD *);
typedef DWORD (WINAPI *wtext_fn)(HKEY, const GUID *, const GUID *, const GUID *, UCHAR *, DWORD);
typedef DWORD (WINAPI *possible_fn)(HKEY, const GUID *, const GUID *, ULONG, UCHAR *, DWORD *);
typedef DWORD (WINAPI *restore_fn)(void);

struct log
{
    volatile LONG count;
    GUID setting[8];
    GUID value[8];
};

typedef struct
{
    ULONG (CALLBACK *callback)(void *, ULONG, void *);
    void *context;
} subscribe;

struct setting
{
    GUID guid;
    DWORD length;
    BYTE data[16];
};

static ULONG CALLBACK on_setting(void *context, ULONG type, void *data)
{
    struct log *log = context;
    struct setting *s = data;
    LONG i = log->count;

    if (type == 0x8013 && i < 8)
    {
        log->setting[i] = s->guid;
        if (s->length == sizeof(GUID)) memcpy(&log->value[i], s->data, sizeof(GUID));
        InterlockedIncrement(&log->count);
    }
    return 0;
}

static void wait_count(struct log *log, LONG n)
{
    int i;
    for (i = 0; i < 100 && log->count < n; i++) Sleep(50);
}

int main(void)
{
    HMODULE pp = LoadLibraryA("powrprof.dll");
    index_fn pReadAC = (void *)GetProcAddress(pp, "PowerReadACValueIndex");
    index_fn pReadDC = (void *)GetProcAddress(pp, "PowerReadDCValueIndex");
    write_fn pWriteDC = (void *)GetProcAddress(pp, "PowerWriteDCValueIndex");
    dup_fn pDuplicate = (void *)GetProcAddress(pp, "PowerDuplicateScheme");
    del_fn pDelete = (void *)GetProcAddress(pp, "PowerDeleteScheme");
    range_fn pMax = (void *)GetProcAddress(pp, "PowerReadValueMax");
    text_fn pDescription = (void *)GetProcAddress(pp, "PowerReadDescription");
    wtext_fn pWriteName = (void *)GetProcAddress(pp, "PowerWriteFriendlyName");
    possible_fn pPossibleName = (void *)GetProcAddress(pp, "PowerReadPossibleFriendlyName");
    restore_fn pRestore = (void *)GetProcAddress(pp, "PowerRestoreDefaultPowerSchemes");
    GUID *active = NULL, *custom = NULL, guid;
    BYTE buf[512];
    DWORD size, value, type, ret;
    ULONG n;
    struct log log = { 0 };
    subscribe params = { on_setting, &log };
    HPOWERNOTIFY notify = NULL, notify2 = NULL;
    POWER_PLATFORM_ROLE role;

    check(pReadAC && pReadDC && pWriteDC && pDuplicate && pDelete && pMax && pDescription && pWriteName
          && pPossibleName && pRestore, "the scheme calls are there");
    if (failures) goto done;
    pRestore();

    check(PowerGetActiveScheme(NULL, &active) == ERROR_SUCCESS && active && IsEqualGUID(active, &balanced),
          "PowerGetActiveScheme: Balanced (was not implemented)");
    LocalFree(active);

    for (n = 0; ; n++)
    {
        size = sizeof(guid);
        if (PowerEnumerate(NULL, NULL, NULL, ACCESS_SCHEME, n, (UCHAR *)&guid, &size)) break;
    }
    size = 0;
    check(n == 3 && PowerEnumerate(NULL, NULL, NULL, ACCESS_SCHEME, 0, NULL, &size) == ERROR_SUCCESS && size == sizeof(GUID),
          "PowerEnumerate: three schemes (was not implemented)");
    size = sizeof(guid);
    check(PowerEnumerate(NULL, &balanced, &video, ACCESS_INDIVIDUAL_SETTING, 0, (UCHAR *)&guid, &size) == ERROR_SUCCESS
          && IsEqualGUID(&guid, &video_off), "the display subgroup's setting");

    size = sizeof(buf);
    check(PowerReadFriendlyName(NULL, &balanced, NULL, NULL, buf, &size) == ERROR_SUCCESS && !lstrcmpW((WCHAR *)buf, L"Balanced")
          && size == sizeof(L"Balanced"), "PowerReadFriendlyName: Balanced");
    size = 4;
    check(PowerReadFriendlyName(NULL, &high, NULL, NULL, buf, &size) == ERROR_MORE_DATA && size == sizeof(L"High performance"),
          "a small buffer: ERROR_MORE_DATA and the size");
    size = sizeof(buf);
    check(pDescription(NULL, &balanced, &video, &video_off, buf, &size) == ERROR_SUCCESS && ((WCHAR *)buf)[0],
          "PowerReadDescription of a setting");

    check(pReadAC(NULL, &balanced, &video, &video_off, &value) == ERROR_SUCCESS && value == 600, "Balanced: display off after 600 s on AC");
    check(pReadDC(NULL, &balanced, &video, &video_off, &value) == ERROR_SUCCESS && value == 300, "and 300 s on battery");
    size = sizeof(value);
    check(PowerReadDCValue(NULL, &balanced, &video, &video_off, &type, (UCHAR *)&value, &size) == ERROR_SUCCESS
          && type == REG_DWORD && value == 300 && size == 4, "PowerReadDCValue: REG_DWORD 300 (was not implemented)");
    size = 2;
    check(PowerReadACValue(NULL, &balanced, &video, &video_off, &type, (UCHAR *)&value, &size) == ERROR_MORE_DATA,
          "PowerReadACValue: a small buffer");

    check(PowerWriteACValueIndex(NULL, &balanced, &video, &video_off, 1200) == ERROR_SUCCESS
          && pReadAC(NULL, &balanced, &video, &video_off, &value) == ERROR_SUCCESS && value == 1200,
          "PowerWriteACValueIndex is kept (it was dropped)");
    check(pWriteDC(NULL, &balanced, &processor, &cpu_max, 150) == ERROR_INVALID_PARAMETER, "150 percent: refused");
    check(pMax(NULL, &processor, &cpu_max, &value) == ERROR_SUCCESS && value == 100, "PowerReadValueMax: 100");
    size = sizeof(buf);
    check(pPossibleName(NULL, &buttons, &lid, 1, buf, &size) == ERROR_SUCCESS && !lstrcmpW((WCHAR *)buf, L"Sleep"),
          "the lid's possible value 1: Sleep");

    /* notifications: the current value at once, and changes */
    check(PowerSettingRegisterNotification(&active_note, 2, &params, &notify) == ERROR_SUCCESS && notify, "PowerSettingRegisterNotification");
    wait_count(&log, 1);
    check(log.count == 1 && IsEqualGUID(&log.setting[0], &active_note) && IsEqualGUID(&log.value[0], &balanced),
          "told the active scheme at once");
    check(PowerSetActiveScheme(NULL, &high) == ERROR_SUCCESS && PowerGetActiveScheme(NULL, &active) == ERROR_SUCCESS
          && IsEqualGUID(active, &high), "PowerSetActiveScheme: kept (it was dropped)");
    LocalFree(active);
    wait_count(&log, 2);
    check(log.count == 2 && IsEqualGUID(&log.value[1], &high), "and told of the change");
    {
        struct log ulog = { 0 };
        subscribe uparams = { on_setting, &ulog };
        HPOWERNOTIFY unotify = RegisterPowerSettingNotification(&uparams, &active_note, 2);
        wait_count(&ulog, 1);
        check(unotify && ulog.count == 1 && IsEqualGUID(&ulog.value[0], &high),
              "user32's RegisterPowerSettingNotification: the active scheme too (it said Balanced)");
        if (unotify) UnregisterPowerSettingNotification(unotify);
    }
    check(PowerSetActiveScheme(NULL, &video) == ERROR_FILE_NOT_FOUND, "no such scheme: ERROR_FILE_NOT_FOUND");
    check(PowerSettingUnregisterNotification(notify) == ERROR_SUCCESS && PowerSettingUnregisterNotification(notify) != ERROR_SUCCESS,
          "unregistered, once");

    /* a scheme of one's own */
    check(pDuplicate(NULL, &balanced, &custom) == ERROR_SUCCESS && custom, "PowerDuplicateScheme");
    for (n = 0; ; n++)
    {
        size = sizeof(guid);
        if (PowerEnumerate(NULL, NULL, NULL, ACCESS_SCHEME, n, (UCHAR *)&guid, &size)) break;
    }
    check(n == 4, "four schemes");
    check(custom && pReadAC(NULL, custom, &video, &video_off, &value) == ERROR_SUCCESS && value == 1200, "it has what Balanced had");
    if (custom)
    {
        size = sizeof(buf);
        pWriteName(NULL, custom, NULL, NULL, (UCHAR *)L"SG plan", sizeof(L"SG plan"));
        check(PowerReadFriendlyName(NULL, custom, NULL, NULL, buf, &size) == ERROR_SUCCESS && !lstrcmpW((WCHAR *)buf, L"SG plan"),
              "PowerWriteFriendlyName");
        log.count = 0;
        PowerSettingRegisterNotification(&personality, 2, &params, &notify2);
        PowerSetActiveScheme(NULL, custom);
        wait_count(&log, 2);
        check(log.count >= 2 && IsEqualGUID(&log.value[log.count - 1], &balanced), "its personality is Balanced's");
        PowerSettingUnregisterNotification(notify2);
        check(pDelete(NULL, custom) == ERROR_ACCESS_DENIED, "the active scheme cannot be deleted");
        PowerSetActiveScheme(NULL, &balanced);
        check(pDelete(NULL, custom) == ERROR_SUCCESS && pReadAC(NULL, custom, &video, &video_off, &value) == ERROR_FILE_NOT_FOUND,
              "PowerDeleteScheme");
        LocalFree(custom);
    }

    /* the platform role */
    role = PowerDeterminePlatformRoleEx(POWER_PLATFORM_ROLE_V2);
    printf("      role %d\n", role);
    check(role > PlatformRoleUnspecified && role < PlatformRoleMaximum, "PowerDeterminePlatformRoleEx: a role");
    check(PowerDeterminePlatformRoleEx(POWER_PLATFORM_ROLE_V1) != PlatformRoleSlate, "no slates for version 1");
    check(PowerDeterminePlatformRoleEx(99) == PlatformRoleUnspecified, "an unknown version: unspecified (was desktop)");
    check(PowerDeterminePlatformRole() == PowerDeterminePlatformRoleEx(POWER_PLATFORM_ROLE_V1), "PowerDeterminePlatformRole: the same");

    /* suspend and resume */
    notify = NULL;
    check(PowerRegisterSuspendResumeNotification(2, &params, &notify) == ERROR_SUCCESS && notify && notify != (HPOWERNOTIFY)0xdeadbeef,
          "PowerRegisterSuspendResumeNotification: a handle of its own");
    check(PowerRegisterSuspendResumeNotification(2, NULL, &notify2) == ERROR_INVALID_PARAMETER, "no recipient: refused");
    check(PowerUnregisterSuspendResumeNotification(notify) == ERROR_SUCCESS
          && PowerUnregisterSuspendResumeNotification((HPOWERNOTIFY)0xdeadbeef) == ERROR_INVALID_HANDLE, "unregistered; a bad handle refused");

    ret = pRestore();
    check(ret == ERROR_SUCCESS && pReadAC(NULL, &balanced, &video, &video_off, &value) == ERROR_SUCCESS && value == 600,
          "PowerRestoreDefaultPowerSchemes");
done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
