/* netapi32: server and workstation information, display information, domain controller queries, global groups */
#include <windows.h>
#include <lm.h>
#include <stdio.h>
#include <string.h>
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); fflush(stdout); } } while (0)

typedef struct
{
    DWORD sv102_platform_id;
    LMSTR sv102_name;
    DWORD sv102_version_major;
    DWORD sv102_version_minor;
    DWORD sv102_type;
    LMSTR sv102_comment;
    DWORD sv102_users;
    LONG  sv102_disc;
    BOOL  sv102_hidden;
    DWORD sv102_announce;
    DWORD sv102_anndelta;
    DWORD sv102_licenses;
    LMSTR sv102_userpath;
} SV102;

typedef struct { DWORD MachineRole; DWORD Flags; LPWSTR DomainNameFlat; LPWSTR DomainNameDns; LPWSTR DomainForestName; GUID DomainGuid; } DSROLE_BASIC;

static WCHAR computer[MAX_COMPUTERNAME_LENGTH + 1];
static WCHAR workgroup[256];

static void test_server_enum(void)
{
    SERVER_INFO_101 *info = NULL;
    DWORD read = 9, total = 9, ret;
    LPBYTE buf;

    ret = NetServerEnum(NULL, 101, (LPBYTE *)&info, MAX_PREFERRED_LENGTH, &read, &total, SV_TYPE_ALL, NULL, NULL);
    CHECK(ret == NERR_Success && read == 1 && total == 1 && info, "NetServerEnum %lu %lu %lu", ret, read, total);
    if (info)
    {
        CHECK(!lstrcmpiW(info->sv101_name, computer), "name %ls", info->sv101_name);
        CHECK((info->sv101_type & (SV_TYPE_WORKSTATION | SV_TYPE_SERVER)) == (SV_TYPE_WORKSTATION | SV_TYPE_SERVER), "type %#lx", info->sv101_type);
        CHECK(info->sv101_comment && !info->sv101_comment[0] && info->sv101_platform_id == PLATFORM_ID_NT, "comment/platform");
        NetApiBufferFree(info);
    }
    ret = NetServerEnum(NULL, 100, &buf, MAX_PREFERRED_LENGTH, &read, &total, SV_TYPE_WORKSTATION, workgroup, NULL);
    CHECK(ret == NERR_Success && read == 1 && !lstrcmpiW(((SERVER_INFO_100 *)buf)->sv100_name, computer), "level 100 in the workgroup %lu", ret);
    if (ret == NERR_Success) NetApiBufferFree(buf);
    ret = NetServerEnum(NULL, 101, &buf, MAX_PREFERRED_LENGTH, &read, &total, SV_TYPE_DOMAIN_CTRL, NULL, NULL);
    CHECK(ret == NERR_Success && read == 0 && total == 0, "no domain controller %lu %lu", ret, read);
    ret = NetServerEnum(NULL, 101, &buf, MAX_PREFERRED_LENGTH, &read, &total, SV_TYPE_ALL, L"NOSUCHGROUP", NULL);
    CHECK(ret == ERROR_NO_BROWSER_SERVERS_FOUND, "other domain %lu", ret);
    ret = NetServerEnum(NULL, 102, &buf, MAX_PREFERRED_LENGTH, &read, &total, SV_TYPE_ALL, NULL, NULL);
    CHECK(ret == ERROR_INVALID_LEVEL, "level 102 %lu", ret);
    ret = NetServerEnum(NULL, 101, &buf, 10, &read, &total, SV_TYPE_ALL, NULL, NULL);
    CHECK(ret == ERROR_MORE_DATA && read == 0 && total == 1, "short buffer %lu %lu %lu", ret, read, total);
    ret = NetServerEnum(NULL, 101, NULL, MAX_PREFERRED_LENGTH, &read, &total, SV_TYPE_ALL, NULL, NULL);
    CHECK(ret == ERROR_INVALID_PARAMETER, "no buffer pointer %lu", ret);
    ret = NetServerEnum(L"\\\\nosuchhost", 101, &buf, MAX_PREFERRED_LENGTH, &read, &total, SV_TYPE_ALL, NULL, NULL);
    CHECK(ret == ERROR_INVALID_PARAMETER, "servername is reserved %lu", ret);
}

