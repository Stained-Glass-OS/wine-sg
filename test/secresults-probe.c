/* secresults-probe: AccessCheckByTypeResultList, the access checks with audits, GetInheritanceSource,
 * the impersonate trustees and the A forms (Stained Glass OS wine-sg 6057). Prints KEY=VALUE lines. */
#define COBJMACROS
#include <windows.h>
#include <aclapi.h>
#include <stdio.h>

#define P(...) printf(__VA_ARGS__)

/* the MinGW headers declare these with a newer target only */
BOOL WINAPI AccessCheckByTypeResultList(PSECURITY_DESCRIPTOR, PSID, HANDLE, DWORD, POBJECT_TYPE_LIST, DWORD,
                                        PGENERIC_MAPPING, PPRIVILEGE_SET, LPDWORD, LPDWORD, LPDWORD);
typedef BOOL (WINAPI *by_type_audit_fn)(LPCWSTR, LPVOID, LPCWSTR, LPCWSTR, PSECURITY_DESCRIPTOR, PSID, DWORD, int,
                                        DWORD, POBJECT_TYPE_LIST, DWORD, PGENERIC_MAPPING, BOOL, LPDWORD, LPBOOL, LPBOOL);
typedef DWORD (WINAPI *tree_set_a_fn)(LPSTR, SE_OBJECT_TYPE, SECURITY_INFORMATION, PSID, PSID, PACL, PACL, DWORD,
                                      void *, int, void *);

static GUID guids[5] = {
    {0x11111111,0,0,{0,0,0,0,0,0,0,1}}, {0x22222222,0,0,{0,0,0,0,0,0,0,2}}, {0x33333333,0,0,{0,0,0,0,0,0,0,3}},
    {0x44444444,0,0,{0,0,0,0,0,0,0,4}}, {0x55555555,0,0,{0,0,0,0,0,0,0,5}} };

static SID_IDENTIFIER_AUTHORITY world_auth = { SECURITY_WORLD_SID_AUTHORITY };

