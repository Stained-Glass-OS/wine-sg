/* crypt32: decrypting, decoding and signing without a certificate
 * (patches/sg/2418), run by test/crypt32dec-gate.sh. Certificates with RSA keys
 * are made here; CryptDecryptMessage, CryptSignAndEncryptMessage,
 * CryptDecryptAndVerifyMessageSignature, CryptDecodeMessage,
 * CryptSignMessageWithKey, CryptVerifyMessageSignatureWithKey and
 * CryptMsgCalculateEncodedLength were stubs. */
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
    /* the keys are kept once the creating context is released; a cert's key is opened by name */
    CryptReleaseContext(id->prov, 0);
    if (!CryptAcquireContextA(&id->prov, container, MS_ENHANCED_PROV_A, PROV_RSA_FULL, 0)) return 0;
    return id->cert != NULL;
}

static PCCERT_CONTEXT WINAPI pick_cert(void *arg, DWORD enc, PCERT_INFO id, HCERTSTORE store)
{
    return CertDuplicateCertificateContext((PCCERT_CONTEXT)arg);
}

int main(void)
{
    struct ident alice, bob, carol;
    static const BYTE plain[] = "The quick brown fox jumps over the lazy dog";
    const BYTE *pieces[1] = { plain };
    DWORD sizes[1] = { sizeof(plain) };
    HCERTSTORE alice_store, bob_store, empty_store;
    CRYPT_ENCRYPT_MESSAGE_PARA enc;
    CRYPT_SIGN_MESSAGE_PARA sign;
    CRYPT_DECRYPT_MESSAGE_PARA dec;
    CRYPT_VERIFY_MESSAGE_PARA ver;
    PCCERT_CONTEXT recip[1], xchg, signer;
    BYTE *blob, out[256], *signed_enc;
    DWORD blobSize, outSize, i;
    BOOL ret;

    check(make_ident(&alice, "sgdec-alice", "Alice") && make_ident(&bob, "sgdec-bob", "Bob") &&
          make_ident(&carol, "sgdec-carol", "Carol"), "three identities with RSA keys");
    alice_store = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, CERT_STORE_CREATE_NEW_FLAG, NULL);
    bob_store = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, CERT_STORE_CREATE_NEW_FLAG, NULL);
    empty_store = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, CERT_STORE_CREATE_NEW_FLAG, NULL);
    CertAddCertificateContextToStore(alice_store, alice.cert, CERT_STORE_ADD_ALWAYS, NULL);
    CertAddCertificateContextToStore(bob_store, bob.cert, CERT_STORE_ADD_ALWAYS, NULL);

    memset(&enc, 0, sizeof(enc));
    enc.cbSize = sizeof(enc);
    enc.dwMsgEncodingType = PKCS_7_ASN_ENCODING;
    enc.hCryptProv = 0;
    enc.ContentEncryptionAlgorithm.pszObjId = (char *)szOID_RSA_RC4;
    recip[0] = alice.cert;
    blobSize = 0;
    ret = CryptEncryptMessage(&enc, 1, recip, plain, sizeof(plain), NULL, &blobSize);
    blob = HeapAlloc(GetProcessHeap(), 0, blobSize + 1);
    ret = ret && CryptEncryptMessage(&enc, 1, recip, plain, sizeof(plain), blob, &blobSize);
    check(ret, "a message for Alice is encrypted");

    memset(&dec, 0, sizeof(dec));
    dec.cbSize = sizeof(dec);
    dec.dwMsgAndCertEncodingType = PKCS_7_ASN_ENCODING;
    dec.cCertStore = 1;
    dec.rghCertStore = &alice_store;

    /* CryptDecryptMessage */
    outSize = 0;
    xchg = NULL;
    ret = CryptDecryptMessage(&dec, blob, blobSize, NULL, &outSize, &xchg);
    check(ret && outSize == sizeof(plain), "the decrypted size is asked for");
    if (xchg) CertFreeCertificateContext(xchg);
    xchg = NULL;
    outSize = 3;
    SetLastError(0xdeadbeef);
    ret = CryptDecryptMessage(&dec, blob, blobSize, out, &outSize, &xchg);
    check(!ret && GetLastError() == ERROR_MORE_DATA && outSize == sizeof(plain), "a short buffer: ERROR_MORE_DATA and the size");
    if (xchg) CertFreeCertificateContext(xchg);
    xchg = NULL;
    outSize = sizeof(out);
    memset(out, 0, sizeof(out));
    ret = CryptDecryptMessage(&dec, blob, blobSize, out, &outSize, &xchg);
    check(ret && outSize == sizeof(plain) && !memcmp(out, plain, sizeof(plain)), "Alice decrypts it");
    check(xchg && xchg->cbCertEncoded == alice.cert->cbCertEncoded &&
          !memcmp(xchg->pbCertEncoded, alice.cert->pbCertEncoded, xchg->cbCertEncoded), "and gets her certificate back");
    if (xchg) CertFreeCertificateContext(xchg);
    dec.rghCertStore = &bob_store;
    SetLastError(0xdeadbeef);
    outSize = sizeof(out);
    ret = CryptDecryptMessage(&dec, blob, blobSize, out, &outSize, NULL);
    check(!ret && GetLastError() == CRYPT_E_NO_DECRYPT_CERT, "Bob has no way to: CRYPT_E_NO_DECRYPT_CERT");
    dec.rghCertStore = &empty_store;
    SetLastError(0xdeadbeef);
    ret = CryptDecryptMessage(&dec, blob, blobSize, out, &outSize, NULL);
    check(!ret && GetLastError() == CRYPT_E_NO_DECRYPT_CERT, "nor an empty store");
    dec.rghCertStore = &alice_store;
    SetLastError(0xdeadbeef);
    { CRYPT_DECRYPT_MESSAGE_PARA bad = dec; bad.cbSize = 3; ret = CryptDecryptMessage(&bad, blob, blobSize, out, &outSize, NULL); }
    check(!ret && GetLastError() == E_INVALIDARG, "a parameter block of the wrong size: E_INVALIDARG");
    SetLastError(0xdeadbeef);
    ret = CryptDecryptMessage(&dec, plain, sizeof(plain), out, &outSize, NULL);
    check(!ret, "bytes that are no message do not decrypt");

    /* CryptSignAndEncryptMessage / CryptDecryptAndVerifyMessageSignature */
    memset(&sign, 0, sizeof(sign));
    sign.cbSize = sizeof(sign);
    sign.dwMsgEncodingType = PKCS_7_ASN_ENCODING;
    sign.pSigningCert = bob.cert;
    sign.HashAlgorithm.pszObjId = (char *)szOID_RSA_SHA1RSA;
    sign.cMsgCert = 1;
    sign.rgpMsgCert = &bob.cert;
    blobSize = 0;
    ret = CryptSignAndEncryptMessage(&sign, &enc, 1, recip, plain, sizeof(plain), NULL, &blobSize);
    check(ret && blobSize > 0, "a message signed by Bob and encrypted for Alice is sized");
    signed_enc = HeapAlloc(GetProcessHeap(), 0, blobSize + 1);
    ret = CryptSignAndEncryptMessage(&sign, &enc, 1, recip, plain, sizeof(plain), signed_enc, &blobSize);
    check(ret, "and made");
    memset(&ver, 0, sizeof(ver));
    ver.cbSize = sizeof(ver);
    ver.dwMsgAndCertEncodingType = PKCS_7_ASN_ENCODING;
    outSize = sizeof(out);
    memset(out, 0, sizeof(out));
    xchg = signer = NULL;
    ret = CryptDecryptAndVerifyMessageSignature(&dec, &ver, 0, signed_enc, blobSize, out, &outSize, &xchg, &signer);
    check(ret && outSize == sizeof(plain) && !memcmp(out, plain, sizeof(plain)), "Alice decrypts it and the signature verifies");
    check(xchg && signer && signer->cbCertEncoded == bob.cert->cbCertEncoded &&
          !memcmp(signer->pbCertEncoded, bob.cert->pbCertEncoded, signer->cbCertEncoded), "the signer is Bob");
    if (xchg) CertFreeCertificateContext(xchg);
    if (signer) CertFreeCertificateContext(signer);
    {
        BYTE *copy = HeapAlloc(GetProcessHeap(), 0, blobSize);
        outSize = sizeof(out);
        SetLastError(0xdeadbeef);
        memcpy(copy, signed_enc, blobSize);
        copy[blobSize - 7] ^= 0x55;  /* in the encrypted content */
        ret = CryptDecryptAndVerifyMessageSignature(&dec, &ver, 0, copy, blobSize, out, &outSize, NULL, NULL);
        check(!ret, "a damaged message fails");
        HeapFree(GetProcessHeap(), 0, copy);
    }
    /* signed by someone else than the certificate the callback names */
    ver.pfnGetSignerCertificate = pick_cert;
    ver.pvGetArg = (void *)carol.cert;
    outSize = sizeof(out);
    ret = CryptDecryptAndVerifyMessageSignature(&dec, &ver, 0, signed_enc, blobSize, out, &outSize, NULL, NULL);
    check(!ret, "verified against another signer's certificate it fails");
    ver.pfnGetSignerCertificate = NULL;
    /* decrypting alone gives the signed message */
    outSize = 0;
    ret = CryptDecryptMessage(&dec, signed_enc, blobSize, NULL, &outSize, NULL);
    check(ret && CryptGetMessageSignerCount(PKCS_7_ASN_ENCODING, signed_enc, blobSize) < 1 && outSize > sizeof(plain),
          "decrypting alone gives the signed message (the outer one is no signed message)");

    /* CryptDecodeMessage, one layer a call */
    {
        DWORD type = 99, inner = 99;
        BYTE *layer = HeapAlloc(GetProcessHeap(), 0, 4096);

        outSize = 4096;
        xchg = signer = NULL;
        ret = CryptDecodeMessage(CMSG_ENVELOPED_FLAG | CMSG_SIGNED_FLAG, &dec, &ver, 0, signed_enc, blobSize, 0, &type, &inner,
         layer, &outSize, &xchg, &signer);
        check(ret && type == CMSG_ENVELOPED && inner == CMSG_SIGNED && !signer, "the outer layer is enveloped and holds a signed message");
        if (xchg) CertFreeCertificateContext(xchg);
        if (ret)
        {
            BYTE *inside = HeapAlloc(GetProcessHeap(), 0, outSize);
            DWORD insideSize = outSize;
            memcpy(inside, layer, outSize);
            type = inner = 99;
            outSize = sizeof(out);
            memset(out, 0, sizeof(out));
            ret = CryptDecodeMessage(CMSG_ENVELOPED_FLAG | CMSG_SIGNED_FLAG, &dec, &ver, 0, inside, insideSize, 0, &type, &inner,
             out, &outSize, NULL, &signer);
            check(ret && type == CMSG_SIGNED && inner == CMSG_DATA && outSize == sizeof(plain) && !memcmp(out, plain, sizeof(plain)),
                  "the next is signed and holds the data");
            check(signer != NULL, "and names its signer");
            if (signer) CertFreeCertificateContext(signer);
            HeapFree(GetProcessHeap(), 0, inside);
        }
        SetLastError(0xdeadbeef);
        outSize = 4096;
        ret = CryptDecodeMessage(CMSG_DATA_FLAG, &dec, &ver, 0, signed_enc, blobSize, 0, &type, &inner, layer, &outSize, NULL, NULL);
        check(!ret && GetLastError() == CRYPT_E_INVALID_MSG_TYPE, "a type not asked for: CRYPT_E_INVALID_MSG_TYPE");
        SetLastError(0xdeadbeef);
        outSize = 4096;
        ret = CryptDecodeMessage(CMSG_SIGNED_FLAG, &dec, &ver, 0, plain, sizeof(plain), 0, &type, &inner, layer, &outSize, NULL, NULL);
        check(!ret, "bytes that are no message are no layer");
        /* a hashed message */
        {
            CRYPT_HASH_MESSAGE_PARA hp;
            BYTE hashed[512];
            DWORD hsize = sizeof(hashed);

            memset(&hp, 0, sizeof(hp));
            hp.cbSize = sizeof(hp);
            hp.dwMsgEncodingType = PKCS_7_ASN_ENCODING;
            hp.HashAlgorithm.pszObjId = (char *)szOID_RSA_MD5;
            ret = CryptHashMessage(&hp, FALSE, 1, pieces, sizes, hashed, &hsize, NULL, NULL);
            check(ret, "a hashed message is made");
            outSize = sizeof(out);
            memset(out, 0, sizeof(out));
            ret = CryptDecodeMessage(CMSG_HASHED_FLAG, &dec, &ver, 0, hashed, hsize, 0, &type, &inner, out, &outSize, NULL, NULL);
            check(ret && type == CMSG_HASHED && outSize == sizeof(plain) && !memcmp(out, plain, sizeof(plain)),
                  "and decoded, its hash checked");
        }
        HeapFree(GetProcessHeap(), 0, layer);
    }

    /* signed with a bare key */
    {
        CRYPT_KEY_SIGN_MESSAGE_PARA kp;
        CRYPT_KEY_VERIFY_MESSAGE_PARA vp;
        CERT_PUBLIC_KEY_INFO *pub, *other;
        DWORD pubSize = 0, otherSize = 0;
        BYTE *sig;
        DWORD sigSize = 0;

        CryptExportPublicKeyInfo(carol.prov, AT_KEYEXCHANGE, X509_ASN_ENCODING, NULL, &pubSize);
        pub = HeapAlloc(GetProcessHeap(), 0, pubSize);
        CryptExportPublicKeyInfo(carol.prov, AT_KEYEXCHANGE, X509_ASN_ENCODING, pub, &pubSize);
        CryptExportPublicKeyInfo(bob.prov, AT_KEYEXCHANGE, X509_ASN_ENCODING, NULL, &otherSize);
        other = HeapAlloc(GetProcessHeap(), 0, otherSize);
        CryptExportPublicKeyInfo(bob.prov, AT_KEYEXCHANGE, X509_ASN_ENCODING, other, &otherSize);

        memset(&kp, 0, sizeof(kp));
        kp.cbSize = sizeof(kp);
        kp.dwMsgAndCertEncodingType = PKCS_7_ASN_ENCODING;
        kp.hCryptProv = carol.prov;
        kp.dwKeySpec = AT_KEYEXCHANGE;
        kp.HashAlgorithm.pszObjId = (char *)szOID_RSA_SHA1RSA;
        ret = CryptSignMessageWithKey(&kp, plain, sizeof(plain), NULL, &sigSize);
        check(ret && sigSize > sizeof(plain), "CryptSignMessageWithKey sizes a message");
        sig = HeapAlloc(GetProcessHeap(), 0, sigSize);
        ret = CryptSignMessageWithKey(&kp, plain, sizeof(plain), sig, &sigSize);
        check(ret, "and makes it");
        check(CryptGetMessageSignerCount(PKCS_7_ASN_ENCODING, sig, sigSize) == 1, "it has one signer");

        memset(&vp, 0, sizeof(vp));
        vp.cbSize = sizeof(vp);
        vp.dwMsgEncodingType = PKCS_7_ASN_ENCODING;
        outSize = sizeof(out);
        memset(out, 0, sizeof(out));
        ret = CryptVerifyMessageSignatureWithKey(&vp, pub, sig, sigSize, out, &outSize);
        check(ret && outSize == sizeof(plain) && !memcmp(out, plain, sizeof(plain)), "it verifies with the signer's public key and gives the content");
        outSize = 0;
        ret = CryptVerifyMessageSignatureWithKey(&vp, pub, sig, sigSize, NULL, &outSize);
        check(ret && outSize == sizeof(plain), "the content size is asked for");
        SetLastError(0xdeadbeef);
        outSize = sizeof(out);
        ret = CryptVerifyMessageSignatureWithKey(&vp, other, sig, sigSize, out, &outSize);
        check(!ret, "it does not verify with another key");
        {
            BYTE *copy = HeapAlloc(GetProcessHeap(), 0, sigSize);
            memcpy(copy, sig, sigSize);
            for (i = 0; i + sizeof(plain) <= sigSize; i++)
                if (!memcmp(copy + i, plain, 8)) { copy[i + 2] ^= 1; break; }
            outSize = sizeof(out);
            ret = CryptVerifyMessageSignatureWithKey(&vp, pub, copy, sigSize, out, &outSize);
            check(!ret, "nor with changed content");
            HeapFree(GetProcessHeap(), 0, copy);
        }
        SetLastError(0xdeadbeef);
        kp.cbSize = 4;
        ret = CryptSignMessageWithKey(&kp, plain, sizeof(plain), sig, &sigSize);
        check(!ret && GetLastError() == E_INVALIDARG, "a parameter block of the wrong size: E_INVALIDARG");
        HeapFree(GetProcessHeap(), 0, sig);
        HeapFree(GetProcessHeap(), 0, pub);
        HeapFree(GetProcessHeap(), 0, other);
    }

    /* CryptMsgCalculateEncodedLength */
    {
        CMSG_HASHED_ENCODE_INFO hi;
        CMSG_SIGNED_ENCODE_INFO si;
        CMSG_SIGNER_ENCODE_INFO sg;
        DWORD calc, real = 0;
        HCRYPTMSG msg;

        calc = CryptMsgCalculateEncodedLength(PKCS_7_ASN_ENCODING, 0, CMSG_DATA, NULL, NULL, 100);
        msg = CryptMsgOpenToEncode(PKCS_7_ASN_ENCODING, 0, CMSG_DATA, NULL, NULL, NULL);
        { BYTE zeros[100] = {0}; CryptMsgUpdate(msg, zeros, 100, TRUE); }
        CryptMsgGetParam(msg, CMSG_CONTENT_PARAM, 0, NULL, &real);
        CryptMsgClose(msg);
        check(calc && calc == real && calc > 100, "a data message of 100 bytes: the length it has");

        memset(&hi, 0, sizeof(hi));
        hi.cbSize = sizeof(hi);
        hi.HashAlgorithm.pszObjId = (char *)szOID_RSA_MD5;
        calc = CryptMsgCalculateEncodedLength(PKCS_7_ASN_ENCODING, 0, CMSG_HASHED, &hi, NULL, 1000);
        check(calc > 1000 + 16, "a hashed message of 1000 bytes is longer than its parts");
        check(CryptMsgCalculateEncodedLength(PKCS_7_ASN_ENCODING, 0, CMSG_HASHED, &hi, NULL, 2000) == calc + 1000 + 1 + 1 ||
              CryptMsgCalculateEncodedLength(PKCS_7_ASN_ENCODING, 0, CMSG_HASHED, &hi, NULL, 2000) > calc + 990,
              "and grows with the data");

        memset(&sg, 0, sizeof(sg));
        sg.cbSize = sizeof(sg);
        sg.pCertInfo = bob.cert->pCertInfo;
        sg.hCryptProv = bob.prov;
        sg.dwKeySpec = AT_KEYEXCHANGE;
        sg.HashAlgorithm.pszObjId = (char *)szOID_RSA_SHA1RSA;
        memset(&si, 0, sizeof(si));
        si.cbSize = sizeof(si);
        si.cSigners = 1;
        si.rgSigners = &sg;
        calc = CryptMsgCalculateEncodedLength(PKCS_7_ASN_ENCODING, 0, CMSG_SIGNED, &si, NULL, 50);
        check(calc > 50 + 128, "a signed message of 50 bytes carries a signature of the key's size");
        SetLastError(0xdeadbeef);
        check(CryptMsgCalculateEncodedLength(X509_ASN_ENCODING, 0, CMSG_DATA, NULL, NULL, 10) == 0, "an encoding type that is not PKCS7: 0");
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
