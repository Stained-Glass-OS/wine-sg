/* Streamed CryptMsgOpenToEncode of signed and enveloped messages (patch 3757) */
#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>
#include <string.h>

static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

struct acc { BYTE *data; DWORD len, calls; BOOL final_seen; DWORD calls_after_final; };

static BOOL WINAPI out_cb(const void *arg, BYTE *pb, DWORD cb, BOOL final)
{
    struct acc *a = (struct acc *)arg;
    if (a->final_seen) a->calls_after_final++;
    a->data = realloc(a->data, a->len + cb + 1);
    memcpy(a->data + a->len, pb, cb);
    a->len += cb;
    a->calls++;
    if (final) a->final_seen = TRUE;
    return TRUE;
}

static HCRYPTPROV prov;
static PCCERT_CONTEXT sign_cert, kx_cert;

static void setup(void)
{
    HCRYPTKEY key;
    CERT_NAME_BLOB name = { 0, NULL };
    CRYPT_KEY_PROV_INFO kpi;
    DWORD sz;
    BYTE *enc;
    CERT_RDN_ATTR attr;
    CERT_RDN rdn;
    CERT_NAME_INFO ni;
    int i;

    CryptAcquireContextA(&prov, "FastbStreamKeys", MS_ENHANCED_PROV_A, PROV_RSA_FULL, CRYPT_DELETEKEYSET);
    CHECK(CryptAcquireContextA(&prov, "FastbStreamKeys", MS_ENHANCED_PROV_A, PROV_RSA_FULL, CRYPT_NEWKEYSET), "container: %lu", GetLastError());
    CHECK(CryptGenKey(prov, AT_SIGNATURE, 1024 << 16, &key), "gen signature key: %lu", GetLastError());
    CryptDestroyKey(key);
    CHECK(CryptGenKey(prov, AT_KEYEXCHANGE, 1024 << 16, &key), "gen exchange key: %lu", GetLastError());
    CryptDestroyKey(key);

    attr.pszObjId = (LPSTR)szOID_COMMON_NAME;
    attr.dwValueType = CERT_RDN_PRINTABLE_STRING;
    attr.Value.pbData = (BYTE *)"FastB stream";
    attr.Value.cbData = 12;
    rdn.cRDNAttr = 1; rdn.rgRDNAttr = &attr;
    ni.cRDN = 1; ni.rgRDN = &rdn;
    CryptEncodeObjectEx(X509_ASN_ENCODING, X509_NAME, &ni, CRYPT_ENCODE_ALLOC_FLAG, NULL, &enc, &sz);
    name.pbData = enc; name.cbData = sz;
    for (i = 0; i < 2; i++)
    {
        static WCHAR cont[] = L"FastbStreamKeys", provn[] = MS_ENHANCED_PROV_W;
        memset(&kpi, 0, sizeof(kpi));
        kpi.pwszContainerName = cont; kpi.pwszProvName = provn; kpi.dwProvType = PROV_RSA_FULL; kpi.dwKeySpec = i ? AT_KEYEXCHANGE : AT_SIGNATURE;
        if (i == 0) sign_cert = CertCreateSelfSignCertificate(prov, &name, 0, &kpi, NULL, NULL, NULL, NULL);
        else kx_cert = CertCreateSelfSignCertificate(prov, &name, 0, &kpi, NULL, NULL, NULL, NULL);
    }
    LocalFree(enc);
    CHECK(sign_cert && kx_cert, "certificates: %lu", GetLastError());
}

static BYTE *content(DWORD n)
{
    BYTE *p = malloc(n + 1);
    DWORD i;
    for (i = 0; i < n; i++) p[i] = (BYTE)(i * 7 + 3);
    return p;
}

/* the decoded message of a complete output, non-streamed */
static HCRYPTMSG decode(const struct acc *a, DWORD flags, DWORD type)
{
    HCRYPTMSG m = CryptMsgOpenToDecode(PKCS_7_ASN_ENCODING, flags, type, 0, NULL, NULL);
    if (m && !CryptMsgUpdate(m, a->data, a->len, TRUE)) { printf("note: decode of the streamed output failed: %lx\n", GetLastError()); CryptMsgClose(m); return NULL; }
    return m;
}

