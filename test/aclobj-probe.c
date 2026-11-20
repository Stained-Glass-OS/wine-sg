/* SetEntriesInAclW with an object trustee (TRUSTEE_IS_OBJECTS_AND_SID) adds
 * an object ACE in an ACL of revision 4, and GetExplicitEntriesFromAclW gives
 * object and audit ACEs back as entries (patches/sg/8210). Before, the object
 * trustee forms logged a FIXME and added nothing usable (an ACE for an
 * uninitialised SID), and the entries came back as NOT_USED_ACCESS. */
#include <windows.h>
#include <aclapi.h>
#include <stdio.h>

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL  "); printf(__VA_ARGS__); printf("\n"); } \
                           else { printf("ok    "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const GUID guid_a = { 0xbf967aba, 0x0de6, 0x11d0, { 0xa2, 0x85, 0x00, 0xaa, 0x00, 0x30, 0x49, 0xe2 } };
static const GUID guid_b = { 0x4828cc14, 0x1437, 0x45bc, { 0x9b, 0x07, 0xad, 0x6f, 0x01, 0x5e, 0x5f, 0x28 } };

/* the documented object ACE layout: header, mask, flags, the GUIDs present, the SID */
static const BYTE *object_ace_sid( const ACE_HEADER *ace, DWORD *flags, const GUID **type, const GUID **inh )
{
    const DWORD *p = (const DWORD *)(ace + 1);
    const BYTE *pos = (const BYTE *)(p + 2);
    *flags = p[1];
    *type = *inh = NULL;
    if (*flags & ACE_OBJECT_TYPE_PRESENT) { *type = (const GUID *)pos; pos += sizeof(GUID); }
    if (*flags & ACE_INHERITED_OBJECT_TYPE_PRESENT) { *inh = (const GUID *)pos; pos += sizeof(GUID); }
    return pos;
}