int main( void )
{
    GENERIC_MAPPING mapping = { FILE_GENERIC_READ, FILE_GENERIC_WRITE, FILE_GENERIC_EXECUTE, FILE_ALL_ACCESS };
    BYTE acl_buf[1024], sd_buf[SECURITY_DESCRIPTOR_MIN_LENGTH], priv_buf[256];
    PSECURITY_DESCRIPTOR sd = sd_buf;
    PACL acl = (PACL)acl_buf;
    PSID world;
    HANDLE token, imp;
    OBJECT_TYPE_LIST types[5];
    DWORD granted[5], status[5], priv_len = sizeof(priv_buf), granted1 = 0, i;
    BOOL ok, st = TRUE, on_close = TRUE, ret;
    TOKEN_OWNER *owner;
    BYTE owner_buf[256];
    DWORD len;
    HMODULE advapi = GetModuleHandleA( "advapi32.dll" );
    by_type_audit_fn by_type_audit = (void *)GetProcAddress( advapi, "AccessCheckByTypeAndAuditAlarmW" );
    tree_set_a_fn tree_set_a = (void *)GetProcAddress( advapi, "TreeSetNamedSecurityInfoA" );

    setvbuf( stdout, NULL, _IONBF, 0 );
    AllocateAndInitializeSid( &world_auth, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &world );
    OpenProcessToken( GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_IMPERSONATE, &token );
    DuplicateToken( token, SecurityImpersonation, &imp );
    GetTokenInformation( token, TokenOwner, owner_buf, sizeof(owner_buf), &len );
    owner = (TOKEN_OWNER *)owner_buf;

    /* the tree: A > B > (C, E), A > D. Denied on C, allowed on B: B and E get access, A, C and D do not */
    InitializeAcl( acl, sizeof(acl_buf), ACL_REVISION_DS );
    AddAccessDeniedObjectAce( acl, ACL_REVISION_DS, 0, FILE_READ_DATA, &guids[2], NULL, world );
    AddAccessAllowedObjectAce( acl, ACL_REVISION_DS, 0, FILE_READ_DATA, &guids[1], NULL, world );
    AddAccessAllowedAce( acl, ACL_REVISION_DS, READ_CONTROL, world );
    InitializeSecurityDescriptor( sd, SECURITY_DESCRIPTOR_REVISION );
    SetSecurityDescriptorOwner( sd, owner->Owner, FALSE );
    SetSecurityDescriptorGroup( sd, owner->Owner, FALSE );
    SetSecurityDescriptorDacl( sd, TRUE, acl, FALSE );
    types[0].Level = 0; types[0].ObjectType = &guids[0];
    types[1].Level = 1; types[1].ObjectType = &guids[1];
    types[2].Level = 2; types[2].ObjectType = &guids[2];
    types[3].Level = 2; types[3].ObjectType = &guids[4];
    types[4].Level = 1; types[4].ObjectType = &guids[3];
    for (i = 0; i < 5; i++) granted[i] = status[i] = 0xdead;
    ok = AccessCheckByTypeResultList( sd, NULL, imp, FILE_READ_DATA, types, 5, &mapping, (PPRIVILEGE_SET)priv_buf,
                                      &priv_len, granted, status );
    P( "ResultList=%d", ok );
    for (i = 0; i < 5; i++) P( " %lx/%lu", granted[i], status[i] );
    P( "\n" );
    priv_len = sizeof(priv_buf);
    ok = AccessCheckByTypeResultList( sd, NULL, imp, READ_CONTROL, types, 5, &mapping, (PPRIVILEGE_SET)priv_buf,
                                      &priv_len, granted, status );
    P( "ResultListPlain=%d %lx/%lu %lx/%lu\n", ok, granted[0], status[0], granted[2], status[2] );
    SetLastError( 0xdead );
    ok = AccessCheckByTypeResultList( sd, NULL, imp, FILE_READ_DATA, NULL, 0, &mapping, (PPRIVILEGE_SET)priv_buf,
                                      &priv_len, granted, status );
    P( "ResultListNoTypes=%d %lu\n", ok, GetLastError() );

    /* audits: the caller has no SeAuditPrivilege */
    SetLastError( 0xdead );
    ok = AccessCheckAndAuditAlarmW( L"SG", NULL, (WCHAR *)L"File", (WCHAR *)L"x", sd, READ_CONTROL, &mapping, FALSE,
                                    &granted1, &st, &on_close );
    P( "AuditAlarm=%d %lu\n", ok, GetLastError() );
    if (by_type_audit)
    {
        SetLastError( 0xdead );
        ok = by_type_audit( L"SG", NULL, L"File", L"x", sd, NULL, FILE_READ_DATA, 0, AUDIT_ALLOW_NO_PRIVILEGE, types, 5,
                            &mapping, FALSE, &granted1, &st, &on_close );
        P( "ByTypeAuditNoImpersonation=%d %lu\n", ok, GetLastError() );
        SetThreadToken( NULL, imp );
        granted1 = 0xdead; st = 7; on_close = 7;
        ok = by_type_audit( L"SG", NULL, L"File", L"x", sd, NULL, READ_CONTROL, 0, AUDIT_ALLOW_NO_PRIVILEGE, types, 1,
                            &mapping, FALSE, &granted1, &st, &on_close );
        P( "ByTypeAudit=%d %lx %d %d\n", ok, granted1, st, on_close );
        SetLastError( 0xdead );
        ok = by_type_audit( L"SG", NULL, L"File", L"x", sd, NULL, READ_CONTROL, 0, 0, types, 1,
                            &mapping, FALSE, &granted1, &st, &on_close );
        P( "ByTypeAuditNoPrivilege=%d %lu\n", ok, GetLastError() );
        RevertToSelf();
    }

    /* trustees */
    {
        TRUSTEEW t1 = { 0 }, t2 = { 0 };
        EXPLICIT_ACCESSA ea = { 0 };
        TRUSTEEA ta = { 0 };
        BuildTrusteeWithNameW( &t2, (WCHAR *)L"Everyone" );
        BuildImpersonateTrusteeW( &t1, &t2 );
        BuildImpersonateExplicitAccessWithNameA( &ea, (char *)"Everyone", &ta, GENERIC_READ, GRANT_ACCESS, 0 );
        P( "Trustee=%d %d %d %s %d\n", GetMultipleTrusteeW( &t1 ) == &t2, GetMultipleTrusteeOperationW( &t1 ),
           ea.Trustee.pMultipleTrustee == &ta, ea.Trustee.ptstrName, GetMultipleTrusteeOperationA( &ea.Trustee ) );
    }

    /* inheritance sources: SGInh gives an inheritable ACE down to SGInh\a\b\leaf; SGInh\a has the same
     * rights for the same trustee explicitly, but not inheritable */
    {
        static const WCHAR rootW[] = L"CURRENT_USER\\Software\\SGInh", aW[] = L"CURRENT_USER\\Software\\SGInh\\a",
                           leafW[] = L"CURRENT_USER\\Software\\SGInh\\a\\b\\leaf";
        GENERIC_MAPPING key_mapping = { KEY_READ, KEY_WRITE, KEY_EXECUTE, KEY_ALL_ACCESS };
        BYTE a_buf[256], i_buf[256];
        PACL a_acl = (PACL)a_buf, i_acl = (PACL)i_buf, leaf_acl = NULL;
        PSECURITY_DESCRIPTOR leaf_sd = NULL;
        INHERITED_FROMW from[16];
        INHERITED_FROMA from_a[16];
        ACL_SIZE_INFORMATION info;
        DWORD r, k;
        LONG gap = -99, gap_a = -99;
        WCHAR name[MAX_PATH] = L"(none)";
        char name_a[MAX_PATH] = "(none)";
        HKEY key;

        RegCreateKeyExW( HKEY_CURRENT_USER, L"Software\\SGInh\\a\\b\\leaf", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL );
        RegCloseKey( key );
        InitializeAcl( a_acl, sizeof(a_buf), ACL_REVISION );
        AddAccessAllowedAceEx( a_acl, ACL_REVISION, 0, KEY_READ, world );
        AddAccessAllowedAceEx( a_acl, ACL_REVISION, 0, KEY_ALL_ACCESS, owner->Owner );
        r = SetNamedSecurityInfoW( (WCHAR *)aW, SE_REGISTRY_KEY, DACL_SECURITY_INFORMATION, NULL, NULL, a_acl, NULL );
        P( "SetExplicit=%lu\n", r );
        InitializeAcl( i_acl, sizeof(i_buf), ACL_REVISION );
        AddAccessAllowedAceEx( i_acl, ACL_REVISION, CONTAINER_INHERIT_ACE, KEY_READ, world );
        AddAccessAllowedAceEx( i_acl, ACL_REVISION, CONTAINER_INHERIT_ACE, KEY_ALL_ACCESS, owner->Owner );
        r = tree_set_a ? tree_set_a( (char *)"CURRENT_USER\\Software\\SGInh", SE_REGISTRY_KEY, DACL_SECURITY_INFORMATION,
                                     NULL, NULL, i_acl, NULL, 1 /* TREE_SEC_INFO_SET */, NULL, 1, NULL ) : 0xdead;
        P( "TreeSetA=%lu\n", r );
        r = GetNamedSecurityInfoW( leafW, SE_REGISTRY_KEY, DACL_SECURITY_INFORMATION, NULL, NULL, &leaf_acl, NULL, &leaf_sd );
        GetAclInformation( leaf_acl, &info, sizeof(info), AclSizeInformation );
        memset( from, 0, sizeof(from) );
        r = GetInheritanceSourceW( (WCHAR *)leafW, SE_REGISTRY_KEY, DACL_SECURITY_INFORMATION, TRUE,
                                   NULL, 0, leaf_acl, NULL, &key_mapping, from );
        for (k = 0; k < info.AceCount; k++)
        {
            ACCESS_ALLOWED_ACE *ace;
            GetAce( leaf_acl, k, (void **)&ace );
            if (!EqualSid( (PSID)&ace->SidStart, world )) continue;
            gap = from[k].GenerationGap;
            if (from[k].AncestorName) lstrcpyW( name, from[k].AncestorName );
        }
        P( "InheritanceSource=%lu %ld %ls\n", r, gap, name );
        FreeInheritedFromArray( from, info.AceCount, NULL );
        r = GetInheritanceSourceA( (char *)"CURRENT_USER\\Software\\SGInh\\a\\b\\leaf", SE_REGISTRY_KEY,
                                   DACL_SECURITY_INFORMATION, TRUE, NULL, 0, leaf_acl, NULL, &key_mapping, from_a );
        for (k = 0; k < info.AceCount; k++)
        {
            ACCESS_ALLOWED_ACE *ace;
            GetAce( leaf_acl, k, (void **)&ace );
            if (!EqualSid( (PSID)&ace->SidStart, world )) continue;
            gap_a = from_a[k].GenerationGap;
            if (from_a[k].AncestorName) lstrcpyA( name_a, from_a[k].AncestorName );
        }
        P( "InheritanceSourceA=%lu %ld %s\n", r, gap_a, name_a );
        FreeInheritedFromArray( (PINHERITED_FROMW)from_a, info.AceCount, NULL );
        LocalFree( leaf_sd );
        (void)rootW;
    }
    P( "done=1\n" );
    return 0;
}