static BOOL get_content(HCRYPTMSG m, BYTE **out, DWORD *len)
{
    *len = 0;
    if (!CryptMsgGetParam(m, CMSG_CONTENT_PARAM, 0, NULL, len)) return FALSE;
    *out = malloc(*len + 1);
    return CryptMsgGetParam(m, CMSG_CONTENT_PARAM, 0, *out, len);
}

static void signed_roundtrip(const char *name, DWORD flags, DWORD cbContent, const DWORD *chunks, int nchunks, DWORD total, BOOL tamper)
{
    CMSG_SIGNER_ENCODE_INFO signer;
    CMSG_SIGNED_ENCODE_INFO si;
    CERT_BLOB cb;
    struct acc a = { 0 };
    CMSG_STREAM_INFO stream = { cbContent, out_cb, &a };
    HCRYPTMSG m, d;
    BYTE *data = content(total), *got = NULL;
    DWORD off = 0, i, glen;
    BOOL ok;

    memset(&signer, 0, sizeof(signer));
    signer.cbSize = sizeof(signer);
    signer.pCertInfo = sign_cert->pCertInfo;
    signer.hCryptProv = prov;
    signer.dwKeySpec = AT_SIGNATURE;
    signer.HashAlgorithm.pszObjId = (LPSTR)szOID_OIWSEC_sha1;
    memset(&si, 0, sizeof(si));
    si.cbSize = sizeof(si);
    si.cSigners = 1;
    si.rgSigners = &signer;
    cb.cbData = sign_cert->cbCertEncoded; cb.pbData = sign_cert->pbCertEncoded;
    si.cCertEncoded = 1; si.rgCertEncoded = &cb;

    m = CryptMsgOpenToEncode(PKCS_7_ASN_ENCODING, flags, CMSG_SIGNED, &si, NULL, &stream);
    CHECK(m != NULL, "%s: open: %lx", name, GetLastError());
    if (!m) return;
    CHECK(a.calls == 0, "%s: nothing is sent before the first update", name);
    for (i = 0; i < (DWORD)nchunks; i++)
    {
        DWORD n = chunks[i];
        BOOL last = i == (DWORD)nchunks - 1;
        ok = CryptMsgUpdate(m, data + off, n, last);
        CHECK(ok, "%s: update %lu: %lx", name, i, GetLastError());
        off += n;
        if (i == 0) CHECK(a.len >= 20, "%s: the header came with the first update: %lu bytes", name, a.len);
        if (!last) CHECK(!a.final_seen, "%s: not final yet", name);
    }
    CHECK(a.final_seen && a.calls_after_final == 0, "%s: the output ended with a final call", name);
    CHECK(a.len > ((flags & CMSG_DETACHED_FLAG) ? 0 : total), "%s: %lu bytes of output", name, a.len);
    CHECK(a.data[0] == 0x30 && a.data[1] == 0x80 || (flags & CMSG_BARE_CONTENT_FLAG), "%s: indefinite-length message", name);
    if (tamper && total > 3)
    {
        /* flip one content byte in the output: the signature must fail */
        DWORD j;
        for (j = 0; j + 4 <= a.len; j++)
            if (!memcmp(a.data + j, data, 4)) { a.data[j + 1] ^= 0xff; break; }
    }
    d = decode(&a, (flags & CMSG_DETACHED_FLAG), (flags & CMSG_BARE_CONTENT_FLAG) ? CMSG_SIGNED : 0);
    CHECK(d != NULL, "%s: the streamed output decodes", name);
    if (d)
    {
        if (flags & CMSG_DETACHED_FLAG)
            CHECK(CryptMsgUpdate(d, data, total, TRUE), "%s: detached content: %lx", name, GetLastError());
        else
        {
            ok = get_content(d, &got, &glen);
            if (!tamper) CHECK(ok && glen == total && !memcmp(got, data, total), "%s: content %d len %lu / %lu", name, ok, glen, total);
        }
        SetLastError(0xdeadbeef);
        ok = CryptMsgControl(d, 0, CMSG_CTRL_VERIFY_SIGNATURE, sign_cert->pCertInfo);
        if (tamper) CHECK(!ok, "%s: the tampered message must not verify", name);
        else CHECK(ok, "%s: the signature verifies: %lx", name, GetLastError());
        CryptMsgClose(d);
    }
    if (!(flags & CMSG_DETACHED_FLAG) && !tamper && total)
    {
        /* the bytes of the content as the stream carries them */
        BOOL found = FALSE;
        DWORD j;
        for (j = 0; j + total <= a.len && total >= 8; j++) if (!memcmp(a.data + j, data, total) || (total > 300 && 0)) found = TRUE;
        if (cbContent != 0xffffffff) CHECK(found, "%s: definite-length content is contiguous in the output", name);
    }
    if ((flags & CMSG_DETACHED_FLAG) && total >= 16)
    {
        DWORD j;
        BOOL found = FALSE;
        for (j = 0; j + 16 <= a.len; j++) if (!memcmp(a.data + j, data, 16)) found = TRUE;
        CHECK(!found, "%s: a detached message does not carry the content", name);
    }
    free(got); free(data); free(a.data);
    CryptMsgClose(m);
}