static void test_server_info(void)
{
    SERVER_INFO_101 *i101 = NULL;
    SV102 *i102 = NULL;
    DWORD *value = NULL, ret;
    LMSTR *comment = NULL;

    ret = NetServerGetInfo(NULL, 101, (LPBYTE *)&i101);
    CHECK(ret == NERR_Success && i101 && (i101->sv101_type & (SV_TYPE_WORKSTATION | SV_TYPE_SERVER)) == (SV_TYPE_WORKSTATION | SV_TYPE_SERVER),
          "level 101 %lu", ret);
    if (i101) NetApiBufferFree(i101);

    ret = NetServerGetInfo(NULL, 102, (LPBYTE *)&i102);
    if (ret == ERROR_ACCESS_DENIED) printf("level 102 needs an administrator here\n");
    else
    {
        CHECK(ret == NERR_Success && i102, "level 102 %lu", ret);
        if (i102)
        {
            CHECK(!lstrcmpiW(i102->sv102_name, computer) && i102->sv102_disc == 15 && !i102->sv102_hidden && i102->sv102_announce == 240 &&
                  i102->sv102_anndelta == 3000 && i102->sv102_users == ~0u && i102->sv102_userpath && i102->sv102_userpath[0] && !i102->sv102_comment[0],
                  "level 102 content");
            NetApiBufferFree(i102);
        }
    }
    ret = NetServerGetInfo(NULL, 1010, (LPBYTE *)&value);
    CHECK(ret == NERR_Success && value && *value == 15, "1010 %lu", ret);
    if (value) NetApiBufferFree(value);
    ret = NetServerGetInfo(NULL, 1017, (LPBYTE *)&value);
    CHECK(ret == NERR_Success && value && *value == 240, "1017 %lu", ret);
    if (value) NetApiBufferFree(value);
    ret = NetServerGetInfo(NULL, 1016, (LPBYTE *)&value);
    CHECK(ret == NERR_Success && value && *value == 0, "1016 %lu", ret);
    if (value) NetApiBufferFree(value);
    ret = NetServerGetInfo(NULL, 1018, (LPBYTE *)&value);
    CHECK(ret == NERR_Success && value && *value == 3000, "1018 %lu", ret);
    if (value) NetApiBufferFree(value);
    ret = NetServerGetInfo(NULL, 1107, (LPBYTE *)&value);
    CHECK(ret == NERR_Success && value && *value == ~0u, "1107 %lu", ret);
    if (value) NetApiBufferFree(value);
    ret = NetServerGetInfo(NULL, 1005, (LPBYTE *)&comment);
    CHECK(ret == NERR_Success && comment && *comment && !(*comment)[0], "1005 %lu", ret);
    if (comment) NetApiBufferFree(comment);
    ret = NetServerGetInfo(NULL, 77, (LPBYTE *)&value);
    CHECK(ret == ERROR_INVALID_LEVEL, "bad level %lu", ret);
}

