/* Certificate store resync and store locations (patches/sg/1662), run by
 * test/certresync-gate.sh:
 *  - CERT_STORE_CTRL_AUTO_RESYNC: a store sees a certificate another handle
 *    added (a store without it does not); CERT_STORE_CTRL_NOTIFY_CHANGE's
 *    event is signalled, and no longer after CERT_STORE_CTRL_CANCEL_NOTIFY;
 *  - CERT_SYSTEM_STORE_SERVICES ("service\store"), _USERS ("sid\store"),
 *    _CURRENT_SERVICE (from a service the probe installs and starts) and
 *    _LOCAL_MACHINE_ENTERPRISE open, keep what is added and are listed by
 *    CertEnumSystemStore;
 *  - the machine's Root system store holds its enterprise store's
 *    certificates, and CertEnumPhysicalStore lists both.
 * These were stubs (FIXME and failure, or nothing done). */
#include <windows.h>
#include <wincrypt.h>
#include <sddl.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static const BYTE cert1[] = { 0x30, 0x7a, 0x02, 0x01, 0x01, 0x30, 0x02, 0x06,
 0x00, 0x30, 0x15, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13,
 0x0a, 0x4a, 0x75, 0x61, 0x6e, 0x20, 0x4c, 0x61, 0x6e, 0x67, 0x00, 0x30, 0x22,
 0x18, 0x0f, 0x31, 0x36, 0x30, 0x31, 0x30, 0x31, 0x30, 0x31, 0x30, 0x30, 0x30,
 0x30, 0x30, 0x30, 0x5a, 0x18, 0x0f, 0x31, 0x36, 0x30, 0x31, 0x30, 0x31, 0x30,
 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x5a, 0x30, 0x15, 0x31, 0x13, 0x30,
 0x11, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x0a, 0x4a, 0x75, 0x61, 0x6e, 0x20,
 0x4c, 0x61, 0x6e, 0x67, 0x00, 0x30, 0x07, 0x30, 0x02, 0x06, 0x00, 0x03, 0x01,
 0x00, 0xa3, 0x16, 0x30, 0x14, 0x30, 0x12, 0x06, 0x03, 0x55, 0x1d, 0x13, 0x01,
 0x01, 0xff, 0x04, 0x08, 0x30, 0x06, 0x01, 0x01, 0xff, 0x02, 0x01, 0x01 };
static const BYTE cert2[] = { 0x30, 0x7a, 0x02, 0x01, 0x01, 0x30, 0x02, 0x06,
 0x00, 0x30, 0x15, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13,
 0x0a, 0x41, 0x6c, 0x65, 0x78, 0x20, 0x4c, 0x61, 0x6e, 0x67, 0x00, 0x30, 0x22,
 0x18, 0x0f, 0x31, 0x36, 0x30, 0x31, 0x30, 0x31, 0x30, 0x31, 0x30, 0x30, 0x30,
 0x30, 0x30, 0x30, 0x5a, 0x18, 0x0f, 0x31, 0x36, 0x30, 0x31, 0x30, 0x31, 0x30,
 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x5a, 0x30, 0x15, 0x31, 0x13, 0x30,
 0x11, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x0a, 0x41, 0x6c, 0x65, 0x78, 0x20,
 0x4c, 0x61, 0x6e, 0x67, 0x00, 0x30, 0x07, 0x30, 0x02, 0x06, 0x00, 0x03, 0x01,
 0x00, 0xa3, 0x16, 0x30, 0x14, 0x30, 0x12, 0x06, 0x03, 0x55, 0x1d, 0x13, 0x01,
 0x01, 0xff, 0x04, 0x08, 0x30, 0x06, 0x01, 0x01, 0xff, 0x02, 0x01, 0x01 };

#define SVC_NAME L"SGCertSvc"

static HCERTSTORE open_sys(DWORD location, const WCHAR *name)
{
    return CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0, location, name);
}

static int count_certs(HCERTSTORE store)
{
    const CERT_CONTEXT *ctx = NULL;
    int n = 0;

    while ((ctx = CertEnumCertificatesInStore(store, ctx))) n++;
    return n;
}

static BOOL add(HCERTSTORE store, const BYTE *der, DWORD size)
{
    return CertAddEncodedCertificateToStore(store, X509_ASN_ENCODING, der, size, CERT_STORE_ADD_ALWAYS, NULL);
}

