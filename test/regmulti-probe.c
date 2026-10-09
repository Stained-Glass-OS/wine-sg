/* Registry odds and ends in ntdll (patches/sg/2205):
 * NtQueryMultipleValueKey was a stub returning success without touching the
 * caller's buffers (and had the wrong prototype: the buffer length is a
 * PULONG in/out); NtQueryKey rejected the Virtualization / HandleTags / Trust
 * classes; NtSetInformationKey accepted any class and any length;
 * NtEnumerateValueKey rejected KeyValuePartialInformationAlign64. */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

#define S_BUFFER_OVERFLOW      ((NTSTATUS)0x80000005)
#define S_BUFFER_TOO_SMALL     ((NTSTATUS)0xC0000023)
#define S_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004)
#define S_INVALID_INFO_CLASS   ((NTSTATUS)0xC0000003)
#define S_INVALID_HANDLE       ((NTSTATUS)0xC0000008)
#define S_NAME_NOT_FOUND       ((NTSTATUS)0xC0000034)

typedef struct { PUNICODE_STRING ValueName; ULONG DataLength; ULONG DataOffset; ULONG Type; } MVENT;

static NTSTATUS (WINAPI *pNtQueryMultipleValueKey)(HANDLE, MVENT *, ULONG, void *, ULONG *, ULONG *);
static NTSTATUS (WINAPI *pNtQueryKey)(HANDLE, int, void *, ULONG, ULONG *);
static NTSTATUS (WINAPI *pNtSetInformationKey)(HANDLE, int, void *, ULONG);
static NTSTATUS (WINAPI *pNtEnumerateValueKey)(HANDLE, ULONG, int, void *, ULONG, ULONG *);
static NTSTATUS (WINAPI *pNtOpenKeyEx)(HANDLE *, ACCESS_MASK, OBJECT_ATTRIBUTES *, ULONG);
static void (WINAPI *pRtlInitUnicodeString)(UNICODE_STRING *, const WCHAR *);

enum { KeyVirtualizationInformation_ = 6, KeyHandleTagsInformation_ = 7, KeyTrustInformation_ = 8 };