static void test_wksta(void)
{
    WKSTA_INFO_102 *info = NULL;
    WKSTA_USER_INFO_1 *ui1 = NULL;
    WKSTA_USER_INFO_1101 *ui1101 = NULL;
    HKEY key;
    DWORD ret, trans_read, trans_total, resume = 0;
    LPBYTE buf;
    WCHAR server[MAX_COMPUTERNAME_LENGTH + 3];

    ret = NetWkstaGetInfo(NULL, 102, (LPBYTE *)&info);
    CHECK(ret == NERR_Success && info && !lstrcmpiW(info->wki102_langroup, workgroup), "langroup is the workgroup %lu %ls", ret, info ? info->wki102_langroup : L"-");
    if (info) NetApiBufferFree(info);
    ret = NetWkstaGetInfo(NULL, 103, (LPBYTE *)&info);
    CHECK(ret == ERROR_INVALID_LEVEL, "bad level %lu", ret);

    lstrcpyW(server, L"\\\\");
    lstrcatW(server, computer);
    ret = NetWkstaUserGetInfo(NULL, 1, (LPBYTE *)&ui1);
    CHECK(ret == NERR_Success && ui1, "level 1 %lu", ret);
    if (ui1)
    {
        CHECK(!lstrcmpiW(ui1->wkui1_logon_server, server), "logon server '%ls'", ui1->wkui1_logon_server);
        CHECK(!ui1->wkui1_oth_domains[0], "no other domains '%ls'", ui1->wkui1_oth_domains);
        NetApiBufferFree(ui1);
    }

    RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Services\\LanmanWorkstation\\Parameters", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL);
    if (key)
    {
        RegSetValueExW(key, L"OtherDomains", 0, REG_SZ, (const BYTE *)L"AAA BBB", sizeof(L"AAA BBB"));
        ret = NetWkstaUserGetInfo(NULL, 1101, (LPBYTE *)&ui1101);
        CHECK(ret == NERR_Success && ui1101 && !lstrcmpW(ui1101->wkui1101_oth_domains, L"AAA BBB"), "other domains from the registry %lu", ret);
        if (ui1101) NetApiBufferFree(ui1101);
        ret = NetWkstaUserGetInfo(NULL, 1, (LPBYTE *)&ui1);
        CHECK(ret == NERR_Success && ui1 && !lstrcmpW(ui1->wkui1_oth_domains, L"AAA BBB") && !lstrcmpiW(ui1->wkui1_logon_server, server),
              "other domains in level 1 %lu", ret);
        if (ui1) NetApiBufferFree(ui1);
        RegSetValueExW(key, L"OtherDomains", 0, REG_MULTI_SZ, (const BYTE *)L"XX\0YY\0", sizeof(L"XX\0YY\0"));
        ret = NetWkstaUserGetInfo(NULL, 1101, (LPBYTE *)&ui1101);
        CHECK(ret == NERR_Success && ui1101 && !lstrcmpW(ui1101->wkui1101_oth_domains, L"XX YY"), "list of other domains %lu '%ls'", ret,
              ui1101 ? ui1101->wkui1101_oth_domains : L"-");
        if (ui1101) NetApiBufferFree(ui1101);
        RegDeleteValueW(key, L"OtherDomains");
        RegCloseKey(key);
    }
    ret = NetWkstaUserGetInfo(NULL, 1101, (LPBYTE *)&ui1101);
    CHECK(ret == NERR_Success && ui1101 && !ui1101->wkui1101_oth_domains[0], "no other domains after the value is gone");
    if (ui1101) NetApiBufferFree(ui1101);

    ret = NetWkstaTransportEnum(L"\\\\nosuchhost", 0, &buf, MAX_PREFERRED_LENGTH, &trans_read, &trans_total, &resume);
    CHECK(ret == ERROR_BAD_NETPATH, "transports of another computer %lu", ret);
    ret = NetWkstaTransportEnum(L"\\\\", 0, &buf, MAX_PREFERRED_LENGTH, &trans_read, &trans_total, &resume);
    CHECK(ret == ERROR_INVALID_NAME, "transports, bad name %lu", ret);
}

