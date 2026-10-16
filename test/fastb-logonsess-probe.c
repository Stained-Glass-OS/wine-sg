/* LsaEnumerateLogonSessions / LsaGetLogonSessionData (patch 3743) */
#include <windows.h>
#include <ntsecapi.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <stdlib.h>

static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

#define NT_NO_SUCH_LOGON_SESSION ((LONG)0xc000005f)
#define NT_INVALID_PARAMETER ((LONG)0xc000000d)

static int same(const LSA_UNICODE_STRING *s, const WCHAR *w)
{
    return s->Buffer && s->Length == wcslen(w) * sizeof(WCHAR) && !wmemcmp(s->Buffer, w, s->Length / sizeof(WCHAR)) &&
           s->Buffer[s->Length / sizeof(WCHAR)] == 0;
}

int main(void)
{
    TOKEN_STATISTICS stats;
    TOKEN_USER *tu;
    HANDLE token;
    ULONG count = 0, i, size;
    PLUID list = NULL;
    SECURITY_LOGON_SESSION_DATA *data;
    LUID bogus = { 0x12345, 0 }, system_luid = { 0x3e7, 0 };
    BYTE sid[SECURITY_MAX_SID_SIZE];
    WCHAR user[256], computer[64];
    DWORD len, sid_size, session;
    LONG st;
    int found_system = 0, found_current = 0;

    OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);
    GetTokenInformation(token, TokenStatistics, &stats, sizeof(stats), &len);
    GetTokenInformation(token, TokenUser, NULL, 0, &len);
    tu = malloc(len);
    GetTokenInformation(token, TokenUser, tu, len, &len);
    GetTokenInformation(token, TokenSessionId, &session, sizeof(session), &len);

    st = LsaEnumerateLogonSessions(NULL, &list); CHECK(st == NT_INVALID_PARAMETER, "NULL count: %#lx", st);
    st = LsaEnumerateLogonSessions(&count, NULL); CHECK(st == NT_INVALID_PARAMETER, "NULL list: %#lx", st);
    st = LsaEnumerateLogonSessions(&count, &list);
    CHECK(st == 0 && count >= 2 && list, "enumerate: %#lx count %lu", st, count);
    for (i = 0; st == 0 && i < count; i++)
    {
        if (list[i].LowPart == 0x3e7 && list[i].HighPart == 0) found_system = 1;
        if (!memcmp(&list[i], &stats.AuthenticationId, sizeof(LUID))) found_current = 1;
        st = LsaGetLogonSessionData(&list[i], &data);
        CHECK(st == 0, "session %lu: %#lx", i, st);
        if (st == 0) LsaFreeReturnBuffer(data);
    }
    CHECK(found_system, "the SYSTEM session is listed");
    CHECK(found_current, "the session of the token is listed");
    if (list) LsaFreeReturnBuffer(list);

    st = LsaGetLogonSessionData(&bogus, &data); CHECK(st == NT_NO_SUCH_LOGON_SESSION, "unknown LUID: %#lx", st);
    st = LsaGetLogonSessionData(NULL, &data); CHECK(st == NT_INVALID_PARAMETER, "NULL LUID: %#lx", st);
    st = LsaGetLogonSessionData(&bogus, NULL); CHECK(st == NT_INVALID_PARAMETER, "NULL out: %#lx", st);

    st = LsaGetLogonSessionData(&system_luid, &data);
    CHECK(st == 0 && data, "SYSTEM: %#lx", st);
    if (st == 0 && data)
    {
        sid_size = sizeof(sid);
        CreateWellKnownSid(WinLocalSystemSid, NULL, sid, &sid_size);
        CHECK(data->Size >= sizeof(*data), "Size %lu", data->Size);
        CHECK(!memcmp(&data->LogonId, &system_luid, sizeof(LUID)), "LogonId");
        CHECK(same(&data->UserName, L"SYSTEM"), "user %.*ls", data->UserName.Length / 2, data->UserName.Buffer);
        CHECK(same(&data->LogonDomain, L"NT AUTHORITY"), "domain");
        CHECK(same(&data->AuthenticationPackage, L"Negotiate"), "package");
        CHECK(data->Sid && EqualSid(data->Sid, sid), "SID is S-1-5-18");
        CHECK(data->LogonType == Service, "type %d", data->LogonType);
        CHECK(data->Session == 0, "session %lu", data->Session);
        CHECK(data->LogonTime.QuadPart != 0, "logon time");
        LsaFreeReturnBuffer(data);
    }

    st = LsaGetLogonSessionData(&stats.AuthenticationId, &data);
    CHECK(st == 0 && data, "the token's session: %#lx", st);
    if (st == 0 && data)
    {
        len = ARRAYSIZE(user);
        GetUserNameW(user, &len);
        size = ARRAYSIZE(computer);
        GetComputerNameW(computer, &size);
        CHECK(!memcmp(&data->LogonId, &stats.AuthenticationId, sizeof(LUID)), "LogonId");
        CHECK(same(&data->UserName, user), "user %.*ls / %ls", data->UserName.Length / 2, data->UserName.Buffer, user);
        CHECK(data->LogonDomain.Buffer != NULL && data->LogonDomain.Length > 0, "domain");
        CHECK(same(&data->LogonServer, computer), "server");
        CHECK(same(&data->AuthenticationPackage, L"Negotiate"), "package");
        CHECK(data->Sid && EqualSid(data->Sid, tu->User.Sid), "SID is the token's");
        CHECK(data->LogonType == Interactive, "type %d", data->LogonType);
        CHECK(data->Session == session, "session %lu / %lu", data->Session, session);
        CHECK(data->DnsDomainName.Buffer != NULL && data->Upn.Buffer != NULL, "empty strings are strings");
        LsaFreeReturnBuffer(data);
    }
    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    return f != 0;
}
