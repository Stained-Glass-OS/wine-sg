/* Probe for patches/sg/5003: NtQueryValueKey and NtEnumerateValueKey with
 * KeyValueFullInformationAlign64 give the full layout in an 8-byte aligned
 * buffer and STATUS_DATATYPE_MISALIGNMENT for a misaligned one. Table driven;
 * lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>
#include <winternl.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#define KeyValueFullInformationAlign64_ 3

/* KEY_VALUE_FULL_INFORMATION (wdm.h) */
typedef struct { ULONG TitleIndex, Type, DataOffset, DataLength, NameLength; WCHAR Name[1]; } KVFI;

int main(int argc, char **argv)
{
    HMODULE nt = GetModuleHandleA("ntdll.dll");
    NTSTATUS (WINAPI *query)(HANDLE, UNICODE_STRING *, int, void *, ULONG, ULONG *) = (void *)GetProcAddress(nt, "NtQueryValueKey");
    NTSTATUS (WINAPI *enumv)(HANDLE, ULONG, int, void *, ULONG, ULONG *) = (void *)GetProcAddress(nt, "NtEnumerateValueKey");
    static BYTE buf[512] __attribute__((aligned(16)));
    UNICODE_STRING name;
    KVFI *full;
    DWORD data = 0x12345678;
    HKEY key;
    ULONG len;
    NTSTATUS status;
    unsigned int off;

    RegCreateKeyA(HKEY_CURRENT_USER, "Software\\sg-align64", &key);
    RegSetValueExW(key, L"val", 0, REG_DWORD, (BYTE *)&data, sizeof(data));
    name.Buffer = (WCHAR *)L"val"; name.Length = 6; name.MaximumLength = 8;

    for (off = 0; off < 16; off += 4)
    {
        full = (KVFI *)(buf + off);
        memset(buf, 0xcc, sizeof(buf));
        status = query(key, &name, KeyValueFullInformationAlign64_, full, sizeof(buf) - 16, &len);
        if (off % 8)
            check(status == STATUS_DATATYPE_MISALIGNMENT, "query at offset %u: misaligned %#lx", off, status);
        else
            check(!status && full->Type == REG_DWORD && full->NameLength == 6 && full->DataLength == 4 &&
                  *(DWORD *)((BYTE *)full + full->DataOffset) == data, "query at offset %u: %#lx", off, status);
        memset(buf, 0xcc, sizeof(buf));
        status = enumv(key, 0, KeyValueFullInformationAlign64_, full, sizeof(buf) - 16, &len);
        if (off % 8)
            check(status == STATUS_DATATYPE_MISALIGNMENT, "enumerate at offset %u: misaligned %#lx", off, status);
        else
            check(!status && full->Type == REG_DWORD && full->NameLength == 6 &&
                  *(DWORD *)((BYTE *)full + full->DataOffset) == data, "enumerate at offset %u: %#lx", off, status);
    }
    RegDeleteValueA(key, "val");
    RegCloseKey(key);
    RegDeleteKeyA(HKEY_CURRENT_USER, "Software\\sg-align64");
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