int main(void)
{
    static const WCHAR na[] = L"A", nb[] = L"B", nc[] = L"C", nz[] = L"Missing";
    static const WCHAR hello[] = L"hello";
    static const BYTE bin[5] = { 1, 2, 3, 4, 5 };
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    UNICODE_STRING ua, ub, uc, uz;
    MVENT ent[3], bad[2];
    BYTE buf[256];
    ULONG len, retlen, v, size;
    NTSTATUS st;
    HKEY hkey;
    HANDLE h;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING kn;
    DWORD dw = 0x1234;
    char tmp[128];

    pNtQueryMultipleValueKey = (void *)GetProcAddress(ntdll, "NtQueryMultipleValueKey");
    pNtQueryKey = (void *)GetProcAddress(ntdll, "NtQueryKey");
    pNtSetInformationKey = (void *)GetProcAddress(ntdll, "NtSetInformationKey");
    pNtEnumerateValueKey = (void *)GetProcAddress(ntdll, "NtEnumerateValueKey");
    pNtOpenKeyEx = (void *)GetProcAddress(ntdll, "NtOpenKeyEx");
    pRtlInitUnicodeString = (void *)GetProcAddress(ntdll, "RtlInitUnicodeString");

    RegDeleteKeyA(HKEY_CURRENT_USER, "Software\\SGRegMulti");
    if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\SGRegMulti", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hkey, NULL))
    { printf("FAIL  cannot create test key\n"); return 1; }
    RegSetValueExW(hkey, na, 0, REG_SZ, (const BYTE *)hello, sizeof(hello));
    RegSetValueExW(hkey, nb, 0, REG_DWORD, (const BYTE *)&dw, 4);
    RegSetValueExW(hkey, nc, 0, REG_BINARY, bin, 5);

    pRtlInitUnicodeString(&ua, na); pRtlInitUnicodeString(&ub, nb);
    pRtlInitUnicodeString(&uc, nc); pRtlInitUnicodeString(&uz, nz);
    ent[0].ValueName = &ua; ent[1].ValueName = &ub; ent[2].ValueName = &uc;

    /* ---- NtQueryMultipleValueKey ---- */
    memset(buf, 0xcc, sizeof(buf));
    len = sizeof(buf); retlen = 0;
    st = pNtQueryMultipleValueKey(hkey, ent, 3, buf, &len, &retlen);
    check(st == 0, "three existing values are queried successfully");
    check(ent[0].Type == REG_SZ && ent[1].Type == REG_DWORD && ent[2].Type == REG_BINARY,
          "the types are filled in");
    check(ent[0].DataLength == sizeof(hello) && ent[1].DataLength == 4 && ent[2].DataLength == 5,
          "the data lengths are filled in");
    check(ent[0].DataOffset + ent[0].DataLength <= ent[1].DataOffset &&
          ent[1].DataOffset + ent[1].DataLength <= ent[2].DataOffset,
          "the data blocks are laid out in order without overlap");
    check(ent[2].DataOffset + ent[2].DataLength <= retlen && retlen <= sizeof(buf),
          "the returned length covers all the data");
    check(!memcmp(buf + ent[0].DataOffset, hello, sizeof(hello)), "string data is in the buffer");
    check(*(DWORD *)(buf + ent[1].DataOffset) == 0x1234, "dword data is in the buffer");
    check(!memcmp(buf + ent[2].DataOffset, bin, 5), "binary data is in the buffer");
    check(len == retlen, "the in/out length is the amount used");

    /* too small a buffer: BUFFER_OVERFLOW and the size needed */
    size = retlen;
    len = 8; retlen = 0;
    st = pNtQueryMultipleValueKey(hkey, ent, 3, buf, &len, &retlen);
    check(st == S_BUFFER_OVERFLOW, "a small buffer is STATUS_BUFFER_OVERFLOW");
    check(retlen == size, "...and the length needed is returned");

    /* a missing value fails the call */
    bad[0].ValueName = &ua; bad[1].ValueName = &uz;
    len = sizeof(buf); retlen = 0;
    st = pNtQueryMultipleValueKey(hkey, bad, 2, buf, &len, &retlen);
    check(st == S_NAME_NOT_FOUND, "a missing value is STATUS_OBJECT_NAME_NOT_FOUND");

    /* ---- NtQueryKey: ULONG classes ---- */
    for (v = KeyVirtualizationInformation_; v <= KeyTrustInformation_; v++)
    {
        ULONG val = 0xdeadbeef;
        sprintf(tmp, "NtQueryKey class %lu returns one zero ULONG", v);
        retlen = 0;
        st = pNtQueryKey(hkey, v, &val, sizeof(val), &retlen);
        check(st == 0 && val == 0 && retlen == 4, tmp);
        retlen = 0;
        st = pNtQueryKey(hkey, v, &val, 0, &retlen);
        sprintf(tmp, "...class %lu with no room is STATUS_BUFFER_TOO_SMALL and reports 4", v);
        check(st == S_BUFFER_TOO_SMALL && retlen == 4, tmp);
    }
    {
        ULONG val;
        st = pNtQueryKey((HANDLE)0x1234, KeyVirtualizationInformation_, &val, 4, &retlen);
        check(st == S_INVALID_HANDLE, "a bad handle is STATUS_INVALID_HANDLE");
    }

    /* ---- NtSetInformationKey ---- */
    v = 0;
    st = pNtSetInformationKey(hkey, 99, &v, sizeof(v));
    check(st == S_INVALID_INFO_CLASS, "a bogus information class is STATUS_INVALID_INFO_CLASS");
    st = pNtSetInformationKey(hkey, 1, &v, 2);
    check(st == S_INFO_LENGTH_MISMATCH, "a short buffer is STATUS_INFO_LENGTH_MISMATCH");
    st = pNtSetInformationKey(hkey, 1, &v, sizeof(v));
    check(st == 0, "KeyWow64FlagsInformation with a ULONG succeeds");

    /* ---- NtEnumerateValueKey: PartialInformationAlign64 ---- */
    {
        struct { ULONG Type; ULONG DataLength; UCHAR Data[64]; } info;
        memset(&info, 0xcc, sizeof(info));
        retlen = 0;
        st = pNtEnumerateValueKey(hkey, 1, 4 /* KeyValuePartialInformationAlign64 */, &info, sizeof(info), &retlen);
        check(st == 0, "KeyValuePartialInformationAlign64 enumerates");
        check(info.Type != 0xcccccccc && info.DataLength > 0 && retlen >= 8 + info.DataLength,
              "...with a type, a length and the data");
    }

    /* ---- NtOpenKeyEx with REG_OPTION_BACKUP_RESTORE ---- */
    pRtlInitUnicodeString(&kn, L"\\Registry\\Machine\\Software");
    InitializeObjectAttributes(&oa, &kn, OBJ_CASE_INSENSITIVE, NULL, NULL);
    h = NULL;
    st = pNtOpenKeyEx(&h, KEY_READ, &oa, 4);
    check(st == 0 && h, "NtOpenKeyEx accepts REG_OPTION_BACKUP_RESTORE");
    if (h) CloseHandle(h);

    RegCloseKey(hkey);
    RegDeleteKeyA(HKEY_CURRENT_USER, "Software\\SGRegMulti");
    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures ? 1 : 0;
}
