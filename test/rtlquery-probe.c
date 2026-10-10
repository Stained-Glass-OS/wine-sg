/* RtlQueryRegistryValues SUBKEY / NOVALUE-with-name / REQUIRED-with-default, and
 * NtOpenKey with no access (patches/sg/2213).  Runs against a key of its own
 * under HKLM\Software. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef LONG NTSTATUS;
typedef NTSTATUS (WINAPI *QUERY_ROUTINE)(const WCHAR *name, ULONG type, void *data, ULONG len, void *ctx, void *ectx);
typedef struct
{
    QUERY_ROUTINE routine;
    ULONG flags;
    WCHAR *name;
    void *entry_ctx;
    ULONG default_type;
    void *default_data;
    ULONG default_len;
} QTABLE;
typedef struct { ULONG Length; HANDLE Root; void *ObjectName; ULONG Attr; void *SD, *QoS; } OA;
typedef struct { USHORT Length, MaximumLength; WCHAR *Buffer; } USTR;

#define STATUS_SUCCESS 0
#define STATUS_OBJECT_NAME_NOT_FOUND ((NTSTATUS)0xC0000034)
#define STATUS_ACCESS_DENIED_ ((NTSTATUS)0xC0000022)
#define RTL_QUERY_REGISTRY_SUBKEY   0x01
#define RTL_QUERY_REGISTRY_REQUIRED 0x04
#define RTL_QUERY_REGISTRY_NOVALUE  0x08
#define RTL_REGISTRY_ABSOLUTE 0

static NTSTATUS (WINAPI *pRtlQueryRegistryValues)(ULONG, const WCHAR *, QTABLE *, void *, void *);
static NTSTATUS (WINAPI *pNtOpenKey)(HANDLE *, ACCESS_MASK, OA *);
static NTSTATUS (WINAPI *pNtClose)(HANDLE);

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

struct seen { int calls; WCHAR name[40]; ULONG type; WCHAR text[40]; ULONG len; };
static NTSTATUS WINAPI routine(const WCHAR *name, ULONG type, void *data, ULONG len, void *ctx, void *ectx)
{
    struct seen *s = ctx;

    s->calls++;
    lstrcpynW(s->name, name ? name : L"(null)", 40);
    s->type = type;
    s->len = len;
    s->text[0] = 0;
    if ((type == REG_SZ || type == REG_EXPAND_SZ) && data) lstrcpynW(s->text, data, 40);
    return STATUS_SUCCESS;
}

int main(void)
{
    static const WCHAR path[] = L"\\Registry\\Machine\\Software\\SgProbeRtlQuery";
    HKEY key, sub;
    struct seen s;
    QTABLE q[3];
    NTSTATUS st;
    HANDLE h = (HANDLE)0xdeadbeef;
    USTR us = { sizeof(path) - sizeof(WCHAR), sizeof(path), (WCHAR *)path };
    OA oa = { sizeof(oa), NULL, &us, 0x40, NULL, NULL };
    HMODULE nt = GetModuleHandleA("ntdll.dll");

    pRtlQueryRegistryValues = (void *)GetProcAddress(nt, "RtlQueryRegistryValues");
    pNtOpenKey = (void *)GetProcAddress(nt, "NtOpenKey");
    pNtClose = (void *)GetProcAddress(nt, "NtClose");

    RegDeleteTreeA(HKEY_LOCAL_MACHINE, "Software\\SgProbeRtlQuery");
    if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, "Software\\SgProbeRtlQuery", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL)) { printf("FAIL  cannot create the test key\n"); return 1; }
    RegSetValueExA(key, "WindowsDrive", 0, REG_SZ, (BYTE *)"C:", 3);
    RegCreateKeyExA(key, "subkey", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &sub, NULL);
    RegSetValueExA(sub, "Color", 0, REG_SZ, (BYTE *)"Yellow", 7);

    /* NOVALUE is ignored when the entry has a name */
    memset(q, 0, sizeof(q));
    memset(&s, 0, sizeof(s));
    q[0].routine = routine; q[0].flags = RTL_QUERY_REGISTRY_NOVALUE; q[0].name = (WCHAR *)L"WindowsDrive";
    st = pRtlQueryRegistryValues(RTL_REGISTRY_ABSOLUTE, path, q, &s, NULL);
    check(st == STATUS_SUCCESS && s.calls == 1 && s.type == REG_SZ && !lstrcmpW(s.text, L"C:") && s.len == 6,
          "NOVALUE with a name still reads the value (REG_SZ \"C:\")");
    /* NOVALUE without a name only calls back, with no data */
    memset(&s, 0, sizeof(s));
    memset(q, 0, sizeof(q));
    q[0].routine = routine; q[0].flags = RTL_QUERY_REGISTRY_NOVALUE;
    st = pRtlQueryRegistryValues(RTL_REGISTRY_ABSOLUTE, path, q, &s, NULL);
    check(st == STATUS_SUCCESS && s.calls == 1 && s.type == REG_NONE && s.len == 0, "NOVALUE without a name: one callback, REG_NONE, no data");

    /* SUBKEY with a routine reports the subkey's values */
    memset(&s, 0, sizeof(s));
    memset(q, 0, sizeof(q));
    q[0].routine = routine; q[0].flags = RTL_QUERY_REGISTRY_SUBKEY; q[0].name = (WCHAR *)L"subkey";
    st = pRtlQueryRegistryValues(RTL_REGISTRY_ABSOLUTE, path, q, &s, NULL);
    check(st == STATUS_SUCCESS && s.calls == 1 && s.type == REG_SZ && !lstrcmpW(s.text, L"Yellow"),
          "SUBKEY with a routine reports the values of the subkey");
    /* SUBKEY changes the key for the entries after it */
    memset(&s, 0, sizeof(s));
    memset(q, 0, sizeof(q));
    q[0].flags = RTL_QUERY_REGISTRY_SUBKEY; q[0].name = (WCHAR *)L"subkey";
    q[1].routine = routine; q[1].name = (WCHAR *)L"Color";
    st = pRtlQueryRegistryValues(RTL_REGISTRY_ABSOLUTE, path, q, &s, NULL);
    check(st == STATUS_SUCCESS && s.calls == 1 && !lstrcmpW(s.text, L"Yellow"), "SUBKEY without a routine only changes the key for the next entry");
    /* a subkey that is not there */
    memset(&s, 0, sizeof(s));
    memset(q, 0, sizeof(q));
    q[0].flags = RTL_QUERY_REGISTRY_SUBKEY; q[0].name = (WCHAR *)L"nosuchsubkey";
    q[1].routine = routine; q[1].name = (WCHAR *)L"Color";
    st = pRtlQueryRegistryValues(RTL_REGISTRY_ABSOLUTE, path, q, &s, NULL);
    check(st != STATUS_SUCCESS && s.calls == 0, "a SUBKEY that does not exist fails and nothing is reported");

    /* REQUIRED with a default uses the default */
    memset(&s, 0, sizeof(s));
    memset(q, 0, sizeof(q));
    q[0].routine = routine; q[0].flags = RTL_QUERY_REGISTRY_REQUIRED; q[0].name = (WCHAR *)L"I don't exist";
    q[0].default_type = REG_SZ; q[0].default_data = (void *)L"Some default"; q[0].default_len = 0;
    st = pRtlQueryRegistryValues(RTL_REGISTRY_ABSOLUTE, path, q, &s, NULL);
    check(st == STATUS_SUCCESS && s.calls == 1 && !lstrcmpW(s.text, L"Some default"), "REQUIRED with a default value reports the default");
    q[0].default_type = REG_NONE;
    memset(&s, 0, sizeof(s));
    st = pRtlQueryRegistryValues(RTL_REGISTRY_ABSOLUTE, path, q, &s, NULL);
    check(st == STATUS_OBJECT_NAME_NOT_FOUND && s.calls == 0, "REQUIRED without a default is STATUS_OBJECT_NAME_NOT_FOUND");

    /* NtOpenKey with no access */
    h = (HANDLE)0xdeadbeef;
    st = pNtOpenKey(&h, 0, &oa);
    check(st == STATUS_ACCESS_DENIED_ && !h, "NtOpenKey with an empty access mask is STATUS_ACCESS_DENIED and no handle");
    h = NULL;
    st = pNtOpenKey(&h, KEY_READ, &oa);
    check(st == STATUS_SUCCESS && h, "...with KEY_READ it opens");
    if (h) pNtClose(h);

    RegCloseKey(sub);
    RegCloseKey(key);
    RegDeleteTreeA(HKEY_LOCAL_MACHINE, "Software\\SgProbeRtlQuery");
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
