/* usersacl-probe DIR: grant BUILTIN\Users full control of DIR (inherited by
 * what is made in it), as an installer does for its own folder (Steam's)
 * (patches/sg/0456). Prints the SetNamedSecurityInfo result.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <aclapi.h>
#include <stdio.h>

int wmain(int argc, WCHAR **argv)
{
    EXPLICIT_ACCESS_W ea[2] = {{0}};
    PSID users = NULL, system = NULL;
    SID_IDENTIFIER_AUTHORITY nt = { SECURITY_NT_AUTHORITY };
    PACL acl = NULL;
    DWORD rc;

    if (argc < 2) return 2;
    AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_USERS, 0, 0, 0, 0, 0, 0, &users);
    AllocateAndInitializeSid(&nt, 1, SECURITY_LOCAL_SYSTEM_RID, 0, 0, 0, 0, 0, 0, 0, &system);
    ea[0].grfAccessPermissions = GENERIC_ALL; ea[0].grfAccessMode = SET_ACCESS;
    ea[0].grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
    ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID; ea[0].Trustee.ptstrName = (WCHAR *)users;
    ea[1] = ea[0]; ea[1].Trustee.ptstrName = (WCHAR *)system;
    if ((rc = SetEntriesInAclW(2, ea, NULL, &acl))) { printf("acl %lu\n", rc); return 1; }
    rc = SetNamedSecurityInfoW(argv[1], SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, acl, NULL);
    printf("set %lu\n", rc);
    return 0;
}
