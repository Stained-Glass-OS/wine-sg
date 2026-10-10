/* netapi32 behaviours Wine's own tests record from Windows (patches/sg/2433), run by
 * test/netapi32ground-gate.sh: the WebDAV path conversions (the test's tables),
 * the other DsRole levels, and a remote server that cannot be reached. */
#include <windows.h>
#include <lm.h>
#include <dsrole.h>
#include <stdio.h>
#include <string.h>

static const struct { const WCHAR *path; DWORD ret; const WCHAR *out; } unc_to_http[] =
{
    { L"", ERROR_BAD_NET_NAME, NULL },
    { L"c:\\", ERROR_BAD_NET_NAME, NULL },
    { L"\\\\", ERROR_BAD_NET_NAME, NULL },
    { L"\\a\\b", ERROR_BAD_NET_NAME, NULL },
    { L"\\\\a", ERROR_SUCCESS, L"http://a" },
    { L"\\\\a\\", ERROR_SUCCESS, L"http://a" },
    { L"\\\\a\\b", ERROR_SUCCESS, L"http://a/b" },
    { L"\\\\a\\b\\", ERROR_SUCCESS, L"http://a/b" },
    { L"\\\\a\\b\\c", ERROR_SUCCESS, L"http://a/b/c" },
    { L"\\\\a@SSL\\b", ERROR_SUCCESS, L"https://a/b" },
    { L"\\\\a@ssl\\b", ERROR_SUCCESS, L"https://a/b" },
    { L"\\\\a@tls\\b", ERROR_INVALID_PARAMETER, NULL },
    { L"\\\\a@SSL@443\\b", ERROR_SUCCESS, L"https://a/b" },
    { L"\\\\a@SSL@80\\b", ERROR_SUCCESS, L"https://a:80/b" },
    { L"\\\\a@80@SSL\\b", ERROR_INVALID_PARAMETER, NULL },
    { L"\\\\a@80\\b", ERROR_SUCCESS, L"http://a/b" },
    { L"\\\\a@8080\\b", ERROR_SUCCESS, L"http://a:8080/b" },
    { L"\\\\a\\b/", ERROR_SUCCESS, L"http://a/b" },
    { L"\\\\a/b", ERROR_SUCCESS, L"http://a/b" },
    { L"\\\\a.\\b", ERROR_SUCCESS, L"http://a./b" },
    { L"\\\\.a\\b", ERROR_SUCCESS, L"http://.a/b" },
    { L"//a/b", ERROR_SUCCESS, L"http://a/b" },
    { L"\\\\a\\\\", ERROR_BAD_NET_NAME, NULL },
    { L"\\\\\\a\\", ERROR_BAD_NET_NAME, NULL },
    { L"\\\\a\\b\\\\", ERROR_BAD_NET_NAME, NULL },
    { L"\\\\.\\a", ERROR_BAD_NET_NAME, NULL },
    { L"\\\\a\\b:", ERROR_BAD_NET_NAME, NULL }
};
static const struct { const WCHAR *path; DWORD ret; const WCHAR *out; } http_to_unc[] =
{
    { L"", ERROR_INVALID_PARAMETER, NULL },
    { L"http://server/path", ERROR_SUCCESS, L"\\\\server\\DavWWWRoot\\path" },
    { L"https://host/path", ERROR_SUCCESS, L"\\\\host@SSL\\DavWWWRoot\\path" },
    { L"\\\\server", ERROR_INVALID_PARAMETER, NULL },
    { L"\\\\server\\path", ERROR_INVALID_PARAMETER, NULL },
    { L"\\\\http://server/path", ERROR_INVALID_PARAMETER, NULL },
    { L"http://", ERROR_BAD_NETPATH, NULL },
    { L"http:", ERROR_BAD_NET_NAME, NULL },
    { L"http", ERROR_INVALID_PARAMETER, NULL },
    { L"http:server", ERROR_BAD_NET_NAME, NULL },
    { L"http://server:80", ERROR_SUCCESS, L"\\\\server\\DavWWWRoot" },
    { L"http://server:81", ERROR_SUCCESS, L"\\\\server@81\\DavWWWRoot" },
    { L"https://server:80", ERROR_SUCCESS, L"\\\\server@SSL@80\\DavWWWRoot" },
    { L"HTTP://server/path", ERROR_SUCCESS, L"\\\\server\\DavWWWRoot\\path" },
    { L"http://server:65537", ERROR_BAD_NETPATH, NULL },
    { L"http://server/path/", ERROR_SUCCESS, L"\\\\server\\DavWWWRoot\\path" },
    { L"http://server/path//", ERROR_SUCCESS, L"\\\\server\\DavWWWRoot\\path" },
    { L"http://server:/path", ERROR_BAD_NETPATH, NULL },
    { L"http://server", ERROR_SUCCESS, L"\\\\server\\DavWWWRoot" },
    { L"https://server:443", ERROR_SUCCESS, L"\\\\server@SSL\\DavWWWRoot" }
};

