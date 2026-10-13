/* Probe for patches/sg/4586: msacm32's acmMetrics(ACM_METRIC_MAX_SIZE_FILTER)
 * (system wide, per driver identifier, per open driver), the hardware wave
 * metrics of a software driver, and acmDriverMessage(DRV_CONFIGURE) with no
 * configuration info sent to an open driver (msacm supplies one naming the
 * driver's alias, as it already did for a driver identifier). The probe is
 * its own ACM driver: it exports DriverProc and registers itself under
 * Drivers32 (in the gate's scratch prefix) with two filter tags.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <msacm.h>
/* from msacmdrv.h (not in every SDK) */
#ifndef ACMDM_BASE
#define ACMDM_BASE (DRV_USER + 0x2000)
#endif
#define ACMDM_DRIVER_DETAILS (ACMDM_BASE + 10)
#define ACMDM_FILTERTAG_DETAILS (ACMDM_BASE + 50)
#include <stdio.h>
#include <string.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static const DWORD filter_sizes[] = {24, 40};
static WCHAR config_alias[64];
static LONG configure_calls;

__declspec(dllexport) LRESULT WINAPI DriverProc(DWORD_PTR id, HDRVR drv, UINT msg, LPARAM p1, LPARAM p2)
{
    switch (msg)
    {
    case DRV_LOAD: case DRV_FREE: case DRV_ENABLE: case DRV_DISABLE: case DRV_CLOSE:
        return 1;
    case DRV_OPEN:
        return 1;
    case DRV_QUERYCONFIGURE:
        return 1;
    case DRV_CONFIGURE:
        configure_calls++;
        config_alias[0] = 0;
        if (p2)
        {
            DRVCONFIGINFO *info = (DRVCONFIGINFO *)p2;
            if (info->lpszDCIAliasName) lstrcpynW(config_alias, info->lpszDCIAliasName, ARRAYSIZE(config_alias));
        }
        return DRVCNF_OK;
    case ACMDM_DRIVER_DETAILS:
    {
        ACMDRIVERDETAILSW *d = (ACMDRIVERDETAILSW *)p1, add = {sizeof(add)};
        add.fccType = ACMDRIVERDETAILS_FCCTYPE_AUDIOCODEC;
        add.wMid = 0xffff; add.wPid = 0xffff;
        add.fdwSupport = ACMDRIVERDETAILS_SUPPORTF_FILTER;
        add.cFilterTags = ARRAYSIZE(filter_sizes);
        lstrcpyW(add.szShortName, L"SG probe");
        lstrcpyW(add.szLongName, L"SG probe filter driver");
        memcpy(d, &add, min(d->cbStruct, sizeof(add)));
        d->cbStruct = min(d->cbStruct, sizeof(add));
        return MMSYSERR_NOERROR;
    }
    case ACMDM_FILTERTAG_DETAILS:
    {
        ACMFILTERTAGDETAILSW *t = (ACMFILTERTAGDETAILSW *)p1;
        DWORD i = t->dwFilterTagIndex;

        if ((p2 & ACM_FILTERTAGDETAILSF_QUERYMASK) != ACM_FILTERTAGDETAILSF_INDEX) return ACMERR_NOTPOSSIBLE;
        if (i >= ARRAYSIZE(filter_sizes)) return ACMERR_NOTPOSSIBLE;
        t->dwFilterTag = 1 + i;
        t->cbFilterSize = filter_sizes[i];
        t->fdwSupport = ACMDRIVERDETAILS_SUPPORTF_FILTER;
        t->cStandardFilters = 0;
        lstrcpyW(t->szFilterTag, L"SG filter");
        return MMSYSERR_NOERROR;
    }
    default:
        if (msg >= ACMDM_BASE) return MMSYSERR_NOTSUPPORTED;
        return DefDriverProc(id, drv, msg, p1, p2);
    }
}

