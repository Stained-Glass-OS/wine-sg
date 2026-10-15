/* Streamed CryptMsgOpenToDecode (patch 3758) */
#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>
#include <string.h>

static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

struct acc { BYTE *data; DWORD len, calls; BOOL final_seen; DWORD calls_after_final; };
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))

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


struct dec { BYTE *data; DWORD len, calls, final_calls, after_final; BOOL last_final; };

static BOOL WINAPI dec_cb(const void *arg, BYTE *pb, DWORD cb, BOOL final)
{
    struct dec *d = (struct dec *)arg;
    if (d->final_calls) d->after_final++;
    d->data = realloc(d->data, d->len + cb + 1);
    memcpy(d->data + d->len, pb, cb);
    d->len += cb;
    d->calls++;
    if (final) d->final_calls++;
    d->last_final = final;
    return TRUE;
}

/* the streamed encoder's output for a signed / enveloped message, to feed to the decoder */
static void encode_signed(struct acc *a, const BYTE *data, DWORD total, DWORD cbContent)
{
    CMSG_SIGNER_ENCODE_INFO signer;
    CMSG_SIGNED_ENCODE_INFO si;
    CERT_BLOB cb;
    CMSG_STREAM_INFO stream = { cbContent, out_cb, a };
    HCRYPTMSG m;

    memset(&signer, 0, sizeof(signer));
    signer.cbSize = sizeof(signer);
    signer.pCertInfo = sign_cert->pCertInfo; signer.hCryptProv = prov; signer.dwKeySpec = AT_SIGNATURE;
    signer.HashAlgorithm.pszObjId = (LPSTR)szOID_OIWSEC_sha1;
    memset(&si, 0, sizeof(si));
    si.cbSize = sizeof(si); si.cSigners = 1; si.rgSigners = &signer;
    cb.cbData = sign_cert->cbCertEncoded; cb.pbData = sign_cert->pbCertEncoded;
    si.cCertEncoded = 1; si.rgCertEncoded = &cb;
    m = CryptMsgOpenToEncode(PKCS_7_ASN_ENCODING, 0, CMSG_SIGNED, &si, NULL, &stream);
    CryptMsgUpdate(m, data, total / 2, FALSE);
    CryptMsgUpdate(m, data + total / 2, total - total / 2, TRUE);
    CryptMsgClose(m);
}

static void encode_enveloped(struct acc *a, const BYTE *data, DWORD total, LPSTR algo, DWORD flags)
{
    CMSG_ENVELOPED_ENCODE_INFO ei;
    PCERT_INFO recip = kx_cert->pCertInfo;
    CMSG_STREAM_INFO stream = { 0xffffffff, out_cb, a };
    HCRYPTMSG m;

    memset(&ei, 0, sizeof(ei));
    ei.cbSize = sizeof(ei);
    ei.ContentEncryptionAlgorithm.pszObjId = algo;
    ei.cRecipients = 1; ei.rgpRecipientCert = &recip; ei.hCryptProv = prov;
    m = CryptMsgOpenToEncode(PKCS_7_ASN_ENCODING, flags, CMSG_ENVELOPED, &ei, NULL, &stream);
    CryptMsgUpdate(m, data, total / 3, FALSE);
    CryptMsgUpdate(m, data + total / 3, total - total / 3, TRUE);
    CryptMsgClose(m);
}

/* feeds the bytes in pieces of "piece"; the last update is final */
static BOOL feed(HCRYPTMSG m, const BYTE *p, DWORD len, DWORD piece, DWORD *first_output_at, struct dec *d)
{
    DWORD off = 0;
    BOOL ok = TRUE;

    while (off < len && ok)
    {
        DWORD n = min(piece, len - off);
        ok = CryptMsgUpdate(m, p + off, n, FALSE);
        off += n;
        if (first_output_at && !*first_output_at && d->calls) *first_output_at = off;
    }
    return ok && CryptMsgUpdate(m, NULL, 0, TRUE);
}

