/* tdh (patch 3587): decoding handles, provider enumeration from the registered event log publishers. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL  line %d  ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef struct { GUID guid; ULONG source, name_off; } PROV;
typedef struct { ULONG count, reserved; PROV p[1]; } ENUMINFO;

int main(void)
{
    HMODULE m = LoadLibraryA("tdh.dll");
    ULONG (WINAPI *open_h)(HANDLE *) = (void *)GetProcAddress(m, "TdhOpenDecodingHandle");
    ULONG (WINAPI *close_h)(HANDLE) = (void *)GetProcAddress(m, "TdhCloseDecodingHandle");
    ULONG (WINAPI *enum_p)(ENUMINFO *, ULONG *) = (void *)GetProcAddress(m, "TdhEnumerateProviders");
    ULONG (WINAPI *get_info)(void *, ULONG, void *, void *, ULONG *) = (void *)GetProcAddress(m, "TdhGetEventInformation");
    HANDLE h = NULL;
    ULONG size = 0, i;
    ENUMINFO *info;
    HKEY key;
    static const char pubs[] = "Software\\Microsoft\\Windows\\CurrentVersion\\WINEVT\\Publishers\\{12345678-1234-1234-1234-1234567890ab}";
    ULONG base;

    CHECK(open_h && close_h && enum_p && get_info, "exports");
    CHECK(open_h(NULL) == ERROR_INVALID_PARAMETER, "open NULL");
    CHECK(open_h(&h) == ERROR_SUCCESS && h, "open");
    CHECK(close_h(h) == ERROR_SUCCESS, "close");
    CHECK(close_h(NULL) == ERROR_INVALID_HANDLE, "close NULL");
    CHECK(get_info(NULL, 0, NULL, NULL, &size) == ERROR_INVALID_PARAMETER, "event info without event");
    {
        char event[128] = {0};
        CHECK(get_info(event, 0, NULL, NULL, &size) == ERROR_NOT_FOUND, "event info without schema");
    }

    CHECK(enum_p(NULL, NULL) == ERROR_INVALID_PARAMETER, "enum no size");
    size = 0;
    CHECK(enum_p(NULL, &size) == ERROR_INSUFFICIENT_BUFFER && size >= 8, "enum size %lu", size);
    base = size;

    CHECK(!RegCreateKeyExA(HKEY_LOCAL_MACHINE, pubs, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL), "create publisher key");
    RegSetValueExA(key, NULL, 0, REG_SZ, (const BYTE *)"Test Publisher", 15);
    RegCloseKey(key);
    size = 0;
    CHECK(enum_p(NULL, &size) == ERROR_INSUFFICIENT_BUFFER && size > base, "enum grows %lu > %lu", size, base);
    info = calloc(1, size);
    CHECK(enum_p(info, &size) == ERROR_SUCCESS, "enum");
    for (i = 0; i < info->count; i++)
        if (info->p[i].guid.Data1 == 0x12345678)
        {
            const WCHAR *name = (const WCHAR *)((BYTE *)info + info->p[i].name_off);
            CHECK(!wcscmp(name, L"Test Publisher"), "provider name %ls", name);
            CHECK(info->p[i].source == 0, "schema source");
            break;
        }
    CHECK(i < info->count, "registered publisher listed (%lu providers)", info->count);
    free(info);
    RegDeleteKeyA(HKEY_LOCAL_MACHINE, pubs);

    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
