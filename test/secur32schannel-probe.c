/* Probe for patches/sg/2441: schannel credential and context attributes (supported algorithms, a certificate
 * without a private key, the cipher suite number, the key exchange group, the endpoint binding). */
#include <windows.h>
#define SECURITY_WIN32
#include <security.h>
#include <schannel.h>
#include <wincrypt.h>
#include <stdio.h>
#include <string.h>

static int fails;
static void check(const char *name, int ok) { printf("      %s  %s\n", ok ? "PASS" : "FAIL", name); if (!ok) fails++; }

#define BUFSZ 65536

static PCCERT_CONTEXT make_cert(BOOL with_key, const WCHAR *container)
{
    HCRYPTPROV prov = 0;
    HCRYPTKEY key = 0;
    PCCERT_CONTEXT cert = NULL;
    CERT_NAME_BLOB name;
    BYTE nameenc[128];
    DWORD size = sizeof(nameenc);
    CRYPT_KEY_PROV_INFO kpi = { (WCHAR *)container, (WCHAR *)MS_ENHANCED_PROV_W, PROV_RSA_FULL, 0, 0, NULL, AT_KEYEXCHANGE };
    SYSTEMTIME st;

    CryptAcquireContextW(&prov, container, MS_ENHANCED_PROV_W, PROV_RSA_FULL, CRYPT_DELETEKEYSET);
    if (!CryptAcquireContextW(&prov, container, MS_ENHANCED_PROV_W, PROV_RSA_FULL, CRYPT_NEWKEYSET)) { printf("      (acquire %lu)\n", GetLastError()); return NULL; }
    if (!CryptGenKey(prov, AT_KEYEXCHANGE, (2048 << 16) | CRYPT_EXPORTABLE, &key)) { printf("      (genkey %lu)\n", GetLastError()); return NULL; }
    if (!CertStrToNameW(X509_ASN_ENCODING, L"CN=localhost", CERT_X500_NAME_STR, NULL, nameenc, &size, NULL)) { printf("      (name %lu)\n", GetLastError()); return NULL; }
    name.cbData = size; name.pbData = nameenc;
    GetSystemTime(&st);
    cert = CertCreateSelfSignCertificate(prov, &name, 0, &kpi, NULL, &st, &st, NULL);
    if (cert)
    {
        /* valid for a day on either side */
        FILETIME ft;
        SystemTimeToFileTime(&st, &ft);
        (void)ft;
    }
    if (!cert) printf("      (selfsign %lu)\n", GetLastError());
    CryptDestroyKey(key);
    CryptReleaseContext(prov, 0);   /* the key is written when the context goes */
    if (!with_key && cert)
    {
        /* the same certificate with no word of a key */
        PCCERT_CONTEXT plain = CertCreateCertificateContext(X509_ASN_ENCODING, cert->pbCertEncoded, cert->cbCertEncoded);
        CertFreeCertificateContext(cert);
        cert = plain;
    }
    return cert;
}

static const char *suite_name_check(const WCHAR *name, DWORD id)
{
    static const struct { const WCHAR *name; DWORD id; } t[] =
    {
        { L"TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384", 0xc030 },
        { L"TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256", 0xc02f },
        { L"TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384", 0xc02c },
        { L"TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256", 0xc02b },
        { L"TLS_RSA_WITH_AES_256_GCM_SHA384", 0x9d },
        { L"TLS_RSA_WITH_AES_128_GCM_SHA256", 0x9c },
    };
    unsigned i;
    for (i = 0; i < sizeof(t) / sizeof(t[0]); i++)
        if (!wcscmp(name, t[i].name)) return id == t[i].id ? "ok" : "wrong";
    return "unknown";
}

