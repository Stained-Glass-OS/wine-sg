/* netapi32 local accounts and groups (patch 3180), run by test/fastnetacct-gate.sh
 * against a stand-in sg-admind spool and a stand-in /etc (SG_ACCOUNTS_ROOT).
 *   fastnetacct-probe.exe admin     every call as an administrator
 *   fastnetacct-probe.exe nonadmin  the same from a thread without Administrators */
#include <windows.h>
#include <lm.h>
#include <sddl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static int failures;
static void expect(DWORD got, DWORD want, const char *what)
{
    int ok = got == want;
    printf("%s  %s (%lu, wanted %lu)\n", ok ? "PASS" : "FAIL", what, got, want);
    fflush(stdout);
    if (!ok) failures++;
}
static void checkstr(const WCHAR *got, const WCHAR *want, const char *what)
{
    int ok = got && !wcscmp(got, want);
    printf("%s  %s (%ls, wanted %ls)\n", ok ? "PASS" : "FAIL", what, got ? got : L"(null)", want);
    fflush(stdout);
    if (!ok) failures++;
}

static HANDLE restricted_thread_token(void)
{
    HANDLE proc, restricted, imp = NULL;
    SID_IDENTIFIER_AUTHORITY nt = { SECURITY_NT_AUTHORITY };
    PSID admins;
    SID_AND_ATTRIBUTES sa;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &proc)) return NULL;
    AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &admins);
    sa.Sid = admins; sa.Attributes = 0;
    if (CreateRestrictedToken(proc, DISABLE_MAX_PRIVILEGE, 1, &sa, 0, NULL, 0, NULL, &restricted))
    {
        DuplicateTokenEx(restricted, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenImpersonation, &imp);
        CloseHandle(restricted);
    }
    CloseHandle(proc);
    FreeSid(admins);
    return imp;
}

static WCHAR comp[MAX_COMPUTERNAME_LENGTH + 1];

static DWORD add_user(const WCHAR *name, const WCHAR *pw)
{
    USER_INFO_1 u = {0};
    u.usri1_name = (WCHAR *)name; u.usri1_password = (WCHAR *)pw; u.usri1_priv = USER_PRIV_USER; u.usri1_flags = UF_SCRIPT;
    return NetUserAdd(NULL, 1, (BYTE *)&u, NULL);
}

static DWORD members3(const WCHAR *group, const WCHAR *names[], DWORD n, int op)
{
    LOCALGROUP_MEMBERS_INFO_3 m[8];
    WCHAR text[8][96];
    DWORD i;
    for (i = 0; i < n; i++) { swprintf(text[i], 96, L"%ls\\%ls", comp, names[i]); m[i].lgrmi3_domainandname = text[i]; }
    return op == 0 ? NetLocalGroupAddMembers(NULL, group, 3, (BYTE *)m, n)
         : op == 1 ? NetLocalGroupDelMembers(NULL, group, 3, (BYTE *)m, n)
         : NetLocalGroupSetMembers(NULL, group, 3, (BYTE *)m, n);
}

/* the members of the group, as one string "a,b" (sorted order not assumed: names are checked by membership) */
static DWORD get_members(const WCHAR *group, WCHAR *out, size_t len)
{
    LOCALGROUP_MEMBERS_INFO_3 *m = NULL;
    DWORD read = 0, total = 0, i, ret;
    out[0] = 0;
    ret = NetLocalGroupGetMembers(NULL, group, 3, (BYTE **)&m, MAX_PREFERRED_LENGTH, &read, &total, NULL);
    if (ret) return ret;
    for (i = 0; i < read; i++)
    {
        const WCHAR *bs = wcschr(m[i].lgrmi3_domainandname, '\\');
        if (out[0]) wcscat(out, L",");
        wcsncat(out, bs ? bs + 1 : m[i].lgrmi3_domainandname, len - wcslen(out) - 2);
    }
    NetApiBufferFree(m);
    return NERR_Success;
}

