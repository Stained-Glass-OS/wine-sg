/* pipeimp-probe: does a named pipe server impersonate its CLIENT? (wine-sg 0140)
 *
 *   pipeimp-probe server NAME   serve one client: impersonate it, print
 *                               "SERVER <own sid>" and "CLIENT <sid> ADMIN <0|1>"
 *   pipeimp-probe client NAME   connect, print "SELF <own sid>" and the reply
 *
 * Copyright 2026 Stained Glass OS contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <sddl.h>
#include <stdio.h>

static void token_sid(HANDLE token, char *out, size_t cch)
{
    char buf[256];
    DWORD len;
    char *s = NULL;
    snprintf(out, cch, "?");
    if (GetTokenInformation(token, TokenUser, buf, sizeof(buf), &len) &&
        ConvertSidToStringSidA(((TOKEN_USER *)buf)->User.Sid, &s))
    {
        snprintf(out, cch, "%s", s);
        LocalFree(s);
    }
}

static int is_admin(HANDLE token)
{
    SID_IDENTIFIER_AUTHORITY nt = { SECURITY_NT_AUTHORITY };
    PSID admins;
    BOOL member = FALSE;
    HANDLE dup = NULL;
    if (!AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &admins))
        return -1;
    if (DuplicateToken(token, SecurityIdentification, &dup))
    {
        CheckTokenMembership(dup, admins, &member);
        CloseHandle(dup);
    }
    FreeSid(admins);
    return member ? 1 : 0;
}

int main(int argc, char **argv)
{
    char path[256], sid[128], reply[256];
    HANDLE pipe, token;
    DWORD n;

    if (argc < 3) return 2;
    snprintf(path, sizeof(path), "\\\\.\\pipe\\%s", argv[2]);
    if (!strcmp(argv[1], "server"))
    {
        SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, FALSE };
        char buf[64];
        ConvertStringSecurityDescriptorToSecurityDescriptorA("D:(A;;GA;;;WD)", SDDL_REVISION_1,
                                                             &sa.lpSecurityDescriptor, NULL);
        OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);
        token_sid(token, sid, sizeof(sid));
        CloseHandle(token);
        printf("SERVER %s\n", sid);
        fflush(stdout);
        pipe = CreateNamedPipeA(path, PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                                1, 512, 512, 0, &sa);
        if (pipe == INVALID_HANDLE_VALUE) { printf("ERROR create %lu\n", GetLastError()); return 1; }
        printf("LISTENING\n");
        fflush(stdout);
        if (!ConnectNamedPipe(pipe, NULL) && GetLastError() != ERROR_PIPE_CONNECTED)
        { printf("ERROR connect %lu\n", GetLastError()); return 1; }
        ReadFile(pipe, buf, sizeof(buf), &n, NULL);
        if (!ImpersonateNamedPipeClient(pipe)) { printf("ERROR impersonate %lu\n", GetLastError()); return 1; }
        if (!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY | TOKEN_DUPLICATE, TRUE, &token))
        { printf("ERROR openthreadtoken %lu\n", GetLastError()); return 1; }
        token_sid(token, sid, sizeof(sid));
        snprintf(reply, sizeof(reply), "CLIENT %s ADMIN %d", sid, is_admin(token));
        CloseHandle(token);
        RevertToSelf();
        printf("%s\n", reply);
        WriteFile(pipe, reply, (DWORD)strlen(reply) + 1, &n, NULL);
        FlushFileBuffers(pipe);
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
        return 0;
    }
    OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);
    token_sid(token, sid, sizeof(sid));
    CloseHandle(token);
    printf("SELF %s\n", sid);
    for (n = 0; n < 100; n++)
    {
        pipe = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (pipe != INVALID_HANDLE_VALUE) break;
        Sleep(100);
    }
    if (pipe == INVALID_HANDLE_VALUE) { printf("ERROR open %lu\n", GetLastError()); return 1; }
    WriteFile(pipe, "hi", 3, &n, NULL);
    if (ReadFile(pipe, reply, sizeof(reply) - 1, &n, NULL)) { reply[n] = 0; printf("REPLY %s\n", reply); }
    CloseHandle(pipe);
    return 0;
}