static void enveloped_roundtrip(const char *name, DWORD flags, LPSTR algo, const DWORD *chunks, int nchunks, DWORD total)
{
    CMSG_ENVELOPED_ENCODE_INFO ei;
    PCERT_INFO recip = kx_cert->pCertInfo;
    struct acc a = { 0 };
    CMSG_STREAM_INFO stream = { 0xffffffff, out_cb, &a };
    HCRYPTMSG m, d;
    BYTE *data = content(total), *got = NULL;
    DWORD off = 0, i, glen;
    CMSG_CTRL_DECRYPT_PARA dp;
    BOOL ok;

    memset(&ei, 0, sizeof(ei));
    ei.cbSize = sizeof(ei);
    ei.ContentEncryptionAlgorithm.pszObjId = algo;
    ei.cRecipients = 1;
    ei.rgpRecipientCert = &recip;
    ei.hCryptProv = prov;
    m = CryptMsgOpenToEncode(PKCS_7_ASN_ENCODING, flags, CMSG_ENVELOPED, &ei, NULL, &stream);
    CHECK(m != NULL, "%s: open: %lx", name, GetLastError());
    if (!m) return;
    for (i = 0; i < (DWORD)nchunks; i++)
    {
        BOOL last = i == (DWORD)nchunks - 1;
        ok = CryptMsgUpdate(m, data + off, chunks[i], last);
        CHECK(ok, "%s: update %lu: %lx", name, i, GetLastError());
        off += chunks[i];
    }
    CHECK(a.final_seen && a.calls_after_final == 0, "%s: ended with a final call", name);
    CHECK(a.len > total, "%s: %lu bytes of output for %lu", name, a.len, total);
    d = decode(&a, 0, (flags & CMSG_BARE_CONTENT_FLAG) ? CMSG_ENVELOPED : 0);
    CHECK(d != NULL, "%s: the streamed output decodes", name);
    if (d)
    {
        memset(&dp, 0, sizeof(dp));
        dp.cbSize = sizeof(dp);
        dp.hCryptProv = prov;
        dp.dwKeySpec = AT_KEYEXCHANGE;
        dp.dwRecipientIndex = 0;
        ok = CryptMsgControl(d, 0, CMSG_CTRL_DECRYPT, &dp);
        CHECK(ok, "%s: decrypt: %lx", name, GetLastError());
        ok = get_content(d, &got, &glen);
        CHECK(ok && glen == total && !memcmp(got, data, total), "%s: plaintext %d len %lu / %lu", name, ok, glen, total);
        CryptMsgClose(d);
    }
    /* the plain text does not appear in the output */
    if (total >= 16)
    {
        DWORD j;
        BOOL found = FALSE;
        for (j = 0; j + 16 <= a.len; j++) if (!memcmp(a.data + j, data, 16)) found = TRUE;
        CHECK(!found, "%s: the output is encrypted", name);
    }
    free(got); free(data); free(a.data);
    CryptMsgClose(m);
}


static BOOL WINAPI sink_cb(const void *arg, BYTE *pb, DWORD cb, BOOL final) { return TRUE; }

