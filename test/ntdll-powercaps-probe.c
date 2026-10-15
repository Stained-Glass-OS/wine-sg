/* NtPowerInformation(SystemPowerCapabilities): the machine's own buttons, lid, S3 and disk
 * spin-down, checked against what the host says (read through Z:, the Unix root). */
#include <windows.h>
#include <winternl.h>
#include <powrprof.h>
#include <stdio.h>
#include <string.h>

typedef LONG (WINAPI *power_fn)(int, void *, ULONG, void *, ULONG);

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static int read_file(const char *path, char *buf, size_t size)
{
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    DWORD got, total = 0;

    if (h == INVALID_HANDLE_VALUE) return 0;
    while (total + 1 < size && ReadFile(h, buf + total, size - 1 - total, &got, NULL) && got) total += got;
    buf[total] = 0;
    CloseHandle(h);
    return 1;
}

static int spinning_disk(void)
{
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA("Z:\\sys\\block\\*", &fd);
    int ret = 0;

    if (h == INVALID_HANDLE_VALUE) return 0;
    do
    {
        char path[300], value[16];
        const char *n = fd.cFileName;
        if (n[0] == '.' || !strncmp(n, "loop", 4) || !strncmp(n, "ram", 3) || !strncmp(n, "zram", 4)
            || !strncmp(n, "dm-", 3) || !strncmp(n, "sr", 2) || !strncmp(n, "nbd", 3)) continue;
        sprintf(path, "Z:\\sys\\block\\%s\\queue\\rotational", n);
        if (read_file(path, value, sizeof(value)) && value[0] == '1') ret = 1;
    } while (!ret && FindNextFileA(h, &fd));
    FindClose(h);
    return ret;
}

int main(void)
{
    power_fn NtPowerInformation = (power_fn)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtPowerInformation");
    SYSTEM_POWER_CAPABILITIES caps;
    static char devices[65536], state[256], mem_sleep[256];
    int have_devices, have_mem_sleep, have_state;
    LONG status;
    struct { const char *name; BOOLEAN actual; BOOLEAN expected; } rows[5];
    int i, lid_expected, pb_expected;
    DWORD attr;

    status = NtPowerInformation(4 /* SystemPowerCapabilities */, NULL, 0, &caps, sizeof(caps));
    check(status == 0, "SystemPowerCapabilities succeeds");
    status = NtPowerInformation(4, NULL, 0, &caps, sizeof(caps) - 1);
    check(status == (LONG)0xC0000023, "a short buffer is STATUS_BUFFER_TOO_SMALL");
    NtPowerInformation(4, NULL, 0, &caps, sizeof(caps));

    have_devices = read_file("Z:\\proc\\bus\\input\\devices", devices, sizeof(devices));
    have_mem_sleep = read_file("Z:\\sys\\power\\mem_sleep", mem_sleep, sizeof(mem_sleep));
    have_state = read_file("Z:\\sys\\power\\state", state, sizeof(state));
    printf("      devices %d mem_sleep %d state %d\n", have_devices, have_mem_sleep, have_state);

    attr = GetFileAttributesA("Z:\\proc\\acpi\\button\\lid");
    lid_expected = (have_devices && strstr(devices, "Name=\"Lid Switch\"")) || attr != INVALID_FILE_ATTRIBUTES;
    pb_expected = !have_devices || strstr(devices, "Name=\"Power Button\"") != NULL;

    rows[0].name = "PowerButtonPresent"; rows[0].actual = caps.PowerButtonPresent; rows[0].expected = pb_expected;
    rows[1].name = "SleepButtonPresent"; rows[1].actual = caps.SleepButtonPresent;
    rows[1].expected = have_devices && strstr(devices, "Name=\"Sleep Button\"") != NULL;
    rows[2].name = "LidPresent"; rows[2].actual = caps.LidPresent; rows[2].expected = lid_expected;
    rows[3].name = "SystemS3"; rows[3].actual = caps.SystemS3;
    rows[3].expected = have_mem_sleep ? strstr(mem_sleep, "deep") != NULL : (have_state && strstr(state, "mem") != NULL);
    rows[4].name = "DiskSpinDown"; rows[4].actual = caps.DiskSpinDown; rows[4].expected = spinning_disk();
    for (i = 0; i < 5; ++i)
    {
        char what[100];
        printf("      %s: got %d, host says %d\n", rows[i].name, rows[i].actual, rows[i].expected);
        sprintf(what, "%s follows the host", rows[i].name);
        check(!!rows[i].actual == !!rows[i].expected, what);
    }

    /* the fields that were always right stay as they are */
    check(caps.SystemS5 && caps.FullWake, "S5 and full wake");
    check(caps.ProcessorMaxThrottle == 100, "processor max throttle");

    /* through the Win32 call as well */
    {
        BOOLEAN (WINAPI *pGetPwrCapabilities)(PSYSTEM_POWER_CAPABILITIES);
        SYSTEM_POWER_CAPABILITIES caps2;
        HMODULE powrprof = LoadLibraryA("powrprof.dll");
        pGetPwrCapabilities = (void *)GetProcAddress(powrprof, "GetPwrCapabilities");
        memset(&caps2, 0, sizeof(caps2));
        check(pGetPwrCapabilities && pGetPwrCapabilities(&caps2) && caps2.LidPresent == caps.LidPresent
                && caps2.DiskSpinDown == caps.DiskSpinDown, "GetPwrCapabilities gives the same answer");
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