static BOOL has_cert(HCERTSTORE store, const BYTE *der, DWORD size)
{
    const CERT_CONTEXT *ctx = NULL;
    BOOL found = FALSE;

    while (!found && (ctx = CertEnumCertificatesInStore(store, ctx)))
        found = ctx->cbCertEncoded == size && !memcmp(ctx->pbCertEncoded, der, size);
    if (ctx) CertFreeCertificateContext(ctx);
    return found;
}

struct names { WCHAR list[2048]; };

static BOOL WINAPI enum_sys(const void *name, DWORD flags, CERT_SYSTEM_STORE_INFO *info, void *reserved, void *arg)
{
    struct names *n = arg;
    wcscat(n->list, L"|");
    wcscat(n->list, name);
    return TRUE;
}

static BOOL WINAPI enum_phys(const void *sys, DWORD flags, const WCHAR *name, CERT_PHYSICAL_STORE_INFO *info,
                             void *reserved, void *arg)
{
    struct names *n = arg;
    wcscat(n->list, L"|");
    wcscat(n->list, name);
    return TRUE;
}

static BOOL listed(const struct names *n, const WCHAR *name)
{
    WCHAR want[300];
    const WCHAR *p;
    size_t len;

    swprintf(want, ARRAYSIZE(want), L"|%ls", name);
    len = wcslen(want);
    for (p = n->list; (p = wcsstr(p, want)); p += len)
        if (!p[len] || p[len] == '|') return TRUE;
    return FALSE;
}

/* the service: adds cert1 to its own (current service) My store */
static SERVICE_STATUS_HANDLE svc_handle;
static SERVICE_STATUS svc_status;
static DWORD WINAPI svc_ctrl(DWORD ctrl, DWORD type, void *data, void *ctx) { return NO_ERROR; }
static void WINAPI svc_main(DWORD argc, WCHAR **argv)
{
    HCERTSTORE store;
    DWORD result = 1;

    svc_handle = RegisterServiceCtrlHandlerExW(SVC_NAME, svc_ctrl, NULL);
    svc_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    svc_status.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(svc_handle, &svc_status);
    if ((store = CertOpenStore(CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, 0, CERT_SYSTEM_STORE_CURRENT_SERVICE, L"My")))
    {
        if (add(store, cert1, sizeof(cert1))) result = 0;
        CertCloseStore(store, 0);
    }
    svc_status.dwCurrentState = SERVICE_STOPPED;
    svc_status.dwWin32ExitCode = result ? ERROR_SERVICE_SPECIFIC_ERROR : 0;
    svc_status.dwServiceSpecificExitCode = result;
    SetServiceStatus(svc_handle, &svc_status);
}

static void test_resync(void)
{
    HCERTSTORE watched, plain, writer;
    HANDLE event = CreateEventW(NULL, FALSE, FALSE, NULL);

    watched = open_sys(CERT_SYSTEM_STORE_CURRENT_USER, L"SGResync");
    plain = CertOpenStore(CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, 0, CERT_SYSTEM_STORE_CURRENT_USER, L"SGResync");
    check(watched && plain, "the store opens twice");
    check(count_certs(watched) == 0 && count_certs(plain) == 0, "it starts empty");
    check(CertControlStore(watched, 0, CERT_STORE_CTRL_AUTO_RESYNC, NULL), "CERT_STORE_CTRL_AUTO_RESYNC succeeds");
    check(CertControlStore(watched, 0, CERT_STORE_CTRL_NOTIFY_CHANGE, &event), "CERT_STORE_CTRL_NOTIFY_CHANGE succeeds");

    writer = CertOpenStore(CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, 0, CERT_SYSTEM_STORE_CURRENT_USER, L"SGResync");
    check(add(writer, cert1, sizeof(cert1)), "another handle adds a certificate");
    CertCloseStore(writer, 0);
    check(WaitForSingleObject(event, 5000) == WAIT_OBJECT_0, "the change event is signalled");
    check(count_certs(watched) == 1, "the auto-resync store sees the new certificate");
    check(count_certs(plain) == 0, "a store without auto-resync does not (until it resyncs)");

    check(CertControlStore(watched, 0, CERT_STORE_CTRL_CANCEL_NOTIFY, &event), "CERT_STORE_CTRL_CANCEL_NOTIFY succeeds");
    SetLastError(0);
    check(!CertControlStore(watched, 0, CERT_STORE_CTRL_CANCEL_NOTIFY, &event) && GetLastError() == E_INVALIDARG,
          "cancelling it again fails with E_INVALIDARG");
    writer = CertOpenStore(CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, 0, CERT_SYSTEM_STORE_CURRENT_USER, L"SGResync");
    check(add(writer, cert2, sizeof(cert2)), "a second certificate is added elsewhere");
    CertCloseStore(writer, 0);
    check(WaitForSingleObject(event, 1500) == WAIT_TIMEOUT, "the cancelled event is no longer signalled");
    check(count_certs(watched) == 2, "auto-resync goes on: both certificates seen");
    check(CertControlStore(plain, 0, CERT_STORE_CTRL_RESYNC, NULL) && count_certs(plain) == 2,
          "the plain store sees them after an explicit resync");
    CertCloseStore(watched, 0);
    CertCloseStore(plain, 0);
    CloseHandle(event);
}

