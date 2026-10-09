/* WinVerifyTrust cache actions and the certificate helpers (patches/sg/2404).
 * Table driven: each row is one check; the run fails on any row that fails. */
#include <windows.h>
#include <wincrypt.h>
#include <wintrust.h>
#include <softpub.h>
#include <stdio.h>
#include <string.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

typedef HRESULT (WINAPI *check_sig_t)(CRYPT_PROVIDER_DATA *);
typedef BOOL (WINAPI *self_signed_t)(DWORD, CERT_INFO *);

static int failures;
static void check(int ok, const char *what, long got)
{
    printf("%s  %s (got %#lx)\n", ok ? "ok  " : "FAIL", what, got);
    if (!ok) failures++;
}

static PCCERT_CONTEXT make_self_signed(HCRYPTPROV *prov, const char *container, const char *cn)
{
    CERT_NAME_BLOB name = {0};
    CRYPT_KEY_PROV_INFO ki = {0};
    WCHAR wc[64];
    char subj[96];
    PCCERT_CONTEXT cert;
    HCRYPTKEY key;
    WCHAR wcont[64];

    MultiByteToWideChar(CP_ACP, 0, container, -1, wcont, 64);
    CryptAcquireContextA(prov, container, MS_DEF_PROV_A, PROV_RSA_FULL, CRYPT_DELETEKEYSET);
    if (!CryptAcquireContextA(prov, container, MS_DEF_PROV_A, PROV_RSA_FULL, CRYPT_NEWKEYSET))
        return NULL;
    if (!CryptGenKey(*prov, AT_KEYEXCHANGE, 1024 << 16, &key)) return NULL;
    CryptDestroyKey(key);
    sprintf(subj, "CN=%s", cn);
    (void)wc;
    CertStrToNameA(X509_ASN_ENCODING, subj, 0, NULL, NULL, &name.cbData, NULL);
    name.pbData = HeapAlloc(GetProcessHeap(), 0, name.cbData);
    CertStrToNameA(X509_ASN_ENCODING, subj, 0, NULL, name.pbData, &name.cbData, NULL);
    ki.pwszContainerName = wcont;
    ki.dwProvType = PROV_RSA_FULL;
    ki.dwKeySpec = AT_KEYEXCHANGE;
    cert = CertCreateSelfSignCertificate(*prov, &name, 0, &ki, NULL, NULL, NULL, NULL);
    HeapFree(GetProcessHeap(), 0, name.pbData);
    return cert;
}

/* a certificate for 'subject' public key, issued (named and signed) by issuer_prov */
static PCCERT_CONTEXT make_issued(PCCERT_CONTEXT subject, PCCERT_CONTEXT issuer, HCRYPTPROV issuer_prov)
{
    CERT_INFO info = *subject->pCertInfo;
    CRYPT_ALGORITHM_IDENTIFIER alg = { (char *)szOID_RSA_SHA1RSA, { 0, NULL } };
    BYTE *enc;
    DWORD len = 0;
    PCCERT_CONTEXT ret;

    info.Issuer = issuer->pCertInfo->Subject;
    info.SignatureAlgorithm = alg;
    if (!CryptSignAndEncodeCertificate(issuer_prov, AT_KEYEXCHANGE, X509_ASN_ENCODING, X509_CERT_TO_BE_SIGNED,
                                       &info, &alg, NULL, NULL, &len)) return NULL;
    enc = HeapAlloc(GetProcessHeap(), 0, len);
    if (!CryptSignAndEncodeCertificate(issuer_prov, AT_KEYEXCHANGE, X509_ASN_ENCODING, X509_CERT_TO_BE_SIGNED,
                                       &info, &alg, NULL, enc, &len)) return NULL;
    ret = CertCreateCertificateContext(X509_ASN_ENCODING, enc, len);
    HeapFree(GetProcessHeap(), 0, enc);
    return ret;
}

static PCCERT_CONTEXT tampered(PCCERT_CONTEXT c)
{
    DWORD len = c->cbCertEncoded;
    BYTE *copy = HeapAlloc(GetProcessHeap(), 0, len);
    PCCERT_CONTEXT ret;
    memcpy(copy, c->pbCertEncoded, len);
    copy[len - 5] ^= 0x55; /* inside the signature value */
    ret = CertCreateCertificateContext(X509_ASN_ENCODING, copy, len);
    HeapFree(GetProcessHeap(), 0, copy);
    return ret;
}

static HRESULT run_chain(check_sig_t fn, PCCERT_CONTEXT *certs, DWORD n, DWORD *step_errors)
{
    CRYPT_PROVIDER_DATA pd = {0};
    CRYPT_PROVIDER_SGNR sgnr = {0};
    CRYPT_PROVIDER_CERT chain[4] = {{0}};
    DWORD i;

    for (i = 0; i < n; i++) { chain[i].cbStruct = sizeof(chain[i]); chain[i].pCert = certs[i]; }
    sgnr.cbStruct = sizeof(sgnr);
    sgnr.csCertChain = n;
    sgnr.pasCertChain = chain;
    pd.cbStruct = sizeof(pd);
    pd.csSigners = 1;
    pd.pasSigners = &sgnr;
    pd.padwTrustStepErrors = step_errors;
    return fn(&pd);
}

