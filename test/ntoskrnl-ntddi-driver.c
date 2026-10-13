/* Probe driver for patches/sg/5076: RtlIsNtDdiVersionAvailable is TRUE for the running
 * Windows version (RtlGetVersion) and every earlier NTDDI value, FALSE for later ones.
 * Each row: an NTDDI value and the first Windows 10 build that has it (0: any Windows 10,
 * ~0: none yet). */
#include "sg-driver.h"

/* the DDK maps the name to its wdmlib helper; the probe wants the export */
#undef RtlIsNtDdiVersionAvailable
__declspec(dllimport) BOOLEAN NTAPI RtlIsNtDdiVersionAvailable(ULONG);


static void run_probe(DRIVER_OBJECT *driver)
{
    static const struct { ULONG ntddi; ULONG build; } rows[] =
    {
        {0x06010000, 0},        /* NTDDI_WIN7 */
        {0x06030000, 0},        /* NTDDI_WINBLUE */
        {0x0A000000, 0},        /* NTDDI_WIN10 */
        {0x0A000002, 14393},    /* RS1 */
        {0x0A000006, 17763},    /* RS5 */
        {0x0A000008, 19041},    /* VB */
        {0x0A00000A, 20348},    /* FE */
        {0x0A00000C, 22621},    /* NI */
        {0x0A0000FF, ~0u},      /* a release to come */
        {0x0B000000, ~0u},      /* a Windows to come */
        {0x0A000100, ~0u},      /* a Windows 10 service pack */
    };
    RTL_OSVERSIONINFOW info = { sizeof(info) };
    unsigned int i;
    BOOLEAN ret;

    RtlGetVersion(&info);
    check(info.dwMajorVersion == 10, "the running version %lu.%lu.%lu", info.dwMajorVersion, info.dwMinorVersion,
          info.dwBuildNumber);
    for (i = 0; i < ARRAYSIZE(rows); i++)
    {
        BOOLEAN expect = rows[i].build != ~0u && info.dwBuildNumber >= rows[i].build;
        ret = RtlIsNtDdiVersionAvailable(rows[i].ntddi);
        check(ret == expect, "%#lx: %d (build %lu)", rows[i].ntddi, ret, info.dwBuildNumber);
    }
}
