/* netapi32, fast mode (patch 3505): the local, read-only calls that were
 * stubs: NetRemoteTOD, NetSessionEnum, NetShareEnum, NetFileEnum,
 * NetServerDiskEnum, NetWkstaUserEnum, NetUseGetInfo, DsGetSiteName,
 * DsGetDcNameA. */
#include <windows.h>
#include <lm.h>
#include <lmremutl.h>
#include <dsgetdc.h>
#include <winerror.h>
#include <stdio.h>
#include <time.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

int main(void)
{
    BYTE *buf = NULL;
    DWORD read = 99, total = 99, ret;
    LPWSTR site = (LPWSTR)1;
    TIME_OF_DAY_INFO *tod;
    SYSTEMTIME st;
    time_t now = time(NULL);

    ret = NetRemoteTOD(NULL, &buf);
    check(ret == NERR_Success && buf, "NetRemoteTOD");
    if (ret == NERR_Success && buf)
    {
        tod = (TIME_OF_DAY_INFO *)buf;
        GetSystemTime(&st);
        check(tod->tod_elapsedt + 5 >= (DWORD)now && tod->tod_elapsedt <= (DWORD)now + 5, "seconds since 1970");
        check(tod->tod_year == st.wYear && tod->tod_month == st.wMonth, "year and month");
        check(tod->tod_hours <= 23 && tod->tod_mins <= 59 && tod->tod_secs <= 59 && tod->tod_hunds <= 99, "time fields");
        check(tod->tod_tinterval == 310, "clock interval");
        NetApiBufferFree(buf);
    }
    check(NetRemoteTOD(NULL, NULL) == ERROR_INVALID_PARAMETER, "NetRemoteTOD argument check");

    buf = (BYTE *)1; read = 99; total = 99;
    ret = NetSessionEnum(NULL, NULL, NULL, 10, &buf, MAX_PREFERRED_LENGTH, &read, &total, NULL);
    check(ret == NERR_Success && read == 0 && total == 0 && buf == NULL, "NetSessionEnum is empty");
    ret = NetSessionEnum(NULL, NULL, NULL, 77, &buf, MAX_PREFERRED_LENGTH, &read, &total, NULL);
    check(ret == ERROR_INVALID_LEVEL, "NetSessionEnum level check");

    buf = (BYTE *)1; read = 99; total = 99;
    ret = NetShareEnum(NULL, 1, &buf, MAX_PREFERRED_LENGTH, &read, &total, NULL);
    check(ret == NERR_Success && read == 0 && total == 0 && buf == NULL, "NetShareEnum is empty");
    ret = NetShareGetInfo(NULL, (LPWSTR)L"nothing", 1, &buf);
    check(ret == NERR_NetNameNotFound, "NetShareGetInfo unknown share");

    buf = (BYTE *)1; read = 99; total = 99;
    ret = NetFileEnum(NULL, NULL, NULL, 3, &buf, MAX_PREFERRED_LENGTH, &read, &total, NULL);
    check(ret == NERR_Success && read == 0 && total == 0, "NetFileEnum is empty");
    ret = NetFileEnum(NULL, NULL, NULL, 9, &buf, MAX_PREFERRED_LENGTH, &read, &total, NULL);
    check(ret == ERROR_INVALID_LEVEL, "NetFileEnum level check");

    buf = NULL; read = 0; total = 0;
    ret = NetServerDiskEnum(NULL, 0, &buf, MAX_PREFERRED_LENGTH, &read, &total, NULL);
    check(ret == NERR_Success && buf && read == total, "NetServerDiskEnum");
    if (ret == NERR_Success && buf)
    {
        WCHAR *p = (WCHAR *)buf;
        DWORD i;
        int ok = 1;

        for (i = 0; i < read; i++)
            if (p[i * 3 + 1] != ':' || p[i * 3 + 2] != 0) ok = 0;
        check(ok, "disks are \"X:\"");
        check(p[read * 3] == 0, "the list ends with an empty entry");
        NetApiBufferFree(buf);
    }

    buf = NULL; read = 0; total = 0;
    ret = NetWkstaUserEnum(NULL, 0, &buf, MAX_PREFERRED_LENGTH, &read, &total, NULL);
    check(ret == NERR_Success && read == 1 && total == 1 && buf, "NetWkstaUserEnum level 0");
    if (buf)
    {
        WCHAR name[256];
        DWORD size = 256;
        GetUserNameW(name, &size);
        check(!lstrcmpW(((WKSTA_USER_INFO_0 *)buf)->wkui0_username, name), "the user is the current one");
        NetApiBufferFree(buf);
    }
    ret = NetWkstaUserEnum(NULL, 5, &buf, MAX_PREFERRED_LENGTH, &read, &total, NULL);
    check(ret == ERROR_INVALID_LEVEL, "NetWkstaUserEnum level check");

    ret = NetUseGetInfo(NULL, (LPWSTR)L"Z:\\nope", 1, &buf);
    check(ret == NERR_UseNotFound, "NetUseGetInfo unknown connection");

    ret = DsGetSiteNameW(NULL, &site);
    check(ret == ERROR_NO_SITENAME && site == NULL, "DsGetSiteNameW outside a domain");
    {
        LPSTR sitea = (LPSTR)1;
        ret = DsGetSiteNameA(NULL, &sitea);
        check(ret == ERROR_NO_SITENAME && sitea == NULL, "DsGetSiteNameA outside a domain");
    }
    {
        DOMAIN_CONTROLLER_INFOA *info = (DOMAIN_CONTROLLER_INFOA *)1;
        ret = DsGetDcNameA(NULL, "no-such-domain.invalid", NULL, NULL, 0, &info);
        check(ret != ERROR_CALL_NOT_IMPLEMENTED, "DsGetDcNameA is implemented");
        if (!ret && info) NetApiBufferFree(info);
        ret = DsGetDcNameA(NULL, NULL, NULL, NULL, 0, NULL);
        check(ret == ERROR_INVALID_PARAMETER, "DsGetDcNameA argument check");
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
