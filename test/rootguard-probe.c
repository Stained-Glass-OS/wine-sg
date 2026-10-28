/* rootguard-probe: crypt32's one-time root import after the importing process died (patches/sg/7600).
 *   use   open the machine's Root store (the import runs) and count its roots
 *   dead  take the import's guards as an importer does, leave them to a child that keeps them
 *         open (the processes waiting for it, in the field) and end without releasing them */
#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "use"))
    {
        HCERTSTORE store = CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0,
                                         CERT_SYSTEM_STORE_LOCAL_MACHINE | CERT_STORE_READONLY_FLAG, L"Root");
        PCCERT_CONTEXT cert = NULL;
        int n = 0;
        if (!store) { printf("open failed %lu\n", GetLastError()); return 1; }
        while ((cert = CertEnumCertificatesInStore(store, cert))) n++;
        CertCloseStore(store, 0);
        printf("roots=%d\n", n);
        return n ? 0 : 1;
    }
    if (argc > 1 && !strcmp(argv[1], "dead"))
    {
        SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        char cmd[MAX_PATH + 16];
        HANDLE mutex = CreateMutexW(&sa, TRUE, L"crypt32_root_mutex");
        HANDLE sem = CreateSemaphoreW(&sa, 0, 1, L"crypt32_root_semaphore");
        if (!mutex || !sem) { printf("guards failed %lu\n", GetLastError()); return 1; }
        GetModuleFileNameA(NULL, cmd, MAX_PATH);
        lstrcatA(cmd, " keep");
        if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) { printf("keeper failed\n"); return 1; }
        printf("importer ends holding its guards; keeper %lu\n", pi.dwProcessId);
        fflush(stdout);
        ExitProcess(9);
    }
    if (argc > 1 && !strcmp(argv[1], "keep"))
    {
        Sleep(120000);
        return 0;
    }
    printf("usage: rootguard-probe use|dead\n");
    return 2;
}