int main(void)
{
    HMODULE wt = LoadLibraryA("wintrust.dll");
    check_sig_t check_sig = (check_sig_t)GetProcAddress(wt, "WTHelperCertCheckValidSignature");
    self_signed_t is_self = (self_signed_t)GetProcAddress(wt, "WTHelperCertIsSelfSigned");
    HCRYPTPROV pa, pb;
    PCCERT_CONTEXT a, b, c, bad, chain[3];
    DWORD errs[64];
    HRESULT hr;
    char path[MAX_PATH];
    WCHAR wpath[MAX_PATH];
    HANDLE f;
    DWORD w;

    check(check_sig != NULL, "WTHelperCertCheckValidSignature exported", 0);
    check(is_self != NULL, "WTHelperCertIsSelfSigned exported", 0);
    if (!check_sig || !is_self) goto done;

    a = make_self_signed(&pa, "sgwt-a", "Probe A");
    b = make_self_signed(&pb, "sgwt-b", "Probe B");
    check(a && b, "created test certificates", 0);
    if (!a || !b) goto done;
    c = make_issued(a, b, pb);          /* A's key, issued by B */
    check(c != NULL, "issued certificate created", GetLastError());
    if (!c) goto done;
    bad = tampered(a);
    check(bad != NULL, "tampered certificate created", GetLastError());
    if (!bad) goto done;

    /* WTHelperCertIsSelfSigned */
    SetLastError(0xdeadbeef);
    check(is_self(X509_ASN_ENCODING, a->pCertInfo) == TRUE, "self-signed certificate is self-signed", 0);
    check(is_self(X509_ASN_ENCODING, b->pCertInfo) == TRUE, "second self-signed certificate", 0);
    check(is_self(X509_ASN_ENCODING, c->pCertInfo) == FALSE, "issued certificate is not self-signed", 0);
    SetLastError(0xdeadbeef);
    check(is_self(X509_ASN_ENCODING, NULL) == FALSE && GetLastError() == ERROR_INVALID_PARAMETER,
          "NULL CERT_INFO fails with ERROR_INVALID_PARAMETER", GetLastError());

    /* WTHelperCertCheckValidSignature */
    memset(errs, 0, sizeof(errs));
    chain[0] = a;
    hr = run_chain(check_sig, chain, 1, errs);
    check(hr == S_OK, "single valid self-signed certificate", hr);
    chain[0] = c; chain[1] = b;
    hr = run_chain(check_sig, chain, 2, errs);
    check(hr == S_OK, "leaf signed by the next certificate", hr);
    memset(errs, 0, sizeof(errs));
    chain[0] = a; chain[1] = b;
    hr = run_chain(check_sig, chain, 2, errs);
    check(hr == TRUST_E_CERT_SIGNATURE, "leaf not signed by the next certificate", hr);
    check(errs[TRUSTERROR_STEP_FINAL_CERTPROV] == (DWORD)TRUST_E_CERT_SIGNATURE, "step error recorded",
          errs[TRUSTERROR_STEP_FINAL_CERTPROV]);
    chain[0] = bad;
    hr = run_chain(check_sig, chain, 1, errs);
    check(hr == TRUST_E_CERT_SIGNATURE, "corrupted signature", hr);
    check(check_sig(NULL) == E_INVALIDARG, "NULL provider data", check_sig(NULL));

    /* WinVerifyTrust cache actions on an unsigned file */
    GetTempPathA(sizeof(path), path);
    strcat(path, "sgwt-unsigned.exe");
    f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(f, "MZ not really", 13, &w, NULL);
    CloseHandle(f);
    MultiByteToWideChar(CP_ACP, 0, path, -1, wpath, MAX_PATH);
    {
        static const GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
        static const DWORD actions[] = { WTD_STATEACTION_VERIFY, WTD_STATEACTION_AUTO_CACHE };
        static const char *const names[] = { "VERIFY", "AUTO_CACHE" };
        unsigned i;
        LONG expect = 0, r;

        for (i = 0; i < ARRAY_SIZE(actions); i++)
        {
            WINTRUST_FILE_INFO fi = { sizeof(fi), wpath };
            WINTRUST_DATA wd = { sizeof(wd) };
            char what[96];

            wd.dwUIChoice = WTD_UI_NONE;
            wd.dwUnionChoice = WTD_CHOICE_FILE;
            wd.pFile = &fi;
            wd.dwStateAction = actions[i];
            r = WinVerifyTrust(INVALID_HANDLE_VALUE, (GUID *)&action, &wd);
            if (!i) expect = r;
            sprintf(what, "%s: unsigned file fails like the first action", names[i]);
            check(r != 0 && r == expect, what, r);
            sprintf(what, "%s: leaves state data for the caller", names[i]);
            check(wd.hWVTStateData != NULL, what, (long)(INT_PTR)wd.hWVTStateData);
            wd.dwStateAction = (actions[i] == WTD_STATEACTION_VERIFY) ? WTD_STATEACTION_CLOSE : WTD_STATEACTION_AUTO_CACHE_FLUSH;
            r = WinVerifyTrust(INVALID_HANDLE_VALUE, (GUID *)&action, &wd);
            sprintf(what, "%s: closing releases the state", names[i]);
            check(r == 0 && wd.hWVTStateData == NULL, what, r);
        }
    }
    DeleteFileA(path);
done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