int main(int argc, char **argv)
{
    char self[MAX_PATH];
    HKEY key;
    HACMDRIVERID hadid;
    HACMDRIVER had;
    DWORD size, value;
    MMRESULT mr;
    LRESULT lr;

    GetModuleFileNameA(NULL, self, sizeof(self));
    if (RegCreateKeyA(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows NT\\CurrentVersion\\Drivers32", &key)
            || RegSetValueExA(key, "msacm.sgprobe", 0, REG_SZ, (BYTE *)self, strlen(self) + 1))
    {
        printf("FAIL cannot register the driver\nRESULT: FAIL\n");
        return 1;
    }
    RegCloseKey(key);
    mr = acmDriverAddA(&hadid, NULL, (LPARAM)"msacm.sgprobe", 0, ACM_DRIVERADDF_NAME);
    check(mr == MMSYSERR_NOERROR, "acmDriverAdd(msacm.sgprobe) %u", mr);
    if (mr) { printf("RESULT: FAIL\n"); return 1; }

    /* the largest WAVEFILTER */
    size = 0xdead;
    mr = acmMetrics(NULL, ACM_METRIC_MAX_SIZE_FILTER, &size);
    check(mr == MMSYSERR_NOERROR && size == 40, "MAX_SIZE_FILTER of the system: %u %lu", mr, size);
    size = 0xdead;
    mr = acmMetrics((HACMOBJ)hadid, ACM_METRIC_MAX_SIZE_FILTER, &size);
    check(mr == MMSYSERR_NOERROR && size == 40, "MAX_SIZE_FILTER of the driver identifier: %u %lu", mr, size);
    mr = acmDriverOpen(&had, hadid, 0);
    check(mr == MMSYSERR_NOERROR, "acmDriverOpen %u", mr);
    size = 0xdead;
    mr = acmMetrics((HACMOBJ)had, ACM_METRIC_MAX_SIZE_FILTER, &size);
    check(mr == MMSYSERR_NOERROR && size == 40, "MAX_SIZE_FILTER of the open driver: %u %lu", mr, size);
    check(acmMetrics((HACMOBJ)had, ACM_METRIC_MAX_SIZE_FILTER, NULL) == MMSYSERR_INVALPARAM, "MAX_SIZE_FILTER, NULL");
    check(acmMetrics((HACMOBJ)1, ACM_METRIC_MAX_SIZE_FILTER, &size) == MMSYSERR_INVALHANDLE, "MAX_SIZE_FILTER, bad handle");
    {
        /* a driver without filters: the PCM converter (any format driver) */
        HACMDRIVERID pcm = NULL;
        mr = acmDriverID((HACMOBJ)had, &pcm, 0);
        check(mr == MMSYSERR_NOERROR && pcm == hadid, "acmDriverID of the open driver");
    }

    /* a software driver has no waveform-audio device */
    mr = acmMetrics((HACMOBJ)hadid, ACM_METRIC_HARDWARE_WAVE_INPUT, &value);
    check(mr == MMSYSERR_NOTSUPPORTED, "HARDWARE_WAVE_INPUT: %u", mr);
    mr = acmMetrics((HACMOBJ)hadid, ACM_METRIC_HARDWARE_WAVE_OUTPUT, &value);
    check(mr == MMSYSERR_NOTSUPPORTED, "HARDWARE_WAVE_OUTPUT: %u", mr);

    /* DRV_CONFIGURE without configuration info: msacm supplies one with the alias */
    configure_calls = 0;
    lr = acmDriverMessage((HACMDRIVER)hadid, DRV_CONFIGURE, 0, 0);
    check(lr == DRVCNF_OK && configure_calls == 1 && !lstrcmpW(config_alias, L"msacm.sgprobe"),
          "DRV_CONFIGURE to the driver identifier: %Id, alias %ls", lr, config_alias);
    configure_calls = 0;
    lr = acmDriverMessage(had, DRV_CONFIGURE, 0, 0);
    check(lr == DRVCNF_OK && configure_calls == 1 && !lstrcmpW(config_alias, L"msacm.sgprobe"),
          "DRV_CONFIGURE to the open driver: %Id, alias %ls", lr, config_alias);

    acmDriverClose(had, 0);
    acmDriverRemove(hadid, 0);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
