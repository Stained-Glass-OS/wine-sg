/* mssign32: SignerSign / SignerSignEx sign a file through its SIP (patch 3728). The signatures are checked with
 * wintrust and crypt32, not with mssign32 itself. */
#include <windows.h>
#include <wincrypt.h>
#include <wintrust.h>
#include <softpub.h>
#include <mssip.h>
#include <stdio.h>
#include <string.h>

static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* the declarations of include/wine/mssign.h, which the mingw headers lack */
typedef struct { DWORD cbSize, cbBlob; BYTE *pbBlob; } SG_CONTEXT;
typedef struct { DWORD cbSize; const WCHAR *pwszFileName; HANDLE hFile; } SG_FILE_INFO;
typedef struct { DWORD cbSize; DWORD *pdwIndex; DWORD dwSubjectChoice; SG_FILE_INFO *pFile; } SG_SUBJECT;
typedef struct { DWORD cbSize; const CERT_CONTEXT *pSigningCert; DWORD dwCertPolicy; HCERTSTORE hCertStore; } SG_STORE_INFO;
typedef struct { DWORD cbSize; DWORD dwCertChoice; SG_STORE_INFO *pStore; HWND hwnd; } SG_CERT;
typedef struct { DWORD cbSize; BOOL fCommercial, fIndividual; const WCHAR *pwszName, *pwszInfo; } SG_AUTHCODE;
typedef struct { DWORD cbSize; ALG_ID algidHash; DWORD dwAttrChoice; SG_AUTHCODE *pAuthcode; CRYPT_ATTRIBUTES *psAuth, *psUnauth; } SG_SIGINFO;
typedef struct { DWORD cbSize; const WCHAR *pwszProviderName; DWORD dwProviderType, dwKeySpec, dwPvkChoice; WCHAR *pwszContainer; } SG_PROVIDER;

static HRESULT (WINAPI *pSignerSignEx)(DWORD, SG_SUBJECT *, SG_CERT *, SG_SIGINFO *, SG_PROVIDER *, const WCHAR *, CRYPT_ATTRIBUTES *, void *, SG_CONTEXT **);
static HRESULT (WINAPI *pSignerSign)(SG_SUBJECT *, SG_CERT *, SG_SIGINFO *, SG_PROVIDER *, const WCHAR *, CRYPT_ATTRIBUTES *, void *);
static HRESULT (WINAPI *pFree)(SG_CONTEXT *);

static const WCHAR container[] = L"FastbSignKey";
static const WCHAR other_container[] = L"FastbOtherKey";

static const CERT_CONTEXT *make_cert(const WCHAR *cont, const WCHAR *name, BOOL add_root)
{
    HCRYPTPROV prov = 0; HCRYPTKEY key = 0;
    CERT_NAME_BLOB nb = { 0 }; CRYPT_KEY_PROV_INFO kpi = { 0 }; const CERT_CONTEXT *cert;
    SYSTEMTIME st1, st2; DWORD len = 0;

    CryptAcquireContextW(&prov, cont, NULL, PROV_RSA_AES, CRYPT_DELETEKEYSET);
    if (!CryptAcquireContextW(&prov, cont, NULL, PROV_RSA_AES, CRYPT_NEWKEYSET)) { printf("no key container %lu\n", GetLastError()); return NULL; }
    if (!CryptGenKey(prov, AT_SIGNATURE, 1024 << 16, &key)) { printf("no key %lu\n", GetLastError()); return NULL; }
    CryptDestroyKey(key);
    CertStrToNameW(X509_ASN_ENCODING, name, 0, NULL, NULL, &len, NULL);
    nb.pbData = malloc(len); nb.cbData = len;
    CertStrToNameW(X509_ASN_ENCODING, name, 0, NULL, nb.pbData, &len, NULL);
    kpi.pwszContainerName = (WCHAR *)cont; kpi.dwProvType = PROV_RSA_AES; kpi.dwKeySpec = AT_SIGNATURE;
    GetSystemTime(&st1); st2 = st1; st1.wYear--; st2.wYear += 5;
    cert = CertCreateSelfSignCertificate(prov, &nb, 0, &kpi, NULL, &st1, &st2, NULL);
    free(nb.pbData);
    if (cert && add_root)
    {
        HCERTSTORE root = CertOpenSystemStoreW(0, L"Root");
        BOOL ok = root && CertAddCertificateContextToStore(root, cert, CERT_STORE_ADD_REPLACE_EXISTING, NULL);
        if (!ok) printf("note: could not add the root: %lu\n", GetLastError());
        if (root) CertCloseStore(root, 0);
    }
    CryptReleaseContext(prov, 0);
    return cert;
}