static void data_cases(void)
{
    /* an indefinite-length data message with chunks 4+4, a definite-length one, a bare one */
    static const BYTE indef[] = { 0x30,0x80,0x06,0x09,0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x07,0x01,0xa0,0x80,0x24,0x80,
     0x04,0x04,1,2,3,4,0x04,0x04,5,6,7,8,0,0,0,0,0,0 };
    static const BYTE definite[] = { 0x30,0x13,0x06,0x09,0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x07,0x01,0xa0,0x06,0x04,0x04,1,2,3,4 };
    static const BYTE constr_def[] = { 0x30,0x19,0x06,0x09,0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x07,0x01,0xa0,0x0c,0x24,0x0a,
     0x04,0x03,9,8,7,0x04,0x01,6,0x04,0x00 };
    static const BYTE empty[] = { 0x30,0x0f,0x06,0x09,0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x07,0x01,0xa0,0x02,0x04,0x00 };
    static const BYTE bare[] = { 0x04,0x04,1,2,3,4 };
    static const struct { const char *name; const BYTE *p; DWORD len; DWORD type; BYTE want[8]; DWORD wantlen; } t[] =
    {
        { "indefinite", indef, sizeof(indef), 0, { 1,2,3,4,5,6,7,8 }, 8 },
        { "definite", definite, sizeof(definite), 0, { 1,2,3,4 }, 4 },
        { "constructed definite", constr_def, sizeof(constr_def), 0, { 9,8,7,6 }, 4 },
        { "empty", empty, sizeof(empty), 0, { 0 }, 0 },
        { "bare", bare, sizeof(bare), CMSG_DATA, { 1,2,3,4 }, 4 },
    };
    static const DWORD pieces[] = { 1, 2, 3, 5, 100 };
    DWORD i, j;

    for (i = 0; i < ARRAY_SIZE(t); i++)
        for (j = 0; j < ARRAY_SIZE(pieces); j++)
        {
            struct dec d = { 0 };
            CMSG_STREAM_INFO si = { 0, dec_cb, &d };
            HCRYPTMSG m = CryptMsgOpenToDecode(PKCS_7_ASN_ENCODING, 0, t[i].type, 0, NULL, &si);
            DWORD first = 0;
            BOOL ok = feed(m, t[i].p, t[i].len, pieces[j], &first, &d);

            CHECK(ok, "%s/%lu: update: %lx", t[i].name, pieces[j], GetLastError());
            CHECK(d.len == t[i].wantlen && !memcmp(d.data, t[i].want, t[i].wantlen), "%s/%lu: content %lu bytes", t[i].name, pieces[j], d.len);
            CHECK(d.final_calls == 1 && d.last_final && !d.after_final, "%s/%lu: one final call, the last: %lu %d %lu", t[i].name, pieces[j], d.final_calls, d.last_final, d.after_final);
            if (pieces[j] == 1 && t[i].wantlen > 1 && t[i].type == 0)
                CHECK(first && first < t[i].len, "%s: output before the end of the input: at byte %lu of %lu", t[i].name, first, t[i].len);
            if (pieces[j] == 1) CHECK(d.calls >= 1, "%s: calls %lu", t[i].name, d.calls);
            {
                BYTE *got = NULL;
                DWORD glen;
                if (get_content(m, &got, &glen)) CHECK(glen == t[i].wantlen && !memcmp(got, t[i].want, glen), "%s: CMSG_CONTENT_PARAM", t[i].name);
                else CHECK(0, "%s: CMSG_CONTENT_PARAM: %lx", t[i].name, GetLastError());
                free(got);
            }
            CryptMsgClose(m);
            free(d.data);
        }
}

static BOOL WINAPI fail_cb(const void *arg, BYTE *pb, DWORD cb, BOOL final) { SetLastError(ERROR_CANCELLED); return FALSE; }

