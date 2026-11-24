/* manyusers-probe: per-user security tables of the server with many users (patches/sg/9151).
 *   sess                 this process's session -> SESSION <id>
 *   fill BASE N          create HKEY_USERS\S-1-5-21-0-0-0-<BASE+i>\Software (i < N) and open
 *                        each to read and to write, as a logon of N users would -> FILL <made> <opened>
 *   mk KEY               create KEY (HKU\..., HKCU\...) -> MK <win32 error>
 *   open KEY ACCESS      open KEY with ACCESS (hex) -> OPEN <win32 error>
 *   setval KEY           write a value into KEY -> SET <win32 error>
 *   del KEY              delete KEY -> DEL <win32 error>
 *   rread                a restricted token (Everyone, Users, RESTRICTED) of this user reads its
 *                        HKEY_USERS\<SID>\Software -> RREAD <win32 error>
 */
#include <windows.h>
#include <sddl.h>
#include <stdio.h>

static HKEY root_of(const char **path)
{
    if (!_strnicmp(*path, "HKCU\\", 5)) { *path += 5; return HKEY_CURRENT_USER; }
    if (!_strnicmp(*path, "HKU\\", 4)) { *path += 4; return HKEY_USERS; }
    if (!_stricmp(*path, "HKU")) { *path += 3; return HKEY_USERS; }
    return NULL;
}

int main(int argc, char **argv)
{
    const char *path = argc > 2 ? argv[2] : "";
    HKEY root = argc > 2 ? root_of(&path) : NULL, key;
    LONG rc;

    if (argc > 1 && !strcmp(argv[1], "sess"))
    {
        DWORD id = ~0u;
        ProcessIdToSessionId(GetCurrentProcessId(), &id);
        printf("SESSION %lu\n", id);
        return 0;
    }
    if (argc > 3 && !strcmp(argv[1], "fill"))
    {
        unsigned long base = strtoul(argv[2], NULL, 0), n = strtoul(argv[3], NULL, 0), i, made = 0, opened = 0;
        char sub[96];
        for (i = 0; i < n; i++)
        {
            snprintf(sub, sizeof(sub), "S-1-5-21-0-0-0-%lu\\Software", base + i);
            if (!RegCreateKeyExA(HKEY_USERS, sub, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL)) { made++; RegCloseKey(key); }
            if (!RegOpenKeyExA(HKEY_USERS, sub, 0, KEY_READ | KEY_SET_VALUE, &key)) { opened++; RegCloseKey(key); }
        }
        printf("FILL %lu %lu\n", made, opened);
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "rread"))
    {
        SID_IDENTIFIER_AUTHORITY world = { SECURITY_WORLD_SID_AUTHORITY }, nt = { SECURITY_NT_AUTHORITY };
        SID_AND_ATTRIBUTES rs[3] = { { 0 } };
        HANDLE tok, rtok;
        BYTE buf[256];
        DWORD len;
        char *sid = NULL, sub[256];

        OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &tok);
        GetTokenInformation(tok, TokenUser, buf, sizeof(buf), &len);
        ConvertSidToStringSidA(((TOKEN_USER *)buf)->User.Sid, &sid);
        printf("USER %s\n", sid);
        AllocateAndInitializeSid(&world, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &rs[0].Sid);
        AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_USERS, 0, 0, 0, 0, 0, 0, &rs[1].Sid);
        AllocateAndInitializeSid(&nt, 1, SECURITY_RESTRICTED_CODE_RID, 0, 0, 0, 0, 0, 0, 0, &rs[2].Sid);
        if (!CreateRestrictedToken(tok, 0, 0, NULL, 0, NULL, 3, rs, &rtok)) { printf("RREAD ? %lu\n", GetLastError()); return 0; }
        snprintf(sub, sizeof(sub), "%s\\Software", sid);
        if (!ImpersonateLoggedOnUser(rtok)) { printf("RREAD ? imp %lu\n", GetLastError()); return 0; }
        rc = RegOpenKeyExA(HKEY_USERS, sub, 0, KEY_READ, &key);
        if (!rc) RegCloseKey(key);
        printf("RREAD %ld\n", rc);
        RevertToSelf();
        return 0;
    }
    if (!root) { printf("usage: manyusers-probe sess | fill BASE N | rread | mk|setval|del KEY | open KEY ACCESS\n"); return 2; }
    if (!strcmp(argv[1], "mk"))
    {
        rc = RegCreateKeyExA(root, path, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL);
        printf("MK %ld\n", rc);
        if (!rc) RegCloseKey(key);
    }
    else if (!strcmp(argv[1], "del"))
        printf("DEL %ld\n", RegDeleteKeyExA(root, path, 0, 0));
    else if (!strcmp(argv[1], "open") && argc > 3)
    {
        rc = RegOpenKeyExA(root, path, 0, strtoul(argv[3], NULL, 16), &key);
        printf("OPEN %ld\n", rc);
        if (!rc) RegCloseKey(key);
    }
    else if (!strcmp(argv[1], "setval"))
    {
        DWORD one = 1;
        if (!(rc = RegOpenKeyExA(root, path, 0, KEY_SET_VALUE, &key)))
        {
            rc = RegSetValueExA(key, "SgManyUsers", 0, REG_DWORD, (BYTE *)&one, sizeof(one));
            RegCloseKey(key);
        }
        printf("SET %ld\n", rc);
    }
    return 0;
}
