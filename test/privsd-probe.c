/* CreatePrivateObjectSecurity(Ex) builds the descriptor from the creator, the parent and the token
 * (patches/sg/8211); ConvertToAutoInheritPrivateObjectSecurity marks what the parent passes on as
 * inherited. Before, every new private object got Everyone as owner and group and an Everyone
 * full-access DACL and SACL whatever was asked, and the conversion failed (not implemented). */
#include <windows.h>
#include <sddl.h>
#include <stdio.h>

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL  "); } else printf("ok    "); \
                           printf(__VA_ARGS__); printf("\n"); } while (0)

/* SDDL writes the DS rights letters for the low bits: CC 1, RP 0x10, WP 0x20, LO 0x80, CR 0x100 */
static GENERIC_MAPPING mapping = { 0x1, 0x2, 0x4, 0xf };  /* read, write, execute, all */

static PSECURITY_DESCRIPTOR sd_from( const char *sddl )
{
    PSECURITY_DESCRIPTOR sd = NULL;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorA( sddl, SDDL_REVISION_1, &sd, NULL ))
        printf( "bad SDDL %s (%lu)\n", sddl, GetLastError() );
    return sd;
}

static char *sddl_of( PSECURITY_DESCRIPTOR sd, SECURITY_INFORMATION info )
{
    static char buf[4][1024];
    static int n;
    char *str = NULL;
    char *out = buf[n++ & 3];
    if (!ConvertSecurityDescriptorToStringSecurityDescriptorA( sd, SDDL_REVISION_1, info, &str, NULL ))
        sprintf( out, "<error %lu>", GetLastError() );
    else
    {
        lstrcpynA( out, str, 1024 );
        LocalFree( str );
    }
    return out;
}

/* a SID as SDDL writes it (an abbreviation for a well-known one) */
static void sid_sddl( PSID sid, char *out )
{
    SECURITY_DESCRIPTOR abs;
    char *str = NULL;
    InitializeSecurityDescriptor( &abs, SECURITY_DESCRIPTOR_REVISION );
    SetSecurityDescriptorOwner( &abs, sid, FALSE );
    if (ConvertSecurityDescriptorToStringSecurityDescriptorA( &abs, SDDL_REVISION_1, OWNER_SECURITY_INFORMATION, &str, NULL ))
    {
        strcpy( out, str + 2 );
        LocalFree( str );
    }
    else strcpy( out, "?" );
}

static void dacl_is( PSECURITY_DESCRIPTOR sd, const char *want, const char *what )
{
    char *got = sddl_of( sd, DACL_SECURITY_INFORMATION );
    CHECK( !strcmp( got, want ), "%s: DACL %s (want %s)", what, got, want );
}