int main(void)
{
    SID_IDENTIFIER_AUTHORITY nt = { SECURITY_NT_AUTHORITY }, world = { SECURITY_WORLD_SID_AUTHORITY };
    PSID users, everyone;
    EXPLICIT_ACCESSW ea[3], *list = NULL;
    OBJECTS_AND_SID os_allow, os_deny;
    ACL *acl = NULL, *acl2 = NULL;
    ACE_HEADER *ace;
    ULONG count = 0, i;
    DWORD err, flags;
    const GUID *type, *inh;
    const BYTE *sid;
    int seen_obj = 0, seen_audit = 0;

    AllocateAndInitializeSid( &nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_USERS, 0, 0, 0, 0, 0, 0, &users );
    AllocateAndInitializeSid( &world, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &everyone );

    memset( ea, 0, sizeof(ea) );
    BuildTrusteeWithObjectsAndSidW( &ea[0].Trustee, &os_allow, (GUID *)&guid_a, (GUID *)&guid_b, users );
    ea[0].grfAccessMode = GRANT_ACCESS;
    ea[0].grfAccessPermissions = 0x30;  /* ADS_RIGHT_DS_READ_PROP | ADS_RIGHT_DS_WRITE_PROP */
    ea[0].grfInheritance = CONTAINER_INHERIT_ACE;
    BuildTrusteeWithObjectsAndSidW( &ea[1].Trustee, &os_deny, (GUID *)&guid_a, NULL, everyone );
    ea[1].grfAccessMode = DENY_ACCESS;
    ea[1].grfAccessPermissions = 0x20;
    ea[1].grfInheritance = NO_INHERITANCE;
    err = SetEntriesInAclW( 2, ea, NULL, &acl );
    CHECK( !err && acl, "SetEntriesInAclW with object trustees: %lu", err );
    if (err || !acl) goto done;
    CHECK( acl->AclRevision == ACL_REVISION_DS, "ACL revision %u (want 4)", acl->AclRevision );
    CHECK( acl->AceCount == 2, "ACE count %u", acl->AceCount );
    CHECK( IsValidAcl( acl ), "IsValidAcl" );

    /* denies first */
    if (GetAce( acl, 0, (void **)&ace ))
    {
        sid = object_ace_sid( ace, &flags, &type, &inh );
        CHECK( ace->AceType == ACCESS_DENIED_OBJECT_ACE_TYPE, "ACE 0 type %u (want 6)", ace->AceType );
        CHECK( flags == ACE_OBJECT_TYPE_PRESENT, "ACE 0 object flags %lu (want 1)", flags );
        CHECK( type && IsEqualGUID( type, &guid_a ), "ACE 0 object type GUID" );
        CHECK( EqualSid( (PSID)sid, everyone ), "ACE 0 SID is Everyone" );
        CHECK( ((DWORD *)(ace + 1))[0] == 0x20, "ACE 0 mask %#lx", ((DWORD *)(ace + 1))[0] );
        CHECK( ace->AceSize == sizeof(ACE_HEADER) + 8 + sizeof(GUID) + GetLengthSid( everyone ),
               "ACE 0 size %u", ace->AceSize );
    }
    else CHECK( 0, "GetAce 0" );
    if (GetAce( acl, 1, (void **)&ace ))
    {
        sid = object_ace_sid( ace, &flags, &type, &inh );
        CHECK( ace->AceType == ACCESS_ALLOWED_OBJECT_ACE_TYPE, "ACE 1 type %u (want 5)", ace->AceType );
        CHECK( flags == (ACE_OBJECT_TYPE_PRESENT | ACE_INHERITED_OBJECT_TYPE_PRESENT), "ACE 1 object flags %lu (want 3)", flags );
        CHECK( type && IsEqualGUID( type, &guid_a ) && inh && IsEqualGUID( inh, &guid_b ), "ACE 1 GUIDs" );
        CHECK( EqualSid( (PSID)sid, users ), "ACE 1 SID is Users" );
        CHECK( ace->AceFlags == CONTAINER_INHERIT_ACE, "ACE 1 flags %#x", ace->AceFlags );
    }
    else CHECK( 0, "GetAce 1" );

    /* a plain entry on top keeps the object ACEs and the revision */
    memset( ea, 0, sizeof(ea) );
    ea[0].grfAccessMode = GRANT_ACCESS;
    ea[0].grfAccessPermissions = GENERIC_READ;
    ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea[0].Trustee.ptstrName = (WCHAR *)everyone;
    ea[1].grfAccessMode = SET_AUDIT_SUCCESS;
    ea[1].grfAccessPermissions = DELETE;
    ea[1].Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea[1].Trustee.ptstrName = (WCHAR *)users;
    err = SetEntriesInAclW( 2, ea, acl, &acl2 );
    CHECK( !err && acl2, "SetEntriesInAclW over an ACL with object ACEs: %lu", err );
    if (err || !acl2) goto done;
    CHECK( acl2->AclRevision == ACL_REVISION_DS, "merged ACL revision %u (want 4)", acl2->AclRevision );
    CHECK( acl2->AceCount == 4, "merged ACE count %u", acl2->AceCount );
    CHECK( IsValidAcl( acl2 ), "merged IsValidAcl" );

    err = GetExplicitEntriesFromAclW( acl2, &count, &list );
    CHECK( !err && count == 4, "GetExplicitEntriesFromAclW: %lu, %lu entries", err, count );
    for (i = 0; !err && i < count; i++)
    {
        if (list[i].Trustee.TrusteeForm == TRUSTEE_IS_OBJECTS_AND_SID)
        {
            OBJECTS_AND_SID *os = (OBJECTS_AND_SID *)list[i].Trustee.ptstrName;
            seen_obj++;
            if (list[i].grfAccessMode == GRANT_ACCESS)
                CHECK( os && os->ObjectsPresent == 3 && IsEqualGUID( &os->ObjectTypeGuid, &guid_a ) &&
                       IsEqualGUID( &os->InheritedObjectTypeGuid, &guid_b ) && EqualSid( os->pSid, users ) &&
                       list[i].grfAccessPermissions == 0x30, "entry %lu: object grant to Users", i );
            else
                CHECK( list[i].grfAccessMode == DENY_ACCESS && os && os->ObjectsPresent == 1 &&
                       IsEqualGUID( &os->ObjectTypeGuid, &guid_a ) && EqualSid( os->pSid, everyone ),
                       "entry %lu: object deny to Everyone (mode %d)", i, list[i].grfAccessMode );
        }
        else if (list[i].grfAccessMode == SET_AUDIT_SUCCESS)
        {
            seen_audit++;
            CHECK( list[i].Trustee.TrusteeForm == TRUSTEE_IS_SID && EqualSid( (PSID)list[i].Trustee.ptstrName, users ) &&
                   list[i].grfAccessPermissions == DELETE, "entry %lu: audit success for Users", i );
        }
    }
    CHECK( seen_obj == 2, "object entries back: %d (want 2)", seen_obj );
    CHECK( seen_audit == 1, "audit entries back: %d (want 1)", seen_audit );

done:
    LocalFree( list );
    LocalFree( acl );
    LocalFree( acl2 );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
