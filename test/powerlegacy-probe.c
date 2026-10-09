/* The legacy numbered power scheme API (patches/sg/2405), run by
 * test/powerlegacy-gate.sh: EnumPwrSchemes, ReadPwrScheme, WritePwrScheme,
 * DeletePwrScheme, Get/SetActivePwrScheme, Read/WriteGlobalPwrPolicy,
 * Read/WriteProcessorPwrScheme and GetCurrentPowerPolicies all failed with
 * ERROR_CALL_NOT_IMPLEMENTED. */
#include <windows.h>
#include <powrprof.h>
#include <stdio.h>

#define NEW_SCHEME ((UINT)-1)

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static const GUID balanced = {0x381b4222,0xf694,0x41f0,{0x96,0x85,0xff,0x5b,0xb2,0x60,0xdf,0x2e}};
static const GUID high = {0x8c5e7fda,0xe8bf,0x4a96,{0x9a,0x85,0xa6,0xe2,0x3a,0x8c,0x63,0x5c}};
static const GUID saver = {0xa1841308,0x3541,0x4fab,{0xbc,0x81,0xf7,0x15,0x56,0xf2,0x0b,0x4a}};

typedef BOOLEAN (CALLBACK *enum_cb)(UINT, DWORD, LPWSTR, DWORD, LPWSTR, PPOWER_POLICY, LPARAM);
typedef BOOLEAN (WINAPI *enum_fn)(enum_cb, LPARAM);
typedef BOOLEAN (WINAPI *read_fn)(UINT, PPOWER_POLICY);
typedef BOOLEAN (WINAPI *write_fn)(PUINT, LPWSTR, LPWSTR, PPOWER_POLICY);
typedef BOOLEAN (WINAPI *del_fn)(UINT);
typedef BOOLEAN (WINAPI *getact_fn)(PUINT);
typedef BOOLEAN (WINAPI *setact_fn)(UINT, PGLOBAL_POWER_POLICY, PPOWER_POLICY);
typedef BOOLEAN (WINAPI *rglobal_fn)(PGLOBAL_POWER_POLICY);
typedef BOOLEAN (WINAPI *rproc_fn)(UINT, PMACHINE_PROCESSOR_POWER_POLICY);
typedef BOOLEAN (WINAPI *cur_fn)(PGLOBAL_POWER_POLICY, PPOWER_POLICY);
typedef DWORD (WINAPI *getsch_fn)(HKEY, GUID **);
typedef DWORD (WINAPI *setsch_fn)(HKEY, const GUID *);

static enum_fn pEnumPwrSchemes;
static read_fn pReadPwrScheme;
static write_fn pWritePwrScheme;
static del_fn pDeletePwrScheme;
static getact_fn pGetActivePwrScheme;
static setact_fn pSetActivePwrScheme;
static rglobal_fn pReadGlobalPwrPolicy, pWriteGlobalPwrPolicy;
static rproc_fn pReadProcessorPwrScheme, pWriteProcessorPwrScheme;
static cur_fn pGetCurrentPowerPolicies;
static getsch_fn pPowerGetActiveScheme;
static setsch_fn pPowerSetActiveScheme;

struct entry
{
    UINT id;
    DWORD name_size, desc_size;
    WCHAR name[64], desc[128];
    POWER_POLICY policy;
};

struct list
{
    int count, stop_after;
    struct entry e[32];
};

static BOOLEAN CALLBACK collect(UINT id, DWORD name_size, LPWSTR name, DWORD desc_size, LPWSTR desc,
                                PPOWER_POLICY policy, LPARAM param)
{
    struct list *l = (struct list *)param;
    struct entry *e = &l->e[l->count++];

    e->id = id;
    e->name_size = name_size;
    e->desc_size = desc_size;
    lstrcpynW( e->name, name, 64 );
    lstrcpynW( e->desc, desc, 128 );
    e->policy = *policy;
    return !(l->stop_after && l->count >= l->stop_after);
}

static struct entry *find(struct list *l, UINT id)
{
    int i;
    for (i = 0; i < l->count; i++) if (l->e[i].id == id) return &l->e[i];
    return NULL;
}

static void get_list(struct list *l, int stop_after)
{
    memset( l, 0, sizeof(*l) );
    l->stop_after = stop_after;
    SetLastError( 0xdeadbeef );
    if (!pEnumPwrSchemes( collect, (LPARAM)l )) printf("      EnumPwrSchemes failed %lu\n", GetLastError());
}

static const struct
{
    UINT id;
    const WCHAR *name;
} builtins[] =
{
    { 0, L"Home/Office Desk" }, { 1, L"Portable/Laptop" }, { 2, L"Presentation" },
    { 3, L"Always On" }, { 4, L"Minimal Power Management" }, { 5, L"Max Battery" },
};

