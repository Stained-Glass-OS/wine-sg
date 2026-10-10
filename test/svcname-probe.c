/* GetServiceDisplayNameA / GetServiceKeyNameA size outputs on failure
 * (patches/sg/2216).  On any failure but "buffer too small" the size is left as
 * it was, but never below 1, and 1 when there is no buffer; a too-small buffer
 * gets an upper estimate.  Uses a service of its own (created and removed). */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
static void sizeck(DWORD got, DWORD want, const char *what)
{
    char b[200];
    snprintf(b, sizeof(b), "%s (size %lu, want %lu)", what, (unsigned long)got, (unsigned long)want);
    check(got == want, b);
}

int main(void)
{
    static const char key[] = "sgsvcname_probe", display[] = "SG Service Name Probe";
    SC_HANDLE scm, svc;
    char buf[200];
    DWORD size, err;
    BOOL ret;

    scm = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!scm) { printf("FAIL  OpenSCManager %lu\n", (unsigned long)GetLastError()); return 1; }
    svc = OpenServiceA(scm, key, DELETE);
    if (svc) { DeleteService(svc); CloseServiceHandle(svc); }
    svc = CreateServiceA(scm, key, display, DELETE, SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START, SERVICE_ERROR_IGNORE,
                         "C:\\windows\\system32\\sgprobe_none.exe", NULL, NULL, NULL, NULL, NULL);
    if (!svc) { printf("FAIL  CreateService %lu\n", (unsigned long)GetLastError()); return 1; }

    /* --- display name of a service that does not exist --- */
    SetLastError(0xdeadbeef);
    size = 200;
    ret = GetServiceDisplayNameA(scm, "sgnosuchservice", NULL, &size);
    check(!ret && GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST, "no such service: ERROR_SERVICE_DOES_NOT_EXIST");
    sizeck(size, 1, "...with no buffer the size becomes 1");
    size = 15; strcpy(buf, "ABC");
    ret = GetServiceDisplayNameA(scm, "sgnosuchservice", buf, &size);
    check(!ret && GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST && buf[0] == 0, "...a buffer is emptied");
    sizeck(size, 15, "...and the size is left as it was");
    size = 0; strcpy(buf, "ABC");
    ret = GetServiceDisplayNameA(scm, "sgnosuchservice", buf, &size);
    check(!ret && buf[0] == 'A', "a size of 0 leaves the buffer alone");
    sizeck(size, 1, "...and becomes 1");
    size = 1; strcpy(buf, "ABC");
    GetServiceDisplayNameA(scm, "sgnosuchservice", buf, &size);
    sizeck(size, 1, "a size of 1 stays 1");
    size = 2; strcpy(buf, "ABC");
    GetServiceDisplayNameA(scm, "sgnosuchservice", buf, &size);
    sizeck(size, 2, "a size of 2 stays 2");

    /* --- display name: NULL service name with a good handle --- */
    size = 200;
    SetLastError(0xdeadbeef);
    ret = GetServiceDisplayNameA(scm, NULL, buf, &size);
    err = GetLastError();
    check(!ret && (err == ERROR_INVALID_ADDRESS || err == ERROR_INVALID_PARAMETER), "NULL service name is ERROR_INVALID_ADDRESS");
    sizeck(size, 200, "...the size is left as it was");
    size = 200;
    ret = GetServiceDisplayNameA(scm, NULL, NULL, &size);
    sizeck(size, 1, "...and with no buffer too: 1");

    /* --- key name of a display name that does not exist --- */
    size = 200;
    SetLastError(0xdeadbeef);
    ret = GetServiceKeyNameA(scm, "sg no such display name", NULL, &size);
    check(!ret && GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST, "no such display name: ERROR_SERVICE_DOES_NOT_EXIST");
    sizeck(size, 1, "...with no buffer the size becomes 1");
    size = 15; strcpy(buf, "ABC");
    ret = GetServiceKeyNameA(scm, "sg no such display name", buf, &size);
    check(!ret && buf[0] == 0, "...a buffer is emptied");
    sizeck(size, 15, "...and the size is left as it was");
    size = 0; strcpy(buf, "ABC");
    GetServiceKeyNameA(scm, "sg no such display name", buf, &size);
    sizeck(size, 1, "a size of 0 becomes 1");
    check(buf[0] == 'A', "...and leaves the buffer alone");
    size = 2; strcpy(buf, "ABC");
    GetServiceKeyNameA(scm, "sg no such display name", buf, &size);
    sizeck(size, 2, "a size of 2 stays 2");

    /* --- a service that exists --- */
    size = sizeof(buf);
    ret = GetServiceDisplayNameA(scm, key, buf, &size);
    check(ret && !strcmp(buf, display), "the display name of an existing service");
    sizeck(size, sizeof(buf), "...the size is not changed on success");
    size = sizeof(buf);
    ret = GetServiceKeyNameA(scm, display, buf, &size);
    check(ret && !strcmp(buf, key), "the key name of an existing service");
    sizeck(size, sizeof(buf), "...the size is not changed on success");
    size = 4;
    SetLastError(0xdeadbeef);
    ret = GetServiceDisplayNameA(scm, key, buf, &size);
    check(!ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER, "a buffer that is too small is ERROR_INSUFFICIENT_BUFFER");
    check(size >= strlen(display), "...and the size is an estimate of at least the length needed");
    size = 4;
    SetLastError(0xdeadbeef);
    ret = GetServiceKeyNameA(scm, display, buf, &size);
    check(!ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER, "same for the key name");
    check(size >= strlen(key), "...with an estimate of at least the length needed");

    DeleteService(svc);
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
