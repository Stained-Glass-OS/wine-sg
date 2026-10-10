/* crypt32: countersignatures and CryptEnumOIDFunction (patches/sg/2419), run by
 * test/crypt32cs-gate.sh. CryptMsgCountersignEncoded makes the signer info of a
 * countersigner over a signer's signature value; CryptMsgCountersign adds it to
 * the signer of a decoded signed message as a counterSign attribute;
 * CryptEnumOIDFunction goes through the registered OID functions. */
#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

struct ident { HCRYPTPROV prov; PCCERT_CONTEXT cert; };

static int make_ident(struct ident *id, const char *container, const char *cn)
{
    CERT_NAME_BLOB name = {0};
    CRYPT_KEY_PROV_INFO ki = {0};
    WCHAR wcont[64];
    char subj[96];
    HCRYPTKEY key;

    MultiByteToWideChar(CP_ACP, 0, container, -1, wcont, 64);
    CryptAcquireContextA(&id->prov, container, MS_ENHANCED_PROV_A, PROV_RSA_FULL, CRYPT_DELETEKEYSET);
    if (!CryptAcquireContextA(&id->prov, container, MS_ENHANCED_PROV_A, PROV_RSA_FULL, CRYPT_NEWKEYSET)) return 0;
    if (!CryptGenKey(id->prov, AT_KEYEXCHANGE, 1024 << 16, &key)) return 0;
    CryptDestroyKey(key);
    sprintf(subj, "CN=%s", cn);
    CertStrToNameA(X509_ASN_ENCODING, subj, 0, NULL, NULL, &name.cbData, NULL);
    name.pbData = HeapAlloc(GetProcessHeap(), 0, name.cbData);
    CertStrToNameA(X509_ASN_ENCODING, subj, 0, NULL, name.pbData, &name.cbData, NULL);
    ki.pwszContainerName = wcont;
    ki.pwszProvName = (WCHAR *)MS_ENHANCED_PROV_W;
    ki.dwProvType = PROV_RSA_FULL;
    ki.dwKeySpec = AT_KEYEXCHANGE;
    id->cert = CertCreateSelfSignCertificate(id->prov, &name, 0, &ki, NULL, NULL, NULL, NULL);
    CryptReleaseContext(id->prov, 0);
    if (!CryptAcquireContextA(&id->prov, container, MS_ENHANCED_PROV_A, PROV_RSA_FULL, 0)) return 0;
    return id->cert != NULL;
}

static void signer_info(CMSG_SIGNER_ENCODE_INFO *si, struct ident *id)
{
    memset(si, 0, sizeof(*si));
    si->cbSize = sizeof(*si);
    si->pCertInfo = id->cert->pCertInfo;
    si->hCryptProv = id->prov;
    si->dwKeySpec = AT_KEYEXCHANGE;
    si->HashAlgorithm.pszObjId = (char *)szOID_OIWSEC_sha1;
}

