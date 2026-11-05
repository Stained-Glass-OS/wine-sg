/* SSPI identities copied, compared and marshalled (6258): OneDrive's sync session code
 * (FileSyncSessions.dll, OneDrive.App.dll) imports SspiCopyAuthIdentity,
 * SspiCompareAuthIdentities, SspiMarshalAuthIdentity and SspiUnmarshalAuthIdentity from secur32,
 * which were missing. */
#include <windows.h>
#include <stdio.h>

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } else printf("ok   %s\n", #c); } while (0)

typedef LONG (WINAPI *encode_fn)(const WCHAR *, const WCHAR *, const WCHAR *, void **);
typedef LONG (WINAPI *strings_fn)(void *, const WCHAR **, const WCHAR **, const WCHAR **);
typedef LONG (WINAPI *copy_fn)(void *, void **);
typedef LONG (WINAPI *compare_fn)(void *, void *, BOOLEAN *, BOOLEAN *);
typedef LONG (WINAPI *marshal_fn)(void *, ULONG *, char **);
typedef LONG (WINAPI *unmarshal_fn)(ULONG, char *, void **);
typedef LONG (WINAPI *validate_fn)(void *);
typedef BOOLEAN (WINAPI *encrypted_fn)(void *);
typedef void (WINAPI *free_fn)(void *);

struct exw { ULONG Version, Length; WCHAR *User; ULONG UserLength; WCHAR *Domain; ULONG DomainLength;
             WCHAR *Password; ULONG PasswordLength; ULONG Flags; WCHAR *PackageList; ULONG PackageListLength; };

int main(void)
{
    HMODULE m = LoadLibraryA("secur32.dll");
    encode_fn encode = (void *)GetProcAddress(m, "SspiEncodeStringsAsAuthIdentity");
    strings_fn strings = (void *)GetProcAddress(m, "SspiEncodeAuthIdentityAsStrings");
    copy_fn copy = (void *)GetProcAddress(m, "SspiCopyAuthIdentity");
    compare_fn compare = (void *)GetProcAddress(m, "SspiCompareAuthIdentities");
    marshal_fn marshal = (void *)GetProcAddress(m, "SspiMarshalAuthIdentity");
    unmarshal_fn unmarshal = (void *)GetProcAddress(m, "SspiUnmarshalAuthIdentity");
    validate_fn validate = (void *)GetProcAddress(m, "SspiValidateAuthIdentity");
    encrypted_fn encrypted = (void *)GetProcAddress(m, "SspiIsAuthIdentityEncrypted");
    free_fn free_id = (void *)GetProcAddress(m, "SspiFreeAuthIdentity"), local_free = (void *)GetProcAddress(m, "SspiLocalFree");
    void *id1 = NULL, *id2 = NULL, *id3 = NULL, *id4 = NULL, *id5 = NULL, *id6 = NULL, *bad = NULL;
    const WCHAR *u = NULL, *d = NULL, *p = NULL;
    BOOLEAN same_user = 9, same_id = 9;
    WCHAR eu[] = L"carol", ed[] = L"WORK", ep[] = L"secret";
    struct exw ex = { 0x200, sizeof(ex), eu, 5, ed, 4, ep, 6, 2 /* UNICODE */, NULL, 0 };
    ULONG len = 0;
    char *bytes = NULL;

    setvbuf(stdout, NULL, _IONBF, 0);
    CHECK(encode && strings && copy && compare && marshal && unmarshal && validate && encrypted && free_id && local_free);
    if (fails) { printf("RESULT: FAIL\n"); return 1; }
    CHECK(encode(L"bob", L"EVERGREEN", L"pw1", &id1) == 0);
    CHECK(copy(id1, &id2) == 0 && id2 && id2 != id1);
    CHECK(compare(id1, id2, &same_user, &same_id) == 0 && same_user == TRUE && same_id == TRUE);
    CHECK(strings(id2, &u, &d, &p) == 0 && u && d && p && !lstrcmpW(u, L"bob") && !lstrcmpW(d, L"EVERGREEN") && !lstrcmpW(p, L"pw1"));
    CHECK(encode(L"BOB", L"evergreen", L"other", &id3) == 0);
    CHECK(compare(id1, id3, &same_user, &same_id) == 0 && same_user == TRUE && same_id == FALSE);
    CHECK(encode(L"alice", L"EVERGREEN", L"pw1", &id4) == 0);
    CHECK(compare(id1, id4, &same_user, &same_id) == 0 && same_user == FALSE && same_id == FALSE);
    CHECK(marshal(id1, &len, &bytes) == 0 && bytes && len > 40 && *(ULONG *)bytes == 0x201);
    CHECK(unmarshal(len, bytes, &id5) == 0 && id5);
    CHECK(id5 && compare(id1, id5, &same_user, &same_id) == 0 && same_user == TRUE && same_id == TRUE);
    u = d = p = NULL;
    CHECK(id5 && strings(id5, &u, &d, &p) == 0 && u && p && !lstrcmpW(u, L"bob") && !lstrcmpW(p, L"pw1"));
    CHECK(validate(bytes) == 0);   /* a marshalled identity is an identity too */
    CHECK(unmarshal(20, bytes, &bad) != 0 && !bad);
    CHECK(unmarshal(4, (char *)"\x01\x02\x03\x04", &bad) != 0 && !bad);
    CHECK(validate(id1) == 0);
    CHECK(encrypted(id1) == FALSE);
    CHECK(copy(&ex, &id6) == 0 && id6);
    u = d = p = NULL;
    CHECK(id6 && strings(id6, &u, &d, &p) == 0 && u && d && p && !lstrcmpW(u, L"carol") && !lstrcmpW(d, L"WORK") && !lstrcmpW(p, L"secret"));
    local_free(bytes);
    free_id(id1); free_id(id2); free_id(id3); free_id(id4); free_id(id5); free_id(id6);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