static void extra_checks(void)
{
    CMSG_SIGNER_ENCODE_INFO signer;
    CMSG_SIGNED_ENCODE_INFO si;
    CERT_BLOB cb;
    CMSG_STREAM_INFO stream;
    struct acc a = { 0 };
    HCRYPTMSG m, d, d2;
    BYTE data[100], *enc1, *enc2;
    DWORD l1 = 0, l2 = 0, len;
    BOOL ok;

    memset(data, 0x5a, sizeof(data));
    memset(&signer, 0, sizeof(signer));
    signer.cbSize = sizeof(signer);
    signer.pCertInfo = sign_cert->pCertInfo;
    signer.hCryptProv = prov;
    signer.dwKeySpec = AT_SIGNATURE;
    signer.HashAlgorithm.pszObjId = (LPSTR)szOID_OIWSEC_sha1;
    memset(&si, 0, sizeof(si));
    si.cbSize = sizeof(si); si.cSigners = 1; si.rgSigners = &signer;
    cb.cbData = sign_cert->cbCertEncoded; cb.pbData = sign_cert->pbCertEncoded;
    si.cCertEncoded = 1; si.rgCertEncoded = &cb;

    /* the signature of the streamed message is the one of the same message in one piece */
    stream.cbContent = 0xffffffff; stream.pfnStreamOutput = out_cb; stream.pvArg = &a;
    m = CryptMsgOpenToEncode(PKCS_7_ASN_ENCODING, 0, CMSG_SIGNED, &si, NULL, &stream);
    CryptMsgUpdate(m, data, 30, FALSE);
    CryptMsgUpdate(m, data + 30, 70, TRUE);
    CryptMsgClose(m);
    d = decode(&a, 0, 0);
    m = CryptMsgOpenToEncode(PKCS_7_ASN_ENCODING, 0, CMSG_SIGNED, &si, NULL, NULL);
    CryptMsgUpdate(m, data, sizeof(data), TRUE);
    len = 0;
    CryptMsgGetParam(m, CMSG_CONTENT_PARAM, 0, NULL, &len);
    enc2 = malloc(len);
    CryptMsgGetParam(m, CMSG_CONTENT_PARAM, 0, enc2, &len);
    d2 = CryptMsgOpenToDecode(PKCS_7_ASN_ENCODING, 0, 0, 0, NULL, NULL);
    CryptMsgUpdate(d2, enc2, len, TRUE);
    CryptMsgClose(m);
    if (d && d2)
    {
        CryptMsgGetParam(d, CMSG_ENCODED_SIGNER, 0, NULL, &l1);
        CryptMsgGetParam(d2, CMSG_ENCODED_SIGNER, 0, NULL, &l2);
        enc1 = malloc(l1); enc2 = malloc(l2);
        ok = CryptMsgGetParam(d, CMSG_ENCODED_SIGNER, 0, enc1, &l1) && CryptMsgGetParam(d2, CMSG_ENCODED_SIGNER, 0, enc2, &l2);
        CHECK(ok && l1 == l2 && l1 > 20 && !memcmp(enc1, enc2, l1), "the streamed signer info is the one-piece signer info: %lu / %lu", l1, l2);
        free(enc1); free(enc2);
    }
    if (d) CryptMsgClose(d);
    if (d2) CryptMsgClose(d2);
    free(a.data);

    /* more content than cbContent said; less */
    memset(&a, 0, sizeof(a));
    stream.cbContent = 50; stream.pfnStreamOutput = out_cb; stream.pvArg = &a;
    m = CryptMsgOpenToEncode(PKCS_7_ASN_ENCODING, 0, CMSG_SIGNED, &si, NULL, &stream);
    SetLastError(0xdeadbeef);
    ok = CryptMsgUpdate(m, data, 51, TRUE);
    CHECK(!ok && GetLastError() == E_INVALIDARG, "more than cbContent: %d %lx", ok, GetLastError());
    CryptMsgClose(m);
    free(a.data); memset(&a, 0, sizeof(a));
    m = CryptMsgOpenToEncode(PKCS_7_ASN_ENCODING, 0, CMSG_SIGNED, &si, NULL, &stream);
    SetLastError(0xdeadbeef);
    ok = CryptMsgUpdate(m, data, 49, TRUE);
    CHECK(!ok && GetLastError() == CRYPT_E_STREAM_INSUFFICIENT_DATA, "less than cbContent: %d %lx", ok, GetLastError());
    CryptMsgClose(m);
    free(a.data); memset(&a, 0, sizeof(a));

    /* a NULL callback: the update faults and fails */
    stream.cbContent = 0xffffffff; stream.pfnStreamOutput = NULL; stream.pvArg = NULL;
    m = CryptMsgOpenToEncode(PKCS_7_ASN_ENCODING, 0, CMSG_SIGNED, &si, NULL, &stream);
    SetLastError(0xdeadbeef);
    ok = CryptMsgUpdate(m, data, 10, FALSE);
    CHECK(!ok && GetLastError() == STATUS_ACCESS_VIOLATION, "NULL callback: %d %lx", ok, GetLastError());
    CryptMsgClose(m);

    /* a message that ended takes no more; a callback that fails fails the update */
    stream.pfnStreamOutput = sink_cb;
    m = CryptMsgOpenToEncode(PKCS_7_ASN_ENCODING, 0, CMSG_SIGNED, &si, NULL, &stream);
    CHECK(CryptMsgUpdate(m, data, 10, TRUE), "update");
    SetLastError(0xdeadbeef);
    ok = CryptMsgUpdate(m, data, 10, TRUE);
    CHECK(!ok && GetLastError() == CRYPT_E_MSG_ERROR, "after the final update: %d %lx", ok, GetLastError());
    CryptMsgClose(m);

    /* BER from elsewhere: an indefinite-length data message with two chunks decodes as one content */
    {
        static const BYTE ber[] = { 0x30,0x80,0x06,0x09,0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x07,0x01,0xa0,0x80,0x24,0x80,
         0x04,0x04,1,2,3,4,0x04,0x04,5,6,7,8,0,0,0,0,0,0 };
        struct acc b;
        BYTE *got = NULL;
        DWORD glen = 0;

        b.data = (BYTE *)ber; b.len = sizeof(ber);
        d = decode(&b, 0, 0);
        CHECK(d != NULL, "an indefinite-length data message decodes");
        if (d)
        {
            static const BYTE want[] = { 1,2,3,4,5,6,7,8 };
            ok = get_content(d, &got, &glen);
            CHECK(ok && glen == 8 && !memcmp(got, want, 8), "its content: %d %lu", ok, glen);
            free(got);
            CryptMsgClose(d);
        }
        /* cut short: it must fail, not read past the end */
        b.len = sizeof(ber) - 4;
        d = CryptMsgOpenToDecode(PKCS_7_ASN_ENCODING, 0, 0, 0, NULL, NULL);
        SetLastError(0xdeadbeef);
        ok = CryptMsgUpdate(d, ber, b.len, TRUE);
        CHECK(!ok, "a truncated indefinite-length message fails: %lx", GetLastError());
        CryptMsgClose(d);
    }
}

