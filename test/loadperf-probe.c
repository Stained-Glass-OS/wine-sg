/* loadperf probe: installs counter names from an .ini file, reads the registry back */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>

typedef DWORD (WINAPI *loadfn)(const WCHAR *, BOOL);
typedef DWORD (WINAPI *installfn)(const WCHAR *, const WCHAR *, ULONG_PTR);
typedef struct { ULONG TitleIndex, Type, DataLength; UCHAR Data[1]; } PARTIAL_INFO;
static NTSTATUS (WINAPI *pNtOpenKey)(HANDLE *, ACCESS_MASK, OBJECT_ATTRIBUTES *);
static NTSTATUS (WINAPI *pNtQueryValueKey)(HANDLE, UNICODE_STRING *, int, void *, ULONG, ULONG *);
static void (WINAPI *pRtlInitUnicodeString)(UNICODE_STRING *, const WCHAR *);

static void write_file(const char *path, const void *data, DWORD size)
{
    HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0);
    DWORD w;
    WriteFile(f, data, size, &w, NULL);
    CloseHandle(f);
}

static void write_ini(const char *path, const WCHAR *text)
{
    DWORD len = lstrlenW(text) * sizeof(WCHAR);
    BYTE *buf = HeapAlloc(GetProcessHeap(), 0, len + 2);
    buf[0] = 0xff; buf[1] = 0xfe;
    memcpy(buf + 2, text, len);
    write_file(path, buf, len + 2);
    HeapFree(GetProcessHeap(), 0, buf);
}

static void make_driver(const char *name)
{
    char key[200];
    HKEY k;
    sprintf(key, "SYSTEM\\CurrentControlSet\\Services\\%s\\Performance", name);
    RegCreateKeyExA(HKEY_LOCAL_MACHINE, key, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL);
    RegCloseKey(k);
}

static void dump_dwords(const char *tag, const char *name)
{
    char key[200];
    HKEY k;
    DWORD v, size, i;
    const char *vals[] = { "First Counter", "Last Counter", "First Help", "Last Help" };
    sprintf(key, "SYSTEM\\CurrentControlSet\\Services\\%s\\Performance", name);
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, key, 0, KEY_READ, &k)) { printf("%s: no key\n", tag); return; }
    printf("%s:", tag);
    for (i = 0; i < 4; i++)
    {
        size = sizeof(v);
        if (RegQueryValueExA(k, vals[i], NULL, NULL, (BYTE *)&v, &size)) printf(" -");
        else printf(" %lu", v);
    }
    printf("\n");
    RegCloseKey(k);
}

static void dump_perflib_last(void)
{
    HKEY k;
    DWORD a = 0, b = 0, size = sizeof(a);
    RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Perflib", 0, KEY_READ, &k);
    RegQueryValueExA(k, "Last Counter", NULL, NULL, (BYTE *)&a, &size);
    size = sizeof(b);
    RegQueryValueExA(k, "Last Help", NULL, NULL, (BYTE *)&b, &size);
    printf("perflib last: %lu %lu\n", a, b);
    RegCloseKey(k);
}

/* \Registry\... below ntdll, since kernelbase maps Perflib\009 to HKEY_PERFORMANCE_TEXT */
static void dump_text(const char *lang, const WCHAR *value)
{
    WCHAR path[200], vname[32];
    UNICODE_STRING name, vn;
    OBJECT_ATTRIBUTES attr;
    HANDLE key;
    char buf[8192];
    PARTIAL_INFO *info = (void *)buf;
    ULONG got;
    const WCHAR *p;
    WCHAR wl[16];

    MultiByteToWideChar(CP_ACP, 0, lang, -1, wl, 16);
    wsprintfW(path, L"\\Registry\\Machine\\Software\\Microsoft\\Windows NT\\CurrentVersion\\Perflib\\%s", wl);
    pRtlInitUnicodeString(&name, path);
    memset(&attr, 0, sizeof(attr));
    attr.Length = sizeof(attr); attr.ObjectName = &name; attr.Attributes = OBJ_CASE_INSENSITIVE;
    lstrcpyW(vname, value);
    pRtlInitUnicodeString(&vn, vname);
    printf("%s %ls:", lang, value);
    if (!strcmp(lang, "009"))
    {
        /* the text key of the performance registry */
        DWORD size = sizeof(buf);
        if (RegQueryValueExW(HKEY_PERFORMANCE_TEXT, value, NULL, NULL, (BYTE *)buf, &size)) { printf(" novalue\n"); return; }
        p = (const WCHAR *)buf;
    }
    else
    {
        if (pNtOpenKey(&key, KEY_READ, &attr)) { printf(" nokey\n"); return; }
        if (pNtQueryValueKey(key, &vn, 2, buf, sizeof(buf), &got)) { printf(" novalue\n"); CloseHandle(key); return; }
        p = (const WCHAR *)info->Data;
    }
    for (; *p;)
    {
        const WCHAR *t = p + lstrlenW(p) + 1;
        if (_wtoi(p) >= 1848) printf(" %ls=%ls", p, t);
        p = t + lstrlenW(t) + 1;
    }
    printf("\n");
}

