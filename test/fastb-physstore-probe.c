/* CertRegisterPhysicalStore, CertUnregisterPhysicalStore, registered physical stores (patch 3756) */
#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>
#include <string.h>

static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const BYTE cert_1[] = {0x30,0x82,0x01,0x86,0x30,0x82,0x01,0x2d,0xa0,0x03,0x02,0x01,0x02,0x02,0x14,0x3c,0x6b,0xd8,0x60,0xb9,0xa5,0xac,0x1d,0x7f,0x7c,0x9b,0x17,0xd5,0x46,0x98,0x69,0x48,0xe2,0x81,0xd0,0x30,0x0a,0x06,0x08,0x2a,0x86,0x48,0xce,0x3d,0x04,0x03,0x02,0x30,0x19,0x31,0x17,0x30,0x15,0x06,0x03,0x55,0x04,0x03,0x0c,0x0e,0x46,0x61,0x73,0x74,0x42,0x20,0x54,0x65,0x73,0x74,0x20,0x4f,0x6e,0x65,0x30,0x1e,0x17,0x0d,0x32,0x36,0x31,0x30,0x31,0x35,0x30,0x38,0x34,0x32,0x30,0x39,0x5a,0x17,0x0d,0x33,0x36,0x31,0x30,0x31,0x32,0x30,0x38,0x34,0x32,0x30,0x39,0x5a,0x30,0x19,0x31,0x17,0x30,0x15,0x06,0x03,0x55,0x04,0x03,0x0c,0x0e,0x46,0x61,0x73,0x74,0x42,0x20,0x54,0x65,0x73,0x74,0x20,0x4f,0x6e,0x65,0x30,0x59,0x30,0x13,0x06,0x07,0x2a,0x86,0x48,0xce,0x3d,0x02,0x01,0x06,0x08,0x2a,0x86,0x48,0xce,0x3d,0x03,0x01,0x07,0x03,0x42,0x00,0x04,0x54,0xd1,0x5e,0x52,0x4e,0x53,0xee,0x10,0x85,0xec,0x58,0xd2,0x97,0x60,0x9f,0x61,0xee,0xc2,0xc7,0x5b,0x29,0xc6,0x1c,0x90,0x22,0xac,0x3f,0x4e,0x1b,0x30,0xaa,0x21,0xfa,0xf9,0x09,0xbe,0x4f,0xf0,0x51,0x24,0xcd,0x80,0xcc,0x6d,0x14,0x43,0xa8,0x2b,0xd8,0x48,0x31,0x9f,0xc3,0x06,0xcf,0xa1,0xfe,0xda,0xf2,0xf8,0xf8,0xa3,0x55,0x9f,0xa3,0x53,0x30,0x51,0x30,0x1d,0x06,0x03,0x55,0x1d,0x0e,0x04,0x16,0x04,0x14,0x2c,0x36,0xa2,0x9d,0x07,0xf4,0xf8,0x9a,0x9b,0x76,0x67,0x29,0x45,0x8d,0x8e,0x26,0x20,0x4a,0x05,0xe2,0x30,0x1f,0x06,0x03,0x55,0x1d,0x23,0x04,0x18,0x30,0x16,0x80,0x14,0x2c,0x36,0xa2,0x9d,0x07,0xf4,0xf8,0x9a,0x9b,0x76,0x67,0x29,0x45,0x8d,0x8e,0x26,0x20,0x4a,0x05,0xe2,0x30,0x0f,0x06,0x03,0x55,0x1d,0x13,0x01,0x01,0xff,0x04,0x05,0x30,0x03,0x01,0x01,0xff,0x30,0x0a,0x06,0x08,0x2a,0x86,0x48,0xce,0x3d,0x04,0x03,0x02,0x03,0x47,0x00,0x30,0x44,0x02,0x20,0x68,0x61,0x09,0x28,0x10,0x8a,0xde,0xb2,0x8c,0x50,0x54,0x37,0x72,0x98,0x1f,0xc4,0x19,0x45,0x1f,0x07,0xba,0xf8,0xae,0x9c,0x67,0x9f,0x7a,0x14,0x23,0x49,0x7a,0x23,0x02,0x20,0x68,0x1e,0x64,0x98,0x58,0x13,0x37,0x10,0x79,0x9e,0x77,0x02,0x5d,0x87,0x10,0x03,0x0b,0x89,0xce,0xc1,0x90,0x25,0xe2,0x86,0xe1,0x1f,0xa6,0x71,0xa6,0x5b,0x07,0xc4};
static const BYTE cert_2[] = {0x30,0x82,0x01,0x86,0x30,0x82,0x01,0x2d,0xa0,0x03,0x02,0x01,0x02,0x02,0x14,0x4d,0x61,0xa5,0xee,0x87,0x52,0x6e,0xf9,0xda,0x2d,0xcc,0x26,0x91,0xf1,0xf8,0xfb,0xab,0x4a,0xea,0x04,0x30,0x0a,0x06,0x08,0x2a,0x86,0x48,0xce,0x3d,0x04,0x03,0x02,0x30,0x19,0x31,0x17,0x30,0x15,0x06,0x03,0x55,0x04,0x03,0x0c,0x0e,0x46,0x61,0x73,0x74,0x42,0x20,0x54,0x65,0x73,0x74,0x20,0x54,0x77,0x6f,0x30,0x1e,0x17,0x0d,0x32,0x36,0x31,0x30,0x31,0x35,0x30,0x38,0x34,0x32,0x30,0x39,0x5a,0x17,0x0d,0x33,0x36,0x31,0x30,0x31,0x32,0x30,0x38,0x34,0x32,0x30,0x39,0x5a,0x30,0x19,0x31,0x17,0x30,0x15,0x06,0x03,0x55,0x04,0x03,0x0c,0x0e,0x46,0x61,0x73,0x74,0x42,0x20,0x54,0x65,0x73,0x74,0x20,0x54,0x77,0x6f,0x30,0x59,0x30,0x13,0x06,0x07,0x2a,0x86,0x48,0xce,0x3d,0x02,0x01,0x06,0x08,0x2a,0x86,0x48,0xce,0x3d,0x03,0x01,0x07,0x03,0x42,0x00,0x04,0xd5,0xd3,0x6c,0x02,0xa9,0x1e,0xbb,0x25,0xec,0x29,0x12,0x97,0x4b,0x55,0xb5,0xc2,0x98,0xa0,0x7c,0x54,0xcf,0xdd,0xc7,0x4b,0x74,0xaf,0x0e,0xec,0x9d,0xb9,0x0e,0x48,0xdb,0xc0,0xd2,0x50,0x1c,0x8d,0xed,0xdb,0x08,0x49,0xba,0xeb,0x02,0xa6,0x0b,0xd8,0x23,0xee,0x3d,0x8a,0xda,0x5f,0xff,0x08,0xa3,0x20,0xde,0xd4,0xf1,0xa7,0x48,0xd9,0xa3,0x53,0x30,0x51,0x30,0x1d,0x06,0x03,0x55,0x1d,0x0e,0x04,0x16,0x04,0x14,0xe8,0x0a,0x14,0xc1,0x77,0x71,0xb8,0x49,0x9f,0xbf,0x7c,0x39,0xe4,0x1b,0xc1,0xc4,0xa4,0x1b,0x27,0x15,0x30,0x1f,0x06,0x03,0x55,0x1d,0x23,0x04,0x18,0x30,0x16,0x80,0x14,0xe8,0x0a,0x14,0xc1,0x77,0x71,0xb8,0x49,0x9f,0xbf,0x7c,0x39,0xe4,0x1b,0xc1,0xc4,0xa4,0x1b,0x27,0x15,0x30,0x0f,0x06,0x03,0x55,0x1d,0x13,0x01,0x01,0xff,0x04,0x05,0x30,0x03,0x01,0x01,0xff,0x30,0x0a,0x06,0x08,0x2a,0x86,0x48,0xce,0x3d,0x04,0x03,0x02,0x03,0x47,0x00,0x30,0x44,0x02,0x20,0x04,0x1c,0x0d,0x74,0xd5,0x07,0x54,0xb3,0xb7,0xe6,0x98,0x15,0x5f,0x1f,0xdc,0xff,0x4f,0xb8,0x58,0x64,0xfd,0xe1,0x79,0x39,0x35,0x5c,0x8f,0x4a,0xd1,0xb9,0x72,0x48,0x02,0x20,0x4e,0x05,0x21,0x1e,0x1d,0x5a,0xb4,0xb4,0xf7,0x50,0xb3,0x49,0x9d,0x06,0x95,0x85,0x38,0x49,0x32,0x07,0x9b,0x75,0x84,0xfb,0x02,0xcc,0xc1,0x88,0x32,0x12,0xd6,0x1d};