int main(void)
{
    PSECURITY_DESCRIPTOR parent, creator, sd = NULL, conv = NULL;
    HANDLE token;
    TOKEN_OWNER *owner;
    TOKEN_PRIMARY_GROUP *group;
    BYTE obuf[256], gbuf[256];
    DWORD len;
    char owner_str[200], want[512];
    PSID sid;
    BOOL defaulted;
    BOOL ret;

    OpenProcessToken( GetCurrentProcess(), TOKEN_QUERY, &token );
    GetTokenInformation( token, TokenOwner, obuf, sizeof(obuf), &len );
    GetTokenInformation( token, TokenPrimaryGroup, gbuf, sizeof(gbuf), &len );
    owner = (TOKEN_OWNER *)obuf;
    group = (TOKEN_PRIMARY_GROUP *)gbuf;
    sid_sddl( owner->Owner, owner_str );

    /* 1: no parent, no creator: the token's owner, group and default DACL */
    ret = CreatePrivateObjectSecurity( NULL, NULL, &sd, FALSE, token, &mapping );
    CHECK( ret, "no parent, no creator: %lu", ret ? 0 : GetLastError() );
    if (ret)
    {
        GetSecurityDescriptorOwner( sd, &sid, &defaulted );
        CHECK( sid && EqualSid( sid, owner->Owner ), "owner is the token owner (%s)", sddl_of( sd, OWNER_SECURITY_INFORMATION ) );
        GetSecurityDescriptorGroup( sd, &sid, &defaulted );
        CHECK( sid && EqualSid( sid, group->PrimaryGroup ), "group is the token primary group (%s)", sddl_of( sd, GROUP_SECURITY_INFORMATION ) );
        CHECK( !strstr( sddl_of( sd, DACL_SECURITY_INFORMATION ), "WD)" ), "DACL is not Everyone's (%s)",
               sddl_of( sd, DACL_SECURITY_INFORMATION ) );
        DestroyPrivateObjectSecurity( &sd );
    }

    /* 2: an explicit creator DACL is used with generic rights mapped */
    creator = sd_from( "D:(A;;GR;;;BU)(A;;GA;;;SY)" );
    ret = CreatePrivateObjectSecurity( NULL, creator, &sd, FALSE, token, &mapping );
    CHECK( ret, "creator DACL: %lu", ret ? 0 : GetLastError() );
    if (ret) { dacl_is( sd, "D:(A;;CC;;;BU)(A;;CCDCLCSW;;;SY)", "creator DACL" ); DestroyPrivateObjectSecurity( &sd ); }
    LocalFree( creator );

    /* 3: inheritance from a container parent to an object and to a container */
    parent = sd_from( "O:BAG:SYD:(A;OICI;GR;;;BU)(A;CI;0x10;;;AU)(A;OI;0x20;;;IU)(A;OICIIO;GA;;;CO)(A;;0x40;;;WD)(A;OICINP;0x80;;;SY)" );
    ret = CreatePrivateObjectSecurity( parent, NULL, &sd, FALSE, token, &mapping );
    CHECK( ret, "object under a parent: %lu", ret ? 0 : GetLastError() );
    if (ret)
    {
        sprintf( want, "D:(A;;CC;;;BU)(A;;WP;;;IU)(A;;CCDCLCSW;;;%s)(A;;LO;;;SY)", owner_str );
        dacl_is( sd, want, "object under a parent" );
        DestroyPrivateObjectSecurity( &sd );
    }
    ret = CreatePrivateObjectSecurity( parent, NULL, &sd, TRUE, token, &mapping );
    CHECK( ret, "container under a parent: %lu", ret ? 0 : GetLastError() );
    if (ret)
    {
        sprintf( want, "D:(A;;CC;;;BU)(A;OICIIO;GR;;;BU)(A;CI;RP;;;AU)(A;OIIO;WP;;;IU)(A;;CCDCLCSW;;;%s)(A;OICIIO;GA;;;CO)(A;;LO;;;SY)",
                 owner_str );
        dacl_is( sd, want, "container under a parent" );
        DestroyPrivateObjectSecurity( &sd );
    }

    /* 4: auto-inheritance: explicit creator ACEs first, then the parent's marked inherited */
    creator = sd_from( "D:(A;;0x100;;;BG)" );
    ret = CreatePrivateObjectSecurityEx( parent, creator, &sd, NULL, FALSE, SEF_DACL_AUTO_INHERIT, token, &mapping );
    CHECK( ret, "auto-inherit: %lu", ret ? 0 : GetLastError() );
    if (ret)
    {
        sprintf( want, "D:AI(A;;CR;;;BG)(A;ID;CC;;;BU)(A;ID;WP;;;IU)(A;ID;CCDCLCSW;;;%s)(A;ID;LO;;;SY)", owner_str );
        dacl_is( sd, want, "auto-inherit" );
        DestroyPrivateObjectSecurity( &sd );
    }
    /* without auto-inheritance the explicit creator DACL wins alone */
    ret = CreatePrivateObjectSecurityEx( parent, creator, &sd, NULL, FALSE, 0, token, &mapping );
    if (ret) { dacl_is( sd, "D:(A;;CR;;;BG)", "explicit creator over a parent" ); DestroyPrivateObjectSecurity( &sd ); }
    else CHECK( 0, "explicit creator over a parent: %lu", GetLastError() );
    LocalFree( creator );

    /* 5: owner and group from the parent on request; an owner the token cannot assign fails */
    ret = CreatePrivateObjectSecurityEx( parent, NULL, &sd, NULL, FALSE,
                                         SEF_DEFAULT_OWNER_FROM_PARENT | SEF_DEFAULT_GROUP_FROM_PARENT, token, &mapping );
    if (ret)
    {
        char *o = sddl_of( sd, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION );
        CHECK( !strcmp( o, "O:BAG:SY" ), "owner and group from the parent: %s", o );
        DestroyPrivateObjectSecurity( &sd );
    }
    else CHECK( 0, "owner from parent: %lu", GetLastError() );
    creator = sd_from( "O:S-1-5-21-1-2-3-4242D:(A;;0x1;;;WD)" );
    SetLastError( 0xdeadbeef );
    ret = CreatePrivateObjectSecurity( NULL, creator, &sd, FALSE, token, &mapping );
    CHECK( !ret && GetLastError() == ERROR_INVALID_OWNER, "foreign owner refused: %d %lu", ret, GetLastError() );
    if (ret) DestroyPrivateObjectSecurity( &sd );
    ret = CreatePrivateObjectSecurityEx( NULL, creator, &sd, NULL, FALSE, SEF_AVOID_OWNER_CHECK, token, &mapping );
    CHECK( ret, "foreign owner with SEF_AVOID_OWNER_CHECK: %lu", ret ? 0 : GetLastError() );
    if (ret)
    {
        char *o = sddl_of( sd, OWNER_SECURITY_INFORMATION );
        CHECK( !strcmp( o, "O:S-1-5-21-1-2-3-4242" ), "creator owner kept: %s", o );
        DestroyPrivateObjectSecurity( &sd );
    }
    LocalFree( creator );

    /* 6: a creator SACL needs SeSecurityPrivilege (not enabled here) */
    creator = sd_from( "S:(AU;SA;0x1;;;WD)" );
    SetLastError( 0xdeadbeef );
    ret = CreatePrivateObjectSecurity( NULL, creator, &sd, FALSE, token, &mapping );
    CHECK( !ret && GetLastError() == ERROR_PRIVILEGE_NOT_HELD, "creator SACL without the privilege: %d %lu", ret, GetLastError() );
    if (ret) DestroyPrivateObjectSecurity( &sd );
    ret = CreatePrivateObjectSecurityEx( NULL, creator, &sd, NULL, FALSE, SEF_AVOID_PRIVILEGE_CHECK, token, &mapping );
    CHECK( ret, "creator SACL with SEF_AVOID_PRIVILEGE_CHECK: %lu", ret ? 0 : GetLastError() );
    if (ret)
    {
        char *s = sddl_of( sd, SACL_SECURITY_INFORMATION );
        CHECK( !strcmp( s, "S:(AU;SA;CC;;;WD)" ), "SACL kept: %s", s );
        DestroyPrivateObjectSecurity( &sd );
    }
    LocalFree( creator );

    /* 7: converting an old descriptor: what the parent passes on becomes inherited */
    sprintf( want, "O:BAG:SYD:(A;;0x200;;;BG)(A;;0x1;;;BU)(A;;0x20;;;IU)(A;;0xf;;;BA)(A;;0x80;;;SY)" );
    creator = sd_from( want );
    ret = ConvertToAutoInheritPrivateObjectSecurity( parent, creator, &conv, NULL, FALSE, &mapping );
    CHECK( ret, "ConvertToAutoInheritPrivateObjectSecurity: %lu", ret ? 0 : GetLastError() );
    if (ret)
    {
        dacl_is( conv, "D:AI(A;;0x200;;;BG)(A;ID;CC;;;BU)(A;ID;WP;;;IU)(A;ID;CCDCLCSW;;;BA)(A;ID;LO;;;SY)", "converted" );
        DestroyPrivateObjectSecurity( &conv );
    }
    LocalFree( creator );
    /* one inheritable ACE missing: set on purpose, kept as it is and protected */
    creator = sd_from( "O:BAG:SYD:(A;;0x200;;;BG)(A;;0x1;;;BU)" );
    ret = ConvertToAutoInheritPrivateObjectSecurity( parent, creator, &conv, NULL, FALSE, &mapping );
    if (ret) { dacl_is( conv, "D:PAI(A;;0x200;;;BG)(A;;CC;;;BU)", "converted, protected" ); DestroyPrivateObjectSecurity( &conv ); }
    else CHECK( 0, "convert protected: %lu", GetLastError() );
    LocalFree( creator );
    LocalFree( parent );

    CloseHandle( token );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