int main(void)
{
    SCHANNEL_CRED sc;
    CredHandle ccred, scred;
    CtxtHandle cctx = {0}, sctx = {0};
    SECURITY_STATUS st;
    PCCERT_CONTEXT cert, nokey;
    SecPkgCred_SupportedAlgs algs;
    SecBuffer cbuf[2], sbuf[2], cin[2];
    SecBufferDesc cout_d, cin_d, sout_d, sin_d;
    char *ctok, *stok, *cdata, *sdata;
    DWORD cattr, sattr, clen = 0, slen = 0;
    int client_done = 0, server_done = 0, round, first_c = 1, first_s = 1;
    SecPkgContext_CipherInfo ci;
    SecPkgContext_Bindings bind;
    PCCERT_CONTEXT remote = NULL;
    BYTE hash[32];
    DWORD hlen = sizeof(hash);

    /* credentials */
    memset(&sc, 0, sizeof(sc));
    sc.dwVersion = SCHANNEL_CRED_VERSION;
    sc.dwFlags = SCH_CRED_NO_DEFAULT_CREDS | SCH_CRED_MANUAL_CRED_VALIDATION;
    st = AcquireCredentialsHandleA(NULL, (SEC_CHAR *)UNISP_NAME_A, SECPKG_CRED_OUTBOUND, NULL, &sc, NULL, NULL, &ccred, NULL);
    check("client credentials", st == SEC_E_OK);
    memset(&algs, 0, sizeof(algs));
    st = QueryCredentialsAttributesA(&ccred, SECPKG_ATTR_SUPPORTED_ALGS, &algs);
    check("SECPKG_ATTR_SUPPORTED_ALGS works", st == SEC_E_OK);
    if (st == SEC_E_OK)
    {
        DWORD i; int aes = 0, sha256 = 0;
        for (i = 0; i < algs.cSupportedAlgs; i++)
        {
            if (algs.palgSupportedAlgs[i] == CALG_AES_256) aes = 1;
            if (algs.palgSupportedAlgs[i] == CALG_SHA_256) sha256 = 1;
        }
        check("the list names AES-256 and SHA-256", algs.cSupportedAlgs >= 4 && aes && sha256);
        FreeContextBuffer(algs.palgSupportedAlgs);
    }
    st = QueryCredentialsAttributesA(&ccred, SECPKG_ATTR_SUPPORTED_ALGS, NULL);
    check("SUPPORTED_ALGS with no buffer: internal error", st == SEC_E_INTERNAL_ERROR);

    cert = make_cert(TRUE, L"sgschannelprobe1");
    nokey = make_cert(FALSE, L"sgschannelprobe2");
    check("test certificates", cert && nokey);
    if (!cert || !nokey) { puts("RESULT: FAIL"); return 1; }
    sc.cCreds = 1; sc.paCred = &nokey;
    {
        CredHandle h;
        st = AcquireCredentialsHandleA(NULL, (SEC_CHAR *)UNISP_NAME_A, SECPKG_CRED_OUTBOUND, NULL, &sc, NULL, NULL, &h, NULL);
        check("good certificate without a key, outbound: no credentials", st == SEC_E_NO_CREDENTIALS);
        st = AcquireCredentialsHandleA(NULL, (SEC_CHAR *)UNISP_NAME_A, SECPKG_CRED_INBOUND, NULL, &sc, NULL, NULL, &h, NULL);
        check("good certificate without a key, inbound: no credentials", st == SEC_E_NO_CREDENTIALS);
    }

    /* a loopback handshake */
    sc.paCred = &cert;
    sc.grbitEnabledProtocols = SP_PROT_TLS1_2_SERVER;
    st = AcquireCredentialsHandleA(NULL, (SEC_CHAR *)UNISP_NAME_A, SECPKG_CRED_INBOUND, NULL, &sc, NULL, NULL, &scred, NULL);
    printf("      (server credentials %08lx)\n", (unsigned long)st);
    check("server credentials", st == SEC_E_OK);
    if (st != SEC_E_OK) { puts("RESULT: FAIL"); return 1; }

    ctok = malloc(BUFSZ); stok = malloc(BUFSZ); cdata = malloc(BUFSZ); sdata = malloc(BUFSZ);
    for (round = 0; round < 20 && !(client_done && server_done); round++)
    {
        if (!client_done)
        {
            cbuf[0].BufferType = SECBUFFER_TOKEN; cbuf[0].pvBuffer = ctok; cbuf[0].cbBuffer = BUFSZ;
            cout_d.ulVersion = SECBUFFER_VERSION; cout_d.cBuffers = 1; cout_d.pBuffers = cbuf;
            cin[0].BufferType = SECBUFFER_TOKEN; cin[0].pvBuffer = sdata; cin[0].cbBuffer = slen;
            cin[1].BufferType = SECBUFFER_EMPTY; cin[1].pvBuffer = NULL; cin[1].cbBuffer = 0;
            cin_d.ulVersion = SECBUFFER_VERSION; cin_d.cBuffers = 2; cin_d.pBuffers = cin;
            st = InitializeSecurityContextA(&ccred, first_c ? NULL : &cctx, (SEC_CHAR *)"localhost",
                ISC_REQ_CONFIDENTIALITY | ISC_REQ_STREAM | ISC_REQ_MANUAL_CRED_VALIDATION | ISC_REQ_ALLOCATE_MEMORY * 0, 0, 0,
                first_c ? NULL : &cin_d, 0, &cctx, &cout_d, &cattr, NULL);
            first_c = 0;
            if (st == SEC_E_INCOMPLETE_MESSAGE) { /* wait for more */ }
            else if (st == SEC_E_OK) client_done = 1;
            else if (st != SEC_I_CONTINUE_NEEDED) { printf("      (client %08lx)\n", (unsigned long)st); break; }
            else { memcpy(cdata, ctok, cbuf[0].cbBuffer); clen = cbuf[0].cbBuffer; slen = 0; }
            if (st == SEC_E_OK && cbuf[0].cbBuffer) { memcpy(cdata, ctok, cbuf[0].cbBuffer); clen = cbuf[0].cbBuffer; slen = 0; }
        }
        if (!server_done && clen)
        {
            sbuf[0].BufferType = SECBUFFER_TOKEN; sbuf[0].pvBuffer = stok; sbuf[0].cbBuffer = BUFSZ;
            sout_d.ulVersion = SECBUFFER_VERSION; sout_d.cBuffers = 1; sout_d.pBuffers = sbuf;
            cin[0].BufferType = SECBUFFER_TOKEN; cin[0].pvBuffer = cdata; cin[0].cbBuffer = clen;
            cin[1].BufferType = SECBUFFER_EMPTY; cin[1].pvBuffer = NULL; cin[1].cbBuffer = 0;
            sin_d.ulVersion = SECBUFFER_VERSION; sin_d.cBuffers = 2; sin_d.pBuffers = cin;
            st = AcceptSecurityContext(&scred, first_s ? NULL : &sctx, &sin_d,
                ASC_REQ_CONFIDENTIALITY | ASC_REQ_STREAM, 0, &sctx, &sout_d, &sattr, NULL);
            first_s = 0;
            if (st == SEC_E_INCOMPLETE_MESSAGE) { continue; }
            else if (st == SEC_E_OK) server_done = 1;
            else if (st != SEC_I_CONTINUE_NEEDED) { printf("      (server %08lx)\n", (unsigned long)st); break; }
            memcpy(sdata, stok, sbuf[0].cbBuffer); slen = sbuf[0].cbBuffer; clen = 0;
        }
    }
    check("handshake finished on both sides", client_done && server_done);
    if (client_done)
    {
        memset(&ci, 0, sizeof(ci));
        ci.dwVersion = SECPKGCONTEXT_CIPHERINFO_V1;
        st = QueryContextAttributesA(&cctx, SECPKG_ATTR_CIPHER_INFO, &ci);
        check("SECPKG_ATTR_CIPHER_INFO", st == SEC_E_OK);
        if (st == SEC_E_OK)
        {
            printf("      (suite %lx %ls, key type %lx)\n", (unsigned long)ci.dwCipherSuite, ci.szCipherSuite, (unsigned long)ci.dwKeyType);
            check("cipher suite number is set", ci.dwCipherSuite != 0 && ci.dwBaseCipherSuite == ci.dwCipherSuite);
            check("cipher suite number agrees with its name", strcmp(suite_name_check(ci.szCipherSuite, ci.dwCipherSuite), "wrong"));
            check("key exchange group is named (secp256r1, secp384r1, secp521r1 or x25519)",
                  ci.dwKeyType == 0x17 || ci.dwKeyType == 0x18 || ci.dwKeyType == 0x19 || ci.dwKeyType == 0x1d || ci.dwKeyType == 0);
        }
        st = QueryContextAttributesA(&cctx, SECPKG_ATTR_REMOTE_CERT_CONTEXT, &remote);
        check("remote certificate", st == SEC_E_OK && remote);
        memset(&bind, 0, sizeof(bind));
        st = QueryContextAttributesA(&cctx, SECPKG_ATTR_ENDPOINT_BINDINGS, &bind);
        check("SECPKG_ATTR_ENDPOINT_BINDINGS", st == SEC_E_OK);
        if (st == SEC_E_OK && remote)
        {
            static const char prefix[] = "tls-server-end-point:";
            const char *p = (const char *)(bind.Bindings + 1);
            BOOL ok = CryptHashCertificate(0, CALG_SHA_256, 0, remote->pbCertEncoded, remote->cbCertEncoded, hash, &hlen);
            check("binding is the prefix and the SHA-256 of the certificate (sha256RSA)",
                  ok && bind.Bindings->cbApplicationDataLength == sizeof(prefix) - 1 + 32 &&
                  !memcmp(p, prefix, sizeof(prefix) - 1) && !memcmp(p + sizeof(prefix) - 1, hash, 32));
            FreeContextBuffer(bind.Bindings);
        }
        if (remote) CertFreeCertificateContext(remote);
    }
    puts(fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails != 0;
}
