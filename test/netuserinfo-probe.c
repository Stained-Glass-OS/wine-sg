/* NetUserGetInfo levels 2, 3, 4, 11, 20 and 23 (patches/sg/2403), run by
 * test/netuserinfo-gate.sh. These levels used to answer NERR_InternalError.
 * Table-driven: every level is fetched for the current user and its fields
 * are checked against the others (one account, one answer), strings and the
 * SID must live inside the returned buffer, and the single-field "parmnum"
 * levels (1003...1053) belong to NetUserSetInfo and are invalid here. */
#include <windows.h>
#include <lm.h>
#include <stdio.h>
#include <string.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static BYTE *get(const WCHAR *user, DWORD level, NET_API_STATUS *st, DWORD *size)
{
    BYTE *buf = NULL;
    *st = NetUserGetInfo(NULL, user, level, &buf);
    *size = 0;
    if (!*st && buf) NetApiBufferSize(buf, size);
    return buf;
}

/* is p inside buf[0,size) (NULL allowed when allow_null) */
static int inside(const void *p, const BYTE *buf, DWORD size, int allow_null)
{
    if (!p) return allow_null;
    return (const BYTE *)p >= buf && (const BYTE *)p < buf + size;
}

int main(void)
{
    static const DWORD levels[] = { 0, 1, 2, 3, 4, 10, 11, 20, 23 };
    static const DWORD bad_levels[] = { 5, 6, 1003, 1005, 1009, 1017, 1024, 1051, 1053, 10000 };
    WCHAR user[256];
    DWORD usersize = ARRAY_SIZE(user), i, size;
    NET_API_STATUS st;
    BYTE *b[24] = { 0 };
    BYTE sidbuf[SECURITY_MAX_SID_SIZE];
    WCHAR domain[256];
    DWORD sidsize = sizeof(sidbuf), domsize = ARRAY_SIZE(domain), rid;
    SID_NAME_USE use;
    USER_INFO_1 *u1; USER_INFO_2 *u2; USER_INFO_3 *u3; USER_INFO_4 *u4;
    USER_INFO_11 *u11; USER_INFO_20 *u20; USER_INFO_23 *u23;
    char what[128];
    int have_sid;

    GetUserNameW(user, &usersize);
    have_sid = LookupAccountNameW(NULL, user, (PSID)sidbuf, &sidsize, domain, &domsize, &use);
    rid = have_sid ? *GetSidSubAuthority((PSID)sidbuf, *GetSidSubAuthorityCount((PSID)sidbuf) - 1) : 1000;

    for (i = 0; i < ARRAY_SIZE(levels); i++)
    {
        b[levels[i]] = get(user, levels[i], &st, &size);
        snprintf(what, sizeof(what), "NetUserGetInfo level %lu succeeds (got %lu)", levels[i], (unsigned long)st);
        check(st == NERR_Success && b[levels[i]], what);
    }
    u1 = (USER_INFO_1 *)b[1]; u2 = (USER_INFO_2 *)b[2]; u3 = (USER_INFO_3 *)b[3]; u4 = (USER_INFO_4 *)b[4];
    u11 = (USER_INFO_11 *)b[11]; u20 = (USER_INFO_20 *)b[20]; u23 = (USER_INFO_23 *)b[23];
    if (!u1 || !u2 || !u3 || !u4 || !u11 || !u20 || !u23) { printf("RESULT: FAIL\n"); return 1; }

    /* one account: names and shared fields agree across the levels */
    {
        struct { const char *what; const WCHAR *name; } names[] =
        {
            { "level 1 name", u1->usri1_name }, { "level 2 name", u2->usri2_name }, { "level 3 name", u3->usri3_name },
            { "level 4 name", u4->usri4_name }, { "level 11 name", u11->usri11_name },
            { "level 20 name", u20->usri20_name }, { "level 23 name", u23->usri23_name },
        };
        for (i = 0; i < ARRAY_SIZE(names); i++)
        {
            snprintf(what, sizeof(what), "%s is the account name", names[i].what);
            check(names[i].name && !lstrcmpiW(names[i].name, user), what);
        }
    }
    {
        struct { const char *what; DWORD a, b; } same[] =
        {
            { "flags 1 = 2", u1->usri1_flags, u2->usri2_flags }, { "flags 1 = 3", u1->usri1_flags, u3->usri3_flags },
            { "flags 1 = 4", u1->usri1_flags, u4->usri4_flags }, { "flags 1 = 20", u1->usri1_flags, u20->usri20_flags },
            { "flags 1 = 23", u1->usri1_flags, u23->usri23_flags },
            { "priv 1 = 2", u1->usri1_priv, u2->usri2_priv }, { "priv 1 = 3", u1->usri1_priv, u3->usri3_priv },
            { "priv 1 = 4", u1->usri1_priv, u4->usri4_priv }, { "priv 1 = 11", u1->usri1_priv, u11->usri11_priv },
            { "password age 1 = 2", u1->usri1_password_age, u2->usri2_password_age },
            { "password age 1 = 11", u1->usri1_password_age, u11->usri11_password_age },
            { "user id 3 = 20", u3->usri3_user_id, u20->usri20_user_id },
            { "user id is the SID's rid", u3->usri3_user_id, rid },
            { "primary group id 3 = 4", u3->usri3_primary_group_id, u4->usri4_primary_group_id },
            { "primary group is Domain Users", u3->usri3_primary_group_id, 513 },
        };
        for (i = 0; i < ARRAY_SIZE(same); i++) check(same[i].a == same[i].b, same[i].what);
    }

    /* the fields with a fixed answer */
    {
        struct { const char *what; DWORD got, want; } fixed[] =
        {
            { "level 2 code page is the ANSI one", u2->usri2_code_page, GetACP() },
            { "level 3 code page", u3->usri3_code_page, GetACP() },
            { "level 4 code page (the layout after the SID moved)", u4->usri4_code_page, GetACP() },
            { "level 11 code page", u11->usri11_code_page, GetACP() },
            { "level 2 never expires", u2->usri2_acct_expires, TIMEQ_FOREVER },
            { "level 3 never expires", u3->usri3_acct_expires, TIMEQ_FOREVER },
            { "level 4 never expires", u4->usri4_acct_expires, TIMEQ_FOREVER },
            { "level 2 unlimited storage", u2->usri2_max_storage, USER_MAXSTORAGE_UNLIMITED },
            { "level 11 unlimited storage", u11->usri11_max_storage, USER_MAXSTORAGE_UNLIMITED },
            { "level 2 bad password count", u2->usri2_bad_pw_count, 0 },
            { "level 3 password not expired", u3->usri3_password_expired, 0 },
            { "level 4 password not expired", u4->usri4_password_expired, 0 },
        };
        for (i = 0; i < ARRAY_SIZE(fixed); i++)
        {
            snprintf(what, sizeof(what), "%s (got %lu)", fixed[i].what, (unsigned long)fixed[i].got);
            check(fixed[i].got == fixed[i].want, what);
        }
    }
    check(u2->usri2_password == NULL && u3->usri3_password == NULL && u4->usri4_password == NULL,
          "the password is never returned");
    check(u2->usri2_logon_server && !wcscmp(u2->usri2_logon_server, L"\\\\*"), "the logon server is any server");
    check(u11->usri11_logon_server && !wcscmp(u11->usri11_logon_server, L"\\\\*"), "level 11 too");
    check(u2->usri2_full_name && !u2->usri2_full_name[0] && u3->usri3_profile && !u3->usri3_profile[0] &&
          u4->usri4_home_dir_drive && !u4->usri4_home_dir_drive[0] && u11->usri11_workstations &&
          !u11->usri11_workstations[0], "the empty strings are real, empty, strings");
    check(!wcscmp(u1->usri1_home_dir, u2->usri2_home_dir) && !wcscmp(u1->usri1_home_dir, u4->usri4_home_dir) &&
          !wcscmp(u1->usri1_home_dir, u11->usri11_home_dir), "home dir agrees across levels");
    check(!wcscmp(u1->usri1_comment, u2->usri2_comment) && !wcscmp(u1->usri1_comment, u11->usri11_comment),
          "comment agrees across levels");

    /* the SID of levels 4 and 23 */
    check(have_sid ? (u4->usri4_user_sid && EqualSid(u4->usri4_user_sid, (PSID)sidbuf)) : u4->usri4_user_sid == NULL,
          "level 4 carries the account SID");
    check(have_sid ? (u23->usri23_user_sid && EqualSid(u23->usri23_user_sid, (PSID)sidbuf)) : u23->usri23_user_sid == NULL,
          "level 23 carries the account SID");
    check(have_sid && u4->usri4_user_sid && IsValidSid(u4->usri4_user_sid), "and it is a valid SID");

    /* each level is one block: every pointer points into it */
    {
        struct { const char *what; DWORD level; int ok; } blocks[2];
        BYTE *bb; DWORD sz;
        int ok;

        bb = b[2]; NetApiBufferSize(bb, &sz);
        ok = inside(u2->usri2_name, bb, sz, 0) && inside(u2->usri2_home_dir, bb, sz, 0) &&
             inside(u2->usri2_comment, bb, sz, 0) && inside(u2->usri2_script_path, bb, sz, 0) &&
             inside(u2->usri2_full_name, bb, sz, 0) && inside(u2->usri2_usr_comment, bb, sz, 0) &&
             inside(u2->usri2_parms, bb, sz, 0) && inside(u2->usri2_workstations, bb, sz, 0) &&
             inside(u2->usri2_logon_server, bb, sz, 0);
        blocks[0].what = "level 2 strings live inside the one buffer"; blocks[0].ok = ok;
        bb = b[3]; NetApiBufferSize(bb, &sz);
        ok = inside(u3->usri3_name, bb, sz, 0) && inside(u3->usri3_logon_server, bb, sz, 0) &&
             inside(u3->usri3_profile, bb, sz, 0) && inside(u3->usri3_home_dir_drive, bb, sz, 0);
        blocks[1].what = "level 3 strings (including the three extra fields) live inside the buffer"; blocks[1].ok = ok;
        for (i = 0; i < 2; i++) check(blocks[i].ok, blocks[i].what);
        bb = b[4]; NetApiBufferSize(bb, &sz);
        check(inside(u4->usri4_name, bb, sz, 0) && inside(u4->usri4_profile, bb, sz, 0) &&
              inside(u4->usri4_home_dir_drive, bb, sz, 0) && inside(u4->usri4_user_sid, bb, sz, !have_sid),
              "level 4 strings and SID live inside the buffer");
        bb = b[23]; NetApiBufferSize(bb, &sz);
        check(inside(u23->usri23_name, bb, sz, 0) && inside(u23->usri23_full_name, bb, sz, 0) &&
              inside(u23->usri23_comment, bb, sz, 0) && inside(u23->usri23_user_sid, bb, sz, !have_sid),
              "level 23 strings and SID live inside the buffer");
        bb = b[11]; NetApiBufferSize(bb, &sz);
        check(inside(u11->usri11_name, bb, sz, 0) && inside(u11->usri11_comment, bb, sz, 0) &&
              inside(u11->usri11_usr_comment, bb, sz, 0) && inside(u11->usri11_full_name, bb, sz, 0) &&
              inside(u11->usri11_home_dir, bb, sz, 0) && inside(u11->usri11_parms, bb, sz, 0) &&
              inside(u11->usri11_logon_server, bb, sz, 0) && inside(u11->usri11_workstations, bb, sz, 0),
              "level 11 strings live inside the buffer");
    }

    for (i = 0; i < ARRAY_SIZE(levels); i++) NetApiBufferFree(b[levels[i]]);

    for (i = 0; i < ARRAY_SIZE(bad_levels); i++)
    {
        BYTE *buf = NULL;
        st = NetUserGetInfo(NULL, user, bad_levels[i], &buf);
        snprintf(what, sizeof(what), "level %lu is ERROR_INVALID_LEVEL (got %lu)", bad_levels[i], (unsigned long)st);
        check(st == ERROR_INVALID_LEVEL, what);
        if (!st && buf) NetApiBufferFree(buf);
    }
    for (i = 0; i < 4; i++)
    {
        static const DWORD ok_levels[] = { 2, 3, 4, 11 };
        static const DWORD more[] = { 20, 23 };
        BYTE *buf = NULL;
        st = NetUserGetInfo(NULL, L"No Such Account Here", ok_levels[i], &buf);
        snprintf(what, sizeof(what), "level %lu of a missing user is NERR_UserNotFound (got %lu)", ok_levels[i],
                 (unsigned long)st);
        check(st == NERR_UserNotFound, what);
        if (i < 2)
        {
            st = NetUserGetInfo(NULL, L"No Such Account Here", more[i], &buf);
            snprintf(what, sizeof(what), "level %lu of a missing user is NERR_UserNotFound (got %lu)", more[i],
                     (unsigned long)st);
            check(st == NERR_UserNotFound, what);
        }
    }

    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures ? 1 : 0;
}