int main(void)
{
    HMODULE lp = LoadLibraryA("loadperf.dll"), nt = GetModuleHandleA("ntdll.dll");
    loadfn load = (loadfn)GetProcAddress(lp, "LoadPerfCounterTextStringsW");
    loadfn unload = (loadfn)GetProcAddress(lp, "UnloadPerfCounterTextStringsW");
    installfn install = (installfn)GetProcAddress(lp, "InstallPerfDllW");
    static const WCHAR ini1[] = L"[info]\r\ndrivername=SgPerfTest\r\nsymbolfile=sgtest1.h\r\n\r\n[languages]\r\n009=English\r\n007=German\r\n\r\n"
        L"[text]\r\nSG_OBJ_009_NAME=Sg Object\r\nSG_OBJ_009_HELP=Sg object help\r\nSG_C_ONE_009_NAME=Sg One\r\nSG_C_ONE_009_HELP=Sg one help\r\n"
        L"SG_C_TWO_009_NAME=Sg Two\r\nSG_C_TWO_009_HELP=Sg two help\r\nSG_OBJ_007_NAME=Sg Objekt\r\nSG_OBJ_007_HELP=Sg Objekthilfe\r\n"
        L"SG_C_ONE_007_NAME=Sg Eins\r\nSG_C_ONE_007_HELP=Sg Einshilfe\r\n";
    static const WCHAR ini2[] = L"[info]\r\ndrivername=SgPerfTest2\r\nsymbolfile=sgtest2.h\r\n\r\n[languages]\r\n009=English\r\n\r\n"
        L"[text]\r\nOTHER_OBJ_009_NAME=Other Object\r\nOTHER_OBJ_009_HELP=Other help\r\n";
    static const char h1[] = "#ifndef X\r\n#define SG_OBJ 0\r\n  #define SG_C_ONE\t2 // one\r\n#define SG_C_TWO 4\r\n#endif\r\n";
    static const char h2[] = "#define OTHER_OBJ 0\r\n";
    DWORD r;

    pNtOpenKey = (void *)GetProcAddress(nt, "NtOpenKey");
    pNtQueryValueKey = (void *)GetProcAddress(nt, "NtQueryValueKey");
    pRtlInitUnicodeString = (void *)GetProcAddress(nt, "RtlInitUnicodeString");

    CreateDirectoryA("C:\\sgperf", NULL);
    write_file("C:\\sgperf\\sgtest1.h", h1, lstrlenA(h1));
    write_file("C:\\sgperf\\sgtest2.h", h2, lstrlenA(h2));
    write_ini("C:\\sgperf\\sgtest1.ini", ini1);
    write_ini("C:\\sgperf\\sgtest2.ini", ini2);
    make_driver("SgPerfTest");
    make_driver("SgPerfTest2");

    r = load(L"lodctr C:\\sgperf\\sgtest1.ini", TRUE);
    printf("load1 %lu\n", r);
    dump_dwords("test1", "SgPerfTest");
    dump_text("009", L"Counter"); dump_text("009", L"Help");
    dump_text("007", L"Counter"); dump_text("007", L"Help");
    r = install(NULL, L"C:\\sgperf\\sgtest2.ini", 0);
    printf("install2 %lu\n", r);
    dump_dwords("test2", "SgPerfTest2");
    r = install(NULL, L"C:\\sgperf\\sgtest1.ini", 0);
    printf("reinstall1 %lu\n", r);
    dump_dwords("test1b", "SgPerfTest");
    r = unload(L"unlodctr SgPerfTest", TRUE);
    printf("unload1 %lu\n", r);
    dump_dwords("test1c", "SgPerfTest");
    dump_text("009", L"Counter"); dump_text("007", L"Counter");
    dump_perflib_last();
    r = unload(L"unlodctr SgPerfTest", TRUE);
    printf("unload-again %lu\n", r);
    r = load(L"lodctr C:\\sgperf\\nonexistent.ini", TRUE);
    printf("load-missing %lu\n", r);
    unload(L"unlodctr SgPerfTest2", TRUE);
    return 0;
}
