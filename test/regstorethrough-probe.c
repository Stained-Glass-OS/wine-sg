/* registry certificate stores write through (patches/sg/2435), run by test/regstorethrough-gate.sh:
 * what one handle adds or deletes is in the registry for another at once, an auto-resyncing store
 * does not write back what it read, and a plain process has a current-service store. */
#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL  " __VA_ARGS__); printf("\n"); } } while (0)

static const BYTE cert1[] = { 0x30, 0x7a, 0x02, 0x01, 0x01, 0x30, 0x02, 0x06,
 0x00, 0x30, 0x15, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13,
 0x0a, 0x4a, 0x75, 0x61, 0x6e, 0x20, 0x4c, 0x61, 0x6e, 0x67, 0x00, 0x30, 0x22,
 0x18, 0x0f, 0x31, 0x36, 0x30, 0x31, 0x30, 0x31, 0x30, 0x31, 0x30, 0x30, 0x30,
 0x30, 0x30, 0x30, 0x5a, 0x18, 0x0f, 0x31, 0x36, 0x30, 0x31, 0x30, 0x31, 0x30,
 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x5a, 0x30, 0x15, 0x31, 0x13, 0x30,
 0x11, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x0a, 0x4a, 0x75, 0x61, 0x6e, 0x20,
 0x4c, 0x61, 0x6e, 0x67, 0x00, 0x30, 0x07, 0x30, 0x02, 0x06, 0x00, 0x03, 0x01,
 0x00, 0xa3, 0x16, 0x30, 0x14, 0x30, 0x12, 0x06, 0x03, 0x55, 0x1d, 0x13, 0x01,
 0x01, 0xff, 0x04, 0x08, 0x30, 0x06, 0x01, 0x01, 0xff, 0x02, 0x01, 0x01 };
static const BYTE cert2[] = { 0x30, 0x7a, 0x02, 0x01, 0x01, 0x30, 0x02, 0x06,
 0x00, 0x30, 0x15, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13,
 0x0a, 0x41, 0x6c, 0x65, 0x78, 0x20, 0x4c, 0x61, 0x6e, 0x67, 0x00, 0x30, 0x22,
 0x18, 0x0f, 0x31, 0x36, 0x30, 0x31, 0x30, 0x31, 0x30, 0x31, 0x30, 0x30, 0x30,
 0x30, 0x30, 0x30, 0x5a, 0x18, 0x0f, 0x31, 0x36, 0x30, 0x31, 0x30, 0x31, 0x30,
 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x5a, 0x30, 0x15, 0x31, 0x13, 0x30,
 0x11, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x0a, 0x41, 0x6c, 0x65, 0x78, 0x20,
 0x4c, 0x61, 0x6e, 0x67, 0x00, 0x30, 0x07, 0x30, 0x02, 0x06, 0x00, 0x03, 0x01,
 0x00, 0xa3, 0x16, 0x30, 0x14, 0x30, 0x12, 0x06, 0x03, 0x55, 0x1d, 0x13, 0x01,
 0x01, 0xff, 0x04, 0x08, 0x30, 0x06, 0x01, 0x01, 0xff, 0x02, 0x01, 0x01 };

static int count_certs(HCERTSTORE store)
{
    const CERT_CONTEXT *ctx = NULL;
    int n = 0;

    while ((ctx = CertEnumCertificatesInStore(store, ctx))) n++;
    return n;
}