#define CU CERT_SYSTEM_STORE_CURRENT_USER
#define LM CERT_SYSTEM_STORE_LOCAL_MACHINE

static const WCHAR SYS[] = L"FastbSys";
static const WCHAR PHYS_A[] = L"FastbPhysA";   /* the registry system store a physical store points at */
static const WCHAR PHYS_B[] = L"FastbPhysB";

static HCERTSTORE open_sys(DWORD loc, const WCHAR *name, DWORD extra)
{
    return CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0, loc | extra, name);
}
static int count(HCERTSTORE s)
{
    int n = 0;
    PCCERT_CONTEXT c = NULL;
    while ((c = CertEnumCertificatesInStore(s, c))) n++;
    return n;
}
static int count_sys(DWORD loc, const WCHAR *name)
{
    HCERTSTORE s = open_sys(loc, name, 0);
    int n;
    if (!s) return -1;
    n = count(s);
    CertCloseStore(s, 0);
    return n;
}
static void put(DWORD loc, const WCHAR *name, const BYTE *cert, DWORD len)
{
    HCERTSTORE s = CertOpenStore(CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, 0, loc, name);
    if (!s) { printf("FAIL cannot open %ls\n", name); f++; return; }
    CertAddEncodedCertificateToStore(s, X509_ASN_ENCODING, cert, len, CERT_STORE_ADD_REPLACE_EXISTING, NULL);
    CertCloseStore(s, 0);
}
static void clear_sys(DWORD loc, const WCHAR *name)
{
    CertUnregisterSystemStore(name, loc);
}

