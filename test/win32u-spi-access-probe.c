/* Probe for patches/sg/5216: the accessibility SPI_SET and SPI_GET pairs (STICKYKEYS, FILTERKEYS,
 * TOGGLEKEYS, MOUSEKEYS, ACCESSTIMEOUT, SOUNDSENTRY, SERIALKEYS, HIGHCONTRAST) keep what is set:
 * for the process without SPIF_UPDATEINIFILE, in the registry (Control Panel\Accessibility) and for
 * other processes with it; a wrong cbSize or no structure is refused.
 * "probe.exe get" is the second process.  Table driven; last line "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

struct feature
{
    const char *name;
    UINT get, set, size;
    const char *subkey;
    const char *value;      /* a field persisted as a registry string */
    UINT first_field;       /* offset of the first DWORD after cbSize that the test sets */
    UINT nfields;           /* how many DWORDs are set from there on */
    UINT reg_offset;        /* offset of the field mirrored in the registry value */
};

static const struct feature features[] =
{
    { "STICKYKEYS", SPI_GETSTICKYKEYS, SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), "StickyKeys", "Flags", 4, 1, 4 },
    { "FILTERKEYS", SPI_GETFILTERKEYS, SPI_SETFILTERKEYS, sizeof(FILTERKEYS), "Keyboard Response", "BounceTime", 4, 5, 20 },
    { "TOGGLEKEYS", SPI_GETTOGGLEKEYS, SPI_SETTOGGLEKEYS, sizeof(TOGGLEKEYS), "ToggleKeys", "Flags", 4, 1, 4 },
    { "MOUSEKEYS", SPI_GETMOUSEKEYS, SPI_SETMOUSEKEYS, sizeof(MOUSEKEYS), "MouseKeys", "MaximumSpeed", 4, 4, 8 },
    { "ACCESSTIMEOUT", SPI_GETACCESSTIMEOUT, SPI_SETACCESSTIMEOUT, sizeof(ACCESSTIMEOUT), "TimeOut", "TimeToWait", 4, 2, 8 },
    { "SOUNDSENTRY", SPI_GETSOUNDSENTRY, SPI_SETSOUNDSENTRY, sizeof(SOUNDSENTRYW), "SoundSentry", "Flags", 4, 9, 4 },
    { "SERIALKEYS", SPI_GETSERIALKEYS, SPI_SETSERIALKEYS, sizeof(SERIALKEYSW), "SerialKeys", "Flags", 4, 1, 4 },
    { "HIGHCONTRAST", SPI_GETHIGHCONTRAST, SPI_SETHIGHCONTRAST, sizeof(HIGHCONTRASTW), "HighContrast", "Flags", 4, 1, 4 },
};

static DWORD pattern(unsigned int feature, unsigned int n) { return 3 + feature * 16 + n; }

static void fill(const struct feature *f, unsigned int fi, BYTE *buf)
{
    unsigned int n;
    memset(buf, 0, 256);
    *(UINT *)buf = f->size;
    for (n = 0; n < f->nfields; n++) *(DWORD *)(buf + f->first_field + 4 * n) = pattern(fi, n);
}

static DWORD reg_number(const char *subkey, const char *value)
{
    char path[256], str[64];
    DWORD size = sizeof(str), type;
    HKEY key;

    snprintf(path, sizeof(path), "Control Panel\\Accessibility\\%s", subkey);
    if (RegOpenKeyExA(HKEY_CURRENT_USER, path, 0, KEY_READ, &key)) return 0xffffffff;
    if (RegQueryValueExA(key, value, NULL, &type, (BYTE *)str, &size) || type != REG_SZ) { RegCloseKey(key); return 0xfffffffe; }
    RegCloseKey(key);
    return strtoul(str, NULL, 10);
}

int main(int argc, char **argv)
{
    BYTE buf[256], want[256];
    unsigned int fi, n;
    BOOL ret;

    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc > 1 && !strcmp(argv[1], "get"))
    {
        /* the second process: what the first one saved is there */
        for (fi = 0; fi < ARRAYSIZE(features); fi++)
        {
            const struct feature *f = &features[fi];
            memset(buf, 0, sizeof(buf)); *(UINT *)buf = f->size;
            ret = SystemParametersInfoW(f->get, f->size, buf, 0);
            fill(f, fi, want);
            check(ret && !memcmp(buf + f->first_field, want + f->first_field, 4 * f->nfields), "other process: %s kept", f->name);
        }
        printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
        return failures != 0;
    }

    for (fi = 0; fi < ARRAYSIZE(features); fi++)
    {
        const struct feature *f = &features[fi];

        /* defaults: the feature is not on */
        memset(buf, 0xcc, sizeof(buf)); *(UINT *)buf = f->size;
        ret = SystemParametersInfoW(f->get, f->size, buf, 0);
        check(ret, "%s: get works", f->name);

        /* a set without SPIF_UPDATEINIFILE is read back at once */
        fill(f, fi, buf);
        ret = SystemParametersInfoW(f->set, f->size, buf, 0);
        check(ret, "%s: set (%lu)", f->name, GetLastError());
        memset(want, 0, sizeof(want)); *(UINT *)want = f->size;
        ret = SystemParametersInfoW(f->get, f->size, want, 0);
        fill(f, fi, buf);
        check(ret && !memcmp(want + f->first_field, buf + f->first_field, 4 * f->nfields), "%s: read back", f->name);

        /* a wrong size or no structure is refused */
        fill(f, fi, buf);
        *(UINT *)buf = f->size + 4;
        SetLastError(0xdeadbeef);
        ret = SystemParametersInfoW(f->set, f->size, buf, 0);
        check(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "%s: wrong cbSize refused (%d, %lu)", f->name, ret, GetLastError());
        SetLastError(0xdeadbeef);
        ret = SystemParametersInfoW(f->set, f->size, NULL, 0);
        check(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "%s: no structure refused (%d, %lu)", f->name, ret, GetLastError());

        /* saved: the registry has it */
        fill(f, fi, buf);
        ret = SystemParametersInfoW(f->set, f->size, buf, SPIF_UPDATEINIFILE);
        check(ret, "%s: set and save", f->name);
        n = reg_number(f->subkey, f->value);
        check(n == *(DWORD *)(buf + f->reg_offset), "%s: registry %s\\%s = %lu (want %lu)", f->name, f->subkey, f->value, (DWORD)n,
              *(DWORD *)(buf + f->reg_offset));
    }

    /* another process reads what was saved */
    {
        char cmd[MAX_PATH + 16], path[MAX_PATH];
        STARTUPINFOA sti = {sizeof(sti)};
        PROCESS_INFORMATION pi;
        DWORD code = 1;
        GetModuleFileNameA(NULL, path, sizeof(path));
        snprintf(cmd, sizeof(cmd), "\"%s\" get", path);
        if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &sti, &pi))
        {
            WaitForSingleObject(pi.hProcess, 20000);
            GetExitCodeProcess(pi.hProcess, &code);
            check(code == 0, "second process saw the saved settings (exit %lu)", code);
            CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        }
        else check(0, "second process started");
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