static void test_services(const WCHAR *self)
{
    SC_HANDLE scm, svc;
    SERVICE_STATUS st;
    HCERTSTORE store;
    struct names n = {{0}};
    WCHAR cmd[MAX_PATH + 8];
    int i;

    SetLastError(0);
    store = CertOpenStore(CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, 0, CERT_SYSTEM_STORE_CURRENT_SERVICE, L"My");
    check(!store && GetLastError() == ERROR_FILE_NOT_FOUND, "a process that is no service has no current-service store");
    if (store) CertCloseStore(store, 0);

    scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    swprintf(cmd, ARRAYSIZE(cmd), L"\"%ls\" service", self);
    svc = CreateServiceW(scm, SVC_NAME, SVC_NAME, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
                         SERVICE_DEMAND_START, SERVICE_ERROR_IGNORE, cmd, NULL, NULL, NULL, NULL, NULL);
    check(svc != NULL, "the test service is installed");
    if (!svc) return;
    check(StartServiceW(svc, 0, NULL), "the test service starts");
    for (i = 0; i < 100; i++)
    {
        if (QueryServiceStatus(svc, &st) && st.dwCurrentState == SERVICE_STOPPED) break;
        Sleep(100);
    }
    check(st.dwCurrentState == SERVICE_STOPPED && !st.dwWin32ExitCode,
          "the service added a certificate to its current-service My store");
    DeleteService(svc);
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);

    store = CertOpenStore(CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, 0, CERT_SYSTEM_STORE_SERVICES,
                          SVC_NAME L"\\My");
    check(store && has_cert(store, cert1, sizeof(cert1)), "CERT_SYSTEM_STORE_SERVICES \"service\\My\" holds it");
    if (store) CertCloseStore(store, 0);
    check(RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Cryptography\\Services\\" SVC_NAME
                        L"\\SystemCertificates\\My\\Certificates", 0, KEY_READ, (HKEY *)&store) == 0,
          "under HKLM\\Software\\Microsoft\\Cryptography\\Services\\<service>\\SystemCertificates");
    SetLastError(0);
    check(!CertOpenStore(CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, 0, CERT_SYSTEM_STORE_SERVICES, L"My") &&
          GetLastError() == E_INVALIDARG, "a services store without a service name fails with E_INVALIDARG");
    check(CertEnumSystemStore(CERT_SYSTEM_STORE_SERVICES, (void *)SVC_NAME, &n, enum_sys) && listed(&n, L"My"),
          "CertEnumSystemStore lists the service's stores");
    memset(&n, 0, sizeof(n));
    check(CertEnumSystemStore(CERT_SYSTEM_STORE_SERVICES, NULL, &n, enum_sys) && listed(&n, SVC_NAME L"\\My"),
          "and every service's as \"service\\store\"");
}