static LONG verify(const WCHAR *file)
{
    WINTRUST_FILE_INFO fi = { sizeof(fi), file, NULL, NULL };
    WINTRUST_DATA wd = { sizeof(wd) };
    GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    LONG r;

    wd.dwUIChoice = WTD_UI_NONE; wd.fdwRevocationChecks = WTD_REVOKE_NONE;
    wd.dwUnionChoice = WTD_CHOICE_FILE; wd.pFile = &fi; wd.dwStateAction = WTD_STATEACTION_IGNORE;
    wd.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;
    r = WinVerifyTrust(NULL, &action, &wd);
    return r;
}

/* the signature in the file: the signer's certificate, checked against the signature itself */
static BOOL signer_of(const WCHAR *path, const CERT_CONTEXT *want, CRYPT_ATTRIBUTES **attrs_out, BYTE **buf_out)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    SIP_SUBJECTINFO si = { sizeof(si) }; GUID g; DWORD enc = 0, idx = 0, len = 0; BYTE *blob = NULL;
    HCRYPTMSG msg; CERT_INFO *ci = NULL; DWORD cilen = 0; HCERTSTORE store; const CERT_CONTEXT *signer = NULL; BOOL ok = FALSE;

    if (h == INVALID_HANDLE_VALUE) return FALSE;
    if (!CryptSIPRetrieveSubjectGuid(path, h, &g)) { CloseHandle(h); return FALSE; }
    si.pgSubjectType = &g; si.hFile = h; si.pwsFileName = path;
    CryptSIPGetSignedDataMsg(&si, &enc, idx, &len, NULL);
    if (!len) { CloseHandle(h); return FALSE; }
    blob = malloc(len);
    if (!CryptSIPGetSignedDataMsg(&si, &enc, idx, &len, blob)) { free(blob); CloseHandle(h); return FALSE; }
    CloseHandle(h);
    msg = CryptMsgOpenToDecode(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, 0, 0, NULL, NULL);
    if (msg && CryptMsgUpdate(msg, blob, len, TRUE))
    {
        store = CertOpenStore(CERT_STORE_PROV_MSG, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, 0, msg);
        if (CryptMsgGetAndVerifySigner(msg, 0, NULL, CMSG_USE_SIGNER_INDEX_FLAG, &signer, &idx))
        {
            ok = CertCompareCertificate(X509_ASN_ENCODING, signer->pCertInfo, want->pCertInfo);
            CertFreeCertificateContext(signer);
        }
        if (store) CertCloseStore(store, 0);
        if (attrs_out)
        {
            DWORD n = 0;
            if (CryptMsgGetParam(msg, CMSG_SIGNER_AUTH_ATTR_PARAM, 0, NULL, &n))
            {
                *buf_out = malloc(n);
                CryptMsgGetParam(msg, CMSG_SIGNER_AUTH_ATTR_PARAM, 0, *buf_out, &n);
                *attrs_out = (CRYPT_ATTRIBUTES *)*buf_out;
            }
        }
    }
    if (msg) CryptMsgClose(msg);
    (void)ci; (void)cilen;
    free(blob);
    return ok;
}

static BOOL has_attr(CRYPT_ATTRIBUTES *a, const char *oid)
{
    DWORD i;
    if (!a) return FALSE;
    for (i = 0; i < a->cAttr; i++) if (!strcmp(a->rgAttr[i].pszObjId, oid)) return TRUE;
    return FALSE;
}