static CERT_PHYSICAL_STORE_INFO mkinfo(const WCHAR *param, DWORD flags, DWORD prio)
{
    CERT_PHYSICAL_STORE_INFO i;
    memset(&i, 0, sizeof(i));
    i.cbSize = sizeof(i);
    i.pszOpenStoreProvider = (LPSTR)sz_CERT_STORE_PROV_SYSTEM_REGISTRY_W;
    i.dwOpenEncodingType = 0;
    i.dwOpenFlags = CERT_SYSTEM_STORE_CURRENT_USER;
    i.OpenParameters.cbData = (lstrlenW(param) + 1) * sizeof(WCHAR);
    i.OpenParameters.pbData = (BYTE *)param;
    i.dwFlags = flags;
    i.dwPriority = prio;
    return i;
}

struct en { int n; WCHAR names[16][64]; CERT_PHYSICAL_STORE_INFO info[16]; char prov[16][64]; BYTE par[16][64]; };
static BOOL WINAPI en_cb(const void *sys, DWORD flags, LPCWSTR name, PCERT_PHYSICAL_STORE_INFO info, void *res, void *arg)
{
    struct en *e = arg;
    if (e->n < 16)
    {
        lstrcpynW(e->names[e->n], name, 64);
        e->info[e->n] = *info;
        if (info->pszOpenStoreProvider && !IS_INTRESOURCE(info->pszOpenStoreProvider)) lstrcpynA(e->prov[e->n], info->pszOpenStoreProvider, 64);
        if (info->OpenParameters.pbData && info->OpenParameters.cbData <= 64) memcpy(e->par[e->n], info->OpenParameters.pbData, info->OpenParameters.cbData);
        e->n++;
    }
    return TRUE;
}
static int find(struct en *e, const WCHAR *name)
{
    int i;
    for (i = 0; i < e->n; i++) if (!lstrcmpiW(e->names[i], name)) return i;
    return -1;
}

static int child(const char *mode)
{
    /* runs without the administrators group: the machine's stores are out of reach */
    CERT_PHYSICAL_STORE_INFO i = mkinfo(PHYS_B, 0, 0);
    DWORD err;
    BOOL ok;
    if (!strcmp(mode, "reguser"))
    {
        SetLastError(0xdeadbeef);
        ok = CertRegisterPhysicalStore(SYS, CU, L"UserPhys", &i, NULL);
        err = GetLastError();
        if (ok) CertUnregisterPhysicalStore(SYS, CU, L"UserPhys");
        printf("CHILD reguser ok=%d err=%lu\n", ok, err);
        return 0;
    }
    if (!strcmp(mode, "reg"))
    {
        SetLastError(0xdeadbeef);
        ok = CertRegisterPhysicalStore(SYS, LM, L"ChildPhys", &i, NULL);
        err = GetLastError();
        printf("CHILD reg ok=%d err=%lu\n", ok, err);
        return 0;
    }
    SetLastError(0xdeadbeef);
    ok = CertUnregisterPhysicalStore(SYS, LM, L"AdminPhys");
    err = GetLastError();
    printf("CHILD unreg ok=%d err=%lu\n", ok, err);
    return 0;
}