static void test_display(void)
{
    NET_DISPLAY_USER *users;
    NET_DISPLAY_GROUP *groups;
    DWORD count, ret;
    void *buf;

    ret = NetQueryDisplayInformation(NULL, 1, 0, 100, MAX_PREFERRED_LENGTH, &count, (void **)&users);
    CHECK(ret == NERR_Success && count == 3, "three users %lu %lu", ret, count);
    if (ret == NERR_Success)
    {
        CHECK(users[0].usri1_next_index == 1 && users[1].usri1_next_index == 2 && users[2].usri1_next_index == 3, "next indexes");
        CHECK(!lstrcmpW(users[1].usri1_name, L"Administrator") && users[1].usri1_user_id == DOMAIN_USER_RID_ADMIN, "administrator");
        CHECK(!lstrcmpW(users[2].usri1_name, L"Guest") && (users[2].usri1_flags & UF_ACCOUNTDISABLE), "guest");
        NetApiBufferFree(users);
    }
    ret = NetQueryDisplayInformation(NULL, 1, 0, 2, MAX_PREFERRED_LENGTH, &count, (void **)&users);
    CHECK(ret == ERROR_MORE_DATA && count == 2 && users[1].usri1_next_index == 2, "two requested %lu %lu", ret, count);
    if (count) NetApiBufferFree(users);
    ret = NetQueryDisplayInformation(NULL, 1, 2, 100, MAX_PREFERRED_LENGTH, &count, (void **)&users);
    CHECK(ret == NERR_Success && count == 1 && !lstrcmpW(users[0].usri1_name, L"Guest"), "from index 2 %lu %lu", ret, count);
    if (count) NetApiBufferFree(users);
    ret = NetQueryDisplayInformation(NULL, 1, 1, 100, 1, &count, (void **)&users);
    CHECK(ret == ERROR_MORE_DATA && count == 1 && users[0].usri1_next_index == 2, "preferred length of one entry %lu %lu", ret, count);
    if (count) NetApiBufferFree(users);
    ret = NetQueryDisplayInformation(NULL, 1, 7, 100, MAX_PREFERRED_LENGTH, &count, (void **)&users);
    CHECK(ret == NERR_Success && count == 0, "index past the end %lu %lu", ret, count);
    ret = NetQueryDisplayInformation(NULL, 2, 0, 100, MAX_PREFERRED_LENGTH, &count, &buf);
    CHECK(ret == NERR_Success && count == 0, "no machine accounts %lu %lu", ret, count);
    ret = NetQueryDisplayInformation(NULL, 3, 0, 100, MAX_PREFERRED_LENGTH, &count, (void **)&groups);
    CHECK(ret == NERR_Success && count == 1 && !lstrcmpW(groups[0].grpi3_name, L"None") && groups[0].grpi3_group_id == DOMAIN_GROUP_RID_USERS &&
          groups[0].grpi3_attributes == 7 && groups[0].grpi3_next_index == 1, "groups %lu %lu", ret, count);
    if (count) NetApiBufferFree(groups);
    ret = NetQueryDisplayInformation(NULL, 4, 0, 100, MAX_PREFERRED_LENGTH, &count, &buf);
    CHECK(ret == ERROR_INVALID_LEVEL, "bad level %lu", ret);
    ret = NetQueryDisplayInformation(NULL, 1, 0, 100, MAX_PREFERRED_LENGTH, NULL, &buf);
    CHECK(ret == ERROR_INVALID_PARAMETER, "no count %lu", ret);
    ret = NetQueryDisplayInformation(L"\\\\nosuchhost", 1, 0, 100, MAX_PREFERRED_LENGTH, &count, &buf);
    CHECK(ret == ERROR_ACCESS_DENIED, "another computer %lu", ret);
}

