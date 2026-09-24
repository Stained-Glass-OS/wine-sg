/* What Chromium's sandbox asks of tokens (patches/sg/0055-0056), answered
 * with AccessCheck against descriptors built here: restricting SIDs, write-
 * restricted tokens, integrity levels and AppContainer (lowbox) tokens.
 * Prints NAME=VALUE lines; test/edge-e2e.sh compares them with Windows'
 * documented answers. */
#include <windows.h>
#include <sddl.h>
#include <stdio.h>

static GENERIC_MAPPING mapping = { FILE_GENERIC_READ, FILE_GENERIC_WRITE, FILE_GENERIC_EXECUTE, FILE_ALL_ACCESS };

/* the access token (impersonation copy) gets to an object whose DACL is sddl */
static DWORD granted( HANDLE token, const char *sddl, DWORD want )
{
    PSECURITY_DESCRIPTOR sd;
    PRIVILEGE_SET privs;
    DWORD privs_len = sizeof(privs), access = 0;
    BOOL status = FALSE;
    HANDLE imp;

    if (!DuplicateToken( token, SecurityImpersonation, &imp )) return 0xdead;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorA( sddl, SDDL_REVISION_1, &sd, NULL )) return 0xbad;
    MapGenericMask( &want, &mapping );
    if (!AccessCheck( sd, imp, want, &mapping, &privs, &privs_len, &access, &status )) { printf( "AccessCheck failed %lu\n", GetLastError() ); access = 0; }
    else if (!status) access = 0;
    LocalFree( sd );
    CloseHandle( imp );
    return access;
}