static void error_cases(void)
{
    static const BYTE empty[] = { 0x30,0x0f,0x06,0x09,0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x07,0x01,0xa0,0x02,0x04,0x00 };
    static const BYTE bogus[] = { 1,2,3,4 };
    static const BYTE hashed[] = { 0x30,0x0f,0x06,0x09,0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x07,0x05,0xa0,0x02,0x30,0x00 };
    struct dec d = { 0 };
    CMSG_STREAM_INFO si = { 0, dec_cb, &d };
    HCRYPTMSG m;
    BOOL ok;
    DWORD len;
    BYTE buf[16];

    /* bytes that are not a message */
    m = CryptMsgOpenToDecode(PKCS_7_ASN_ENCODING, 0, 0, 0, NULL, &si);
    SetLastError(0xdeadbeef);
    ok = CryptMsgUpdate(m, bogus, sizeof(bogus), FALSE);
    CHECK(!ok && GetLastError() == CRYPT_E_ASN1_BADTAG, "bogus non-final update: %d %lx", ok, GetLastError());
    /* the failed update left nothing behind: the message still takes a good one */
    ok = CryptMsgUpdate(m, empty, sizeof(empty), TRUE);
    CHECK(ok, "a good message after a bad update: %lx", GetLastError());
    CHECK(d.final_calls == 1, "its callback");
    CryptMsgClose(m);
    free(d.data); memset(&d, 0, sizeof(d));

    /* a SET where the SEQUENCE belongs, the rest valid */
    m = CryptMsgOpenToDecode(PKCS_7_ASN_ENCODING, 0, 0, 0, NULL, &si);
    {
        BYTE bad[sizeof(empty)];
        memcpy(bad, empty, sizeof(empty));
        bad[0] = 0x31;
        SetLastError(0xdeadbeef);
        ok = CryptMsgUpdate(m, bad, sizeof(bad), FALSE);
        CHECK(!ok && GetLastError() == CRYPT_E_ASN1_BADTAG, "SET for SEQUENCE: %d %lx", ok, GetLastError());
    }
    CryptMsgClose(m);

    /* a chunk that is longer than the string it is in */
    {
        static const BYTE over[] = { 0x30,0x15,0x06,0x09,0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x07,0x01,0xa0,0x08,0x24,0x03,0x04,0x05,1,2,3,4,5 };
        struct dec d2 = { 0 };
        CMSG_STREAM_INFO si2 = { 0, dec_cb, &d2 };

        m = CryptMsgOpenToDecode(PKCS_7_ASN_ENCODING, 0, 0, 0, NULL, &si2);
        SetLastError(0xdeadbeef);
        ok = CryptMsgUpdate(m, over, sizeof(over), FALSE);
        CHECK(!ok && GetLastError() == CRYPT_E_ASN1_BADTAG, "chunk longer than its string: %d %lx", ok, GetLastError());
        CryptMsgClose(m);
        free(d2.data);
    }

    /* a detached message has no content to hand on */
    {
        struct dec d2 = { 0 };
        CMSG_STREAM_INFO si2 = { 0, dec_cb, &d2 };
        static const BYTE dm[] = { 0x30,0x13,0x06,0x09,0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x07,0x01,0xa0,0x06,0x04,0x04,1,2,3,4 };

        m = CryptMsgOpenToDecode(PKCS_7_ASN_ENCODING, CMSG_DETACHED_FLAG, 0, 0, NULL, &si2);
        ok = CryptMsgUpdate(m, dm, sizeof(dm), FALSE);
        CHECK(ok && d2.calls == 0, "detached: no callbacks (%lu)", d2.calls);
        CryptMsgClose(m);
        free(d2.data);
    }

    /* a wrong tag in the header */
    m = CryptMsgOpenToDecode(PKCS_7_ASN_ENCODING, 0, 0, 0, NULL, &si);
    {
        BYTE bad[sizeof(empty)];
        memcpy(bad, empty, sizeof(empty));
        bad[13] = 0xa1;   /* [1] instead of [0] */
        SetLastError(0xdeadbeef);
        ok = CryptMsgUpdate(m, bad, sizeof(bad), FALSE);
        CHECK(!ok && GetLastError() == CRYPT_E_ASN1_BADTAG, "wrong [0]: %d %lx", ok, GetLastError());
        memcpy(bad, empty, sizeof(empty));
        bad[15] = 0x05;   /* NULL instead of OCTET STRING */
        SetLastError(0xdeadbeef);
        ok = CryptMsgUpdate(m, bad, sizeof(bad), FALSE);
        CHECK(!ok && GetLastError() == CRYPT_E_ASN1_BADTAG, "wrong content tag: %d %lx", ok, GetLastError());
    }
    CryptMsgClose(m);

    /* an end that comes too early */
    m = CryptMsgOpenToDecode(PKCS_7_ASN_ENCODING, 0, 0, 0, NULL, &si);
    CHECK(CryptMsgUpdate(m, empty, 10, FALSE), "part of a message");
    SetLastError(0xdeadbeef);
    ok = CryptMsgUpdate(m, NULL, 0, TRUE);
    CHECK(!ok && GetLastError() == CRYPT_E_STREAM_INSUFFICIENT_DATA, "final update, content not complete: %d %lx", ok, GetLastError());
    CryptMsgClose(m);

    /* parameters are not there before the end of a data or signed message */
    m = CryptMsgOpenToDecode(PKCS_7_ASN_ENCODING, 0, 0, 0, NULL, &si);
    CryptMsgUpdate(m, empty, 5, FALSE);
    len = sizeof(buf);
    SetLastError(0xdeadbeef);
    ok = CryptMsgGetParam(m, CMSG_CONTENT_PARAM, 0, buf, &len);
    CHECK(!ok && GetLastError() == CRYPT_E_STREAM_MSG_NOT_READY, "param before the end: %d %lx", ok, GetLastError());
    CryptMsgClose(m);

    /* a callback that refuses fails the update, which leaves nothing behind */
    {
        CMSG_STREAM_INFO fsi = { 0, fail_cb, NULL };

        m = CryptMsgOpenToDecode(PKCS_7_ASN_ENCODING, 0, 0, 0, NULL, &fsi);
        SetLastError(0xdeadbeef);
        ok = CryptMsgUpdate(m, empty, sizeof(empty), FALSE);
        CHECK(!ok && GetLastError() == ERROR_CANCELLED, "a failing callback: %d %lx", ok, GetLastError());
        CryptMsgClose(m);
    }

    /* a hashed message is not streamed: no callbacks, it decodes at the end as before */
    memset(&d, 0, sizeof(d));
    m = CryptMsgOpenToDecode(PKCS_7_ASN_ENCODING, 0, 0, 0, NULL, &si);
    ok = CryptMsgUpdate(m, hashed, sizeof(hashed), FALSE);
    CHECK(ok, "hashed update: %lx", GetLastError());
    CHECK(d.calls == 0, "no callbacks for a hashed message");
    CryptMsgClose(m);
    free(d.data);
}