static void test_dc_and_groups(void)
{
    LPBYTE buf = (LPBYTE)0x1234;
    DWORD ret, read = 5, total = 5;
    DSROLE_BASIC *basic = NULL;
    WCHAR *name = NULL;
    NETSETUP_JOIN_STATUS status;
    HMODULE dsrole = GetModuleHandleA("netapi32.dll");
    DWORD (WINAPI *role_info)(LPCWSTR, int, PBYTE *) = (void *)GetProcAddress(dsrole, "DsRoleGetPrimaryDomainInformation");
    void (WINAPI *role_free)(void *) = (void *)GetProcAddress(dsrole, "DsRoleFreeMemory");

    CHECK(NetGetDCName(NULL, NULL, &buf) == NERR_DCNotFound, "NetGetDCName");
    CHECK(NetGetDCName(NULL, L"DOM", &buf) == NERR_DCNotFound, "NetGetDCName with a domain");
    CHECK(NetGetDCName(L"\\\\nosuchhost", NULL, &buf) == ERROR_BAD_NETPATH, "NetGetDCName, another computer");
    CHECK(NetGetDCName(L"\\\\", NULL, &buf) == ERROR_INVALID_NAME, "NetGetDCName, bad name");
    CHECK(NetGetDCName(NULL, NULL, NULL) == ERROR_INVALID_PARAMETER, "NetGetDCName without buffer");
    CHECK(NetGetAnyDCName(NULL, NULL, &buf) == ERROR_NO_LOGON_SERVERS, "NetGetAnyDCName");
    CHECK(NetGetAnyDCName(NULL, L"DOM", &buf) == ERROR_NO_SUCH_DOMAIN, "NetGetAnyDCName with a domain");
    CHECK(NetGetAnyDCName(L"\\\\nosuchhost", NULL, &buf) == ERROR_BAD_NETPATH, "NetGetAnyDCName, another computer");
    CHECK(NetGetAnyDCName(NULL, NULL, NULL) == ERROR_INVALID_PARAMETER, "NetGetAnyDCName without buffer");

    buf = (LPBYTE)0x1234;
    ret = NetGroupEnum(NULL, 0, &buf, MAX_PREFERRED_LENGTH, &read, &total, NULL);
    CHECK(ret == NERR_InvalidComputer && buf == NULL && read == 0 && total == 0, "NetGroupEnum %lu", ret);
    CHECK(NetGroupEnum(NULL, 4, &buf, MAX_PREFERRED_LENGTH, &read, &total, NULL) == ERROR_INVALID_LEVEL, "NetGroupEnum level 4");
    CHECK(NetGroupEnum(L"\\\\nosuchhost", 0, &buf, MAX_PREFERRED_LENGTH, &read, &total, NULL) == ERROR_BAD_NETPATH, "NetGroupEnum, another computer");
    CHECK(NetGroupEnum(NULL, 0, NULL, MAX_PREFERRED_LENGTH, &read, &total, NULL) == ERROR_INVALID_PARAMETER, "NetGroupEnum without buffer");
    ret = NetGroupGetInfo(NULL, L"None", 1, &buf);
    CHECK(ret == NERR_InvalidComputer, "NetGroupGetInfo %lu", ret);
    CHECK(NetGroupGetInfo(NULL, L"None", 9, &buf) == ERROR_INVALID_LEVEL, "NetGroupGetInfo level 9");
    CHECK(NetGroupGetInfo(L"\\\\nosuchhost", L"None", 1, &buf) == ERROR_BAD_NETPATH, "NetGroupGetInfo, another computer");
    CHECK(NetGroupGetInfo(NULL, NULL, 1, &buf) == ERROR_INVALID_PARAMETER, "NetGroupGetInfo without name");

    /* the role of the computer: a stand-alone workstation of its workgroup */
    ret = NetGetJoinInformation(NULL, &name, &status);
    CHECK(ret == NERR_Success && status == NetSetupWorkgroupName, "join information");
    ret = role_info(NULL, 1, (PBYTE *)&basic);
    CHECK(ret == ERROR_SUCCESS && basic, "DsRoleGetPrimaryDomainInformation %lu", ret);
    if (basic)
    {
        CHECK(basic->MachineRole == 0, "stand-alone workstation %lu", basic->MachineRole);
        CHECK(name && !lstrcmpW(basic->DomainNameFlat, name), "workgroup name '%ls' / '%ls'", basic->DomainNameFlat, name);
        CHECK(!basic->DomainNameDns && !basic->DomainForestName && basic->Flags == 0, "no DNS names");
        role_free(basic);
    }
    ret = role_info(L"\\\\nosuchhost", 1, (PBYTE *)&basic);
    CHECK(ret == RPC_S_SERVER_UNAVAILABLE, "role of another computer %lu", ret);
    if (name) NetApiBufferFree(name);
}

int main(void)
{
    DWORD len = ARRAY_SIZE(computer);
    LPWSTR name = NULL;
    NETSETUP_JOIN_STATUS status;

    GetComputerNameW(computer, &len);
    if (NetGetJoinInformation(NULL, &name, &status) == NERR_Success && name)
    {
        lstrcpynW(workgroup, name, ARRAY_SIZE(workgroup));
        NetApiBufferFree(name);
    }
    test_server_enum();
    test_server_info();
    test_wksta();
    test_display();
    test_dc_and_groups();
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    fflush(stdout);
    return fails;
}