int main( void )
{
    BOOL (WINAPI *create_ac)(HANDLE, SECURITY_CAPABILITIES *, HANDLE *) =
        (void *)GetProcAddress( GetModuleHandleA( "kernelbase.dll" ), "CreateAppContainerToken" );
    char user_sddl[256], sddl[512], *user_str, *str;
    BYTE buf[512];
    DWORD len;
    HANDLE token, restricted, write_restricted, lowbox;
    SID_AND_ATTRIBUTES world = { NULL, 0 };
    PSID package, cap;
    SECURITY_CAPABILITIES caps;
    SID_AND_ATTRIBUTES cap_attr;
    TOKEN_MANDATORY_LABEL *label = (void *)buf;

    OpenProcessToken( GetCurrentProcess(), TOKEN_ALL_ACCESS, &token );
    GetTokenInformation( token, TokenUser, buf, sizeof(buf), &len );
    ConvertSidToStringSidA( ((TOKEN_USER *)buf)->User.Sid, &user_str );
    snprintf( user_sddl, sizeof(user_sddl), "O:SYG:SYD:(A;;FA;;;%s)", user_str );

    printf( "Plain=%d\n", granted( token, user_sddl, GENERIC_READ ) != 0 );

    /* a token restricted to Everyone: the user's ACE alone no longer grants */
    ConvertStringSidToSidA( "S-1-1-0", &world.Sid );
    CreateRestrictedToken( token, 0, 0, NULL, 0, NULL, 1, &world, &restricted );
    printf( "Restricted=%d\n", granted( restricted, user_sddl, GENERIC_READ ) != 0 );
    snprintf( sddl, sizeof(sddl), "%s(A;;FA;;;WD)", user_sddl );
    printf( "RestrictedWithWorld=%d\n", granted( restricted, sddl, GENERIC_READ ) != 0 );
    GetTokenInformation( restricted, TokenHasRestrictions, &len, sizeof(len), &len );
    printf( "HasRestrictions=%lu\n", len );

    /* write-restricted: reads as before, writes need the restricting SIDs too */
    CreateRestrictedToken( token, WRITE_RESTRICTED, 0, NULL, 0, NULL, 1, &world, &write_restricted );
    printf( "WriteRestrictedRead=%d\n", granted( write_restricted, user_sddl, GENERIC_READ ) != 0 );
    printf( "WriteRestrictedWrite=%d\n", granted( write_restricted, user_sddl, GENERIC_WRITE ) != 0 );

    /* integrity level: a token may lower its own */
    GetTokenInformation( restricted, TokenIntegrityLevel, buf, sizeof(buf), &len );
    {
        SID_AND_ATTRIBUTES low = { NULL, SE_GROUP_INTEGRITY };
        TOKEN_MANDATORY_LABEL set;
        ConvertStringSidToSidA( "S-1-16-4096", &low.Sid );
        set.Label = low;
        printf( "LowerIntegrity=%d\n", SetTokenInformation( restricted, TokenIntegrityLevel, &set,
                                                              sizeof(set) + GetLengthSid( low.Sid ) ) );
        GetTokenInformation( restricted, TokenIntegrityLevel, buf, sizeof(buf), &len );
        ConvertSidToStringSidA( label->Label.Sid, &str );
        printf( "Integrity=%s\n", str );
        LocalFree( str );
    }

    /* AppContainer */
    if (!create_ac) { printf( "CreateAppContainerToken=missing\n" ); return 0; }
    ConvertStringSidToSidA( "S-1-15-2-1111-2222-3333-4444-5555-6666-7777", &package );
    ConvertStringSidToSidA( "S-1-15-3-1", &cap ); /* internetClient */
    cap_attr.Sid = cap;
    cap_attr.Attributes = SE_GROUP_ENABLED;
    caps.AppContainerSid = package;
    caps.Capabilities = &cap_attr;
    caps.CapabilityCount = 1;
    caps.Reserved = 0;
    printf( "CreateAppContainerToken=%d\n", create_ac( token, &caps, &lowbox ) );

    GetTokenInformation( lowbox, TokenIsAppContainer, &len, sizeof(len), &len );
    printf( "IsAppContainer=%lu\n", len );
    GetTokenInformation( token, TokenIsAppContainer, &len, sizeof(len), &len );
    printf( "ParentIsAppContainer=%lu\n", len );
    if (GetTokenInformation( lowbox, TokenAppContainerSid, buf, sizeof(buf), &len ) &&
        ConvertSidToStringSidA( ((TOKEN_APPCONTAINER_INFORMATION *)buf)->TokenAppContainer, &str ))
    {
        printf( "Package=%s\n", str );
        LocalFree( str );
    }
    if (GetTokenInformation( lowbox, TokenCapabilities, buf, sizeof(buf), &len ) &&
        ConvertSidToStringSidA( ((TOKEN_GROUPS *)buf)->Groups[0].Sid, &str ))
    {
        printf( "Capabilities=%lu,%s\n", ((TOKEN_GROUPS *)buf)->GroupCount, str );
        LocalFree( str );
    }
    GetTokenInformation( lowbox, TokenIntegrityLevel, buf, sizeof(buf), &len );
    ConvertSidToStringSidA( label->Label.Sid, &str );
    printf( "AppContainerIntegrity=%s\n", str );
    LocalFree( str );

    /* the package must be named too: by its SID, a capability, or ALL
     * APPLICATION PACKAGES */
    printf( "AppContainerUserOnly=%d\n", granted( lowbox, user_sddl, GENERIC_READ ) != 0 );
    snprintf( sddl, sizeof(sddl), "%s(A;;FR;;;AC)", user_sddl );
    printf( "AppContainerAllPackages=%d\n", granted( lowbox, sddl, GENERIC_READ ) != 0 );
    snprintf( sddl, sizeof(sddl), "%s(A;;FR;;;S-1-15-2-1111-2222-3333-4444-5555-6666-7777)", user_sddl );
    printf( "AppContainerPackage=%d\n", granted( lowbox, sddl, GENERIC_READ ) != 0 );
    printf( "AppContainerPackageNoWrite=%d\n", granted( lowbox, sddl, GENERIC_WRITE ) != 0 );
    snprintf( sddl, sizeof(sddl), "%s(A;;FR;;;S-1-15-3-1)", user_sddl );
    printf( "AppContainerCapability=%d\n", granted( lowbox, sddl, GENERIC_READ ) != 0 );
    snprintf( sddl, sizeof(sddl), "O:SYG:SYD:(A;;FR;;;AC)" );
    printf( "AppContainerPackageOnly=%d\n", granted( lowbox, sddl, GENERIC_READ ) != 0 );
    return 0;
}