/* SHA-1 of data, by the probe's own means */
static int sha1_of(const BYTE *data, DWORD len, BYTE out[20])
{
    HCRYPTPROV p;
    HCRYPTHASH h;
    DWORD n = 20;
    int ok = 0;

    if (!CryptAcquireContextA(&p, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) return 0;
    if (CryptCreateHash(p, CALG_SHA1, 0, 0, &h))
    {
        ok = CryptHashData(h, data, len, 0) && CryptGetHashParam(h, HP_HASHVAL, out, &n, 0);
        CryptDestroyHash(h);
    }
    CryptReleaseContext(p, 0);
    return ok;
}

static int seen;
static DWORD seen_type[8];
static char seen_func[8][64], seen_oid[8][64];
static DWORD seen_values[8], seen_dword[8];
static BOOL stop_after_first;

static BOOL WINAPI enum_cb(DWORD enc, LPCSTR func, LPCSTR oid, DWORD cValue, const DWORD types[], LPCWSTR const names[],
                           const BYTE *const data[], const DWORD sizes[], void *arg)
{
    DWORD i;

    if (seen < 8)
    {
        seen_type[seen] = enc;
        snprintf(seen_func[seen], 64, "%s", func);
        if ((ULONG_PTR)oid < 0x10000) snprintf(seen_oid[seen], 64, "#%u", (unsigned)(ULONG_PTR)oid);
        else snprintf(seen_oid[seen], 64, "%s", oid);
        seen_values[seen] = cValue;
        seen_dword[seen] = 0;
        for (i = 0; i < cValue; i++)
            if (!wcscmp(names[i], L"Extra") && types[i] == REG_DWORD && sizes[i] == 4) seen_dword[seen] = *(const DWORD *)data[i];
        seen++;
    }
    return !stop_after_first;
}

static int find_seen(const char *oid)
{
    int i;
    for (i = 0; i < seen; i++) if (!strcmp(seen_oid[i], oid)) return i;
    return -1;
}

int main(void)
{
    struct ident alice, bob, carol;
    static const BYTE plain[] = "countersign me";
    const BYTE *pieces[1] = { plain };
    DWORD sizes[1] = { sizeof(plain) };
    CRYPT_SIGN_MESSAGE_PARA sp;
    BYTE signed_blob[2048], *signer, *counter;
    DWORD signed_size = sizeof(signed_blob), signer_size = 0, counter_size = 0, size;
    CMSG_SIGNER_ENCODE_INFO bob_info, carol_info, two[2];
    HCRYPTMSG msg;
    BOOL ret;

    check(make_ident(&alice, "sgcs-alice", "Alice") && make_ident(&bob, "sgcs-bob", "Bob") &&
          make_ident(&carol, "sgcs-carol", "Carol"), "three identities");
    memset(&sp, 0, sizeof(sp));
    sp.cbSize = sizeof(sp);
    sp.dwMsgEncodingType = PKCS_7_ASN_ENCODING;
    sp.pSigningCert = alice.cert;
    sp.HashAlgorithm.pszObjId = (char *)szOID_RSA_SHA1RSA;
    ret = CryptSignMessage(&sp, FALSE, 1, pieces, sizes, signed_blob, &signed_size);
    check(ret, "Alice signs a message");
    msg = CryptMsgOpenToDecode(PKCS_7_ASN_ENCODING, 0, 0, 0, NULL, NULL);
    check(CryptMsgUpdate(msg, signed_blob, signed_size, TRUE), "and it is decoded");
    CryptMsgGetParam(msg, CMSG_ENCODED_SIGNER, 0, NULL, &signer_size);
    signer = HeapAlloc(GetProcessHeap(), 0, signer_size);
    check(CryptMsgGetParam(msg, CMSG_ENCODED_SIGNER, 0, signer, &signer_size), "her signer info is taken");

    signer_info(&bob_info, &bob);
    signer_info(&carol_info, &carol);
    {
        static BYTE attr_value[] = { 0x13, 0x02, 'o', 'k' };  /* PrintableString "ok" */
        static CRYPT_ATTR_BLOB blob = { sizeof(attr_value), attr_value };
        static CRYPT_ATTRIBUTE attr = { (char *)"1.3.6.1.4.1.99999.1", 1, &blob };

        carol_info.cAuthAttr = 1;
        carol_info.rgAuthAttr = &attr;
    }

    /* CryptMsgCountersignEncoded */
    ret = CryptMsgCountersignEncoded(PKCS_7_ASN_ENCODING, signer, signer_size, 1, &bob_info, NULL, &counter_size);
    check(ret && counter_size > 128, "Bob's countersignature is sized");
    counter = HeapAlloc(GetProcessHeap(), 0, counter_size);
    ret = CryptMsgCountersignEncoded(PKCS_7_ASN_ENCODING, signer, signer_size, 1, &bob_info, counter, &counter_size);
    check(ret, "and made");
    {
        CMSG_SIGNER_INFO *alice_si, *bob_si;
        DWORD n;

        CryptDecodeObjectEx(PKCS_7_ASN_ENCODING, PKCS7_SIGNER_INFO, signer, signer_size, CRYPT_DECODE_ALLOC_FLAG, NULL, &alice_si, &n);
        CryptDecodeObjectEx(PKCS_7_ASN_ENCODING, PKCS7_SIGNER_INFO, counter, counter_size, CRYPT_DECODE_ALLOC_FLAG, NULL, &bob_si, &n);
        check(bob_si && bob_si->Issuer.cbData == bob.cert->pCertInfo->Issuer.cbData &&
              !memcmp(bob_si->Issuer.pbData, bob.cert->pCertInfo->Issuer.pbData, bob_si->Issuer.cbData) &&
              bob_si->SerialNumber.cbData == bob.cert->pCertInfo->SerialNumber.cbData &&
              !memcmp(bob_si->SerialNumber.pbData, bob.cert->pCertInfo->SerialNumber.pbData, bob_si->SerialNumber.cbData),
              "it names Bob (issuer and serial number)");
        check(bob_si && bob_si->EncryptedHash.cbData == 128 && alice_si && memcmp(bob_si->EncryptedHash.pbData,
              alice_si->EncryptedHash.pbData, 128), "with a signature of its own");
        LocalFree(alice_si);
        LocalFree(bob_si);
    }
    /* Carol's has an authenticated attribute of its own, and the digest of Alice's signature value */
    {
        BYTE *carol_counter;
        DWORD carol_size = 0;
        CMSG_SIGNER_INFO *alice_si, *carol_si;
        BYTE digest[20];
        DWORD i, n;
        int digest_ok = 0, attr_ok = 0;

        check(CryptMsgCountersignEncoded(PKCS_7_ASN_ENCODING, signer, signer_size, 1, &carol_info, NULL, &carol_size) &&
              (carol_counter = HeapAlloc(GetProcessHeap(), 0, carol_size)) &&
              CryptMsgCountersignEncoded(PKCS_7_ASN_ENCODING, signer, signer_size, 1, &carol_info, carol_counter, &carol_size),
              "Carol's countersignature with an authenticated attribute is made");
        CryptDecodeObjectEx(PKCS_7_ASN_ENCODING, PKCS7_SIGNER_INFO, signer, signer_size, CRYPT_DECODE_ALLOC_FLAG, NULL, &alice_si, &n);
        CryptDecodeObjectEx(PKCS_7_ASN_ENCODING, PKCS7_SIGNER_INFO, carol_counter, carol_size, CRYPT_DECODE_ALLOC_FLAG, NULL, &carol_si, &n);
        if (carol_si && alice_si && sha1_of(alice_si->EncryptedHash.pbData, alice_si->EncryptedHash.cbData, digest))
            for (i = 0; i < carol_si->AuthAttrs.cAttr; i++)
            {
                if (!strcmp(carol_si->AuthAttrs.rgAttr[i].pszObjId, "1.3.6.1.4.1.99999.1")) attr_ok = 1;
                if (!strcmp(carol_si->AuthAttrs.rgAttr[i].pszObjId, szOID_RSA_messageDigest))
                {
                    CRYPT_DATA_BLOB *d = NULL;
                    DWORD dn;
                    if (CryptDecodeObjectEx(PKCS_7_ASN_ENCODING, X509_OCTET_STRING, carol_si->AuthAttrs.rgAttr[i].rgValue[0].pbData,
                                            carol_si->AuthAttrs.rgAttr[i].rgValue[0].cbData, CRYPT_DECODE_ALLOC_FLAG, NULL, &d, &dn) && d)
                    {
                        digest_ok = d->cbData == 20 && !memcmp(d->pbData, digest, 20);
                        LocalFree(d);
                    }
                }
            }
        check(attr_ok, "it carries the attribute");
        check(digest_ok, "and a message digest that is the SHA-1 of Alice's signature value");
        check(CryptMsgVerifyCountersignatureEncodedEx(0, PKCS_7_ASN_ENCODING, signer, signer_size, carol_counter, carol_size,
              CMSG_VERIFY_SIGNER_CERT, (void *)carol.cert, 0, NULL), "which verifies against Carol's certificate");
        check(CryptMsgVerifyCountersignatureEncodedEx(0, PKCS_7_ASN_ENCODING, signer, signer_size, carol_counter, carol_size,
              CMSG_VERIFY_SIGNER_PUBKEY, &carol.cert->pCertInfo->SubjectPublicKeyInfo, 0, NULL), "and against her public key");
        check(!CryptMsgVerifyCountersignatureEncodedEx(0, PKCS_7_ASN_ENCODING, signer, signer_size, carol_counter, carol_size,
              CMSG_VERIFY_SIGNER_CERT, (void *)bob.cert, 0, NULL), "but not Bob's");
        check(CryptMsgVerifyCountersignatureEncoded(0, PKCS_7_ASN_ENCODING, signer, signer_size, carol_counter, carol_size,
              carol.cert->pCertInfo), "the older call agrees");
        {
            BYTE *other = HeapAlloc(GetProcessHeap(), 0, signer_size);
            memcpy(other, signer, signer_size);
            other[signer_size - 1] ^= 1;
            SetLastError(0xdeadbeef);
            check(!CryptMsgVerifyCountersignatureEncodedEx(0, PKCS_7_ASN_ENCODING, other, signer_size, carol_counter, carol_size,
                  CMSG_VERIFY_SIGNER_NULL, NULL, 0, NULL) && GetLastError() == CRYPT_E_HASH_VALUE,
                  "other bytes underneath: the digest no longer matches (CRYPT_E_HASH_VALUE), without a signer too");
            HeapFree(GetProcessHeap(), 0, other);
        }
        LocalFree(alice_si);
        LocalFree(carol_si);
        HeapFree(GetProcessHeap(), 0, carol_counter);
    }
    ret = CryptMsgVerifyCountersignatureEncodedEx(0, PKCS_7_ASN_ENCODING, signer, signer_size, counter, counter_size,
     CMSG_VERIFY_SIGNER_CERT, (void *)bob.cert, 0, NULL);
    check(ret, "it verifies against Bob's certificate");
    SetLastError(0xdeadbeef);
    ret = CryptMsgVerifyCountersignatureEncodedEx(0, PKCS_7_ASN_ENCODING, signer, signer_size, counter, counter_size,
     CMSG_VERIFY_SIGNER_CERT, (void *)carol.cert, 0, NULL);
    check(!ret, "but not against Carol's");
    {
        BYTE *other = HeapAlloc(GetProcessHeap(), 0, signer_size);
        DWORD k;
        memcpy(other, signer, signer_size);
        for (k = signer_size; k-- > 40;) { other[k] ^= 1; break; }  /* inside the signature value */
        ret = CryptMsgVerifyCountersignatureEncodedEx(0, PKCS_7_ASN_ENCODING, other, signer_size, counter, counter_size,
         CMSG_VERIFY_SIGNER_CERT, (void *)bob.cert, 0, NULL);
        check(!ret, "nor with a different signature underneath");
        HeapFree(GetProcessHeap(), 0, other);
    }
    SetLastError(0xdeadbeef);
    memset(two, 0, sizeof(two)); two[0] = bob_info; two[1] = carol_info;
    ret = CryptMsgCountersignEncoded(PKCS_7_ASN_ENCODING, signer, signer_size, 2, two, NULL, &size);
    check(!ret && GetLastError() == E_INVALIDARG, "two countersigners at once: E_INVALIDARG");
    SetLastError(0xdeadbeef);
    ret = CryptMsgCountersignEncoded(PKCS_7_ASN_ENCODING, (BYTE *)plain, sizeof(plain), 1, &bob_info, NULL, &size);
    check(!ret, "bytes that are no signer info: failure");

    /* CryptMsgCountersign */
    {
        DWORD unauth = 0;
        CRYPT_ATTRIBUTES *attrs;
        DWORD before = signer_size, after = 0;

        CryptMsgGetParam(msg, CMSG_SIGNER_UNAUTH_ATTR_PARAM, 0, NULL, &unauth);
        check(CryptMsgCountersign(msg, 0, 1, &carol_info), "Carol countersigns Alice's signer in the decoded message");
        SetLastError(0xdeadbeef);
        check(!CryptMsgCountersign(msg, 5, 1, &carol_info) && GetLastError() == CRYPT_E_INVALID_INDEX, "a signer that is not there: CRYPT_E_INVALID_INDEX");
        size = 0;
        check(CryptMsgGetParam(msg, CMSG_SIGNER_UNAUTH_ATTR_PARAM, 0, NULL, &size) && size > 0, "the signer now has unauthenticated attributes");
        attrs = HeapAlloc(GetProcessHeap(), 0, size);
        if (CryptMsgGetParam(msg, CMSG_SIGNER_UNAUTH_ATTR_PARAM, 0, attrs, &size))
        {
            check(attrs->cAttr == 1 && !strcmp(attrs->rgAttr[0].pszObjId, szOID_RSA_counterSign) && attrs->rgAttr[0].cValue == 1,
                  "one: the counterSign attribute");
            if (attrs->cAttr == 1 && attrs->rgAttr[0].cValue == 1)
                check(CryptMsgVerifyCountersignatureEncodedEx(0, PKCS_7_ASN_ENCODING, signer, signer_size,
                      attrs->rgAttr[0].rgValue[0].pbData, attrs->rgAttr[0].rgValue[0].cbData, CMSG_VERIFY_SIGNER_CERT,
                      (void *)carol.cert, 0, NULL), "and it is Carol's countersignature of Alice's signature");
        }
        check(CryptMsgCountersign(msg, 0, 1, &bob_info), "Bob countersigns too");
        size = 0;
        CryptMsgGetParam(msg, CMSG_SIGNER_UNAUTH_ATTR_PARAM, 0, NULL, &size);
        CryptMsgGetParam(msg, CMSG_ENCODED_SIGNER, 0, NULL, &after);
        check(after > before + 128, "the encoded signer carries them");
        HeapFree(GetProcessHeap(), 0, attrs);
        attrs = HeapAlloc(GetProcessHeap(), 0, size);
        if (CryptMsgGetParam(msg, CMSG_SIGNER_UNAUTH_ATTR_PARAM, 0, attrs, &size))
        {
            check(attrs->cAttr == 2 && !strcmp(attrs->rgAttr[0].pszObjId, szOID_RSA_counterSign) &&
                  !strcmp(attrs->rgAttr[1].pszObjId, szOID_RSA_counterSign), "the signer now has both countersignatures, added one after the other");
            if (attrs->cAttr == 2)
                check(CryptMsgVerifyCountersignatureEncodedEx(0, PKCS_7_ASN_ENCODING, signer, signer_size,
                      attrs->rgAttr[1].rgValue[0].pbData, attrs->rgAttr[1].rgValue[0].cbData, CMSG_VERIFY_SIGNER_CERT,
                      (void *)bob.cert, 0, NULL), "the second is Bob's");
        }
        HeapFree(GetProcessHeap(), 0, attrs);
    }
    CryptMsgClose(msg);

    /* CryptEnumOIDFunction */
    {
        static const WCHAR dll[] = L"sgenum.dll";
        DWORD v = 77;

        CryptUnregisterOIDFunction(1, "SgEnumFunc", "1.2.3.4.5");
        CryptUnregisterOIDFunction(1, "SgEnumFunc", (LPCSTR)4242);
        CryptUnregisterOIDFunction(2, "SgEnumFunc", "1.2.3.4.5");
        CryptUnregisterOIDFunction(1, "SgEnumOther", "1.2.3.4.5");
        check(CryptRegisterOIDFunction(1, "SgEnumFunc", "1.2.3.4.5", dll, "Foo") &&
              CryptRegisterOIDFunction(1, "SgEnumFunc", (LPCSTR)4242, dll, "Bar") &&
              CryptRegisterOIDFunction(2, "SgEnumFunc", "1.2.3.4.5", dll, "Baz") &&
              CryptRegisterOIDFunction(1, "SgEnumOther", "1.2.3.4.5", dll, "Qux"), "four functions are registered");
        CryptSetOIDFunctionValue(1, "SgEnumFunc", "1.2.3.4.5", L"Extra", REG_DWORD, (BYTE *)&v, 4);

        seen = 0; stop_after_first = FALSE;
        ret = CryptEnumOIDFunction(1, "SgEnumFunc", NULL, 0, NULL, enum_cb);
        check(ret && seen == 2 && find_seen("1.2.3.4.5") >= 0 && find_seen("#4242") >= 0 &&
              seen_type[0] == 1 && seen_type[1] == 1 && !strcmp(seen_func[0], "SgEnumFunc"), "one encoding type and function name: its two OIDs, numeric as a number");
        { int i = find_seen("1.2.3.4.5"); check(i >= 0 && seen_values[i] == 3 && seen_dword[i] == 77, "with their values (Dll, FuncName, Extra)"); }
        seen = 0;
        ret = CryptEnumOIDFunction(1, "SgEnumFunc", "1.2.3.4.5", 0, NULL, enum_cb);
        check(ret && seen == 1 && !strcmp(seen_oid[0], "1.2.3.4.5"), "and a single OID");
        seen = 0;
        ret = CryptEnumOIDFunction(1, "SgEnumFunc", (LPCSTR)4242, 0, NULL, enum_cb);
        check(ret && seen == 1 && !strcmp(seen_oid[0], "#4242"), "a numeric one given as a number");
        seen = 0;
        ret = CryptEnumOIDFunction(CRYPT_MATCH_ANY_ENCODING_TYPE, "SgEnumFunc", "1.2.3.4.5", 0, NULL, enum_cb);
        check(ret && seen == 2 && ((seen_type[0] == 1 && seen_type[1] == 2) || (seen_type[0] == 2 && seen_type[1] == 1)), "any encoding type: both types");
        seen = 0;
        ret = CryptEnumOIDFunction(CRYPT_MATCH_ANY_ENCODING_TYPE, NULL, "1.2.3.4.5", 0, NULL, enum_cb);
        check(ret && seen == 3, "any function name too: the three registered for that OID");
        seen = 0; stop_after_first = TRUE;
        ret = CryptEnumOIDFunction(CRYPT_MATCH_ANY_ENCODING_TYPE, NULL, "1.2.3.4.5", 0, NULL, enum_cb);
        check(!ret && seen == 1, "a callback that says no stops it, and the call fails");
        stop_after_first = FALSE;
        seen = 0;
        ret = CryptEnumOIDFunction(3, "SgEnumFunc", NULL, 0, NULL, enum_cb);
        check(ret && seen == 0, "nothing registered for another encoding type");
        SetLastError(0xdeadbeef);
        check(!CryptEnumOIDFunction(1, "SgEnumFunc", NULL, 1, NULL, enum_cb) && GetLastError() == E_INVALIDARG, "flags that are not 0: E_INVALIDARG");

        CryptUnregisterOIDFunction(1, "SgEnumFunc", "1.2.3.4.5");
        CryptUnregisterOIDFunction(1, "SgEnumFunc", (LPCSTR)4242);
        CryptUnregisterOIDFunction(2, "SgEnumFunc", "1.2.3.4.5");
        CryptUnregisterOIDFunction(1, "SgEnumOther", "1.2.3.4.5");
        seen = 0;
        ret = CryptEnumOIDFunction(CRYPT_MATCH_ANY_ENCODING_TYPE, "SgEnumFunc", NULL, 0, NULL, enum_cb);
        check(ret && seen == 0, "unregistered, they are gone");
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
