/* Probe for patches/sg/5029: CertCloseStore with CERT_CLOSE_STORE_CHECK_FLAG
 * fails with CRYPT_E_PENDING_CLOSE while other references remain - for
 * collection, registry (provider) and system stores as for memory stores
 * (crypt32 store tests on Windows) - and succeeds for the last one.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

int main(void)
{
    static const char *kinds[] = {"memory", "collection", "registry", "system"};
    HKEY key;
    unsigned int i;

    RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\sg-closecheck", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL);
    for (i = 0; i < ARRAYSIZE(kinds); i++)
    {
        HCERTSTORE store = NULL, dup;
        BOOL ret;

        switch (i)
        {
        case 0: store = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, 0, NULL); break;
        case 1: store = CertOpenStore(CERT_STORE_PROV_COLLECTION, 0, 0, 0, NULL); break;
        case 2: store = CertOpenStore(CERT_STORE_PROV_REG, 0, 0, 0, key); break;
        case 3: store = CertOpenStore(CERT_STORE_PROV_SYSTEM_A, 0, 0, CERT_SYSTEM_STORE_CURRENT_USER, "MY"); break;
        }
        check(store != NULL, "%s store opened: %#lx", kinds[i], store ? 0 : GetLastError());
        if (!store) continue;
        dup = CertDuplicateStore(store);
        SetLastError(0xdeadbeef);
        ret = CertCloseStore(store, CERT_CLOSE_STORE_CHECK_FLAG);
        check(!ret && GetLastError() == CRYPT_E_PENDING_CLOSE, "%s store, a reference left: %d %#lx", kinds[i], ret, GetLastError());
        SetLastError(0xdeadbeef);
        ret = CertCloseStore(dup, CERT_CLOSE_STORE_CHECK_FLAG);
        check(ret, "%s store, the last reference: %d %#lx", kinds[i], ret, ret ? 0 : GetLastError());
    }
    RegCloseKey(key);
    RegDeleteKeyA(HKEY_CURRENT_USER, "Software\\sg-closecheck");
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
