#include <windows.h>
#include <winspool.h>
#include <stdio.h>

static int failures, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { failures++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(void)
{
    DEVMODEA dm;
    FORM_INFO_1A f1;
    HANDLE bad = (HANDLE)0x1234;
    char buf[128];
    DWORD needed = 0, ret;

    /* IsValidDevmodeA */
    memset(&dm, 0, sizeof(dm));
    dm.dmSize = sizeof(dm);
    CHECK(IsValidDevmodeA(&dm, sizeof(dm)), "valid devmode");
    CHECK(!IsValidDevmodeA(NULL, sizeof(dm)), "null devmode");
    CHECK(!IsValidDevmodeA(&dm, 10), "tiny buffer");
    dm.dmDriverExtra = 100;
    CHECK(!IsValidDevmodeA(&dm, sizeof(dm)), "driver extra beyond buffer");
    dm.dmDriverExtra = 0;
    dm.dmFields = DM_PELSHEIGHT;
    dm.dmSize = FIELD_OFFSET(DEVMODEA, dmFields) + sizeof(DWORD);
    CHECK(!IsValidDevmodeA(&dm, sizeof(dm)), "field beyond dmSize");

    /* handle checks of the A wrappers */
    memset(&f1, 0, sizeof(f1));
    f1.pName = "Sg Test Form";
    SetLastError(0);
    CHECK(!AddFormA(bad, 1, (BYTE *)&f1) && GetLastError() == ERROR_INVALID_HANDLE, "AddFormA bad handle %lu", GetLastError());
    SetLastError(0);
    CHECK(!AddFormA(bad, 7, (BYTE *)&f1) && GetLastError() == ERROR_INVALID_LEVEL, "AddFormA bad level %lu", GetLastError());
    SetLastError(0);
    CHECK(!DeleteFormA(bad, "Sg Test Form") && GetLastError() == ERROR_INVALID_HANDLE, "DeleteFormA bad handle %lu", GetLastError());
    SetLastError(0);
    CHECK(!DeleteFormA(bad, NULL) && GetLastError() == ERROR_INVALID_PARAMETER, "DeleteFormA null name");
    SetLastError(0);
    CHECK(!SetFormA(bad, "Sg Test Form", 1, (BYTE *)&f1) && GetLastError() == ERROR_INVALID_HANDLE, "SetFormA bad handle %lu", GetLastError());
    SetLastError(0);
    CHECK(!ResetPrinterA(bad, NULL) && GetLastError() == ERROR_INVALID_HANDLE, "ResetPrinterA bad handle %lu", GetLastError());
    ret = EnumPrinterKeyA(bad, "", buf, sizeof(buf), &needed);
    CHECK(ret == ERROR_INVALID_HANDLE, "EnumPrinterKeyA bad handle %lu", ret);
    ret = EnumPrinterKeyA(bad, NULL, buf, sizeof(buf), &needed);
    CHECK(ret == ERROR_INVALID_PARAMETER, "EnumPrinterKeyA null key %lu", ret);
    CHECK(ExtDeviceMode(NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0) == -1, "ExtDeviceMode no device");

    /* with a printer, when there is one */
    {
        char name[256];
        DWORD len = sizeof(name);

        if (GetDefaultPrinterA(name, &len))
        {
            HANDLE h;

            if (OpenPrinterA(name, &h, NULL))
            {
                char keys[1024];
                ret = EnumPrinterKeyA(h, "", keys, sizeof(keys), &needed);
                CHECK(ret == ERROR_SUCCESS || ret == ERROR_FILE_NOT_FOUND, "EnumPrinterKeyA root %lu", ret);
                if (ret == ERROR_SUCCESS)
                {
                    DWORD n = 0;
                    const char *p;
                    for (p = keys; *p; p += strlen(p) + 1) n++;
                    printf("printer keys: %lu, bytes %lu\n", n, needed);
                    CHECK(needed >= 2, "double null");
                    ret = EnumPrinterKeyA(h, "", keys, 1, &needed);
                    CHECK(ret == ERROR_MORE_DATA, "small buffer %lu", ret);
                    CHECK(EnumPrinterKeyA(h, "NoSuchKey", keys, sizeof(keys), &needed) == ERROR_FILE_NOT_FOUND, "missing key");
                }
                ClosePrinter(h);
            }
            len = ExtDeviceMode(NULL, NULL, NULL, name, NULL, NULL, NULL, 0);
            printf("ExtDeviceMode size query: %ld\n", (LONG)len);
        }
        else printf("no default printer here\n");
    }
    printf("%d checks, %d failures\n", checks, failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