static void run_child(const char *mode, char *out, size_t n)
{
    HANDLE token = NULL, restricted = NULL, r, w;
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    WCHAR cmd[MAX_PATH + 40], path[MAX_PATH];
    BYTE sidbuf[SECURITY_MAX_SID_SIZE];
    SID_AND_ATTRIBUTES disable;
    DWORD len = sizeof(sidbuf), got = 0;

    out[0] = 0;
    OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &token);
    CreateWellKnownSid(WinBuiltinAdministratorsSid, NULL, sidbuf, &len);
    disable.Sid = sidbuf; disable.Attributes = 0;
    if (!CreateRestrictedToken(token, 0, 1, &disable, 0, NULL, 0, NULL, &restricted)) { strcpy(out, "no restricted token"); return; }
    GetModuleFileNameW(NULL, path, MAX_PATH);
    wsprintfW(cmd, L"\"%s\" child %S", path, mode);
    CreatePipe(&r, &w, &sa, 0);
    SetHandleInformation(r, HANDLE_FLAG_INHERIT, 0);
    si.dwFlags = STARTF_USESTDHANDLES; si.hStdOutput = w; si.hStdError = w; si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    if (CreateProcessAsUserW(restricted, NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
    {
        WaitForSingleObject(pi.hProcess, 30000);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    }
    CloseHandle(w);
    ReadFile(r, out, n - 1, &got, NULL);
    out[got] = 0;
    CloseHandle(r);
}

int main(int argc, char **argv)
{
    CERT_PHYSICAL_STORE_INFO info;
    struct en e;
    HCERTSTORE s, pa;
    PCCERT_CONTEXT c;
    BOOL ok;
    int idx;
    char out[256];

    if (argc >= 3 && !strcmp(argv[1], "child")) return child(argv[2]);

    clear_sys(CU, SYS); clear_sys(CU, PHYS_A); clear_sys(CU, PHYS_B);

    /* the physical stores of a system store that has none registered */
    s = open_sys(CU, SYS, 0);
    CHECK(s != NULL, "open the system store");
    if (s) CertCloseStore(s, 0);
    memset(&e, 0, sizeof(e));
    ok = CertEnumPhysicalStore(SYS, CU, &e, en_cb);
    CHECK(ok, "enum: %lu", GetLastError());
    CHECK(find(&e, L".Default") == 0 && find(&e, L"PhysA") < 0, "only the built-in ones: %d", e.n);

    /* register: the certificates of a registry store show in the system store */
    put(CU, PHYS_A, cert_1, sizeof(cert_1));
    info = mkinfo(PHYS_A, 0, 7);
    SetLastError(0xdeadbeef);
    ok = CertRegisterPhysicalStore(SYS, CU, L"PhysA", &info, NULL);
    CHECK(ok, "register: %lu", GetLastError());
    CHECK(count_sys(CU, SYS) == 1, "the registered store's certificate is in the system store: %d", count_sys(CU, SYS));
    memset(&e, 0, sizeof(e));
    ok = CertEnumPhysicalStore(SYS, CU, &e, en_cb);
    idx = find(&e, L"PhysA");
    CHECK(ok && idx >= 0, "enum lists PhysA: %d", e.n);
    if (idx >= 0)
    {
        CHECK(!strcmp(e.prov[idx], "SystemRegistry"), "provider '%s'", e.prov[idx]);
        CHECK(e.info[idx].dwPriority == 7 && e.info[idx].dwFlags == 0 && e.info[idx].dwOpenFlags == CU, "info prio %lu flags %lu open %08lx", e.info[idx].dwPriority, e.info[idx].dwFlags, e.info[idx].dwOpenFlags);
        CHECK(e.info[idx].OpenParameters.cbData == sizeof(PHYS_A) && !memcmp(e.par[idx], PHYS_A, sizeof(PHYS_A)), "params");
    }
    CHECK(find(&e, L".Default") >= 0, ".Default stays");
    {
        /* where it is kept: the user's hive, under the system store */
        HKEY k;
        char prov[64] = "";
        DWORD sz = sizeof(prov), type = 0, prio = 0, psz = sizeof(prio);
        LONG r = RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\SystemCertificates\\FastbSys\\PhysicalStores\\PhysA", 0, KEY_READ, &k);
        CHECK(!r, "the key in HKCU: %ld", r);
        if (!r)
        {
            r = RegQueryValueExA(k, "OpenStoreProvider", NULL, &type, (BYTE *)prov, &sz);
            CHECK(!r && type == REG_SZ && !strcmp(prov, "SystemRegistry"), "OpenStoreProvider '%s' type %lu", prov, type);
            r = RegQueryValueExA(k, "Priority", NULL, &type, (BYTE *)&prio, &psz);
            CHECK(!r && type == REG_DWORD && prio == 7, "Priority %lu", prio);
            CHECK(!RegQueryValueExA(k, "OpenParameters", NULL, &type, NULL, &sz) && type == REG_BINARY, "OpenParameters is binary");
            RegCloseKey(k);
        }
        r = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\SystemCertificates\\FastbSys\\PhysicalStores\\PhysA", 0, KEY_READ, &k);
        CHECK(r != 0, "and not in HKLM");
        if (!r) RegCloseKey(k);
    }

    /* CERT_STORE_CREATE_NEW_FLAG on an existing one; re-registering changes it */
    SetLastError(0xdeadbeef);
    ok = CertRegisterPhysicalStore(SYS, CU | CERT_STORE_CREATE_NEW_FLAG, L"PhysA", &info, NULL);
    CHECK(!ok && GetLastError() == ERROR_FILE_EXISTS, "create new: %d %lu", ok, GetLastError());
    info.dwPriority = 9;
    ok = CertRegisterPhysicalStore(SYS, CU, L"PhysA", &info, NULL);
    CHECK(ok, "re-register");
    memset(&e, 0, sizeof(e));
    CertEnumPhysicalStore(SYS, CU, &e, en_cb);
    idx = find(&e, L"PhysA");
    CHECK(idx >= 0 && e.info[idx].dwPriority == 9, "priority 9: %lu", idx >= 0 ? e.info[idx].dwPriority : 0);

    /* additions go to the highest priority store that allows them */
    {
        static const WCHAR SYS2[] = L"FastbSys2";
        HCERTSTORE dflt;
        CERT_PHYSICAL_STORE_INFO pb = mkinfo(PHYS_B, 0, 9);

        clear_sys(CU, SYS2); clear_sys(CU, PHYS_B);
        s = open_sys(CU, PHYS_B, 0);   /* creates the empty registry store */
        CertCloseStore(s, 0);
        CertRegisterPhysicalStore(SYS2, CU, L"PhysB", &pb, NULL);
        s = open_sys(CU, SYS2, 0);
        ok = CertAddEncodedCertificateToStore(s, X509_ASN_ENCODING, cert_2, sizeof(cert_2), CERT_STORE_ADD_NEW, NULL);
        CHECK(ok, "add to the collection: %lu", GetLastError());
        CertCloseStore(s, 0);
        CHECK(count_sys(CU, PHYS_B) == 0, "without CERT_PHYSICAL_STORE_ADD_ENABLE_FLAG the physical store did not get it: %d", count_sys(CU, PHYS_B));
        dflt = CertOpenStore(CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, 0, CU, SYS2);
        CHECK(dflt && count(dflt) == 1, "it went to the system store's own: %d", dflt ? count(dflt) : -1);
        if (dflt)   /* take it out again */
        {
            PCCERT_CONTEXT cc = CertEnumCertificatesInStore(dflt, NULL);
            if (cc) CertDeleteCertificateFromStore(cc);
            CertCloseStore(dflt, 0);
        }
        pb.dwFlags = CERT_PHYSICAL_STORE_ADD_ENABLE_FLAG;
        CertRegisterPhysicalStore(SYS2, CU, L"PhysB", &pb, NULL);
        s = open_sys(CU, SYS2, 0);
        ok = CertAddEncodedCertificateToStore(s, X509_ASN_ENCODING, cert_2, sizeof(cert_2), CERT_STORE_ADD_NEW, NULL);
        CHECK(ok, "add to the collection (enabled): %lu", GetLastError());
        CertCloseStore(s, 0);
        CHECK(count_sys(CU, PHYS_B) == 1, "with CERT_PHYSICAL_STORE_ADD_ENABLE_FLAG and the higher priority it got it: %d", count_sys(CU, PHYS_B));
        dflt = CertOpenStore(CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, 0, CU, SYS2);
        CHECK(dflt && count(dflt) == 0, "and the system store's own did not: %d", dflt ? count(dflt) : -1);
        if (dflt) CertCloseStore(dflt, 0);
        /* a read-only system store never adds */
        s = open_sys(CU, SYS2, CERT_STORE_READONLY_FLAG);
        SetLastError(0xdeadbeef);
        ok = CertAddEncodedCertificateToStore(s, X509_ASN_ENCODING, cert_1, sizeof(cert_1), CERT_STORE_ADD_NEW, NULL);
        CHECK(!ok, "a read-only system store refuses additions");
        CertCloseStore(s, 0);
        CHECK(count_sys(CU, PHYS_B) == 1, "nothing was added to the physical store read-only");
        clear_sys(CU, SYS2); clear_sys(CU, PHYS_B);
    }

    /* CERT_PHYSICAL_STORE_OPEN_DISABLE_FLAG: registered, not opened */
    info = mkinfo(PHYS_A, CERT_PHYSICAL_STORE_OPEN_DISABLE_FLAG, 7);
    CertRegisterPhysicalStore(SYS, CU, L"PhysA", &info, NULL);
    {
        int with_disabled = count_sys(CU, SYS);
        CHECK(with_disabled >= 0, "open with a disabled physical store");
        memset(&e, 0, sizeof(e));
        CertEnumPhysicalStore(SYS, CU, &e, en_cb);
        idx = find(&e, L"PhysA");
        CHECK(idx >= 0 && (e.info[idx].dwFlags & CERT_PHYSICAL_STORE_OPEN_DISABLE_FLAG), "still listed, with the flag");
        s = open_sys(CU, SYS, 0);
        c = NULL; ok = FALSE;
        while ((c = CertEnumCertificatesInStore(s, c))) if (c->cbCertEncoded == sizeof(cert_1) && !memcmp(c->pbCertEncoded, cert_1, sizeof(cert_1))) ok = TRUE;
        CertCloseStore(s, 0);
        CHECK(!ok, "the disabled store's certificate is not in the system store");
    }

    /* a physical store named .Default replaces the implicit one */
    clear_sys(CU, SYS);
    put(CU, SYS, cert_2, sizeof(cert_2));
    CHECK(count_sys(CU, SYS) == 1, "the implicit store holds the certificate");
    info = mkinfo(PHYS_B, 0, 0);
    put(CU, PHYS_B, cert_1, sizeof(cert_1));
    ok = CertRegisterPhysicalStore(SYS, CU, L".Default", &info, NULL);
    CHECK(ok, "register .Default");
    s = open_sys(CU, SYS, 0);
    c = CertEnumCertificatesInStore(s, NULL);
    CHECK(c && c->cbCertEncoded == sizeof(cert_1) && !memcmp(c->pbCertEncoded, cert_1, sizeof(cert_1)) && count(s) == 1, ".Default is PhysB now");
    if (c) CertFreeCertificateContext(c);
    CertCloseStore(s, 0);
    memset(&e, 0, sizeof(e));
    CertEnumPhysicalStore(SYS, CU, &e, en_cb);
    idx = find(&e, L".Default");
    CHECK(idx >= 0 && e.par[idx][0] == 'F' && e.n >= 1, ".Default listed once with its parameters");
    {
        int k, d = 0;
        for (k = 0; k < e.n; k++) if (!lstrcmpiW(e.names[k], L".Default")) d++;
        CHECK(d == 1, ".Default listed %d times", d);
    }
    ok = CertUnregisterPhysicalStore(SYS, CU, L".Default");
    CHECK(ok, "unregister .Default");
    CHECK(count_sys(CU, SYS) == 1, "the implicit store is back");
    s = open_sys(CU, SYS, 0);
    c = CertEnumCertificatesInStore(s, NULL);
    CHECK(c && c->cbCertEncoded == sizeof(cert_2) && !memcmp(c->pbCertEncoded, cert_2, sizeof(cert_2)), "with its own certificate");
    if (c) CertFreeCertificateContext(c);
    CertCloseStore(s, 0);

    /* unregister */
    info = mkinfo(PHYS_A, 0, 3);
    CertRegisterPhysicalStore(SYS, CU, L"PhysA", &info, NULL);
    CHECK(count_sys(CU, SYS) == 2, "two certificates: %d", count_sys(CU, SYS));
    ok = CertUnregisterPhysicalStore(SYS, CU, L"PhysA");
    CHECK(ok, "unregister: %lu", GetLastError());
    CHECK(count_sys(CU, SYS) == 1, "the certificate of PhysA left: %d", count_sys(CU, SYS));
    CHECK(count_sys(CU, PHYS_A) == 1, "its store is still there");
    memset(&e, 0, sizeof(e));
    CertEnumPhysicalStore(SYS, CU, &e, en_cb);
    CHECK(find(&e, L"PhysA") < 0, "no longer listed");
    SetLastError(0xdeadbeef);
    ok = CertUnregisterPhysicalStore(SYS, CU, L"PhysA");
    CHECK(!ok && GetLastError() == ERROR_FILE_NOT_FOUND, "unregister twice: %d %lu", ok, GetLastError());

    /* CERT_STORE_DELETE_FLAG deletes the store behind it */
    put(CU, PHYS_A, cert_1, sizeof(cert_1));
    CHECK(count_sys(CU, PHYS_A) == 1, "the store PhysA points at holds a certificate");
    CertRegisterPhysicalStore(SYS, CU, L"PhysA", &info, NULL);
    ok = CertUnregisterPhysicalStore(SYS, CU | CERT_STORE_DELETE_FLAG, L"PhysA");
    CHECK(ok, "unregister with delete: %lu", GetLastError());
    pa = CertOpenStore(CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, 0, CU | CERT_STORE_OPEN_EXISTING_FLAG, PHYS_A);
    CHECK(!pa || count(pa) == 0, "the store PhysA pointed at is deleted");
    if (pa) CertCloseStore(pa, 0);
    memset(&e, 0, sizeof(e));
    CertEnumPhysicalStore(SYS, CU, &e, en_cb);
    CHECK(find(&e, L"PhysA") < 0, "and unregistered");

    /* a physical store that points at its own system store does not recurse forever */
    {
        CERT_PHYSICAL_STORE_INFO loop = mkinfo(SYS, 0, 0);
        loop.pszOpenStoreProvider = (LPSTR)sz_CERT_STORE_PROV_SYSTEM_W;
        CertRegisterPhysicalStore(SYS, CU, L"Loop", &loop, NULL);
        CHECK(count_sys(CU, SYS) >= 1, "opens with a looping physical store: %d", count_sys(CU, SYS));
        CertUnregisterPhysicalStore(SYS, CU, L"Loop");
    }

    /* the provider can be a number */
    {
        CERT_PHYSICAL_STORE_INFO mem;
        memset(&mem, 0, sizeof(mem));
        mem.cbSize = sizeof(mem);
        mem.pszOpenStoreProvider = CERT_STORE_PROV_MEMORY;
        ok = CertRegisterPhysicalStore(SYS, CU, L"Mem", &mem, NULL);
        CHECK(ok, "numeric provider: %lu", GetLastError());
        memset(&e, 0, sizeof(e));
        CertEnumPhysicalStore(SYS, CU, &e, en_cb);
        idx = find(&e, L"Mem");
        CHECK(idx >= 0 && (ULONG_PTR)e.info[idx].pszOpenStoreProvider == (ULONG_PTR)CERT_STORE_PROV_MEMORY, "numeric provider back");
        CHECK(count_sys(CU, SYS) >= 0, "opens with a memory physical store");
        CertUnregisterPhysicalStore(SYS, CU, L"Mem");
    }

    /* bad arguments */
    info = mkinfo(PHYS_A, 0, 0);
    SetLastError(0xdeadbeef); ok = CertRegisterPhysicalStore(SYS, CU, NULL, &info, NULL);
    CHECK(!ok && GetLastError() == E_INVALIDARG, "no name: %d %lx", ok, GetLastError());
    SetLastError(0xdeadbeef); ok = CertRegisterPhysicalStore(SYS, CU, L"", &info, NULL);
    CHECK(!ok && GetLastError() == E_INVALIDARG, "empty name: %d %lx", ok, GetLastError());
    SetLastError(0xdeadbeef); ok = CertRegisterPhysicalStore(SYS, CU, L"a\\b", &info, NULL);
    CHECK(!ok && GetLastError() == E_INVALIDARG, "name with a backslash: %d %lx", ok, GetLastError());
    SetLastError(0xdeadbeef); ok = CertRegisterPhysicalStore(SYS, CU, L"X", NULL, NULL);
    CHECK(!ok && GetLastError() == E_INVALIDARG, "no info: %d %lx", ok, GetLastError());
    SetLastError(0xdeadbeef); ok = CertRegisterPhysicalStore(NULL, CU, L"X", &info, NULL);
    CHECK(!ok && GetLastError() == E_INVALIDARG, "no system store: %d %lx", ok, GetLastError());
    SetLastError(0xdeadbeef); ok = CertRegisterPhysicalStore(SYS, CU, L"X", &info, (void *)1);
    CHECK(!ok && GetLastError() == E_INVALIDARG, "reserved: %d %lx", ok, GetLastError());
    info.pszOpenStoreProvider = NULL;
    SetLastError(0xdeadbeef); ok = CertRegisterPhysicalStore(SYS, CU, L"X", &info, NULL);
    CHECK(!ok && GetLastError() == E_INVALIDARG, "no provider: %d %lx", ok, GetLastError());
    info = mkinfo(PHYS_A, 0, 0); info.cbSize = 8;
    SetLastError(0xdeadbeef); ok = CertRegisterPhysicalStore(SYS, CU, L"X", &info, NULL);
    CHECK(!ok && GetLastError() == E_INVALIDARG, "short cbSize: %d %lx", ok, GetLastError());
    info = mkinfo(PHYS_A, 0, 0);
    SetLastError(0xdeadbeef); ok = CertRegisterPhysicalStore(SYS, CU | CERT_SYSTEM_STORE_RELOCATE_FLAG, L"X", &info, NULL);
    CHECK(!ok && GetLastError() == E_INVALIDARG, "relocate: %d %lx", ok, GetLastError());
    SetLastError(0xdeadbeef); ok = CertRegisterPhysicalStore(SYS, 0x00700000, L"X", &info, NULL);
    CHECK(!ok && GetLastError() == E_INVALIDARG, "bad location: %d %lx", ok, GetLastError());
    SetLastError(0xdeadbeef); ok = CertUnregisterPhysicalStore(SYS, CU, NULL);
    CHECK(!ok && GetLastError() == E_INVALIDARG, "unregister without a name: %d %lx", ok, GetLastError());
    SetLastError(0xdeadbeef); ok = CertUnregisterPhysicalStore(L"NoSuchFastbStore", CU, L"X");
    CHECK(!ok && GetLastError() == ERROR_FILE_NOT_FOUND, "unregister in a store that has none: %d %lu", ok, GetLastError());
    memset(&e, 0, sizeof(e));
    CertEnumPhysicalStore(SYS, CU, &e, en_cb);
    CHECK(find(&e, L"X") < 0 && find(&e, L"a\\b") < 0, "no refused store was created");

    /* the machine's stores are the administrators' */
    info = mkinfo(PHYS_A, 0, 0);
    info.dwOpenFlags = LM;
    ok = CertRegisterPhysicalStore(SYS, LM, L"AdminPhys", &info, NULL);
    CHECK(ok, "an administrator registers in the machine's store: %lu", GetLastError());
    run_child("reguser", out, sizeof(out));
    CHECK(strstr(out, "CHILD reguser ok=1"), "a standard user registers in their own store: %s", out);
    run_child("reg", out, sizeof(out));
    CHECK(strstr(out, "CHILD reg ok=0 err=5"), "a standard user cannot register in the machine's store: %s", out);
    run_child("unreg", out, sizeof(out));
    CHECK(strstr(out, "CHILD unreg ok=0 err=5"), "a standard user cannot unregister it: %s", out);
    memset(&e, 0, sizeof(e));
    CertEnumPhysicalStore(SYS, LM, &e, en_cb);
    CHECK(find(&e, L"AdminPhys") >= 0 && find(&e, L"ChildPhys") < 0, "the machine's list is as the administrator left it");
    ok = CertUnregisterPhysicalStore(SYS, LM, L"AdminPhys");
    CHECK(ok, "the administrator unregisters it");
    clear_sys(LM, SYS);

    clear_sys(CU, SYS); clear_sys(CU, PHYS_A); clear_sys(CU, PHYS_B);
    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    return f != 0;
}