static DWORD (WINAPI *pDavGetHTTPFromUNCPath)(const WCHAR *, WCHAR *, DWORD *);
static DWORD (WINAPI *pDavGetUNCFromHTTPPath)(const WCHAR *, WCHAR *, DWORD *);

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL  " __VA_ARGS__); printf("\n"); } } while (0)

int main(void)
{
    HMODULE m = LoadLibraryA("netapi32.dll");
    WCHAR buf[MAX_PATH];
    DWORD size, ret, i;
    BYTE *info = NULL;
    LPBYTE user = NULL;

    pDavGetHTTPFromUNCPath = (void *)GetProcAddress(m, "DavGetHTTPFromUNCPath");
    pDavGetUNCFromHTTPPath = (void *)GetProcAddress(m, "DavGetUNCFromHTTPPath");

    for (i = 0; i < sizeof(unc_to_http) / sizeof(unc_to_http[0]); i++)
    {
        buf[0] = 0;
        size = MAX_PATH;
        ret = pDavGetHTTPFromUNCPath(unc_to_http[i].path, buf, &size);
        CHECK(ret == unc_to_http[i].ret, "UNC %ls: %lu, expected %lu", unc_to_http[i].path, ret, unc_to_http[i].ret);
        if (!ret && unc_to_http[i].out) CHECK(!wcscmp(buf, unc_to_http[i].out), "UNC %ls: %ls, expected %ls", unc_to_http[i].path, buf, unc_to_http[i].out);
    }
    for (i = 0; i < sizeof(http_to_unc) / sizeof(http_to_unc[0]); i++)
    {
        buf[0] = 0;
        size = MAX_PATH;
        ret = pDavGetUNCFromHTTPPath(http_to_unc[i].path, buf, &size);
        CHECK(ret == http_to_unc[i].ret, "HTTP %ls: %lu, expected %lu", http_to_unc[i].path, ret, http_to_unc[i].ret);
        if (!ret && http_to_unc[i].out) CHECK(!wcscmp(buf, http_to_unc[i].out), "HTTP %ls: %ls, expected %ls", http_to_unc[i].path, buf, http_to_unc[i].out);
    }

    /* the sizes asked for */
    size = 0;
    CHECK(pDavGetHTTPFromUNCPath(L"\\\\a\\b", buf, &size) == ERROR_INSUFFICIENT_BUFFER && size == 11, "UNC size %lu", size);
    size = 0;
    CHECK(pDavGetUNCFromHTTPPath(L"http://server/path", buf, &size) == ERROR_INSUFFICIENT_BUFFER && size == 25, "HTTP size %lu", size);

    /* the other DsRole levels */
    ret = DsRoleGetPrimaryDomainInformation(NULL, DsRoleUpgradeStatus, &info);
    CHECK(ret == ERROR_SUCCESS && info, "upgrade status %lu", ret);
    if (info)
    {
        CHECK(((DSROLE_UPGRADE_STATUS_INFO *)info)->OperationState == 0, "upgrade operation state");
        DsRoleFreeMemory(info);
    }
    info = NULL;
    ret = DsRoleGetPrimaryDomainInformation(NULL, DsRoleOperationState, &info);
    CHECK(ret == ERROR_SUCCESS && info, "operation state %lu", ret);
    if (info)
    {
        CHECK(((DSROLE_OPERATION_STATE_INFO *)info)->OperationState == DsRoleOperationIdle, "operation state not idle");
        DsRoleFreeMemory(info);
    }
    info = NULL;
    CHECK(DsRoleGetPrimaryDomainInformation(NULL, (DSROLE_PRIMARY_DOMAIN_INFO_LEVEL)9, &info) == ERROR_INVALID_PARAMETER, "bad level");

    /* a server that is not this machine and cannot be reached */
    ret = NetUserGetInfo(L"\\\\Ba  path", L"testuser", 0, &user);
    CHECK(ret == ERROR_BAD_NETPATH, "remote user info: %lu", ret);
    ret = NetUserEnum(L"\\\\Ba  path", 0, 0, &user, MAX_PREFERRED_LENGTH, &size, &i, NULL);
    CHECK(ret == ERROR_BAD_NETPATH, "remote user enum: %lu", ret);

    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures != 0;
}