int main(void)
{
    static const DWORD one[] = { 1000 }, three[] = { 10, 0, 990 }, many[] = { 7, 1, 100, 3, 61, 5, 13, 1, 1 };
    static const DWORD zero[] = { 0 }, big[] = { 5000, 60000, 1 };
    char rc2[] = szOID_RSA_RC2CBC, des3[] = szOID_RSA_DES_EDE3_CBC, rc4[] = szOID_RSA_RC4;

    setup();
    if (!sign_cert) { printf("RESULT: FAIL\n"); return 1; }

    signed_roundtrip("signed one update", 0, 0xffffffff, one, 1, 1000, FALSE);
    signed_roundtrip("signed three updates", 0, 0xffffffff, three, 3, 1000, FALSE);
    signed_roundtrip("signed many", 0, 0xffffffff, many, 9, 192, FALSE);
    signed_roundtrip("signed empty", 0, 0xffffffff, zero, 1, 0, FALSE);
    signed_roundtrip("signed definite", 0, 1000, three, 3, 1000, FALSE);
    signed_roundtrip("signed bare", CMSG_BARE_CONTENT_FLAG, 0xffffffff, three, 3, 1000, FALSE);
    signed_roundtrip("signed detached", CMSG_DETACHED_FLAG, 0xffffffff, three, 3, 1000, FALSE);
    signed_roundtrip("signed large", 0, 0xffffffff, big, 3, 65001, FALSE);
    signed_roundtrip("signed tampered", 0, 0xffffffff, three, 3, 1000, TRUE);

    enveloped_roundtrip("enveloped rc2 one", 0, rc2, one, 1, 1000);
    enveloped_roundtrip("enveloped rc2 three", 0, rc2, three, 3, 1000);
    enveloped_roundtrip("enveloped 3des many", 0, des3, many, 9, 192);
    enveloped_roundtrip("enveloped rc4 many", 0, rc4, many, 9, 192);
    enveloped_roundtrip("enveloped empty-ish", 0, rc2, three, 1, 10);
    enveloped_roundtrip("enveloped bare", CMSG_BARE_CONTENT_FLAG, des3, three, 3, 1000);
    enveloped_roundtrip("enveloped large", 0, des3, big, 3, 65001);

    extra_checks();

    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    return f != 0;
}