static void signed_cases(void)
{
    BYTE *data = content(3000), *got;
    DWORD pieces[] = { 1, 7, 64, 100000 }, i, glen;
    struct acc enc;

    for (i = 0; i < ARRAY_SIZE(pieces); i++)
    {
        DWORD k;
        for (k = 0; k < 2; k++)
        {
            struct dec d = { 0 };
            CMSG_STREAM_INFO si = { 0, dec_cb, &d };
            HCRYPTMSG m;
            DWORD first = 0;
            BOOL ok;

            memset(&enc, 0, sizeof(enc));
            encode_signed(&enc, data, 3000, k ? 3000 : 0xffffffff);
            m = CryptMsgOpenToDecode(PKCS_7_ASN_ENCODING, 0, 0, 0, NULL, &si);
            ok = feed(m, enc.data, enc.len, pieces[i], &first, &d);
            CHECK(ok, "signed/%lu/%lu: update: %lx", pieces[i], k, GetLastError());
            CHECK(d.len == 3000 && !memcmp(d.data, data, 3000), "signed/%lu/%lu: content %lu", pieces[i], k, d.len);
            CHECK(d.final_calls == 1 && d.last_final && !d.after_final, "signed/%lu/%lu: final call: %lu", pieces[i], k, d.final_calls);
            if (pieces[i] <= 64) CHECK(first && first < enc.len - 100, "signed/%lu/%lu: output while the content arrives: at %lu of %lu", pieces[i], k, first, enc.len);
            ok = CryptMsgControl(m, 0, CMSG_CTRL_VERIFY_SIGNATURE, sign_cert->pCertInfo);
            CHECK(ok, "signed/%lu/%lu: the signature verifies: %lx", pieces[i], k, GetLastError());
            ok = get_content(m, &got, &glen);
            CHECK(ok && glen == 3000 && !memcmp(got, data, 3000), "signed/%lu/%lu: CMSG_CONTENT_PARAM", pieces[i], k);
            free(got);
            CryptMsgClose(m);
            free(d.data); free(enc.data);
        }
    }
    free(data);
}