static void copy(const WCHAR *from, const WCHAR *to) { CopyFileW(from, to, FALSE); }

int main(void)
{
    HMODULE m = LoadLibraryA("mssign32.dll");
    WCHAR self[MAX_PATH], pe[] = L"fastb-sign1.exe", pe2[] = L"fastb-sign2.exe", pe3[] = L"fastb-sign3.exe", missing[] = L"fastb-nothere.exe";
    const CERT_CONTEXT *cert, *other;
    SG_FILE_INFO fi = { sizeof(fi), pe, NULL };
    DWORD index = 77;
    SG_SUBJECT subj = { sizeof(subj), &index, 1, &fi };
    SG_STORE_INFO store = { sizeof(store), NULL, 0, NULL };
    SG_CERT sc = { sizeof(sc), 2, &store, NULL };
    SG_SIGINFO sig = { sizeof(sig), CALG_SHA1, 0, NULL, NULL, NULL };
    SG_CONTEXT *ctx = NULL;
    HRESULT hr;
    LONG v;
    BYTE *abuf = NULL; CRYPT_ATTRIBUTES *attrs = NULL;

    pSignerSignEx = (void *)GetProcAddress(m, "SignerSignEx");
    pSignerSign = (void *)GetProcAddress(m, "SignerSign");
    pFree = (void *)GetProcAddress(m, "SignerFreeSignerContext");
    if (!pSignerSignEx || !pSignerSign || !pFree) { printf("FAIL exports\nRESULT: FAIL\n"); return 1; }

    GetModuleFileNameW(NULL, self, MAX_PATH);
    copy(self, pe); copy(self, pe2); copy(self, pe3);
    cert = make_cert(container, L"CN=Fastb Test Signer", TRUE);
    other = make_cert(other_container, L"CN=Fastb Other Signer", FALSE);
    if (!cert || !other) { printf("FAIL no certificates\nRESULT: FAIL\n"); return 1; }
    store.pSigningCert = cert;

    /* an unsigned file is unsigned */
    v = verify(pe);
    CHECK(v == TRUST_E_NOSIGNATURE || v == TRUST_E_SUBJECT_FORM_UNKNOWN || v == TRUST_E_PROVIDER_UNKNOWN, "unsigned file: %#lx", v);

    /* argument checks */
    SetLastError(0);
    CHECK(pSignerSignEx(0, NULL, &sc, &sig, NULL, NULL, NULL, NULL, &ctx) == E_INVALIDARG, "NULL subject");
    CHECK(pSignerSignEx(0, &subj, NULL, &sig, NULL, NULL, NULL, NULL, &ctx) == E_INVALIDARG, "NULL cert");
    CHECK(pSignerSignEx(0, &subj, &sc, NULL, NULL, NULL, NULL, NULL, &ctx) == E_INVALIDARG, "NULL signature info");
    CHECK(pSignerSignEx(0, &subj, &sc, &sig, NULL, NULL, NULL, NULL, NULL) == E_INVALIDARG, "NULL context pointer");
    subj.cbSize = 4; CHECK(pSignerSignEx(0, &subj, &sc, &sig, NULL, NULL, NULL, NULL, &ctx) == E_INVALIDARG, "short subject"); subj.cbSize = sizeof(subj);
    subj.dwSubjectChoice = 9; CHECK(pSignerSignEx(0, &subj, &sc, &sig, NULL, NULL, NULL, NULL, &ctx) == E_INVALIDARG, "bad subject choice"); subj.dwSubjectChoice = 1;
    sig.algidHash = CALG_RC4; CHECK(pSignerSignEx(0, &subj, &sc, &sig, NULL, NULL, NULL, NULL, &ctx) == NTE_BAD_ALGID, "not a hash algorithm"); sig.algidHash = CALG_SHA1;
    store.pSigningCert = NULL; CHECK(pSignerSignEx(0, &subj, &sc, &sig, NULL, NULL, NULL, NULL, &ctx) == E_INVALIDARG, "no signing certificate"); store.pSigningCert = cert;
    fi.pwszFileName = missing;
    hr = pSignerSignEx(0, &subj, &sc, &sig, NULL, NULL, NULL, NULL, &ctx);
    CHECK(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), "missing file: %#lx", hr);
    fi.pwszFileName = pe;
    CHECK(verify(pe) != 0, "nothing was signed by the failed calls");

    /* signing */
    ctx = NULL;
    hr = pSignerSignEx(0, &subj, &sc, &sig, NULL, NULL, NULL, NULL, &ctx);
    CHECK(hr == S_OK, "SignerSignEx %#lx", hr);
    CHECK(ctx && ctx->cbBlob > 100 && ctx->pbBlob, "signer context %p", ctx);
    CHECK(index == 0, "the signature index is returned: %lu", index);
    CHECK(signer_of(pe, cert, NULL, NULL), "the signature in the file verifies and names our certificate");
    v = verify(pe);
    CHECK(v == 0, "WinVerifyTrust on the signed file: %#lx", v);
    if (ctx) CHECK(pFree(ctx) == S_OK, "free context");

    /* a change to the file breaks the signature */
    {
        HANDLE h = CreateFileW(pe, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        BYTE b = 0;
        DWORD n;
        SetFilePointer(h, 0x500, NULL, FILE_BEGIN); ReadFile(h, &b, 1, &n, NULL);
        b ^= 0xff; SetFilePointer(h, 0x500, NULL, FILE_BEGIN); WriteFile(h, &b, 1, &n, NULL);
        CloseHandle(h);
        v = verify(pe);
        CHECK(v == (LONG)TRUST_E_BAD_DIGEST, "tampered file: %#lx", v);
    }

    /* another certificate (not in the root store): the signature is valid, the chain is not trusted */
    fi.pwszFileName = pe2; store.pSigningCert = other; index = 5;
    hr = pSignerSignEx(0, &subj, &sc, &sig, NULL, NULL, NULL, NULL, &ctx);
    CHECK(hr == S_OK, "second certificate %#lx", hr);
    if (hr == S_OK) pFree(ctx);
    CHECK(signer_of(pe2, other, NULL, NULL), "signed by the other certificate");
    CHECK(!signer_of(pe2, cert, NULL, NULL), "not by the first one");
    v = verify(pe2);
    CHECK(v == (LONG)CERT_E_UNTRUSTEDROOT || v == (LONG)CERT_E_CHAINING, "untrusted root: %#lx", v);

    /* authenticode attributes; SHA-256 */
    {
        SG_AUTHCODE ac = { sizeof(ac), TRUE, FALSE, L"Fastb Program", L"http://example.invalid/info" };
        fi.pwszFileName = pe3; store.pSigningCert = cert;
        sig.dwAttrChoice = 1; sig.pAuthcode = &ac; sig.algidHash = CALG_SHA_256;
        hr = pSignerSignEx(0, &subj, &sc, &sig, NULL, NULL, NULL, NULL, &ctx);
        CHECK(hr == S_OK, "authcode attributes, SHA-256: %#lx", hr);
        if (hr == S_OK) pFree(ctx);
        CHECK(signer_of(pe3, cert, &attrs, &abuf), "verifies");
        CHECK(has_attr(attrs, "1.3.6.1.4.1.311.2.1.12"), "program name attribute");
        CHECK(has_attr(attrs, "1.3.6.1.4.1.311.2.1.11"), "statement type attribute");
        if (attrs)
        {
            DWORD i, j; BOOL name = FALSE;
            for (i = 0; i < attrs->cAttr; i++)
                if (!strcmp(attrs->rgAttr[i].pszObjId, "1.3.6.1.4.1.311.2.1.12"))
                    for (j = 0; j + 2 < attrs->rgAttr[i].rgValue[0].cbData; j++)
                        if (!memcmp(attrs->rgAttr[i].rgValue[0].pbData + j, "\0F\0a\0s\0t\0b", 9)) name = TRUE;
            CHECK(name, "the program name is in the attribute (UTF-16BE)");
        }
        v = verify(pe3);
        CHECK(v == 0, "SHA-256 signed file: %#lx", v);
        free(abuf);
        sig.dwAttrChoice = 0; sig.pAuthcode = NULL; sig.algidHash = CALG_SHA1;
    }

    /* SignerSign with the key given by container (certificate from the store info) */
    {
        SG_PROVIDER pi = { sizeof(pi), NULL, PROV_RSA_AES, AT_SIGNATURE, 2, (WCHAR *)container };
        copy(self, pe3); fi.pwszFileName = pe3; store.pSigningCert = cert;
        hr = pSignerSign(&subj, &sc, &sig, &pi, NULL, NULL, NULL);
        CHECK(hr == S_OK, "SignerSign with a key container %#lx", hr);
        CHECK(verify(pe3) == 0, "verifies");
        /* the wrong key for the certificate */
        copy(self, pe3);
        pi.pwszContainer = (WCHAR *)other_container;
        hr = pSignerSign(&subj, &sc, &sig, &pi, NULL, NULL, NULL);
        v = verify(pe3);
        CHECK(FAILED(hr) || v != 0, "the wrong key must not give a good signature: hr %#lx verify %#lx", hr, v);
        CHECK(!signer_of(pe3, cert, NULL, NULL), "no signature by the certificate that has another key");
    }

    /* a certificate file (SIGNER_CERT_SPC_FILE) with the key given by container */
    {
        SG_PROVIDER pi = { sizeof(pi), NULL, PROV_RSA_AES, AT_SIGNATURE, 2, (WCHAR *)container };
        struct { DWORD cbSize, dwCertChoice; const WCHAR *spc; HWND hwnd; } spcc = { sizeof(spcc), 1, L"fastb-chain.p7b", NULL };
        HCERTSTORE mem = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, CERT_STORE_CREATE_NEW_FLAG, NULL);

        CertAddCertificateContextToStore(mem, other, CERT_STORE_ADD_ALWAYS, NULL);
        CertAddCertificateContextToStore(mem, cert, CERT_STORE_ADD_ALWAYS, NULL);
        CHECK(CertSaveStore(mem, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, CERT_STORE_SAVE_AS_PKCS7, CERT_STORE_SAVE_TO_FILENAME_W, (void *)spcc.spc, 0),
              "save the certificate file %lu", GetLastError());
        CertCloseStore(mem, 0);
        copy(self, pe3);
        hr = pSignerSign(&subj, (SG_CERT *)&spcc, &sig, &pi, NULL, NULL, NULL);
        CHECK(hr == S_OK, "SignerSign with a certificate file %#lx", hr);
        CHECK(signer_of(pe3, cert, NULL, NULL), "the certificate of the file that owns the key signs, not the first one");
        CHECK(verify(pe3) == 0, "verifies");
        DeleteFileW(spcc.spc);
    }

    /* the store policy puts the certificates of the store into the signature */
    {
        HCERTSTORE mem = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, CERT_STORE_CREATE_NEW_FLAG, NULL);
        CRYPT_ATTRIBUTES *a2 = NULL; BYTE *b2 = NULL;
        HANDLE h;
        SIP_SUBJECTINFO si = { sizeof(si) }; GUID g; DWORD enc = 0, len = 0, count = 0, cl = sizeof(count); BYTE *blob; HCRYPTMSG msg;

        CertAddCertificateContextToStore(mem, other, CERT_STORE_ADD_ALWAYS, NULL);
        store.hCertStore = mem; store.dwCertPolicy = 1; /* SIGNER_CERT_POLICY_STORE */
        copy(self, pe3); fi.pwszFileName = pe3;
        hr = pSignerSignEx(0, &subj, &sc, &sig, NULL, NULL, NULL, NULL, &ctx);
        CHECK(hr == S_OK, "store policy %#lx", hr);
        if (hr == S_OK) pFree(ctx);
        h = CreateFileW(pe3, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        CryptSIPRetrieveSubjectGuid(pe3, h, &g);
        si.pgSubjectType = &g; si.hFile = h; si.pwsFileName = pe3;
        CryptSIPGetSignedDataMsg(&si, &enc, 0, &len, NULL);
        blob = malloc(len + 1);
        CryptSIPGetSignedDataMsg(&si, &enc, 0, &len, blob);
        CloseHandle(h);
        msg = CryptMsgOpenToDecode(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, 0, 0, NULL, NULL);
        CryptMsgUpdate(msg, blob, len, TRUE);
        CryptMsgGetParam(msg, CMSG_CERT_COUNT_PARAM, 0, &count, &cl);
        CHECK(count == 2, "the signature holds the signer and the certificate of the store: %lu", count);
        CryptMsgClose(msg); free(blob); CertCloseStore(mem, 0);
        store.hCertStore = NULL; store.dwCertPolicy = 0;
        (void)a2; (void)b2;
    }

    /* a certificate whose private key is another key must not sign */
    {
        const CERT_CONTEXT *mismatch = CertCreateCertificateContext(X509_ASN_ENCODING, cert->pbCertEncoded, cert->cbCertEncoded);
        CRYPT_KEY_PROV_INFO kpi = { (WCHAR *)other_container, NULL, PROV_RSA_AES, 0, 0, NULL, AT_SIGNATURE };

        CertSetCertificateContextProperty(mismatch, CERT_KEY_PROV_INFO_PROP_ID, 0, &kpi);
        copy(self, pe3); fi.pwszFileName = pe3; store.pSigningCert = mismatch;
        hr = pSignerSignEx(0, &subj, &sc, &sig, NULL, NULL, NULL, NULL, &ctx);
        CHECK(FAILED(hr), "a certificate with the wrong private key signed: %#lx", hr);
        if (SUCCEEDED(hr)) pFree(ctx);
        CHECK(verify(pe3) != 0, "no good signature from the wrong key");
        CertFreeCertificateContext(mismatch);
        store.pSigningCert = cert;
    }

    /* formats without a signature interface, and files that cannot be changed */
    {
        HANDLE h = CreateFileW(L"fastb-sign.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        DWORD n;
        BYTE after[64];
        WriteFile(h, "plain text\r\n", 12, &n, NULL); CloseHandle(h);
        fi.pwszFileName = L"fastb-sign.txt";
        hr = pSignerSignEx(0, &subj, &sc, &sig, NULL, NULL, NULL, NULL, &ctx);
        CHECK(hr == TRUST_E_SUBJECT_FORM_UNKNOWN, "a text file has no signature format: %#lx", hr);
        h = CreateFileW(L"fastb-sign.txt", GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
        memset(after, 0, sizeof(after)); ReadFile(h, after, sizeof(after), &n, NULL); CloseHandle(h);
        CHECK(n == 12 && !memcmp(after, "plain text", 10), "the text file is unchanged (%lu bytes)", n);
        DeleteFileW(L"fastb-sign.txt");

        copy(self, pe3); SetFileAttributesW(pe3, FILE_ATTRIBUTE_READONLY);
        fi.pwszFileName = pe3;
        hr = pSignerSignEx(0, &subj, &sc, &sig, NULL, NULL, NULL, NULL, &ctx);
        CHECK(FAILED(hr), "a read-only file was signed: %#lx", hr);
        SetFileAttributesW(pe3, FILE_ATTRIBUTE_NORMAL);
        CHECK(verify(pe3) != 0, "the read-only file is still unsigned");
        /* an open handle instead of a name */
        h = CreateFileW(pe3, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        fi.pwszFileName = pe3; fi.hFile = h;
        hr = pSignerSignEx(0, &subj, &sc, &sig, NULL, NULL, NULL, NULL, &ctx);
        CHECK(hr == S_OK, "signing through a handle %#lx", hr);
        if (hr == S_OK) pFree(ctx);
        CloseHandle(h); fi.hFile = NULL;
        CHECK(verify(pe3) == 0, "signed through the handle");
    }

    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    DeleteFileW(pe); DeleteFileW(pe2); DeleteFileW(pe3);
    return f != 0;
}