static void sequence(int admin)
{
    DWORD ok = admin ? NERR_Success : ERROR_ACCESS_DENIED;
    WCHAR buf[256];
    const WCHAR *one[1], *two[2];
    LOCALGROUP_INFO_1 g1 = { (WCHAR *)L"Testers", (WCHAR *)L"first comment" };
    LOCALGROUP_INFO_1 *gi = NULL;
    LOCALGROUP_INFO_1 *list = NULL;
    DWORD read = 0, total = 0, i, found;

    GetComputerNameW(comp, &(DWORD){ARRAYSIZE(comp)});

    expect(add_user(L"dave", L"Secret1!"), ok, "NetUserAdd");
    expect(add_user(L"alice", L"Secret1!"), admin ? NERR_UserExists : ERROR_ACCESS_DENIED, "NetUserAdd of an existing account");
    expect(add_user(L"a/b", L"Secret1!"), NERR_BadUsername, "NetUserAdd, bad name");
    expect(NetUserDel(NULL, L"bob"), ok, "NetUserDel");
    expect(NetUserDel(NULL, L"zed"), NERR_UserNotFound, "NetUserDel of no account");
    {
        USER_INFO_1003 p = { (WCHAR *)L"NewPass9!" };
        USER_INFO_10 other = {0};
        expect(NetUserSetInfo(NULL, L"carol", 1003, (BYTE *)&p, NULL), ok, "NetUserSetInfo password");
        expect(NetUserSetInfo(NULL, L"zed", 1003, (BYTE *)&p, NULL), NERR_UserNotFound, "NetUserSetInfo of no account");
        expect(NetUserSetInfo(NULL, L"carol", 77, (BYTE *)&other, NULL), ERROR_INVALID_LEVEL, "NetUserSetInfo bad level");
        expect(NetUserSetInfo(NULL, L"carol", 1011, (BYTE *)&other, NULL), ERROR_NOT_SUPPORTED, "NetUserSetInfo full name (not kept)");
    }

    /* the accounts' groups */
    expect(get_members(L"Administrators", buf, 256), NERR_Success, "members of Administrators");
    checkstr(buf, L"alice", "Administrators are the administrator accounts");
    expect(get_members(L"Users", buf, 256), NERR_Success, "members of Users");
    checkstr(buf, L"bob,carol", "Users are the standard accounts");
    one[0] = L"bob";
    expect(members3(L"Administrators", one, 1, 0), ok, "add an account to Administrators");
    one[0] = L"alice";
    expect(members3(L"Administrators", one, 1, 0), admin ? ERROR_MEMBER_IN_ALIAS : ERROR_ACCESS_DENIED, "add an administrator again");
    one[0] = L"nosuchperson";
    expect(members3(L"Administrators", one, 1, 0), admin ? ERROR_NO_SUCH_MEMBER : ERROR_ACCESS_DENIED, "add an unknown account");
    one[0] = L"alice";
    expect(members3(L"Administrators", one, 1, 1), ok, "remove an account from Administrators");
    one[0] = L"carol";
    expect(members3(L"Administrators", one, 1, 1), admin ? ERROR_MEMBER_NOT_IN_ALIAS : ERROR_ACCESS_DENIED, "remove a non-member");
    expect(NetLocalGroupDelMembers(NULL, L"NoSuchGroup", 3, NULL, 0), admin ? NERR_GroupNotFound : ERROR_ACCESS_DENIED, "unknown group");

    /* other groups: the registry */
    expect(NetLocalGroupAdd(NULL, 1, (BYTE *)&g1, NULL), ok, "NetLocalGroupAdd");
    expect(NetLocalGroupAdd(NULL, 1, (BYTE *)&g1, NULL), admin ? ERROR_ALIAS_EXISTS : ERROR_ACCESS_DENIED, "NetLocalGroupAdd of an existing group");
    {
        LOCALGROUP_INFO_0 bad = { (WCHAR *)L"bad/name" };
        expect(NetLocalGroupAdd(NULL, 0, (BYTE *)&bad, NULL), ERROR_INVALID_PARAMETER, "NetLocalGroupAdd, bad name");
        expect(NetLocalGroupAdd(NULL, 5, (BYTE *)&bad, NULL), ERROR_INVALID_LEVEL, "NetLocalGroupAdd, bad level");
    }
    if (admin)
    {
        expect(NetLocalGroupGetInfo(NULL, L"Testers", 1, (BYTE **)&gi), NERR_Success, "NetLocalGroupGetInfo");
        checkstr(gi ? gi->lgrpi1_comment : NULL, L"first comment", "the group's comment");
        NetApiBufferFree(gi);
        {
            LOCALGROUP_INFO_1002 c = { (WCHAR *)L"second comment" };
            expect(NetLocalGroupSetInfo(NULL, L"Testers", 1002, (BYTE *)&c, NULL), NERR_Success, "NetLocalGroupSetInfo comment");
        }
        gi = NULL;
        NetLocalGroupGetInfo(NULL, L"Testers", 1, (BYTE **)&gi);
        checkstr(gi ? gi->lgrpi1_comment : NULL, L"second comment", "the comment was changed");
        NetApiBufferFree(gi);
        expect(NetLocalGroupGetInfo(NULL, L"NoSuchGroup", 1, (BYTE **)&gi), NERR_GroupNotFound, "NetLocalGroupGetInfo of no group");
        expect(NetLocalGroupEnum(NULL, 1, (BYTE **)&list, MAX_PREFERRED_LENGTH, &read, &total, NULL), NERR_Success, "NetLocalGroupEnum");
        for (i = 0, found = 0; i < read; i++)
            if (!wcscmp(list[i].lgrpi1_name, L"Testers") || !wcscmp(list[i].lgrpi1_name, L"Administrators") ||
                !wcscmp(list[i].lgrpi1_name, L"Users")) found++;
        expect(found, 3, "the enumeration has built-in and created groups");
        NetApiBufferFree(list);
    }
    one[0] = L"alice";
    expect(members3(L"Testers", one, 1, 0), ok, "add a member to a created group");
    if (admin)
    {
        expect(get_members(L"Testers", buf, 256), NERR_Success, "members of Testers");
        checkstr(buf, L"alice", "the member is listed");
        two[0] = L"bob"; two[1] = L"carol";
        expect(members3(L"Testers", two, 2, 2), NERR_Success, "NetLocalGroupSetMembers");
        get_members(L"Testers", buf, 256);
        checkstr(buf, L"bob,carol", "the members were replaced");
        {
            PSID everyone;
            LOCALGROUP_MEMBERS_INFO_0 m0;
            ConvertStringSidToSidW(L"S-1-1-0", &everyone);
            m0.lgrmi0_sid = everyone;
            expect(NetLocalGroupAddMember(NULL, L"Testers", everyone), NERR_Success, "NetLocalGroupAddMember by SID");
            expect(NetLocalGroupAddMembers(NULL, L"Testers", 0, (BYTE *)&m0, 1), ERROR_MEMBER_IN_ALIAS, "the same SID again");
            expect(NetLocalGroupDelMember(NULL, L"Testers", everyone), NERR_Success, "NetLocalGroupDelMember by SID");
            LocalFree(everyone);
        }
        one[0] = L"bob";
        expect(members3(L"Testers", one, 1, 1), NERR_Success, "remove a member");
        get_members(L"Testers", buf, 256);
        checkstr(buf, L"carol", "one member left");
    }
    expect(NetLocalGroupDel(NULL, L"Administrators"), admin ? ERROR_SPECIAL_ACCOUNT : ERROR_ACCESS_DENIED, "built-in group cannot be deleted");
    expect(NetLocalGroupDel(NULL, L"Testers"), ok, "NetLocalGroupDel");
    expect(NetLocalGroupDel(NULL, L"Testers"), admin ? NERR_GroupNotFound : ERROR_ACCESS_DENIED, "NetLocalGroupDel again");
    expect(NetGroupAddUser(NULL, L"Domain Users", L"alice"), admin ? NERR_NotPrimary : ERROR_ACCESS_DENIED, "NetGroupAddUser (no domain controller)");
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "admin")) sequence(1);
    else if (argc > 1 && !strcmp(argv[1], "nonadmin"))
    {
        HANDLE imp = restricted_thread_token();
        expect(imp && SetThreadToken(NULL, imp), 1, "a thread without the Administrators group");
        sequence(0);
    }
    else if (argc > 3 && !strcmp(argv[1], "reply"))
    {
        const WCHAR *one[1] = { L"alice" };
        GetComputerNameW(comp, &(DWORD){ARRAYSIZE(comp)});
        if (!strcmp(argv[2], "user")) expect(add_user(L"dave", L"Secret1!"), atoi(argv[3]), "NetUserAdd answered by sg-admind");
        else expect(members3(L"Administrators", one, 1, 1), atoi(argv[3]), "type change answered by sg-admind");
    }
    else return 2;
    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures != 0;
}