static void enveloped_cases(void)
{
    char rc2[] = szOID_RSA_RC2CBC, des3[] = szOID_RSA_DES_EDE3_CBC, rc4[] = szOID_RSA_RC4;
    LPSTR algs[] = { rc2, des3, rc4 };
    DWORD sizes[] = { 1, 7, 8, 9, 100, 5000 }, a, si_, mode, piece;
    DWORD pieces[] = { 1, 33, 100000 };

    for (a = 0; a < 3; a++)
        for (si_ = 0; si_ < ARRAY_SIZE(sizes); si_++)
            for (mode = 0; mode < 3; mode++)
                for (piece = 0; piece < ARRAY_SIZE(pieces); piece++)
                {
                    DWORD total = sizes[si_];
                    BYTE *data = content(total);
                    struct acc enc = { 0 };
                    struct dec d = { 0 };
                    CMSG_STREAM_INFO si = { 0, dec_cb, &d };
                    CMSG_CTRL_DECRYPT_PARA dp;
                    HCRYPTMSG m;
                    DWORD off = 0, n, calls_before, alg_len = 0;
                    BOOL ok = TRUE, got_alg = FALSE, mid = FALSE;
                    char name[96];

                    if (piece == 1 && total > 100) { free(data); continue; }
                    sprintf(name, "enveloped alg %lu size %lu mode %lu piece %lu", a, total, mode, pieces[piece]);
                    encode_enveloped(&enc, data, total, algs[a], 0);
                    m = CryptMsgOpenToDecode(PKCS_7_ASN_ENCODING, 0, 0, 0, NULL, &si);
                    memset(&dp, 0, sizeof(dp));
                    dp.cbSize = sizeof(dp); dp.hCryptProv = prov; dp.dwKeySpec = AT_KEYEXCHANGE;
                    /* mode 0: decrypt as soon as the header is there; 1: after the whole message; 2: never asked, nothing comes */
                    while (off < enc.len && ok)
                    {
                        n = min(pieces[piece], enc.len - off);
                        alg_len = 0;
                        got_alg = CryptMsgGetParam(m, CMSG_ENVELOPE_ALGORITHM_PARAM, 0, NULL, &alg_len);
                        if (!got_alg && off == 0) CHECK(GetLastError() == CRYPT_E_STREAM_MSG_NOT_READY, "%s: not ready: %lx", name, GetLastError());
                        if (got_alg && !mid)
                        {
                            BYTE abuf[256];
                            DWORD asz = sizeof(abuf);
                            CRYPT_ALGORITHM_IDENTIFIER *ai = (void *)abuf;
                            CHECK(alg_len > sizeof(*ai) && CryptMsgGetParam(m, CMSG_ENVELOPE_ALGORITHM_PARAM, 0, abuf, &asz) &&
                                  ai->pszObjId && !strcmp(ai->pszObjId, algs[a]), "%s: the algorithm: %s", name, ai->pszObjId ? ai->pszObjId : "(none)");
                        }
                        if (got_alg && mode == 0 && !mid)
                        {
                            calls_before = d.calls;
                            ok = CryptMsgControl(m, 0, CMSG_CTRL_DECRYPT, &dp);
                            CHECK(ok, "%s: decrypt: %lx", name, GetLastError());
                            CHECK(d.calls >= calls_before, "calls");
                            mid = TRUE;
                            SetLastError(0xdeadbeef);
                            CHECK(!CryptMsgControl(m, 0, CMSG_CTRL_DECRYPT, &dp) && GetLastError() == CRYPT_E_ALREADY_DECRYPTED, "%s: decrypt twice mid-stream: %lx", name, GetLastError());
                        }
                        ok = ok && CryptMsgUpdate(m, enc.data + off, n, FALSE);
                        off += n;
                    }
                    if (mode == 0 && total >= 100 && pieces[piece] <= 33) CHECK(mid, "%s: the header was there before the end", name);
                    ok = ok && CryptMsgUpdate(m, NULL, 0, TRUE);
                    if (mode == 0 && !mid)
                    {
                        /* the whole message came in one piece: the header was not there to ask about before */
                        ok = CryptMsgControl(m, 0, CMSG_CTRL_DECRYPT, &dp);
                        CHECK(ok, "%s: decrypt after the end: %lx", name, GetLastError());
                    }
                    CHECK(ok, "%s: update: %lx", name, GetLastError());
                    if (mode == 2)
                    {
                        CHECK(d.calls == 0, "%s: no plain text before a key: %lu calls", name, d.calls);
                    }
                    else
                    {
                        if (mode == 1)
                        {
                            CHECK(d.calls == 0, "%s: nothing before the key", name);
                            ok = CryptMsgControl(m, 0, CMSG_CTRL_DECRYPT, &dp);
                            CHECK(ok, "%s: decrypt after the end: %lx", name, GetLastError());
                        }
                        CHECK(d.len == total && !memcmp(d.data, data, total), "%s: plain text %lu / %lu", name, d.len, total);
                        CHECK(d.final_calls == 1 && d.last_final && !d.after_final, "%s: one final call: %lu", name, d.final_calls);
                        SetLastError(0xdeadbeef);
                        ok = CryptMsgControl(m, 0, CMSG_CTRL_DECRYPT, &dp);
                        CHECK(!ok && GetLastError() == CRYPT_E_ALREADY_DECRYPTED, "%s: decrypt twice: %d %lx", name, ok, GetLastError());
                    }
                    CryptMsgClose(m);
                    free(d.data); free(enc.data); free(data);
                }
}

int main(void)
{
    setup();
    if (!sign_cert) { printf("RESULT: FAIL\n"); return 1; }
    data_cases();
    error_cases();
    signed_cases();
    enveloped_cases();
    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    return f != 0;
}