static BOOL same_guid(GUID *g, const GUID *want) { return g && IsEqualGUID( g, want ); }

int main(void)
{
    HMODULE mod = LoadLibraryA( "powrprof.dll" );
    struct list l;
    struct entry *e;
    POWER_POLICY pol, pol2;
    GLOBAL_POWER_POLICY gp, gp2;
    MACHINE_PROCESSOR_POWER_POLICY mp, mp2;
    UINT id, id2, active;
    GUID *guid = NULL;
    WCHAR name[] = L"My scheme", desc[] = L"Mine, for tests";
    BOOLEAN ret;
    int i;

#define GET(n) p##n = (void *)GetProcAddress( mod, #n )
    GET(EnumPwrSchemes); GET(ReadPwrScheme); GET(WritePwrScheme); GET(DeletePwrScheme);
    GET(GetActivePwrScheme); GET(SetActivePwrScheme); GET(ReadGlobalPwrPolicy); GET(WriteGlobalPwrPolicy);
    GET(ReadProcessorPwrScheme); GET(WriteProcessorPwrScheme); GET(GetCurrentPowerPolicies);
    GET(PowerGetActiveScheme); GET(PowerSetActiveScheme);
#undef GET

    /* the built-in schemes */
    get_list( &l, 0 );
    check(l.count == 6, "a fresh profile enumerates 6 schemes");
    for (i = 0; i < 6 && i < l.count; i++)
    {
        char what[80];
        sprintf( what, "scheme %d is %d with the Windows name", i, builtins[i].id );
        check(l.e[i].id == builtins[i].id && !lstrcmpW( l.e[i].name, builtins[i].name ), what);
        sprintf( what, "scheme %d name and description sizes count the NUL, in bytes", i );
        check(l.e[i].name_size == (lstrlenW( l.e[i].name ) + 1) * sizeof(WCHAR)
              && l.e[i].desc_size == (lstrlenW( l.e[i].desc ) + 1) * sizeof(WCHAR) && l.e[i].desc[0], what);
    }
    get_list( &l, 2 );
    check(l.count == 2, "the callback returning FALSE stops the enumeration");
    SetLastError( 0xdeadbeef );
    check(!pEnumPwrSchemes( NULL, 0 ) && GetLastError() == ERROR_INVALID_PARAMETER, "EnumPwrSchemes(NULL) fails");

    /* policies of the built-ins follow the GUID schemes they stand for */
    memset( &pol, 0xcc, sizeof(pol) );
    check(pReadPwrScheme( 3, &pol ), "ReadPwrScheme(3) succeeds");
    check(pol.user.VideoTimeoutAc == 900 && pol.user.VideoTimeoutDc == 600, "Always On turns the display off like High performance");
    check(pol.user.IdleTimeoutAc == 0, "Always On never sleeps on AC");
    check(pol.user.Revision == 1 && pol.mach.Revision == 1, "the policy has revision 1");
    check(pReadPwrScheme( 5, &pol ), "ReadPwrScheme(5) succeeds");
    check(pol.user.VideoTimeoutAc == 300 && pol.user.VideoTimeoutDc == 120, "Max Battery turns the display off like Power saver");
    check(pol.user.IdleTimeoutAc == 900 && pol.user.IdleAc.Action == PowerActionSleep, "Max Battery sleeps after 15 minutes on AC");
    check(pReadPwrScheme( 0, &pol ) && pol.user.VideoTimeoutAc == 600 && pol.user.IdleTimeoutAc == 1800,
          "Home/Office Desk turns off the display after 10 minutes and sleeps after 30");
    SetLastError( 0xdeadbeef );
    check(!pReadPwrScheme( 77, &pol ) && GetLastError() == ERROR_FILE_NOT_FOUND, "ReadPwrScheme of an unknown scheme fails");
    SetLastError( 0xdeadbeef );
    check(!pReadPwrScheme( 0, NULL ) && GetLastError() == ERROR_INVALID_PARAMETER, "ReadPwrScheme(NULL) fails");

    /* the active scheme */
    ret = pGetActivePwrScheme( &active );
    check(ret && active == 0, "the active scheme is Home/Office Desk to start with");
    SetLastError( 0xdeadbeef );
    check(!pGetActivePwrScheme( NULL ) && GetLastError() == ERROR_INVALID_PARAMETER, "GetActivePwrScheme(NULL) fails");
    SetLastError( 0xdeadbeef );
    check(!pSetActivePwrScheme( 77, NULL, NULL ) && GetLastError() == ERROR_FILE_NOT_FOUND, "SetActivePwrScheme of an unknown scheme fails");
    check(pSetActivePwrScheme( 3, NULL, NULL ), "SetActivePwrScheme(3) succeeds");
    check(pGetActivePwrScheme( &active ) && active == 3, "and it is the active scheme");
    check(pPowerGetActiveScheme( NULL, &guid ) == 0 && same_guid( guid, &high ), "the GUID API sees High performance");
    LocalFree( guid ); guid = NULL;
    check(pSetActivePwrScheme( 5, NULL, NULL ) && pPowerGetActiveScheme( NULL, &guid ) == 0 && same_guid( guid, &saver ),
          "scheme 5 makes Power saver active");
    LocalFree( guid ); guid = NULL;
    check(pPowerSetActiveScheme( NULL, &balanced ) == 0, "the GUID API selects Balanced");
    check(pGetActivePwrScheme( &active ) && active == 0, "the legacy API follows it to scheme 0");
    check(pPowerSetActiveScheme( NULL, &high ) == 0 && pGetActivePwrScheme( &active ) && active == 3, "and High performance to scheme 3");
    check(pSetActivePwrScheme( 1, NULL, NULL ) && pGetActivePwrScheme( &active ) && active == 1, "scheme 1 can be made active");

    /* new schemes */
    memset( &pol, 0, sizeof(pol) );
    pReadPwrScheme( 0, &pol );
    pol.user.VideoTimeoutAc = 1234;
    pol.user.IdleTimeoutDc = 4321;
    id = NEW_SCHEME;
    check(pWritePwrScheme( &id, name, desc, &pol ), "WritePwrScheme(NEWSCHEME) succeeds");
    check(id > 5 && id != NEW_SCHEME, "it hands out an id above the built-ins");
    id2 = NEW_SCHEME;
    check(pWritePwrScheme( &id2, name, desc, &pol ) && id2 > 5 && id2 != id, "a second new scheme gets another id");
    get_list( &l, 0 );
    check(l.count == 8, "the enumeration has 8 schemes");
    e = find( &l, id );
    check(e && !lstrcmpW( e->name, name ) && !lstrcmpW( e->desc, desc ), "the new scheme is enumerated with its name and description");
    check(e && e->name_size == sizeof(name) && e->desc_size == sizeof(desc), "its sizes count the NUL");
    check(e && e->policy.user.VideoTimeoutAc == 1234 && e->policy.user.IdleTimeoutDc == 4321, "its policy is enumerated");
    check(l.count == 8 && l.e[6].id < l.e[7].id, "enumeration is in id order");
    memset( &pol2, 0, sizeof(pol2) );
    check(pReadPwrScheme( id, &pol2 ) && !memcmp( &pol, &pol2, sizeof(pol) ), "ReadPwrScheme reads back what was written");

    pol.user.VideoTimeoutAc = 99;
    id2 = id;
    check(pWritePwrScheme( &id2, name, desc, &pol ) && id2 == id, "writing an existing id keeps it");
    check(pReadPwrScheme( id, &pol2 ) && pol2.user.VideoTimeoutAc == 99, "and replaces the policy");
    check(pWritePwrScheme( &(UINT){ 3 }, name, desc, &pol ) && pReadPwrScheme( 3, &pol2 ) && pol2.user.VideoTimeoutAc == 99,
          "a built-in scheme can be overwritten");
    SetLastError( 0xdeadbeef );
    id2 = NEW_SCHEME;
    check(!pWritePwrScheme( &id2, NULL, desc, &pol ) && GetLastError() == ERROR_INVALID_PARAMETER, "WritePwrScheme without a name fails");

    /* activating with a policy */
    pol.user.VideoTimeoutAc = 55;
    check(pSetActivePwrScheme( id, NULL, &pol ) && pReadPwrScheme( id, &pol2 ) && pol2.user.VideoTimeoutAc == 55,
          "SetActivePwrScheme with a policy stores it");
    check(pGetActivePwrScheme( &active ) && active == id, "a new scheme can be active");
    memset( &pol2, 0, sizeof(pol2) );
    check(pGetCurrentPowerPolicies( &gp, &pol2 ) && pol2.user.VideoTimeoutAc == 55, "GetCurrentPowerPolicies returns the active policy");

    /* the global policy */
    memset( &gp, 0xcc, sizeof(gp) );
    check(pReadGlobalPwrPolicy( &gp ), "ReadGlobalPwrPolicy succeeds");
    check(gp.user.PowerButtonAc.Action == PowerActionSleep && gp.user.LidCloseAc.Action == PowerActionSleep,
          "the default power button and lid put the computer to sleep");
    check(gp.user.DischargePolicy[0].Enable && gp.user.DischargePolicy[0].BatteryLevel == 5
          && gp.user.DischargePolicy[0].PowerPolicy.Action == PowerActionHibernate,
          "the critical battery level is 5 percent and hibernates");
    check(gp.user.DischargePolicy[1].BatteryLevel == 10, "the low battery level is 10 percent");
    gp.user.PowerButtonAc.Action = PowerActionShutdown;
    gp.user.DischargePolicy[1].BatteryLevel = 17;
    check(pWriteGlobalPwrPolicy( &gp ), "WriteGlobalPwrPolicy succeeds");
    memset( &gp2, 0, sizeof(gp2) );
    check(pReadGlobalPwrPolicy( &gp2 ) && gp2.user.PowerButtonAc.Action == PowerActionShutdown
          && gp2.user.DischargePolicy[1].BatteryLevel == 17, "and the policy reads back");
    SetLastError( 0xdeadbeef );
    check(!pReadGlobalPwrPolicy( NULL ) && GetLastError() == ERROR_INVALID_PARAMETER, "ReadGlobalPwrPolicy(NULL) fails");
    SetLastError( 0xdeadbeef );
    check(!pWriteGlobalPwrPolicy( NULL ) && GetLastError() == ERROR_INVALID_PARAMETER, "WriteGlobalPwrPolicy(NULL) fails");
    check(pSetActivePwrScheme( 0, &gp, NULL ), "SetActivePwrScheme can write the global policy");

    /* processor policies */
    memset( &mp, 0xcc, sizeof(mp) );
    check(pReadProcessorPwrScheme( 3, &mp ) && mp.ProcessorPolicyAc.DynamicThrottle == PO_THROTTLE_NONE
          && mp.ProcessorPolicyDc.DynamicThrottle == PO_THROTTLE_NONE, "Always On does not throttle the processor");
    check(pReadProcessorPwrScheme( 5, &mp ) && mp.ProcessorPolicyAc.DynamicThrottle == PO_THROTTLE_NONE
          && mp.ProcessorPolicyDc.DynamicThrottle == PO_THROTTLE_ADAPTIVE, "Max Battery throttles adaptively on battery only");
    mp.ProcessorPolicyAc.PolicyCount = 2;
    check(pWriteProcessorPwrScheme( 5, &mp ), "WriteProcessorPwrScheme succeeds");
    memset( &mp2, 0, sizeof(mp2) );
    check(pReadProcessorPwrScheme( 5, &mp2 ) && mp2.ProcessorPolicyAc.PolicyCount == 2, "and reads back");
    check(pReadProcessorPwrScheme( 3, &mp2 ) && mp2.ProcessorPolicyAc.PolicyCount != 2, "without touching another scheme");
    SetLastError( 0xdeadbeef );
    check(!pReadProcessorPwrScheme( 77, &mp ) && GetLastError() == ERROR_FILE_NOT_FOUND, "ReadProcessorPwrScheme of an unknown scheme fails");
    SetLastError( 0xdeadbeef );
    check(!pWriteProcessorPwrScheme( 77, &mp ) && GetLastError() == ERROR_FILE_NOT_FOUND, "WriteProcessorPwrScheme of an unknown scheme fails");
    SetLastError( 0xdeadbeef );
    check(!pReadProcessorPwrScheme( 0, NULL ) && GetLastError() == ERROR_INVALID_PARAMETER, "ReadProcessorPwrScheme(NULL) fails");

    /* deleting */
    check(pSetActivePwrScheme( id, NULL, NULL ), "making the new scheme active");
    SetLastError( 0xdeadbeef );
    check(!pDeletePwrScheme( id ) && GetLastError() == ERROR_ACCESS_DENIED, "the active scheme cannot be deleted");
    check(pReadPwrScheme( id, &pol2 ), "and is still there");
    check(pSetActivePwrScheme( 0, NULL, NULL ), "making another scheme active");
    check(pDeletePwrScheme( id ), "DeletePwrScheme succeeds");
    SetLastError( 0xdeadbeef );
    check(!pReadPwrScheme( id, &pol2 ) && GetLastError() == ERROR_FILE_NOT_FOUND, "a deleted scheme cannot be read");
    SetLastError( 0xdeadbeef );
    check(!pDeletePwrScheme( id ) && GetLastError() == ERROR_FILE_NOT_FOUND, "or deleted twice");
    get_list( &l, 0 );
    check(l.count == 7 && !find( &l, id ), "it is no longer enumerated");
    id2 = NEW_SCHEME;
    check(pWritePwrScheme( &id2, name, desc, &pol ) && id2 != id, "a new scheme does not reuse the deleted id");
    check(pDeletePwrScheme( 4 ) && !pReadPwrScheme( 4, &pol2 ), "a built-in scheme can be deleted");
    get_list( &l, 0 );
    check(!find( &l, 4 ), "and is not enumerated");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