int main(void)
{
    HCERTSTORE a, b, c;
    const CERT_CONTEXT *ctx;

    CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0, CERT_SYSTEM_STORE_CURRENT_USER | CERT_STORE_DELETE_FLAG, L"SGThrough");
    a = CertOpenStore(CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, 0, CERT_SYSTEM_STORE_CURRENT_USER, L"SGThrough");
    CHECK(a != NULL, "open %lu", GetLastError());

    /* an add is seen by another handle while the first is still open */
    CHECK(CertAddEncodedCertificateToStore(a, X509_ASN_ENCODING, cert1, sizeof(cert1), CERT_STORE_ADD_ALWAYS, NULL), "add");
    b = CertOpenStore(CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, 0, CERT_SYSTEM_STORE_CURRENT_USER | CERT_STORE_OPEN_EXISTING_FLAG, L"SGThrough");
    CHECK(b && count_certs(b) == 1, "a second handle sees the added certificate");
    if (b) CertCloseStore(b, 0);

    /* so is a delete */
    ctx = CertFindCertificateInStore(a, X509_ASN_ENCODING, 0, CERT_FIND_ANY, NULL, NULL);
    CHECK(ctx && CertDeleteCertificateFromStore(ctx), "delete");
    b = CertOpenStore(CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, 0, CERT_SYSTEM_STORE_CURRENT_USER | CERT_STORE_OPEN_EXISTING_FLAG, L"SGThrough");
    CHECK(b && count_certs(b) == 0, "a second handle sees the delete");
    if (b) CertCloseStore(b, 0);

    /* resyncing writes nothing back: a certificate added by another handle stays when this one is resynced */
    CHECK(CertAddEncodedCertificateToStore(a, X509_ASN_ENCODING, cert1, sizeof(cert1), CERT_STORE_ADD_ALWAYS, NULL), "add again");
    c = CertOpenStore(CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, 0, CERT_SYSTEM_STORE_CURRENT_USER, L"SGThrough");
    CHECK(CertAddEncodedCertificateToStore(c, X509_ASN_ENCODING, cert2, sizeof(cert2), CERT_STORE_ADD_ALWAYS, NULL), "add to the other handle");
    CHECK(CertControlStore(a, 0, CERT_STORE_CTRL_RESYNC, NULL) && count_certs(a) == 2, "resync sees both");
    CertCloseStore(a, 0);
    CertCloseStore(c, 0);
    b = CertOpenStore(CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, 0, CERT_SYSTEM_STORE_CURRENT_USER | CERT_STORE_OPEN_EXISTING_FLAG, L"SGThrough");
    CHECK(b && count_certs(b) == 2, "after the resync and the closes both are still there: %d", b ? count_certs(b) : -1);
    if (b) CertCloseStore(b, 0);
    CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0, CERT_SYSTEM_STORE_CURRENT_USER | CERT_STORE_DELETE_FLAG, L"SGThrough");

    /* a store that resyncs by itself leaves the registry as the others made it */
    a = CertOpenStore(CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, 0, CERT_SYSTEM_STORE_CURRENT_USER, L"SGThrough");
    CHECK(CertControlStore(a, 0, CERT_STORE_CTRL_AUTO_RESYNC, NULL), "auto resync");
    c = CertOpenStore(CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, 0, CERT_SYSTEM_STORE_CURRENT_USER, L"SGThrough");
    CertAddEncodedCertificateToStore(c, X509_ASN_ENCODING, cert1, sizeof(cert1), CERT_STORE_ADD_ALWAYS, NULL);
    CertCloseStore(c, 0);
    CHECK(count_certs(a) == 1, "the auto-resync store sees the first");
    c = CertOpenStore(CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, 0, CERT_SYSTEM_STORE_CURRENT_USER, L"SGThrough");
    CertAddEncodedCertificateToStore(c, X509_ASN_ENCODING, cert2, sizeof(cert2), CERT_STORE_ADD_ALWAYS, NULL);
    CertCloseStore(c, 0);
    CHECK(count_certs(a) == 2, "and the second");
    b = CertOpenStore(CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, 0, CERT_SYSTEM_STORE_CURRENT_USER | CERT_STORE_OPEN_EXISTING_FLAG, L"SGThrough");
    CHECK(b && count_certs(b) == 2, "the registry still holds both: %d", b ? count_certs(b) : -1);
    if (b) CertCloseStore(b, 0);
    CertCloseStore(a, 0);
    CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0, CERT_SYSTEM_STORE_CURRENT_USER | CERT_STORE_DELETE_FLAG, L"SGThrough");

    /* a plain process has a current-service store, under its program name */
    SetLastError(0);
    a = CertOpenStore(CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, 0, CERT_SYSTEM_STORE_CURRENT_SERVICE, L"My");
    CHECK(a != NULL, "current-service store: %lu", GetLastError());
    if (a) CertCloseStore(a, 0);
    CHECK(CertRegisterSystemStore(L"SGThroughReg", CERT_SYSTEM_STORE_CURRENT_SERVICE, NULL, NULL), "register in the current service: %lu", GetLastError());
    CertUnregisterSystemStore(L"SGThroughReg", CERT_SYSTEM_STORE_CURRENT_SERVICE);

    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures != 0;
}