static void test_users(void)
{
    HANDLE token;
    BYTE buf[256];
    TOKEN_USER *user = (TOKEN_USER *)buf;
    DWORD size;
    WCHAR *sid = NULL, name[300];
    HCERTSTORE store;
    struct names n = {{0}};

    OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);
    GetTokenInformation(token, TokenUser, buf, sizeof(buf), &size);
    ConvertSidToStringSidW(user->User.Sid, &sid);
    CloseHandle(token);
    check(sid != NULL, "the user's SID");
    if (!sid) return;

    store = open_sys(CERT_SYSTEM_STORE_CURRENT_USER, L"SGUsers");
    check(store && add(store, cert2, sizeof(cert2)), "a certificate goes into the user's SGUsers store");
    if (store) CertCloseStore(store, 0);
    swprintf(name, ARRAYSIZE(name), L"%ls\\SGUsers", sid);
    store = CertOpenStore(CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, 0, CERT_SYSTEM_STORE_USERS, name);
    check(store && count_certs(store) == 1 && has_cert(store, cert2, sizeof(cert2)),
          "CERT_SYSTEM_STORE_USERS \"sid\\SGUsers\" is the same store");
    if (store) CertCloseStore(store, 0);
    check(CertEnumSystemStore(CERT_SYSTEM_STORE_USERS, sid, &n, enum_sys) && listed(&n, L"SGUsers"),
          "CertEnumSystemStore lists the user's stores");
    memset(&n, 0, sizeof(n));
    check(CertEnumSystemStore(CERT_SYSTEM_STORE_USERS, NULL, &n, enum_sys) && listed(&n, name),
          "and every user's as \"sid\\store\"");
    LocalFree(sid);
}

static void test_enterprise(void)
{
    HCERTSTORE store;
    struct names n = {{0}};

    store = CertOpenStore(CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, 0, CERT_SYSTEM_STORE_LOCAL_MACHINE_ENTERPRISE,
                          L"Root");
    check(store && add(store, cert2, sizeof(cert2)), "a certificate goes into the enterprise Root store");
    if (store) CertCloseStore(store, 0);
    check(RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\EnterpriseCertificates\\Root\\Certificates", 0,
                        KEY_READ, (HKEY *)&store) == 0, "under HKLM\\Software\\Microsoft\\EnterpriseCertificates");
    store = open_sys(CERT_SYSTEM_STORE_LOCAL_MACHINE, L"Root");
    check(store && has_cert(store, cert2, sizeof(cert2)), "the machine's Root system store holds it");
    if (store) CertCloseStore(store, 0);
    store = open_sys(CERT_SYSTEM_STORE_CURRENT_USER, L"Root");
    check(store && has_cert(store, cert2, sizeof(cert2)), "so does the user's");
    if (store) CertCloseStore(store, 0);
    check(CertEnumSystemStore(CERT_SYSTEM_STORE_LOCAL_MACHINE_ENTERPRISE, NULL, &n, enum_sys) && listed(&n, L"Root"),
          "CertEnumSystemStore lists the enterprise stores");
    memset(&n, 0, sizeof(n));
    check(CertEnumPhysicalStore(L"Root", CERT_SYSTEM_STORE_LOCAL_MACHINE, &n, enum_phys) &&
          listed(&n, L".Default") && listed(&n, L".Enterprise"),
          "CertEnumPhysicalStore lists .Default and .Enterprise");
    SetLastError(0);
    check(!CertEnumPhysicalStore(L"SGNoSuchStore", CERT_SYSTEM_STORE_LOCAL_MACHINE, &n, enum_phys) &&
          GetLastError() == ERROR_FILE_NOT_FOUND, "a store that does not exist has none");
}

int wmain(int argc, WCHAR **argv)
{
    WCHAR self[MAX_PATH];

    if (argc > 1 && !wcscmp(argv[1], L"service"))
    {
        SERVICE_TABLE_ENTRYW table[] = { { (WCHAR *)SVC_NAME, svc_main }, { NULL, NULL } };
        StartServiceCtrlDispatcherW(table);
        return 0;
    }
    GetModuleFileNameW(NULL, self, MAX_PATH);
    /* a rerun starts from empty stores */
    CertOpenStore(CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, 0, CERT_SYSTEM_STORE_CURRENT_USER | CERT_STORE_DELETE_FLAG,
                  L"SGResync");
    CertOpenStore(CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, 0, CERT_SYSTEM_STORE_CURRENT_USER | CERT_STORE_DELETE_FLAG,
                  L"SGUsers");
    CertOpenStore(CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, 0,
                  CERT_SYSTEM_STORE_LOCAL_MACHINE_ENTERPRISE | CERT_STORE_DELETE_FLAG, L"Root");
    CertOpenStore(CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, 0, CERT_SYSTEM_STORE_SERVICES | CERT_STORE_DELETE_FLAG,
                  SVC_NAME L"\\My");
    test_resync();
    test_services(self);
    test_users();
    test_enterprise();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
